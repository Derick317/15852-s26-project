#ifndef GRAPH_CONTRACTION_H_
#define GRAPH_CONTRACTION_H_
#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "helper.h"

auto graph_distance(
  const ssize_t_seq& prevs
) -> ssize_t_seq;


#endif
