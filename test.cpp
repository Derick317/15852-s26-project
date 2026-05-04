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
  ssize_t_seq left_v{1, 2, 3, 4, 5};
  ssize_t_seq as_right_indices(G.size());
  auto [left, right, right_v] = create_left_right(
    G, blocked, left_v, as_right_indices
  );
  CHECK(left == nested_seq{{0}, {1}, {0, 1, 2}, {1, 2}, {2}});
  CHECK(right == nested_seq{{0, 2}, {1, 2, 3}, {2, 3, 4}});
  CHECK(right_v == ssize_t_seq{0, 6, 7});
  CHECK(as_right_indices[0] == 0);
  CHECK(as_right_indices[6] == 1);
  CHECK(as_right_indices[7] == 2);
}

TEST_CASE("Filter left & right") {
  nested_seq right{{0}, {1}, {0, 1, 2}, {1, 2}, {2}};
  ssize_t_seq right_v{10, 11, 12, 13, 14};
  nested_seq left{{0, 2}, {1, 2, 3}, {2, 3, 4}};
  ssize_t_seq left_v{20, 21, 22};
  ssize_t_seq right_remain{false, false, false, true, true};

  filter_bipartite(left, left_v, right, right_v, right_remain);

  CHECK(left == nested_seq{{0}, {0, 1}});
  CHECK(left_v == ssize_t_seq{21, 22});
  CHECK(right == nested_seq{{0, 1}, {1}});
  CHECK(right_v == ssize_t_seq{13, 14});
}

TEST_CASE("Graph Distance Naive") {
  size_t n = 10;
  auto prevs = parlay::tabulate<ssize_t>(n, [] (size_t i) { return i; });
  auto result = graph_distance(prevs);
  CHECK(ssize_t_seq(n, 0) == result);
}

TEST_CASE("Graph Distance No Cycles") {
  auto prevs = ssize_t_seq{1, 2, 3, 4, 5, 5, 6, 8, 8};
  auto result = graph_distance(prevs);
  CHECK(ssize_t_seq{5, 4, 3, 2, 1, 0, 0, 1, 0} == result);
}

void verify_graph_distance(
  const ssize_t_seq& prevs,
  const ssize_t_seq& result
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
  auto prevs = ssize_t_seq{1, 2, 3, 4, 5, 6, 0};
  auto result = graph_distance(prevs);
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
  auto prevs = ssize_t_seq{
    1, 2, 3, 4, 0,
    6, 7, 8, 9, 10, 11, 5, 5, 5, 5, 12, 15
  };
  
  auto result = graph_distance(prevs);
  verify_graph_distance(prevs, result);
}

void verify_maximal_expand(
  const graph& G,
  const parlay::sequence<bool>& visited, 
  const ssize_t_seq& starts,
  const ssize_t_seq& right_v,
  const ssize_t_seq& right_selection,
  ssize_t_seq as_right_indices
) {
  // Check if `right_v` is consistent with `as_right_indices`
  parlay::parallel_for(0, right_v.size(), [&right_v, &as_right_indices] (size_t i) {
    CHECK(i == as_right_indices[right_v[i]]);
    as_right_indices[right_v[i]] = INVALID_INDEX;
  });
  parlay::parallel_for(0, as_right_indices.size(), [&as_right_indices] (size_t i) {
    CHECK(as_right_indices[i] == INVALID_INDEX);
  });

  std::vector<ssize_t> starts_selection_count(starts.size());
  for (size_t i = 0; i < right_v.size(); ++i) {
    CHECK(!visited[right_v[i]]);
    if (auto selected = right_selection[i]; selected != INVALID_INDEX) {
      starts_selection_count[selected] += 1;
    }
  }
  for (auto count: starts_selection_count) {
    CHECK(count != 1);
  }
  for (auto start: starts) {
    size_t unpaired_neighbor_count = 0;
    for (auto neighbor: G[start]) {
      if (!visited[neighbor]) {
        auto right_v_iter = parlay::find(right_v, neighbor);
        CHECK(right_v_iter != right_v.end());
        auto index = right_v_iter - right_v.begin();
        if (right_selection[index] == INVALID_INDEX) {
          ++unpaired_neighbor_count;
          CHECK(starts_selection_count[start] == 0);
        }
      }
    }
    CHECK(unpaired_neighbor_count < 2);
  }
}

TEST_CASE("Maximal Expand Simple") {
  graph G{{3, 4}, {4, 5}, {5, 6}, {0}, {0, 1}, {1, 2}, {2}};
  ssize_t_seq starts{0, 1, 2};
  parlay::sequence<bool> visited{true, true, true, false, false, false, false};
  ssize_t_seq as_right_indices(G.size(), INVALID_INDEX);
  auto [right_v, right_selection] = maximal_expand(
    G, visited, starts, as_right_indices
  );
  verify_maximal_expand(G, visited, starts, right_v, right_selection, as_right_indices);
}

