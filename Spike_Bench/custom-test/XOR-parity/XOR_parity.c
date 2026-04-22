#include "XOR_parity.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

volatile int branch_sink = 0;

static uint32_t lfsr_step(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

void run_xor_data_phase_test(uint64_t N, uint32_t arr_size) {
    // arr_size should be power of 2
    uint32_t *arr = (uint32_t *)malloc(sizeof(uint32_t) * arr_size);
    if (!arr) {
        printf("malloc failed\n");
        return;
    }

    uint32_t seed = 0xCAFEBABE;
    for (uint32_t i = 0; i < arr_size; i++) {
        seed = lfsr_step(seed);
        arr[i] = seed;
    }

    uint32_t hist = 0xBEEF;
    int acc = 0;

    register unsigned long roi_marker_start asm("a0") = 1;
    asm volatile("ebreak" :: "r"(roi_marker_start));
    for (uint64_t i = 0; i < N; i++) {
        uint32_t idx1 = (uint32_t)((i * 13 + (hist & 63)) & (arr_size - 1));
        uint32_t idx2 = (uint32_t)(((i * 29) ^ (hist << 2)) & (arr_size - 1));

        uint32_t a = arr[idx1];
        uint32_t b = arr[idx2];

        int h1  = (hist >> 1) & 1;
        int h6  = (hist >> 6) & 1;
        int h11 = (hist >> 11) & 1;

        int data_bit;
        if (i < N / 4) {
            data_bit = ((a >> 2) ^ (b >> 5)) & 1;
        } else if (i < N / 2) {
            data_bit = ((a >> 7) ^ (a >> 11) ^ (b >> 3)) & 1;
        } else if (i < (3 * N) / 4) {
            data_bit = (((a + b) >> 4) ^ (a >> 9)) & 1;
        } else {
            data_bit = (((a ^ b) >> 6) ^ (b >> 12)) & 1;
        }

        int outcome = data_bit ^ h1 ^ h6 ^ h11;

        if (outcome) {
            acc += (int)(a & 7);
        } else {
            acc -= (int)(b & 3);
        }

        hist = ((hist << 1) | (uint32_t)outcome) & 0xFFFFu;
    }
    register unsigned long roi_marker_end asm("a0") = 2;
    asm volatile("ebreak" :: "r"(roi_marker_end));

    branch_sink = acc;
    free(arr);
}
