// See LICENSE for license details.

#include "perceptron_predictor.h"

perceptron_predictor_t::perceptron_predictor_t(size_t n_entries, size_t hist_len, unsigned w_bits)
  : hist_len(hist_len),
    max_weight((1 << (w_bits - 1)) - 1),
    min_weight(-(1 << (w_bits - 1))),
    bias(n_entries, 0),
    weights(n_entries, std::vector<int>(hist_len, 0)),
    history(hist_len, false)
{
}

int perceptron_predictor_t::index(uint64_t pc) const
{
  return (pc >> 2) & (bias.size() - 1);
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
  bias[idx] = actual_taken ? sat_inc(bias[idx]) : sat_dec(bias[idx]);

  for (size_t i = 0; i < hist_len; ++i) {
    weights[idx][i] =
      history[i] == actual_taken ? sat_inc(weights[idx][i]) : sat_dec(weights[idx][i]);
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
