#include "graph_contraction.h"

static parlay::random_generator gen;
static std::uniform_int_distribution<int> bool_dis(0, 1);

static auto random_bool(size_t i) -> bool {
  auto r = gen[i];
  return bool_dis(r);
}

auto distance_helper(
  const ssize_t_seq& prevs,
  const ssize_t_seq& nums,
  size_t seed
) -> ssize_t_seq {
  if (prevs.empty()) {
    return {};
  }

  auto neighbors = parlay::sequence<bool>(prevs.size(), false);
  parlay::parallel_for(0, prevs.size(), [&prevs, &neighbors] (size_t i) {
    if (prevs[i] != i) {
      neighbors[prevs[i]] = true;
      neighbors[i] = true;
    }
  });

  auto heads = parlay::tabulate<bool>(prevs.size(), [seed] (long i) {
    return random_bool(i + seed);
  });
  auto jumped = parlay::tabulate<bool>(prevs.size(), [&heads, &prevs] (long i) {
    return heads[prevs[i]] && !heads[i];
  });
  auto remains = parlay::tabulate<ssize_t>(prevs.size(), [&neighbors, &jumped] (long i) {
    return neighbors[i] && !jumped[i];
  });

  auto new_nums = parlay::tabulate<ssize_t>(
    prevs.size(), 
    [&jumped, &prevs, &nums] (long i) {
      return jumped[prevs[i]] ? nums[i] + nums[prevs[i]] : nums[i];
    }
  );
  filter_by_onehot(new_nums, remains);

  auto new_prevs = parlay::tabulate<ssize_t>(
    prevs.size(), 
    [&jumped, &prevs] (long i) {
      return jumped[prevs[i]] ? prevs[prevs[i]] : prevs[i];
    }
  );
  filter_by_onehot(new_prevs, remains);
  auto new_indices = parlay::scan(remains).first;
  new_prevs = parlay::map(
    new_prevs,
    [&new_indices] (auto i) { return new_indices[i]; }
  );

  auto old_indices = parlay::tabulate<ssize_t>(prevs.size(), [] (size_t i) { return i; });
  filter_by_onehot(old_indices, remains);

  auto sums = distance_helper(new_prevs, new_nums, seed + 1);
  auto result = ssize_t_seq(prevs.size());
  parlay::parallel_for(
    0, old_indices.size(),
    [&old_indices, &nums, &sums, &result, &jumped, &prevs] (size_t i) {
      auto old_index = old_indices[i];
      if (sums[i] == 0) {
        result[old_index] = 0;
      } else {
        result[old_index] = sums[i];
        if (jumped[prevs[old_index]]) {
          result[old_index] += nums[prevs[old_index]];
        }
      }
    }
  );
  parlay::parallel_for(
    0, result.size(),
    [&result, &prevs, &neighbors, &jumped, &nums] (size_t i) {
      if (!neighbors[i]) {
        result[i] = 0;
      } else if (jumped[i]) {
        auto prev_num = result[prevs[i]] == 0 ? 1 : nums[prevs[i]];
        result[i] = prev_num + result[prevs[i]];
      }
    }
  );

  return result;
}


auto graph_distance(const ssize_t_seq& prevs)
 -> ssize_t_seq {
  auto nums = ssize_t_seq(prevs.size(), 1);
  return distance_helper(prevs, nums, 0);
}