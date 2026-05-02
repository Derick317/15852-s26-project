#include "maximal_leafy.h"

/**
 * Randomly match vertices on the left and vertices on the right
 * such that a vertex in the left hand side matches 0 or at least 2 vertices
 * on the right, and a vertex on the right matches at most 1 vertex on the 
 * left.
 *
 * @param left all inner sequences should be non-empty
 * @param right all inner sequences should be non-empty
 */
auto leafy_match(const nested_seq& left, const nested_seq& right)
  -> std::pair<parlay::sequence<size_t>, parlay::sequence<ssize_t>> {
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
 * @param left_remain a one-hot vector
 */
void filter_left_right(
  nested_seq& left, parlay::sequence<ssize_t>& left_v,
  nested_seq& right, parlay::sequence<ssize_t>& right_v,
  const parlay::sequence<ssize_t>& right_remain
) {
  parlay::parallel_for(0, left_v.size(), [&left, &right_remain] (size_t i) {
    left[i] = parlay::filter(left[i], [&right_remain] (ssize_t j) {
      return right_remain[j];
    });
  });
  const auto left_remain = parlay::map(left, [] (const auto& s) {
    return ssize_t(s.size() > 0); 
  });

  const auto [right_new_indices, right_num] = parlay::scan(right_remain);
  const auto [left_new_indices, left_num] = parlay::scan(left_remain);
  filter_by_onehot(right, right_remain);
  filter_by_onehot(left, left_remain);
  filter_by_onehot(right_v, right_remain);
  filter_by_onehot(left_v, left_remain);
  
  parlay::parallel_for(0, right.size(), [&right, &left_new_indices] (size_t i) {
    right[i] = parlay::map(right[i], [i, &right, &left_new_indices] (ssize_t r) {
      return left_new_indices[r];
    });
  });
  parlay::parallel_for(0, left.size(), [&left, &right_new_indices] (size_t i) {
    left[i] = parlay::map(left[i], [i, &left, &right_new_indices] (ssize_t l) {
      return right_new_indices[l];
    });
  });
}

auto create_left_right(
  const graph& G, const parlay::sequence<bool>& prevented, 
  const parlay::sequence<ssize_t>& left_v,
  parlay::sequence<ssize_t>& as_right_indices
) -> std::tuple<nested_seq, nested_seq, parlay::sequence<ssize_t>> {
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
 */
void maximal_expand(
  const graph& G, 
  parlay::sequence<bool>& visited, 
  parlay::sequence<ssize_t>& starts,
  parlay::sequence<ssize_t>& as_right_indices
) {
  auto [left, right, right_v] = create_left_right(G, visited, starts, as_right_indices);
  auto left_remain = parlay::map(left, [] (const auto& s) { 
    return ssize_t(s.size() > 1);
  });



  return;
}