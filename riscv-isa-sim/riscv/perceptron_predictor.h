// See LICENSE for license details.
#ifndef _RISCV_PERCEPTRON_PREDICTOR_H
#define _RISCV_PERCEPTRON_PREDICTOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

class perceptron_predictor_t
{
public:
  perceptron_predictor_t(size_t n_entries = 512, size_t hist_len = 32, unsigned w_bits = 8);

  bool predict(uint64_t pc) const;
  void train(uint64_t pc, bool actual_taken);
  void reset();

private:
  int index(uint64_t pc) const;
  uint64_t fold_history(size_t hist_len, unsigned width) const;
  int sat_inc(int x) const;
  int sat_dec(int x) const;

  size_t hist_len;
  int max_weight;
  int min_weight;
  std::vector<int> bias;
  std::vector<std::vector<int>> weights;
  std::vector<bool> history;
};

#endif
