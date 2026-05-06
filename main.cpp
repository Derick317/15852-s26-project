#include <iostream>
#include <string>

#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/internal/get_time.h"
#include "maximal_leafy.h"
#include "graph_loader.h"

int main(int argc, char* argv[]) {
  auto exec_file = std::string(argv[0]);
  auto usage = "Usage: " + exec_file + " <graph> <filename> <repeat>\n"
    "\nSupported graphs:\n"
    "\torkut    https://snap.stanford.edu/data/com-Orkut.html \n"
    "\tus_road  https://www.diag.uniroma1.it/challenge9/download.shtml \n";
  if (argc != 4) {
    std::cerr << usage << std::endl;
    return 1;
  }

  size_t repeat;
  try { 
    repeat = std::stol(argv[3]);
  }
  catch (...) {
    repeat = 5;
    std::cout << "Use default repeat: " << repeat << std::endl;
  }

  std::string graph_name(argv[1]), file_path(argv[2]);
  std::optional<graph> G;
  if (graph_name == "orkut") {
    G = load_orkut(file_path);
  } else if (graph_name == "us_road") {
    G = load_us_road(file_path);
  } else {
    std::cerr << "Unsupported graph: " << graph_name << "\n" << usage << std::endl;
    return 1;
  }

  if (!G.has_value()) {
    return 1;
  }
  
  parlay::internal::timer t("Time");
  for (size_t i = 0; i < repeat; i++) {
    auto result = leafy_forest(G.value());
    t.next("leafy forest");
  }
}