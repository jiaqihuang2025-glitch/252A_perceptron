// See LICENSE for license details.

#include "perceptron_predictor.h"

namespace {

static uint64_t mix64(uint64_t x)
{
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return x;
}

static unsigned log2_pow2(size_t value)
{
  unsigned bits = 0;
  while (value > 1) {
    value >>= 1;
    bits++;
  }
  return bits;
}

} // namespace

perceptron_predictor_t::perceptron_predictor_t(size_t n_entries, size_t hist_len, unsigned w_bits)
  : hist_len(hist_len),
    theta(76),
    max_weight((1 << (w_bits - 1)) - 1),
    min_weight(-(1 << (w_bits - 1))),
    bias(n_entries, 0),
    weights(n_entries, std::vector<int>(hist_len, 0)),
    history(hist_len, false)
{
}

int perceptron_predictor_t::index(uint64_t pc) const
{
  // Original PC-only indexing:
  // return static_cast<int>((pc >> 2) & (bias.size() - 1));

  const unsigned idx_bits = log2_pow2(bias.size());
  uint64_t hash = (pc >> 2) ^ fold_history(hist_len, idx_bits);
  hash ^= mix64(pc);
  return static_cast<int>(hash & (bias.size() - 1));
}

uint64_t perceptron_predictor_t::fold_history(size_t hist_len, unsigned width) const
{
  if (width == 0 || hist_len == 0)
    return 0;

  uint64_t folded = 0;
  for (size_t i = 0; i < hist_len && i < history.size(); ++i) {
    if (history[i])
      folded ^= (uint64_t(1) << (i % width));
  }

  if (width == 64)
    return folded;

  return folded & ((uint64_t(1) << width) - 1);
}

int perceptron_predictor_t::sat_inc(int x) const
{
  return x == max_weight ? x : x + 1;
}

int perceptron_predictor_t::sat_dec(int x) const
{
  return x == min_weight ? x : x - 1;
}

bool perceptron_predictor_t::predict(uint64_t pc) const
{
  const int idx = index(pc);
  int score = bias[idx];

  for (size_t i = 0; i < hist_len; ++i)
    score += history[i] ? weights[idx][i] : -weights[idx][i];

  return score >= 0;
}

void perceptron_predictor_t::train(uint64_t pc, bool actual_taken)
{
  const int idx = index(pc);
  int score = bias[idx];
  for (size_t i = 0; i < hist_len; ++i)
    score += history[i] ? weights[idx][i] : -weights[idx][i];

  const bool predicted_taken = score >= 0;
  const bool should_train = (predicted_taken != actual_taken) || (std::abs(score) <= theta);

  if (should_train) {
    bias[idx] = actual_taken ? sat_inc(bias[idx]) : sat_dec(bias[idx]);

    for (size_t i = 0; i < hist_len; ++i) {
      weights[idx][i] =
        history[i] == actual_taken ? sat_inc(weights[idx][i]) : sat_dec(weights[idx][i]);
    }
  }

  history.insert(history.begin(), actual_taken);
  history.pop_back();
}

void perceptron_predictor_t::reset()
{
  std::fill(bias.begin(), bias.end(), 0);
  for (auto& row : weights)
    std::fill(row.begin(), row.end(), 0);
  std::fill(history.begin(), history.end(), false);
}
