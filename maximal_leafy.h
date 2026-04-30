#include <atomic>
#include "parlaylib/include/parlay/primitives.h"
//#include "parlaylib/include/parlay/random.h"
#include "parlaylib/include/parlay/sequence.h"

using nested_seq = parlay::sequence<parlay::sequence<ssize_t>>;
using graph = nested_seq;

constexpr ssize_t INVALID_INDEX = -1; 

template <typename vertex, typename graph>
auto hotspots(parlay::sequence<vertex> U, const graph& G) {
  vertex n = G.size();

  // n indicates that has not been visited
  auto nearest = parlay::tabulate<std::atomic<vertex>>(n, [&] (long i) {return n;});

  // mark the set U as visited from self
  parlay::parallel_for(0, U.size(), [&] (long i) {
    nearest[U[i]] = U[i];});

  // to be filled in
  std::cout << "nothing running : need to implement" << std::endl;

  // convert from sequence of atomics to a regular sequence
  return parlay::tabulate(n, [&] (long i) {return nearest[i].load();});
}

template <typename T>
struct DataAndIndex {
  T data;
  ssize_t index;
  bool operator<(const DataAndIndex& rhs) const {
    return data < rhs.data;
  }
};

template <typename T>
auto reduce_maximum(const parlay::sequence<T>& seq) -> ssize_t {
  auto seq_with_index = parlay::tabulate<DataAndIndex<T>>(seq.size(), [&seq] (long i) {
    return DataAndIndex<T>{seq[i], i};
  });
  auto identity = DataAndIndex<T>{std::numeric_limits<T>::lowest(), INVALID_INDEX};
  return parlay::reduce(seq_with_index, parlay::maximum<DataAndIndex<T>>(identity)).index;
}

template <typename T>
void print_seq(const parlay::sequence<T>& seq, std::string name="") {
  if (!name.empty()) {
    std::cout << name << ": ";
  }
  
  for (auto x: seq) {
    std::cout << x << ' ';
  }
  std::cout << std::endl;
}

template <typename T>
void print_nested_seq(const parlay::sequence<T>& nested_seq, std::string name="") {
  if (!name.empty()) {
    std::cout << name << ":\n";
  }
  for (const auto& x: nested_seq) {
    print_seq(x);
  }
}

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

void filter_left_right(
  nested_seq& left, parlay::sequence<ssize_t>& left_v,
  nested_seq& right, parlay::sequence<ssize_t>& right_v,
  const parlay::sequence<ssize_t>& left_remain
) {
  parlay::parallel_for(0, right_v.size(), [&right, &left_remain] (size_t i) {
    right[i] = parlay::filter(right[i], [&left_remain] (ssize_t j) {
      return left_remain[j];
    });
  });
  auto right_remain = parlay::map(right, [] (const auto& s) {
    return ssize_t(s.size() > 0); 
  });
  auto left_new_indices = parlay::scan(left_remain).first;
  auto right_new_indices = parlay::scan(right_remain).first;
  left = parlay::map(left_new_indices, [&left] (ssize_t i) { return left[i]; });
  right = parlay::map(right_new_indices, [&right] (ssize_t i) {
    return right[i];
  });
  parlay::parallel_for(0, left.size(), [&left, &right_remain, &right_new_indices] (size_t i) {
    left[i] = parlay::filter(left[i], [&right_remain] (ssize_t j) {
      return right_remain[j];
    });
    left[i] = parlay::map(left[i], [i, &left, &right_new_indices] (ssize_t j) {
      return right_new_indices[left[i][j]];
    });
  });
  parlay::parallel_for(0, right.size(), [&right, &left_new_indices] (size_t i) {
    right[i] = parlay::map(right[i], [i, &right, &left_new_indices] (ssize_t j) {
      return left_new_indices[right[i][j]];
    });
  });
}

/**
 * Expand the graph from vertices `starts`. Each vertex in `starts` should
 * expand to 0 or at least 2 unvisited vertices. If a vertex in `starts`
 * is expanded by another, we can discard its expansion.
 */
auto maximal_expand(
  const graph& G, 
  parlay::sequence<bool>& visited, 
  parlay::sequence<ssize_t>& starts,
  parlay::sequence<ssize_t>& as_left_indices,
  parlay::sequence<ssize_t>& as_right_indices
) {
  auto left = parlay::map(starts, [&visited, &G] (ssize_t v) {
    return parlay::filter(G[v], [&visited] (ssize_t r) { return !visited[r]; });
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
  parlay::parallel_for(0, left.size(), [&as_left_indices, &starts] (size_t i) {
    as_left_indices[starts[i]] = i;
  });
  parlay::parallel_for(0, right.size(), [&as_right_indices, &right_v] (size_t i) {
    as_right_indices[right_v[i]] = i;
  });
  parlay::parallel_for(0, left.size(), [&as_right_indices, &left] (size_t i) {
    parlay::parallel_for(0, left[i].size(), [i, &as_right_indices, &left] (size_t j) {
      left[i][j] = as_right_indices[left[i][j]];
    });
  });
  parlay::parallel_for(0, right.size(), [&right, &as_left_indices] (size_t i) {
    parlay::parallel_for(0, right[i].size(), [i, &right, &as_left_indices] (size_t j) {
      right[i][j] = as_left_indices[right[i][j]];
    });
  });

  auto left_remain = parlay::map(left, [] (const auto& s) { 
    return ssize_t(s.size() > 1);
  });


  return;
}