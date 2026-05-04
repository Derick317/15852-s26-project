#include "maximal_leafy.h"
#include "graph_contraction.h"

/**
 * Randomly match vertices on the left and vertices on the right
 * such that a vertex in the left hand side matches 0 or at least 2 vertices
 * on the right, and a vertex on the right matches at most 1 vertex on the 
 * left.
 *
 * @param left all inner sequences should be non-empty
 * @param right all inner sequences should be non-empty
 *
 * @note if an inner sequence in `left` has fewer than 2 elements, it is
 * possible that no elements are matched.
 */
auto leafy_match(const nested_seq& left, const nested_seq& right)
  -> std::pair<parlay::sequence<size_t>, ssize_t_seq> {
  auto priorities = parlay::random_permutation(left.size());
  auto right_neighbor_priorities = parlay::tabulate<parlay::sequence<size_t>>(
    right.size(), [&right, &priorities] (long i) {
      return parlay::map(right[i], [&priorities] (ssize_t v) { return priorities[v]; });
    }
  );
  
  auto right_selection = parlay::tabulate<ssize_t>(
    right.size(), 
    [&right, &right_neighbor_priorities] (long i) {
      return right[i][reduce_maximum(right_neighbor_priorities[i])];
    }
  );

  auto left_selected = parlay::tabulate<parlay::sequence<bool>>(
    left.size(),
    [&left, &right_selection] (long i) {
      return parlay::map(left[i], [i, &right_selection] (ssize_t s) {
        return i == right_selection[s];
      });
    }
  );

  auto left_selected_count = parlay::map(left_selected, [] (const auto& s) {
    return parlay::count(s, true);
  });

  parlay::parallel_for(
    0, right_selection.size(), 
    [&left_selected_count, &right_selection] (size_t i) {
      if (left_selected_count[right_selection[i]] < 2) {
        right_selection[i] = INVALID_INDEX;
      }
    }
  );

  // Now, for all valid left vertices (selected by at least 2 right vertices),
  // match all unmatched neighbors
  parlay::parallel_for(
    0, right.size(), 
    [&right, &right_selection, &left_selected_count] (size_t i) {
      if (right_selection[i] == INVALID_INDEX) {
        auto good_neighbor_iter = parlay::find_if(
          right[i], 
          [&left_selected_count] (size_t l) {
            return left_selected_count[l] >= 2;
          }
        );
        if (good_neighbor_iter != right[i].end()) {
          right_selection[i] = *good_neighbor_iter;
        }
      }
    }
  );

  return {left_selected_count, right_selection};
}

/**
 * @param x_remain a one-hot vector
 */
void filter_bipartite(
  nested_seq& x, ssize_t_seq& x_v,
  nested_seq& y, ssize_t_seq& y_v,
  const ssize_t_seq& y_remain
) {
  parlay::parallel_for(0, x_v.size(), [&x, &y_remain] (size_t i) {
    x[i] = parlay::filter(x[i], [&y_remain] (ssize_t j) {
      return y_remain[j];
    });
  });
  const auto x_remain = parlay::map(x, [] (const auto& s) {
    return ssize_t(s.size() > 0); 
  });

  const auto [y_new_indices, y_num] = parlay::scan(y_remain);
  const auto [x_new_indices, x_num] = parlay::scan(x_remain);
  filter_by_onehot(y, y_remain);
  filter_by_onehot(x, x_remain);
  filter_by_onehot(y_v, y_remain);
  filter_by_onehot(x_v, x_remain);
  
  parlay::parallel_for(0, y.size(), [&y, &x_new_indices] (size_t i) {
    y[i] = parlay::map(y[i], [i, &y, &x_new_indices] (ssize_t r) {
      return x_new_indices[r];
    });
  });
  parlay::parallel_for(0, x.size(), [&x, &y_new_indices] (size_t i) {
    x[i] = parlay::map(x[i], [i, &x, &y_new_indices] (ssize_t l) {
      return y_new_indices[l];
    });
  });
}

/**
 * @post `as_right_indices` should be consistent with `right_v`, such that
 * `as_right_indices[right_v[i]] == i`, and if x is not in `right_v`,
 * `as_right_indices[x]` is unchanged.
 */
