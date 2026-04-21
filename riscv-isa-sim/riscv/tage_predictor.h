// See LICENSE for license details.
#ifndef _RISCV_TAGE_PREDICTOR_H
#define _RISCV_TAGE_PREDICTOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

struct tage_table_info_t
{
  size_t n_entries;
  size_t hist_len;
  unsigned tag_bits;
};

struct tage_params_t
{
  size_t base_entries;
  unsigned base_ctr_bits;
  unsigned tagged_ctr_bits;
  unsigned usefulness_bits;
  bool use_alt_on_weak;
  uint64_t usefulness_reset_period;
  std::vector<tage_table_info_t> table_info;

  static tage_params_t boom_like_default();
};

class tage_predictor_t
{
public:
  explicit tage_predictor_t(const tage_params_t& params = tage_params_t::boom_like_default());

  bool predict(uint64_t pc);
  void train(uint64_t pc, bool actual_taken);
  void reset();

  bool last_prediction_high_confidence() const;
  bool last_used_alternate() const;

private:
  struct tagged_entry_t {
    uint16_t tag;
    int8_t ctr;
    uint8_t useful;
  };

  struct last_lookup_t {
    bool valid;
    uint64_t pc;
    bool final_prediction;
    bool provider_prediction;
    bool alternate_prediction;
    bool provider_hit;
    bool alternate_hit;
    bool provider_weak;
    bool used_alternate;
    int provider_bank;
    int alternate_bank;
    int provider_index;
    int alternate_index;
    uint16_t provider_tag;
    uint16_t alternate_tag;
    std::vector<size_t> indices;
    std::vector<uint16_t> tags;
  };

  int base_index(uint64_t pc) const;
  size_t table_index(size_t table, uint64_t pc) const;
  uint16_t table_tag(size_t table, uint64_t pc) const;
  uint64_t fold_history(size_t hist_len, unsigned width) const;
  bool counter_predicts_taken(int value) const;
  bool counter_is_weak(int value, unsigned bits) const;
  bool counter_is_strong(int value, unsigned bits) const;
  int counter_min(unsigned bits) const;
  int counter_max(unsigned bits) const;
  int sat_inc(int value, unsigned bits) const;
  int sat_dec(int value, unsigned bits) const;
  void age_usefulness();
  void update_usefulness(bool provider_correct, bool alternate_correct);
  void allocate_entries(uint64_t pc, bool actual_taken);
  void push_history(bool taken);

  tage_params_t params;
  size_t max_hist_len;
  uint64_t branch_updates;
  std::vector<int8_t> base_table;
  std::vector<std::vector<tagged_entry_t>> tagged_tables;
  std::vector<bool> history;
  last_lookup_t last_lookup;
};

#endif
