#include <stdint.h>
#include <stdio.h>

#define N 4096
#define ROUNDS 200

static uint32_t data[N];
volatile uint64_t sink = 0;

static void init_data(void) {
  data[0] = 1;
  data[1] = 7;

  for (int i = 2; i < N; ++i) {
    data[i] = (data[i - 1] * 1664525u + data[i - 2] * 1013904223u + (uint32_t) i) & 1023u;
  }
}

int main(void) {
  uint32_t prev = 0x12345678u;

  init_data();

  for (int round = 0; round < ROUNDS; ++round) {
    for (int i = 2; i < N; ++i) {
      uint32_t mix = data[i] ^ (data[i - 1] << 1) ^ (data[i - 2] >> 1) ^ prev;

      if ((mix & 0x20u) == 0) {
        data[i] = (mix + data[i - 1]) & 1023u;
        sink += data[i];
      } else {
        data[i] = (mix ^ data[i - 2]) & 1023u;
        sink -= data[i];
      }

      prev = data[i] + (prev << 1);
    }
  }

  printf("done: %lu %u\n", (unsigned long) sink, prev);
  return 0;
}
