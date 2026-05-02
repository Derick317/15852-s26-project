#ifndef GRAPH_CONTRACTION_H_
#define GRAPH_CONTRACTION_H_
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "helper.h"

auto distance(
  const parlay::sequence<ssize_t>& prevs
) -> parlay::sequence<ssize_t>;


#endif
