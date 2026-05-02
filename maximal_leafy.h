#ifndef MAXIMAL_LEAFY
#define MAXIMAL_LEAFY
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "helper.h"

using nested_seq = parlay::sequence<parlay::sequence<ssize_t>>;
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
  -> std::pair<parlay::sequence<size_t>, parlay::sequence<ssize_t>>;

void filter_left_right(
  nested_seq& left, parlay::sequence<ssize_t>& left_v,
  nested_seq& right, parlay::sequence<ssize_t>& right_v,
  const parlay::sequence<ssize_t>& left_remain
);

auto create_left_right(
  const graph& G, const parlay::sequence<bool>& prevented, 
  const parlay::sequence<ssize_t>& left_v,
  parlay::sequence<ssize_t>& as_right_indices
) -> std::tuple<nested_seq, nested_seq, parlay::sequence<ssize_t>>;

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
);

#endif