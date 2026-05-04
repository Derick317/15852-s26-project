#include <iostream>
#include <string>

#include "parlay/primitives.h"
#include "parlay/sequence.h"
#include "parlay/internal/get_time.h"
#include "maximal_leafy.h"


int main(int argc, char* argv[]) {
  auto seq = parlay::tabulate<long>(1000000000, [&] (long i) {
      return i; });
  auto seq10 = seq.tail(10);
  for (size_t i = 0; i < seq10.size(); ++i) {
    std::cout << seq10[i] << ' ';
  }
  parlay::internal::timer t("Time");
  size_t target = 1000;
  while (target < seq.size()) {
    auto iter = parlay::find_if(seq, [target] (auto x) { return x == target; });
    std::cout << "target: " << target << std::endl;
    t.next("find_if");
    target *= 10;
  }
  return 0;
}