#include "graph_loader.h"
#include <fstream>

std::optional<graph> load_orkut(const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file) {
    std::cerr << "Error opening file\n";
    return std::nullopt;
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
  return G;
}


std::optional<graph> load_us_road(const std::string& file_path) {
  std::ifstream file(file_path);
  if (!file) {
    std::cerr << "Error opening file\n";
    return std::nullopt;
  }

  graph G;
  std::string line;

  while (std::getline(file, line)) {
    // Skip comments or empty lines
    if (line.empty() || line[0] != 'a') continue;

    std::istringstream iss(line.substr(1));
    size_t u, v, w;

    if (!(iss >> u >> v >> w)) continue; // malformed line

    if (std::max(u, v) >= G.size()) {
      G.append(std::max(u, v) - G.size() + 1, {});
    }
    // Undirected graph → add both ways
    G[u].push_back(v);
    G[v].push_back(u);
  }

  std::cout << "Parsed graph with " << G.size() << " nodes\n";
  return G;
}