TEST_CASE("Maximal Expand Medium") {
  size_t num_leaf = 17, num_outside = 23, num_edge = 150;
  parlay::sequence<std::set<ssize_t>> G_set(num_leaf + num_outside);
  for (size_t i = 0; i < num_edge; ++i) {
    auto outside = (i * (i + 1) / 2) % num_outside + num_leaf;
    auto leaf = i % num_leaf;
    G_set[leaf].insert(outside);
    G_set[outside].insert(leaf);
  }
  graph G = parlay::map(
    G_set, 
    [] (auto& n_set) { return ssize_t_seq(n_set.begin(), n_set.end()); }
  );
  auto starts = parlay::tabulate<ssize_t>(num_leaf, [] (auto x) { return x; });
  parlay::sequence<bool> visited(G.size(), false);
  parlay::parallel_for(0, num_leaf, [&visited] (size_t i) { visited[i] = true; });
  ssize_t_seq as_right_indices(G.size(), INVALID_INDEX);
  auto [right_v, right_selection] = maximal_expand(
    G, visited, starts, as_right_indices
  );
  verify_maximal_expand(G, visited, starts, right_v, right_selection, as_right_indices);
}


TEST_CASE("Maximal Expand Complex") {
  size_t num_v = 31, num_edge = 150;
  parlay::sequence<std::set<ssize_t>> G_set(num_v);
  for (size_t i = 0; i < num_edge; ++i) {
    auto v1 = (i * (i + 1) / 2) % num_v;
    auto v2 = i * 3 % num_v;
    if (v1 != v2) {
      G_set[v1].insert(v2);
      G_set[v2].insert(v1);
    }
  }
  graph G = parlay::map(
    G_set, 
    [] (auto& n_set) { return ssize_t_seq(n_set.begin(), n_set.end()); }
  );
  auto starts = parlay::tabulate<ssize_t>(num_v / 2, [] (auto x) { return x; });
  parlay::sequence<bool> visited(G.size(), false);
  parlay::parallel_for(0, num_v / 3, [&visited] (size_t i) { visited[i] = true; });
  ssize_t_seq as_right_indices(G.size(), INVALID_INDEX);
  auto [right_v, right_selection] = maximal_expand(
    G, visited, starts, as_right_indices
  );
  verify_maximal_expand(G, visited, starts, right_v, right_selection, as_right_indices);
}

void verify_level_expand(
  const graph& G,
  const parlay::sequence<bool>& visited,
  const ssize_t_seq& leaves,
  const ssize_t_seq& pendings,
  const ssize_t_seq& pending_parents,
  const ssize_t_seq& right_v,
  const ssize_t_seq& right_selection, 
  const ssize_t_seq& next_pendings, 
  const ssize_t_seq& next_parents
) {
  CHECK(right_v.size() == right_selection.size());

  std::unordered_map<ssize_t, std::unordered_set<ssize_t>> left_selection;
  std::unordered_map<ssize_t, ssize_t> pending2parent;
  std::unordered_map<ssize_t, ssize_t> rv2selection;
  for (auto x: leaves) {
    left_selection[x] = {};
  }
  for (size_t i = 0; i < pendings.size(); ++i) {
    left_selection[pendings[i]] = {};
    pending2parent[pendings[i]] = pending_parents[i];
  }
  for (size_t i = 0; i < right_v.size(); ++i) {
    auto l_v = right_selection[i];
    rv2selection[right_v[i]] = l_v;
    auto iter = left_selection.find(l_v);
    if (iter != left_selection.end()) {
      iter->second.insert(right_v[i]);
    }
  }

  for (const auto& [l_v, selection]: left_selection) {
    CHECK_MESSAGE(selection.size() != 1, std::to_string(l_v));
    auto iter = pending2parent.find(l_v);
    if (selection.size() >= 2 && iter != pending2parent.end()) {
      CHECK(rv2selection[iter->first] == iter->second);
    }
  }

  parlay::parallel_for(
    0, left_selection.size(), 
    [&leaves, &pendings, &G, &rv2selection, &visited, &pending2parent] (size_t i) {
      auto l_v = i < leaves.size() ? leaves[i] : pendings[i - leaves.size()];
      if (rv2selection.find(l_v) != rv2selection.end()) {
        return;
      }
      size_t unselect_count = 0;
      ssize_t only_r = INVALID_INDEX;
      for (auto r: G[l_v]) {
        if (!visited[r] && rv2selection.find(r) == rv2selection.end()) {
          ++unselect_count;
          only_r = r;
        }
      }
      CHECK_MESSAGE(unselect_count < 2, std::to_string(l_v));
      if (unselect_count == 1) {
        CHECK(pending2parent[only_r] == l_v);
      }
    }
  );
}


