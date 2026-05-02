#ifndef HELPER_H_
#define HELPER_H_
#include "parlay/primitives.h"
#include "parlay/sequence.h"

using ssize_t = std::make_signed_t<std::size_t>;
constexpr ssize_t INVALID_INDEX = -1; 

template <typename T>
struct DataAndIndex {
  T data;
  ssize_t index;
  bool operator<(const DataAndIndex<T>& rhs) const {
    return data < rhs.data;
  }
};

template <typename T>
auto reduce_maximum(const parlay::sequence<T>& seq) -> ssize_t {
  auto seq_with_index = parlay::tabulate<DataAndIndex<T>>(seq.size(), [&seq] (long i) {
    return DataAndIndex<T>{seq[i], i};
  });
  auto identity = DataAndIndex<T>{std::numeric_limits<T>::lowest(), INVALID_INDEX};
  return parlay::reduce(seq_with_index, parlay::maximum<DataAndIndex<T>>(identity)).index;
}

template <typename T>
void print_seq(const parlay::sequence<T>& seq, std::string name="") {
  if (!name.empty()) {
    std::cout << name << ": ";
  }
  
  for (auto x: seq) {
    std::cout << x << ' ';
  }
  std::cout << std::endl;
}

template <typename T>
void print_nested_seq(const parlay::sequence<T>& nested_seq, std::string name="") {
  if (!name.empty()) {
    std::cout << name << ":\n";
  }
  for (const auto& x: nested_seq) {
    print_seq(x);
  }
}

template <typename T, typename NUM>
void filter_by_onehot(parlay::sequence<T>& seq, const parlay::sequence<NUM>& onehot) {
  auto old_indices = parlay::tabulate<ssize_t>(seq.size(), [] (long i) { return i; });
  parlay::parallel_for(0, seq.size(), [&onehot, &old_indices] (size_t i) {
    if (!onehot[i]) {
      old_indices[i] = INVALID_INDEX;
    }
  });
  old_indices = parlay::filter(old_indices, [] (ssize_t i) {
    return i != INVALID_INDEX;
  });
  seq = parlay::map(old_indices, [&seq] (ssize_t i) { return seq[i]; });
}


#endif