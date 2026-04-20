#include <stdint.h>
#include <stdio.h>

#define N 4096
#define ROUNDS 200

static uint32_t data[N];
volatile uint64_t sink = 0;

#define read_csr_safe(reg) ({ \
  register unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define write_csr_safe(reg, val) ({ \
  unsigned long __v = (unsigned long)(val); \
  asm volatile ("csrw " #reg ", %0" :: "rK"(__v)); })

static void init_data(void) {
  data[0] = 1;
  data[1] = 7;
  for (int i = 2; i < N; ++i) {
    data[i] = (data[i - 1] * 1664525u + data[i - 2] * 1013904223u + (uint32_t)i) & 1023u;
  }
}

static inline void run_data_dependent_test(void) {
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
}

int main(void) {
  write_csr_safe(mcounteren, -1UL);
  write_csr_safe(scounteren, -1UL);
  write_csr_safe(mcountinhibit, 0);

  write_csr_safe(mhpmcounter3, 0);
  write_csr_safe(mhpmcounter4, 0);
  write_csr_safe(mhpmcounter5, 0);

  write_csr_safe(mhpmevent3, 0x201);
  write_csr_safe(mhpmevent4, 0x1001);
  write_csr_safe(mhpmevent5, 0x401);

  uint64_t p0 = read_csr_safe(mhpmcounter3);
  uint64_t p2 = read_csr_safe(mhpmcounter4);
  uint64_t p4 = read_csr_safe(mhpmcounter5);
  uint64_t instret_i = read_csr_safe(minstret);

  run_data_dependent_test();

  uint64_t p1 = read_csr_safe(mhpmcounter3);
  uint64_t p3 = read_csr_safe(mhpmcounter4);
  uint64_t p5 = read_csr_safe(mhpmcounter5);
  uint64_t instret_f = read_csr_safe(minstret);

  uint64_t mispredict_br = p1 - p0;
  uint64_t branch_resolved = p3 - p2;
  uint64_t mispredict_jalr = p5 - p4;
  uint64_t mispredict_total = mispredict_br + mispredict_jalr;
  uint64_t ins_commit = instret_f - instret_i;

  printf("data_dependent sink: %lu\n", (unsigned long)sink);
  printf("Branch Mispredict before = %lu after = %lu delta = %lu\n",
         (unsigned long)p0, (unsigned long)p1, (unsigned long)mispredict_br);
  printf("Branch Resolved before = %lu after = %lu delta = %lu\n",
         (unsigned long)p2, (unsigned long)p3, (unsigned long)branch_resolved);
  printf("JALR Mispredict before = %lu after = %lu delta = %lu\n",
         (unsigned long)p4, (unsigned long)p5, (unsigned long)mispredict_jalr);
  printf("InstRet before = %lu after = %lu delta = %lu\n",
         (unsigned long)instret_i, (unsigned long)instret_f, (unsigned long)ins_commit);

  if (ins_commit != 0) {
    uint64_t mpki_scaled = (mispredict_total * 1000000ULL) / ins_commit;
    uint64_t mpki_br_scaled = (mispredict_br * 1000000ULL) / ins_commit;
    uint64_t mpki_jalr_scaled = (mispredict_jalr * 1000000ULL) / ins_commit;
    printf("MPKI: %lu.%03lu\n",
           (unsigned long)(mpki_scaled / 1000),
           (unsigned long)(mpki_scaled % 1000));
    printf("Branch MPKI: %lu.%03lu\n",
           (unsigned long)(mpki_br_scaled / 1000),
           (unsigned long)(mpki_br_scaled % 1000));
    printf("JALR MPKI: %lu.%03lu\n",
           (unsigned long)(mpki_jalr_scaled / 1000),
           (unsigned long)(mpki_jalr_scaled % 1000));
    if (branch_resolved != 0) {
      uint64_t miss_rate_scaled = (mispredict_br * 10000ULL) / branch_resolved;
      printf("Miss Rate (BR): %lu.%02lu%%\n",
             (unsigned long)(miss_rate_scaled / 100),
             (unsigned long)(miss_rate_scaled % 100));
    } else {
      printf("Miss Rate (BR): branch_resolved is zero\n");
    }
  } else {
    printf("Instructions committed is zero\n");
  }

  printf("Instructions Committed: %lu\n", (unsigned long)ins_commit);

  return 0;
}
