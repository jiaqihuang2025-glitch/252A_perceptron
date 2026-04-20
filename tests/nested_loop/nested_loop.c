#include <stdint.h>
#include <stdio.h>

volatile uint64_t sink = 0;

int main(void) {
  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 100; j++) {
      sink += (uint64_t)(i + j);
    }
  }

  printf("done: %lu\n", (unsigned long)sink);
  return 0;
}