auto create_left_right(
  const graph& G, const parlay::sequence<bool>& prevented, 
  const ssize_t_seq& left_v,
  ssize_t_seq& as_right_indices
) -> std::tuple<nested_seq, nested_seq, ssize_t_seq> {
  auto left = parlay::map(left_v, [&prevented, &G] (ssize_t v) {
    return parlay::filter(G[v], [&prevented] (ssize_t r) { return !prevented[r]; });
  });

  auto right_grouped = parlay::group_by_key(parlay::flatten(
    parlay::tabulate<parlay::sequence<std::pair<ssize_t, ssize_t>>>(
      left.size(),
      [&left] (long i) {
        return parlay::map(left[i], [i] (ssize_t r) {
          return std::pair<ssize_t, ssize_t>{r, i};
        });
      }
    )
  ));
  auto right = parlay::map(right_grouped, [] (const auto& p) { return p.second; });
  auto right_v = parlay::map(right_grouped, [] (const auto& p) { return p.first; });

  parlay::parallel_for(0, right.size(), [&as_right_indices, &right_v] (size_t i) {
    as_right_indices[right_v[i]] = i;
  });
  parlay::parallel_for(0, left.size(), [&as_right_indices, &left] (size_t i) {
    parlay::parallel_for(0, left[i].size(), [i, &as_right_indices, &left] (size_t j) {
      left[i][j] = as_right_indices[left[i][j]];
    });
  });

  return {left, right, right_v};
}

/**
 * Expand the graph from vertices `starts`. Each vertex in `starts` should
 * expand to 0 or at least 2 unvisited vertices. If a vertex in `starts`
 * is expanded by another, we can discard its expansion.
 *
 * @return right_v vertex IDs of all vertices in the next level
 * @return right_selection vertex IDs of selections of vertices in the next
 * level
 *
 * @post `as_right_indices` should be consistent with `right_v`, such that
 * `as_right_indices[right_v[i]] == i`, and if x is not in `right_v`,
 * `as_right_indices[x]` is unchanged.
 * @post For an unvisited vertex of index i, it selects `right_selection[i]`,
 * and there must be another unvisited vertex also selects `right_selection[i]`.
 */
auto maximal_expand(
  const graph& G, 
  const parlay::sequence<bool>& visited, 
  const ssize_t_seq& starts,
  ssize_t_seq& as_right_indices
) -> std::pair<ssize_t_seq, ssize_t_seq> {
  auto [left, right, right_v] = create_left_right(G, visited, starts, as_right_indices);
  auto right_v_init = right_v;
  auto left_v = starts;
  auto right_selection = ssize_t_seq(right_v.size(), INVALID_INDEX);
  while (true) {
    auto left_remain = parlay::map(left, [] (const auto& s) { 
      return ssize_t(s.size() > 1);
    });
    filter_bipartite(right, right_v, left, left_v, left_remain);
    if (left.empty()) {
      break;
    }
    auto [_, current_right_selection] = leafy_match(left, right);
    parlay::parallel_for(
      0, right_v.size(),
      [&current_right_selection, &right_selection, &left_v, &right_v, &as_right_indices] 
      (size_t i) {
        if (auto idx_in_left = current_right_selection[i]; idx_in_left != INVALID_INDEX) {
          right_selection[as_right_indices[right_v[i]]] = left_v[idx_in_left];
        }
      }
    );

    auto right_remain = parlay::map(current_right_selection, [] (ssize_t x) {
      return ssize_t(x == INVALID_INDEX);
    });
    filter_bipartite(left, left_v, right, right_v, right_remain);
  }

  return {right_v_init, right_selection};
}

/**
 * @pre `leaves` and `pending_parents` are all visited;
 * @pre `pendings` are not visited
 * @pre `as_right_indices` are all `INVALID_INDEX`'s
 * @pre `as_start_indices` are all `INVALID_INDEX`'s
 * @return right_v
 * @return right_selection
 * @return next_pendings
 * @return next_pending_parents
 * @post `as_right_indices` are consistent with `right_v`
 * @post `as_start_indices` are all `INVALID_INDEX`;
 */
