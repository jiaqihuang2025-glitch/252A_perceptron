// See LICENSE for license details.

#include "tage_predictor.h"

#include <algorithm>
#include <cstdlib>

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

tage_params_t tage_params_t::boom_like_default()
{
  tage_params_t params;
  params.base_entries = 2048;
  params.base_ctr_bits = 2;
  params.tagged_ctr_bits = 3;
  params.usefulness_bits = 2;
  params.use_alt_on_weak = true;
  params.usefulness_reset_period = 1 << 14;

  // BOOM config in this tree shows a 64-bit global history TAGE-L BPD.
  // The exact Scala implementation is not present locally, so this keeps a
  // BOOM-like geometric history layout that fits the same modeling intent.
  params.table_info = {
    {256,  4,  7},
    {256,  8,  7},
    {256, 16,  8},
    {256, 32,  9},
    {256, 64, 10},
  };

  return params;
}

tage_predictor_t::tage_predictor_t(const tage_params_t& params)
  : params(params),
    max_hist_len(0),
    branch_updates(0),
    base_table(params.base_entries, 0),
    tagged_tables(),
    history(),
    last_lookup()
{
  for (const auto& info : params.table_info) {
    max_hist_len = std::max(max_hist_len, info.hist_len);
    tagged_tables.emplace_back(info.n_entries, tagged_entry_t{0, 0, 0});
  }

  history.assign(max_hist_len, false);
  reset();
}

int tage_predictor_t::base_index(uint64_t pc) const
{
  return static_cast<int>((pc >> 2) & (params.base_entries - 1));
}

uint64_t tage_predictor_t::fold_history(size_t hist_len, unsigned width) const
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

size_t tage_predictor_t::table_index(size_t table, uint64_t pc) const
{
  const auto& info = params.table_info[table];
  const unsigned idx_bits = log2_pow2(info.n_entries);
  uint64_t hash = (pc >> 2) ^ fold_history(info.hist_len, idx_bits);
  hash ^= mix64(pc + table * 0x9e3779b97f4a7c15ULL);
  return static_cast<size_t>(hash & (info.n_entries - 1));
}

uint16_t tage_predictor_t::table_tag(size_t table, uint64_t pc) const
{
  const auto& info = params.table_info[table];
  uint64_t tag = (pc >> 2) ^ fold_history(info.hist_len, info.tag_bits);
  if (info.tag_bits > 1)
    tag ^= fold_history(info.hist_len / 2 + 1, info.tag_bits - 1);

  tag ^= mix64(pc ^ (table + 1));
  return static_cast<uint16_t>(tag & ((uint64_t(1) << info.tag_bits) - 1));
}

bool tage_predictor_t::counter_predicts_taken(int value) const
{
  return value >= 0;
}

bool tage_predictor_t::counter_is_weak(int value, unsigned bits) const
{
  return std::abs(value) <= 1 || bits <= 1;
}

bool tage_predictor_t::counter_is_strong(int value, unsigned bits) const
{
  return !counter_is_weak(value, bits);
}

int tage_predictor_t::counter_min(unsigned bits) const
{
  return -(1 << (bits - 1));
}

int tage_predictor_t::counter_max(unsigned bits) const
{
  return (1 << (bits - 1)) - 1;
}

int tage_predictor_t::sat_inc(int value, unsigned bits) const
{
  return value == counter_max(bits) ? value : value + 1;
}

int tage_predictor_t::sat_dec(int value, unsigned bits) const
{
  return value == counter_min(bits) ? value : value - 1;
}

bool tage_predictor_t::predict(uint64_t pc)
{
  last_lookup.valid = true;
  last_lookup.pc = pc;
  last_lookup.provider_hit = false;
  last_lookup.alternate_hit = false;
  last_lookup.provider_bank = -1;
  last_lookup.alternate_bank = -1;
  last_lookup.provider_index = -1;
  last_lookup.alternate_index = -1;
  last_lookup.provider_tag = 0;
  last_lookup.alternate_tag = 0;
  last_lookup.indices.assign(params.table_info.size(), 0);
  last_lookup.tags.assign(params.table_info.size(), 0);

  const int bimodal_idx = base_index(pc);
  const bool base_pred = counter_predicts_taken(base_table[bimodal_idx]);

  for (size_t t = 0; t < params.table_info.size(); ++t) {
    last_lookup.indices[t] = table_index(t, pc);
    last_lookup.tags[t] = table_tag(t, pc);
  }

  for (int t = static_cast<int>(params.table_info.size()) - 1; t >= 0; --t) {
    const auto& entry = tagged_tables[t][last_lookup.indices[t]];
    if (entry.tag == last_lookup.tags[t]) {
      if (!last_lookup.provider_hit) {
        last_lookup.provider_hit = true;
        last_lookup.provider_bank = t;
        last_lookup.provider_index = static_cast<int>(last_lookup.indices[t]);
        last_lookup.provider_tag = last_lookup.tags[t];
      } else if (!last_lookup.alternate_hit) {
        last_lookup.alternate_hit = true;
        last_lookup.alternate_bank = t;
        last_lookup.alternate_index = static_cast<int>(last_lookup.indices[t]);
        last_lookup.alternate_tag = last_lookup.tags[t];
        break;
      }
    }
  }

  last_lookup.alternate_prediction = last_lookup.alternate_hit
    ? counter_predicts_taken(tagged_tables[last_lookup.alternate_bank][last_lookup.alternate_index].ctr)
    : base_pred;

  if (!last_lookup.provider_hit) {
    last_lookup.provider_prediction = base_pred;
    last_lookup.provider_weak = counter_is_weak(base_table[bimodal_idx], params.base_ctr_bits);
    last_lookup.used_alternate = false;
    last_lookup.final_prediction = base_pred;
    return last_lookup.final_prediction;
  }

  const auto& provider = tagged_tables[last_lookup.provider_bank][last_lookup.provider_index];
  last_lookup.provider_prediction = counter_predicts_taken(provider.ctr);
  last_lookup.provider_weak = counter_is_weak(provider.ctr, params.tagged_ctr_bits);
  last_lookup.used_alternate =
    params.use_alt_on_weak && last_lookup.provider_weak && last_lookup.alternate_hit;
  last_lookup.final_prediction =
    last_lookup.used_alternate ? last_lookup.alternate_prediction : last_lookup.provider_prediction;

  return last_lookup.final_prediction;
}