TEST_CASE("Level Expand Simple") {
  graph G{{4}, {5}, {6}, {7, 4}, {0, 3, 5}, {1, 4, 6}, {5, 2, 8}, {3}, {6}};
  auto visited = parlay::sequence<bool>{1, 1, 1, 1, 0, 0, 0, 0, 0};
  auto leaves = ssize_t_seq{3};
  auto pendings = ssize_t_seq{4, 5, 6};
  auto pending_parents = ssize_t_seq{0, 1, 2};
  auto as_right_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto as_start_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto [right_v, right_selection, next_pendings, next_parents] = level_expand(
    G, visited, leaves, pendings, pending_parents, as_right_indices, as_start_indices
  );
  CHECK(next_pendings.empty());
  CHECK(next_parents.empty());
  std::unordered_map<ssize_t, ssize_t> r_map;
  for (size_t i = 0; i < right_v.size(); ++i) {
    r_map[right_v[i]] = right_selection[i];
  }
  auto option1 = r_map[7] == 3 && r_map[4] == 3 && r_map[5] == 6 
    && r_map[8] == 6 && r_map[6] == 2;
  auto option2 = r_map[7] == INVALID_INDEX && r_map[4] == 5 && r_map[5] == 1 
    && r_map[8] == INVALID_INDEX && r_map[6] == 5;
  CHECK((option1 || option2));
  verify_level_expand(
    G, visited, leaves, pendings, pending_parents,
    right_v, right_selection, next_pendings, next_parents
  );
}

TEST_CASE("Level Expand Simple Cycle") {
  graph G{{3}, {4}, {5}, {0, 4, 5}, {1, 3, 5}, {2, 3, 4}};
  auto visited = parlay::sequence<bool>{true, true, true, false, false, false};
  auto leaves = ssize_t_seq{};
  auto pendings = ssize_t_seq{3, 4, 5};
  auto pending_parents = ssize_t_seq{0, 1, 2};
  auto as_right_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto as_start_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto [right_v, right_selection, next_pendings, next_parents] = level_expand(
    G, visited, leaves, pendings, pending_parents, as_right_indices, as_start_indices
  );
  CHECK(next_pendings.empty());
  CHECK(next_parents.empty());
  auto head_iter = parlay::find_if(right_selection, [] (auto l) { return l < 3; });
  CHECK(head_iter != right_selection.end());
  auto head_idx = head_iter - right_selection.begin();
  parlay::parallel_for(
    0, 3,
    [head_idx, head_v = right_v[head_idx], &right_selection, pending_parents] (size_t i) {
      if (i == head_idx) {
        CHECK(right_selection[i] == pending_parents[head_v - 3]);
      } else {
        CHECK(right_selection[i] == head_v);
      }
    }
  );
  verify_level_expand(
    G, visited, leaves, pendings, pending_parents,
    right_v, right_selection, next_pendings, next_parents
  );
}

TEST_CASE("Level Expand Complex") {
  size_t num_leaf = 27, num_pending = 61, num_other = 24, num_edge = 300;
  size_t num_v = num_leaf + 2 * num_pending + num_other;
  parlay::sequence<std::set<ssize_t>> G_set(num_v);
  for (size_t i = 0; i < num_edge; ++i) {
    auto v1 = (i * (i + 1) / 2) % (num_v - num_pending);
    auto v2 = i * 3 % (num_v - num_pending);
    if (v1 != v2) {
      G_set[v1].insert(v2);
      G_set[v2].insert(v1);
    }
  }
  graph G = parlay::map(
    G_set, 
    [] (auto& n_set) { return ssize_t_seq(n_set.begin(), n_set.end()); }
  );
  parlay::parallel_for(
    0, num_pending,
    [&G, num_leaf, num_pending, &num_other] (size_t i) {
      ssize_t pending_v = i + num_leaf;
      ssize_t parent_v = pending_v + num_pending + num_other;
      G[pending_v].push_back(parent_v);
      G[parent_v].push_back(pending_v);
    }
  );
  auto visited = parlay::sequence<bool>(num_v, false);
  auto leaves = parlay::tabulate<ssize_t>(num_leaf, [&visited] (auto i) {
    visited[i] = true;
    return i;
  });
  auto pendings = parlay::tabulate<ssize_t>(
    num_pending,
    [num_leaf] (auto i) { return i + num_leaf; }
  );
  auto pending_parents = parlay::tabulate<ssize_t>(
    num_pending,
    [offset = num_v - num_pending, &visited] (auto i) {
      visited[i + offset] = true;
      return i + offset;
    }
  );
  auto as_right_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto as_start_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto [right_v, right_selection, next_pendings, next_parents] = level_expand(
    G, visited, leaves, pendings, pending_parents, as_right_indices, as_start_indices
  );
  
  verify_level_expand(
    G, visited, leaves, pendings, pending_parents,
    right_v, right_selection, next_pendings, next_parents
  );
}


