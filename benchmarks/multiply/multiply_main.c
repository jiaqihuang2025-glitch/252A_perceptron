// See LICENSE for license details.

// *************************************************************************
// multiply filter bencmark
// -------------------------------------------------------------------------
//
// This benchmark tests the software multiply implemenation. The
// input data (and reference data) should be generated using the
// multiply_gendata.pl perl script and dumped to a file named
// dataset1.h

#include "util.h"
#include <stdint.h>
#include <stdio.h>

#include "multiply.h"

//--------------------------------------------------------------------------
// Input/Reference Data

#include "dataset1.h"

//--------------------------------------------------------------------------
// CSR helpers

#define read_csr_safe(reg) ({ \
  register unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define write_csr_safe(reg, val) ({ \
  unsigned long __v = (unsigned long)(val); \
  asm volatile ("csrw " #reg ", %0" :: "rK"(__v)); })

//--------------------------------------------------------------------------
// Main

int main( int argc, char* argv[] )
{
  int i;
  int results_data[DATA_SIZE];

#if PREALLOCATE
  for (i = 0; i < DATA_SIZE; i++)
  {
    results_data[i] = multiply( input_data1[i], input_data2[i] );
  }
#endif

  // Enable counters
  write_csr_safe(mcounteren, -1UL);
  write_csr_safe(scounteren, -1UL);
  write_csr_safe(mcountinhibit, 0);

  // Reset HPM counters
  write_csr_safe(mhpmcounter3, 0);
  write_csr_safe(mhpmcounter4, 0);
  write_csr_safe(mhpmcounter5, 0);
  write_csr_safe(mhpmcounter7, 0);
  write_csr_safe(mhpmcounter8, 0);
  write_csr_safe(mhpmcounter9, 0);

  // Program events
  write_csr_safe(mhpmevent3, 0x401);
  write_csr_safe(mhpmevent4, 0x801);
  write_csr_safe(mhpmevent5, 0x1001);
  write_csr_safe(mhpmevent7, 0x4001);
  write_csr_safe(mhpmevent8, 0x8001);
  write_csr_safe(mhpmevent9, 0x10001);

  // Read initial values
  uint64_t p0   = read_csr_safe(mhpmcounter3);
  uint64_t p2   = read_csr_safe(mhpmcounter4);
  uint64_t p4   = read_csr_safe(mhpmcounter5);
  uint64_t I0_i = read_csr_safe(mhpmcounter7);
  uint64_t I1_i = read_csr_safe(mhpmcounter8);
  uint64_t I2_i = read_csr_safe(mhpmcounter9);

  setStats(1);
  for (i = 0; i < DATA_SIZE; i++)
  {
    results_data[i] = multiply( input_data1[i], input_data2[i] );
  }
  setStats(0);

  // Read final values
  uint64_t p1   = read_csr_safe(mhpmcounter3);
  uint64_t p3   = read_csr_safe(mhpmcounter4);
  uint64_t p5   = read_csr_safe(mhpmcounter5);
  uint64_t I0_f = read_csr_safe(mhpmcounter7);
  uint64_t I1_f = read_csr_safe(mhpmcounter8);
  uint64_t I2_f = read_csr_safe(mhpmcounter9);

  uint64_t mispredict      = p1 - p0;
  uint64_t mispredict_BR   = p3 - p2;
  uint64_t mispredict_JALR = p5 - p4;
  uint64_t Ins_commit      = (I0_f - I0_i) + (I1_f - I1_i) + (I2_f - I2_i);

  printf("Mispredict before = %lu after = %lu delta = %lu\n",
         (unsigned long)p0, (unsigned long)p1, (unsigned long)mispredict);
  printf("Branch Mispredict before = %lu after = %lu delta = %lu\n",
         (unsigned long)p2, (unsigned long)p3, (unsigned long)mispredict_BR);
  printf("JALR Mispredict before = %lu after = %lu delta = %lu\n",
         (unsigned long)p4, (unsigned long)p5, (unsigned long)mispredict_JALR);

  printf("Ins[0] before = %lu after = %lu delta = %lu\n",
         (unsigned long)I0_i, (unsigned long)I0_f, (unsigned long)(I0_f - I0_i));
  printf("Ins[1] before = %lu after = %lu delta = %lu\n",
         (unsigned long)I1_i, (unsigned long)I1_f, (unsigned long)(I1_f - I1_i));
  printf("Ins[2] before = %lu after = %lu delta = %lu\n",
         (unsigned long)I2_i, (unsigned long)I2_f, (unsigned long)(I2_f - I2_i));

  if (Ins_commit != 0) {
    uint64_t mpki_scaled      = (mispredict * 1000000ULL) / Ins_commit;
    uint64_t mpki_br_scaled   = (mispredict_BR * 1000000ULL) / Ins_commit;
    uint64_t mpki_jalr_scaled = (mispredict_JALR * 1000000ULL) / Ins_commit;

    printf("MPKI: %lu.%03lu\n",
           (unsigned long)(mpki_scaled / 1000),
           (unsigned long)(mpki_scaled % 1000));

    printf("Branch MPKI: %lu.%03lu\n",
           (unsigned long)(mpki_br_scaled / 1000),
           (unsigned long)(mpki_br_scaled % 1000));

    printf("JALR MPKI: %lu.%03lu\n",
           (unsigned long)(mpki_jalr_scaled / 1000),
           (unsigned long)(mpki_jalr_scaled % 1000));
  } else {
    printf("Instructions committed is zero\n");
  }

  printf("Instructions Committed: %lu\n", (unsigned long)Ins_commit);

  return verify( DATA_SIZE, results_data, verify_data );
}
