#include <iostream>
#include <string>

#include "parlaylib/include/parlay/primitives.h"
#include "parlaylib/include/parlay/sequence.h"
#include "parlaylib/include/parlay/internal/get_time.h"

#include "maximal_leafy.h"
#include "parlaylib/examples/helper/graph_utils.h"

template<typename Range, typename BinaryOp>
auto my_reduce(const Range& A, BinaryOp&& binop) {
  long n = A.size();
  using T = typename Range::value_type;
  long block_size = 100;
  if (n == 0) return binop.identity;
  if (n <= block_size) {
    T v = A[0];
    for (long i=1; i < n; i++)
      v = binop(v, A[i]);
    return v;
  }

  T L, R;
  parlay::par_do([&] {L = my_reduce(parlay::make_slice(A).cut(0,n/2), binop);},
                 [&] {R = my_reduce(parlay::make_slice(A).cut(n/2,n), binop);});
  return binop(L,R);
}

void test_leafy_match() {
  nested_seq left{{0}, {1}, {2}, {0, 1, 2}};
  nested_seq right{{0, 3}, {1, 3}, {2, 3}};
  auto [left_selected_count, right_selection] = leafy_match(left, right);
  print_seq(right_selection, "result");
  std::cout << std::endl;
}


int main(int argc, char* argv[]) {
  test_leafy_match();
  auto seq = parlay::tabulate<long>(100000000, [&] (long i) {
      return i * i % 1000000007; });
  parlay::internal::timer t("Time");
  for (int i=0; i < 3; i++) {
    auto max = reduce_maximum(seq);
    std::cout << max << ": " << seq[max] << std::endl;
    t.next("reduce");
  }
  return 0;
}