auto level_expand(
  const graph& G, 
  const parlay::sequence<bool>& visited,
  const ssize_t_seq& leaves,
  const ssize_t_seq& pendings,
  const ssize_t_seq& pending_parents,
  ssize_t_seq& as_right_indices,
  ssize_t_seq& as_start_indices
) -> std::tuple<ssize_t_seq, ssize_t_seq, ssize_t_seq, ssize_t_seq> {
  auto starts = ssize_t_seq(leaves.size() + pendings.size());
  parlay::parallel_for(
    0, leaves.size(),
    [&leaves, &starts, &as_start_indices] (size_t i) {
      starts[i] = leaves[i];
      as_start_indices[leaves[i]] = i;
    }
  );
  parlay::parallel_for(
    0, pendings.size(),
    [offset = leaves.size(), &pendings, &starts, &as_start_indices] (size_t i) {
      starts[i + offset] = pendings[i];
      as_start_indices[pendings[i]] = i + offset;
    }
  );

  auto [right_v, right_selection] = maximal_expand(G, visited, starts, as_right_indices);
  auto left_selector = parlay::map(
    starts, 
    [&G, &right_selection, &as_right_indices] (auto l) {
      return parlay::filter(G[l], [&as_right_indices, &right_selection, l] (auto r) {
        return as_right_indices[r] != INVALID_INDEX 
          && l == right_selection[as_right_indices[r]];
      });
    }
  );
  auto start_selection = parlay::tabulate(
    starts.size(), 
    [&starts, &as_right_indices, &as_start_indices, &right_selection] (ssize_t i) {
      auto v = starts[i];
      if (
        auto right_index = as_right_indices[v]; 
        right_index == INVALID_INDEX || right_selection[right_index] == INVALID_INDEX
      ) {
        return i;
      } else {
        return as_start_indices[right_selection[right_index]];
      }
    }
  );
  auto distances = graph_distance(start_selection);
  parlay::parallel_for(
    0, starts.size(),
    [
      &distances, &starts, &right_selection, &left_selector, &pending_parents,
      &as_right_indices, &as_start_indices, leaf_num = leaves.size()
    ] (size_t i) {
      if (distances[i] % 2 == 1) {
        parlay::parallel_for(
          0, left_selector[i].size(),
          [&left_selector, &right_selection, &as_right_indices, i, &as_start_indices] 
          (size_t j) {
            auto r_v = left_selector[i][j];
            if (as_start_indices[r_v] == INVALID_INDEX) {
              auto r_idx = as_right_indices[r_v];
              right_selection[r_idx] = INVALID_INDEX;
            }
          }
        );
      } else if (i >= leaf_num && as_right_indices[starts[i]] != INVALID_INDEX) {
        right_selection[as_right_indices[starts[i]]] = pending_parents[i - leaf_num];
      }
    }
  );

  /**
   * if 2x is the last node in a cycle of odd length, `odd_cycle_last[2x]` is
   * the first node in the cycle; otherwise, it is INVALID_INDEX.
   *
   * (2x) <-- 0 <-
   */
  auto odd_cycle_last = ssize_t_seq(starts.size(), INVALID_INDEX);
  parlay::parallel_for(
    0, starts.size(), 
    [&odd_cycle_last, &distances, &start_selection] (size_t i) {
      auto select_dist = distances[start_selection[i]];
      if (
        distances[i] == 0 && select_dist > 0 && select_dist % 2 == 0) {
        odd_cycle_last[start_selection[i]] = i;
      }
    }
  );

  /**
   * Note a node should be selected by at least 2 nodes to be considered here.
   *
   * - 2x is selected by at least 2 nodes other than 0, just remove 0's selection of 2x
   * - 2x is selected by only 1 other node
   * - - this node is not a starting node, then just ignore 2x
   * - - this node is another starting node, which distance is (2x+1)
   * - - - (2x+1) is selected by another non-starting node, let 2x select (2x+1)
   * - - - (2x+1) is selected by only starting node, then just ignore 2x
   *
   * (2x+2) -x-> (2x+1) ---> 2x <-x- 0 <--- 1 <--- ...
   *               ^          \
   *               |           x-> ...
   *           non-start
   * becomes
   * (2x+2) -x-> (2x+1) <--- 2x <-x- 0 <--- 1 <--- ...
   *               ^          \
   *               |           x-> ...
   *           non-start
   */
  parlay::parallel_for(
    0, starts.size(), 
    [
      &odd_cycle_last, &as_start_indices, &right_selection, &as_right_indices, 
      &left_selector, &starts, &pending_parents
    ] (size_t i) {
      auto zero_index = odd_cycle_last[i];
      if (zero_index != INVALID_INDEX) {
        if (left_selector[i].size() < 3) {
          auto another_selector = left_selector[i][0];
          if (another_selector == starts[zero_index]) {
            another_selector = left_selector[i][1];
          } 
          auto another_index = as_start_indices[another_selector];
          auto right_index = as_right_indices[starts[i]];
          if (another_index == INVALID_INDEX) {
            right_selection[right_index] = INVALID_INDEX;
          } else {
            auto iter =  parlay::find_if(
              left_selector[another_index],
              [&as_start_indices] (auto r_v) {
                return as_start_indices[r_v] == INVALID_INDEX;
              }
            );
            if (iter == left_selector[another_index].end()) {
              right_selection[right_index] = INVALID_INDEX;
            } else {
              right_selection[right_index] = another_selector;
              auto another_right_index = as_right_indices[another_selector];
              right_selection[another_right_index] = pending_parents[another_index];
            }
          }
        }
      }
    }
  );

  // Find the pending list of the next level
  auto become_next_pending = parlay::sequence<std::atomic_flag>(right_v.size());
  parlay::parallel_for(
    0, right_v.size(),
    [&become_next_pending] (auto i) {
      become_next_pending[i].clear();
    }
  );
  auto unselect_neighbor = parlay::map(
    leaves,
    [&G, &as_right_indices, &right_selection, &become_next_pending] (ssize_t v) {
      auto n = parlay::filter(G[v], [&as_right_indices, &right_selection] (ssize_t r) {
        return right_selection[as_right_indices[r]] == INVALID_INDEX;
      });
      if (n.empty()) {
        return INVALID_INDEX;
      }
      auto neighbor = n[0];
      if (become_next_pending[as_right_indices[neighbor]].test_and_set()) {
        return INVALID_INDEX;
      }
      return neighbor;
    }
  );
  auto is_parent = parlay::map(
    unselect_neighbor,
    [] (auto v) { return v != INVALID_INDEX; }
  );
  auto parents = leaves;
  filter_by_onehot(parents, is_parent);
  filter_by_onehot(unselect_neighbor, is_parent);

  // Add pendings to right_v and right_selection
  auto pendings_to_add = parlay::tabulate<bool>(
    pendings.size(),
    [&pendings, &as_right_indices, &left_selector, &leaves] (size_t i) {
      return as_right_indices[pendings[i]] == INVALID_INDEX 
        && left_selector[i + leaves.size()].size() >= 2;
    }
  );
  auto right_v_from_pending = pendings;
  auto right_selection_from_pending = pending_parents;
  filter_by_onehot(right_v_from_pending, pendings_to_add);
  filter_by_onehot(right_selection_from_pending, pendings_to_add);
  right_v.append(right_v_from_pending.begin(), right_v_from_pending.end());
  right_selection.append(
    right_selection_from_pending.begin(),
    right_selection_from_pending.end()
  );

  parlay::parallel_for(0, starts.size(), [&starts, &as_start_indices] (auto i) {
    as_start_indices[starts[i]] = INVALID_INDEX;
  });
  return {right_v, right_selection, unselect_neighbor, parents};
}