void tage_predictor_t::update_usefulness(bool provider_correct, bool alternate_correct)
{
  if (!last_lookup.provider_hit)
    return;

  auto& provider = tagged_tables[last_lookup.provider_bank][last_lookup.provider_index];
  if (provider_correct == alternate_correct)
    return;

  const uint8_t max_useful = static_cast<uint8_t>((1u << params.usefulness_bits) - 1u);
  if (provider_correct) {
    if (provider.useful < max_useful)
      provider.useful++;
  } else if (provider.useful > 0) {
    provider.useful--;
  }
}

void tage_predictor_t::age_usefulness()
{
  if (params.usefulness_reset_period == 0)
    return;
  if ((branch_updates % params.usefulness_reset_period) != 0)
    return;

  for (auto& table : tagged_tables) {
    for (auto& entry : table)
      entry.useful >>= 1;
  }
}

void tage_predictor_t::allocate_entries(uint64_t pc, bool actual_taken)
{
  const size_t start_table = last_lookup.provider_hit ? static_cast<size_t>(last_lookup.provider_bank + 1) : 0;
  const int8_t init_ctr = actual_taken ? 0 : -1;

  for (size_t t = start_table; t < params.table_info.size(); ++t) {
    auto& entry = tagged_tables[t][last_lookup.indices[t]];
    if (entry.useful == 0) {
      entry.tag = last_lookup.tags[t];
      entry.ctr = init_ctr;
      entry.useful = 0;
      return;
    }
  }

  for (size_t t = start_table; t < params.table_info.size(); ++t) {
    auto& entry = tagged_tables[t][last_lookup.indices[t]];
    if (entry.useful > 0)
      entry.useful--;
  }
}

void tage_predictor_t::push_history(bool taken)
{
  if (history.empty())
    return;

  history.insert(history.begin(), taken);
  history.pop_back();
}

void tage_predictor_t::train(uint64_t pc, bool actual_taken)
{
  if (!last_lookup.valid || last_lookup.pc != pc)
    predict(pc);

  const int bimodal_idx = base_index(pc);
  const bool provider_correct = last_lookup.provider_prediction == actual_taken;
  const bool alternate_correct = last_lookup.alternate_prediction == actual_taken;

  if (last_lookup.provider_hit) {
    auto& provider = tagged_tables[last_lookup.provider_bank][last_lookup.provider_index];
    provider.ctr = actual_taken
      ? sat_inc(provider.ctr, params.tagged_ctr_bits)
      : sat_dec(provider.ctr, params.tagged_ctr_bits);

    if (last_lookup.alternate_hit && last_lookup.provider_weak) {
      auto& alternate = tagged_tables[last_lookup.alternate_bank][last_lookup.alternate_index];
      alternate.ctr = actual_taken
        ? sat_inc(alternate.ctr, params.tagged_ctr_bits)
        : sat_dec(alternate.ctr, params.tagged_ctr_bits);
    } else if (!last_lookup.alternate_hit) {
      base_table[bimodal_idx] = actual_taken
        ? sat_inc(base_table[bimodal_idx], params.base_ctr_bits)
        : sat_dec(base_table[bimodal_idx], params.base_ctr_bits);
    }

    update_usefulness(provider_correct, alternate_correct);
  } else {
    base_table[bimodal_idx] = actual_taken
      ? sat_inc(base_table[bimodal_idx], params.base_ctr_bits)
      : sat_dec(base_table[bimodal_idx], params.base_ctr_bits);
  }

  if (last_lookup.final_prediction != actual_taken)
    allocate_entries(pc, actual_taken);

  push_history(actual_taken);
  branch_updates++;
  age_usefulness();
}

void tage_predictor_t::reset()
{
  std::fill(base_table.begin(), base_table.end(), 0);
  for (auto& table : tagged_tables) {
    for (auto& entry : table) {
      entry.tag = 0;
      entry.ctr = 0;
      entry.useful = 0;
    }
  }
  std::fill(history.begin(), history.end(), false);
  branch_updates = 0;
  last_lookup = {};
}

bool tage_predictor_t::last_prediction_high_confidence() const
{
  if (!last_lookup.valid)
    return false;
  if (!last_lookup.provider_hit)
    return counter_is_strong(base_table[base_index(last_lookup.pc)], params.base_ctr_bits);

  const auto& provider = tagged_tables[last_lookup.provider_bank][last_lookup.provider_index];
  return counter_is_strong(provider.ctr, params.tagged_ctr_bits);
}

bool tage_predictor_t::last_used_alternate() const
{
  return last_lookup.valid && last_lookup.used_alternate;
}
