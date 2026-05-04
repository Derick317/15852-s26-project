#ifndef MAXIMAL_LEAFY
#define MAXIMAL_LEAFY
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "helper.h"

using nested_seq = parlay::sequence<ssize_t_seq>;
using graph = nested_seq;

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
  -> std::pair<parlay::sequence<size_t>, ssize_t_seq>;

void filter_bipartite(
  nested_seq& x, ssize_t_seq& x_v,
  nested_seq& y, ssize_t_seq& y_v,
  const ssize_t_seq& y_remain
);

auto create_left_right(
  const graph& G, const parlay::sequence<bool>& prevented, 
  const ssize_t_seq& left_v,
  ssize_t_seq& as_right_indices
) -> std::tuple<nested_seq, nested_seq, ssize_t_seq>;

/**
 * Expand the graph from vertices `starts`. Each vertex in `starts` should
 * expand to 0 or at least 2 unvisited vertices. If a vertex in `starts`
 * is expanded by another, we can discard its expansion.
 */
auto maximal_expand(
  const graph& G, 
  const parlay::sequence<bool>& visited, 
  const ssize_t_seq& starts,
  ssize_t_seq& as_right_indices
) -> std::pair<ssize_t_seq, ssize_t_seq>;

auto level_expand(
  const graph& G, 
  const parlay::sequence<bool>& visited,
  const ssize_t_seq& leaves,
  const ssize_t_seq& pendings,
  const ssize_t_seq& pending_parents,
  ssize_t_seq& as_right_indices,
  ssize_t_seq& as_start_indices
) -> std::tuple<ssize_t_seq, ssize_t_seq, ssize_t_seq, ssize_t_seq>;

auto leafy_forest(const graph& G) -> parlay::sequence<std::tuple<ssize_t, ssize_t>>;

#endif