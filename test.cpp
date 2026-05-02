#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "maximal_leafy.h"
#include "graph_contraction.h"

TEST_CASE("test doctest") {
  CHECK(2 + 2 == 4);
}

TEST_CASE("Create left & right") {
  nested_seq G{{0, 5}, {0}, {6}, {0, 6, 7, 3}, {6, 7, 4}, {7, 3, 4}, {0}, {1}};
  parlay::sequence<bool> blocked{false, false, false, true, true, true, false, false};
  parlay::sequence<ssize_t> left_v{1, 2, 3, 4, 5};
  parlay::sequence<ssize_t> as_right_indices(G.size());
  auto [left, right, right_v] = create_left_right(
    G, blocked, left_v, as_right_indices
  );
  CHECK(left == nested_seq{{0}, {1}, {0, 1, 2}, {1, 2}, {2}});
  CHECK(right == nested_seq{{0, 2}, {1, 2, 3}, {2, 3, 4}});
  CHECK(right_v == parlay::sequence<ssize_t>{0, 6, 7});
  CHECK(as_right_indices[0] == 0);
  CHECK(as_right_indices[6] == 1);
  CHECK(as_right_indices[7] == 2);
}

TEST_CASE("Filter left & right") {
  nested_seq right{{0}, {1}, {0, 1, 2}, {1, 2}, {2}};
  parlay::sequence<ssize_t> right_v{10, 11, 12, 13, 14};
  nested_seq left{{0, 2}, {1, 2, 3}, {2, 3, 4}};
  parlay::sequence<ssize_t> left_v{20, 21, 22};
  parlay::sequence<ssize_t> right_remain{false, false, false, true, true};

  filter_left_right(left, left_v, right, right_v, right_remain);

  CHECK(left == nested_seq{{0}, {0, 1}});
  CHECK(left_v == parlay::sequence<ssize_t>{21, 22});
  CHECK(right == nested_seq{{0, 1}, {1}});
  CHECK(right_v == parlay::sequence<ssize_t>{13, 14});
}

TEST_CASE("Graph Distance Naive") {
  size_t n = 10;
  auto prevs = parlay::tabulate<ssize_t>(n, [] (size_t i) { return i; });
  auto result = distance(prevs);
  CHECK(parlay::sequence<ssize_t>(n, 0) == result);
}

TEST_CASE("Graph Distance No Cycles") {
  auto prevs = parlay::sequence<ssize_t>{1, 2, 3, 4, 5, 5, 6, 8, 8};
  auto result = distance(prevs);
  CHECK(parlay::sequence<ssize_t>{5, 4, 3, 2, 1, 0, 0, 1, 0} == result);
}

void verify_graph_distance(
  const parlay::sequence<ssize_t>& prevs,
  const parlay::sequence<ssize_t>& result
) {
  CHECK(prevs.size() == result.size());
  parlay::parallel_for(
    0, prevs.size(),
    [&prevs, &result] (size_t i) {
      ssize_t v = i;
      size_t dist;
      for (dist = 0; dist < prevs.size(); ++dist) {
        if (result[v] == 0) {
          break;
        }
        v = prevs[v];
      }
      CHECK(result[i] == dist);
      ssize_t u = v;
      for (size_t j = 0; j < prevs.size(); ++j) {
        u = prevs[u];
        if (result[u] == 0) {
          break;
        }
      }
      CHECK(u == v);
    }
  );
}

TEST_CASE("Graph Distance Simple Cycle") {
  auto prevs = parlay::sequence<ssize_t>{1, 2, 3, 4, 5, 6, 0};
  auto result = distance(prevs);
  CHECK(parlay::filter(result, [] (auto i) { return i == 0; }).size() == 1);
  auto head_index = parlay::find(result, 0) - result.begin();
  for (size_t i = 0; i < head_index; ++i) {
    CHECK(result[i] == head_index - i);
  }
  for (size_t i = head_index + 1; i < prevs.size(); ++i) {
    CHECK(result[i] == head_index + prevs.size() - i);
  }
  verify_graph_distance(prevs, result);
}

TEST_CASE("Graph Distance Complex") {
  auto prevs = parlay::sequence<ssize_t>{
    1, 2, 3, 4, 0,
    6, 7, 8, 9, 10, 11, 5, 5, 5, 5, 12, 15
  };
  
  auto result = distance(prevs);
  verify_graph_distance(prevs, result);
}