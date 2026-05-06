#ifndef GRAPH_LOADER_H_
#define GRAPH_LOADER_H_
#include "helper.h"

/**
 * Orkut social network and ground-truth communities
 * https://snap.stanford.edu/data/com-Orkut.html
 */
std::optional<graph> load_orkut(const std::string& file_path);

/**
 * 9th DIMACS Implementation Challenge - Shortest Paths
 * https://www.diag.uniroma1.it/challenge9/download.shtml
 */
std::optional<graph> load_us_road(const std::string& file_path);

#endif