auto leafy_forest(const graph& G) -> parlay::sequence<std::tuple<ssize_t, ssize_t>> {
  ssize_t root = -1;
  parlay::sequence<std::tuple<ssize_t, ssize_t>> forest_edges;
  auto visited = parlay::sequence<bool>(G.size(), false);
  ssize_t_seq as_right_indices(G.size(), INVALID_INDEX);
  ssize_t_seq as_start_indices(G.size(), INVALID_INDEX);
  while (true) {
    auto new_root_iter = parlay::find_if(
      G.tail(G.size() - (root + 1)),
      [&visited] (const auto& vs) { 
        return parlay::count_if(vs, [&visited] (auto v) { return !visited[v]; }) >= 3; 
      }
    );
    root = new_root_iter - G.begin();
    if (root >= G.size()) {
      break;
    }
    visited[root] = true;
    ssize_t_seq leaves{root};
    ssize_t_seq pendings{};
    ssize_t_seq pending_parents{};
    while (!leaves.empty() || !pendings.empty()) {
      auto [right_v, right_selection, next_pendings, next_parents] = level_expand(
        G, visited, leaves, pendings, pending_parents, 
        as_right_indices, as_start_indices
      );
      auto new_edges = parlay::zip(right_v, right_selection);
      new_edges = parlay::filter(
        new_edges, 
        [] (auto e) { return std::get<1>(e) != INVALID_INDEX; }
      );
      parlay::parallel_for(
        0, new_edges.size(),
        [&new_edges, &visited] (size_t i) {
          visited[std::get<0>(new_edges[i])] = true;
        }
      );
      forest_edges.append(new_edges.begin(), new_edges.end());

      auto good_leaf = parlay::sequence<bool>(right_v.size(), true);
      parlay::parallel_for(
        0, right_v.size(),
        [&good_leaf, &right_selection, &as_right_indices] (size_t i) {
          if (right_selection[i] == INVALID_INDEX) {
            good_leaf[i] = false;
            return;
          }
          auto select_r_idx = as_right_indices[right_selection[i]];
          if (select_r_idx != INVALID_INDEX) {
            good_leaf[select_r_idx] = false;
          }
        }
      );
      leaves = right_v;
      filter_by_onehot(leaves, good_leaf);
      pendings = next_pendings;
      pending_parents = next_parents;

      parlay::parallel_for(0, right_v.size(), [&right_v, &as_right_indices] (auto i) {
        as_right_indices[right_v[i]] = INVALID_INDEX;
      });
    }
  }

  return forest_edges;
}