TEST_CASE("Level Expand Odd Cycle") {
  size_t cycle_size = 5, cycle_num = 50, other_num = 0, other_edges = 0;
  size_t num_v = 3 * cycle_num * cycle_size + other_num;
  parlay::sequence<std::set<ssize_t>> G_set(num_v);
  for (size_t i = 0; i < other_edges; ++i) {
    auto v1 = i * 3 % other_num + 2 * cycle_num * cycle_size;
    auto v2 = (i * (i + 1) / 2) % (cycle_size * cycle_num);
    if (v1 != v2) {
      G_set[v1].insert(v2);
      G_set[v2].insert(v1);
    }
  }
  graph G = parlay::map(
    G_set, 
    [] (auto& n_set) { return ssize_t_seq(n_set.begin(), n_set.end()); }
  );
  auto visited = parlay::sequence<bool>(num_v, false);
  parlay::parallel_for(
    0, cycle_num * cycle_size,
    [&G, &visited, cycle_num, cycle_size, other_num] (ssize_t i) {
      ssize_t pending_v = i;
      ssize_t l_v = pending_v + other_num + cycle_num * cycle_size;
      ssize_t parent_v = pending_v + other_num + 2 * cycle_num * cycle_size;
      ssize_t forward = i + 1;
      ssize_t backward = i - 1;
      if (forward % cycle_size == 0) {
        forward -= cycle_size;
      }
      if ((backward + cycle_size) % cycle_size == cycle_size - 1) {
        backward += cycle_size;
      }
      G[pending_v].push_back(parent_v);
      G[parent_v].push_back(pending_v);
      G[pending_v].push_back(l_v);
      G[l_v].push_back(pending_v);
      G[i].push_back(forward);
      G[i].push_back(backward);
      visited[parent_v] = true;
    }
  );
  ssize_t_seq leaves{};
  auto pendings = parlay::tabulate<ssize_t>(
    cycle_num * cycle_size,
    [] (auto i) { return i; }
  );
  auto pending_parents = parlay::tabulate<ssize_t>(
    cycle_num * cycle_size,
    [offset = num_v - cycle_num * cycle_size] (auto i) {
      return i + offset;
    }
  );
  auto as_right_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto as_start_indices = ssize_t_seq(G.size(), INVALID_INDEX);
  auto [right_v, right_selection, next_pendings, next_parents] = level_expand(
    G, visited, leaves, pendings, pending_parents, as_right_indices, as_start_indices
  );
  
  verify_level_expand(
    G, visited, leaves, pendings, pending_parents,
    right_v, right_selection, next_pendings, next_parents
  );
}

TEST_CASE("Leafy Forest Star") {
  ssize_t num_leaf = 10;
  graph G(num_leaf + 1, {num_leaf});
  G[num_leaf] = parlay::tabulate<ssize_t>(num_leaf, [] (auto i) { return i; });
  auto forest = leafy_forest(G);
  CHECK(forest.size() == num_leaf);
  for (auto [v, v_select]: forest) {
    CHECK(v_select == num_leaf);
  }
}

TEST_CASE("Leafy Forest Simple") {
  size_t num_v = 7;
  std::vector<std::pair<ssize_t, ssize_t>> edges = {
    {0, 2},
    {1, 2},
    {2, 3},
    {2, 4},
    {3, 5},
    {4, 5},
    {5, 6}
  };
  graph G(num_v);
  for (auto [x, y]: edges) {
    G[x].push_back(y);
    G[y].push_back(x);
  }
  auto forest = leafy_forest(G);
  CHECK(forest.size() == 4);
}


TEST_CASE("Leafy Forest Simple Many") {
  size_t num_v = 7, copy = 4;
  std::vector<std::pair<ssize_t, ssize_t>> edges = {
    {0, 1},
    {1, 2},
    {1, 3},
    {2, 4},
    {3, 4},
    {4, 5},
    {4, 6}
  };
  graph G(num_v * copy);
  for (auto [x, y]: edges) {
    for (size_t i = 0; i < copy; ++i) {
      G[x + i * num_v].push_back(y + i * num_v);
      G[y + i * num_v].push_back(x + i * num_v);
    }
  }
  auto forest = leafy_forest(G);
  CHECK(forest.size() == 6 * copy);
}