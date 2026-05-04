#include <iostream>
#include <string>
#include <fstream>

#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/internal/get_time.h"
#include "maximal_leafy.h"

int main(int argc, char* argv[]) {
  auto exec_file = std::string(argv[0]);
  auto usage = "Usage: " + exec_file + " <filename> <repeat>";
  if (argc != 3) {
    std::cerr << usage << std::endl;
    return 1;
  }

  size_t repeat;
  try { 
    repeat = std::stol(argv[2]);
  }
  catch (...) {
    repeat = 5;
    std::cout << "Use default repeat: " << repeat << std::endl;
  }

  std::ifstream file(argv[1]);
  if (!file) {
    std::cerr << "Error opening file\n";
    return 1;
  }

  graph G;
  std::string line;

  while (std::getline(file, line)) {
    // Skip comments or empty lines
    if (line.empty() || line[0] == '#') continue;

    std::istringstream iss(line);
    size_t u, v;

    if (!(iss >> u >> v)) continue; // malformed line

    if (std::max(u, v) >= G.size()) {
      G.append(std::max(u, v) - G.size() + 1, {});
    }
    // Undirected graph → add both ways
    G[u].push_back(v);
    G[v].push_back(u);
  }

  std::cout << "Parsed graph with " << G.size() << " nodes\n";
  parlay::internal::timer t("Time");
  for (size_t i = 0; i < repeat; i++) {
    auto result = leafy_forest(G);
    t.next("leafy forest");
  }
}