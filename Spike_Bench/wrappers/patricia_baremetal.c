#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../mibench/network/patricia/patricia.h"

struct route_payload {
  uint32_t id;
};

static inline void roi_start(void)
{
  __asm__ volatile(
    "li a0, 1\n\t"
    "ebreak\n\t"
    :
    :
    : "a0", "memory");
}

static inline void roi_end(void)
{
  __asm__ volatile(
    "li a0, 2\n\t"
    "ebreak\n\t"
    :
    :
    : "a0", "memory");
}

static struct ptree *make_head(void)
{
  struct ptree *head = (struct ptree *)malloc(sizeof(*head));
  struct ptree_mask *mask = (struct ptree_mask *)malloc(sizeof(*mask));
  struct route_payload *payload =
    (struct route_payload *)malloc(sizeof(*payload));

  memset(head, 0, sizeof(*head));
  memset(mask, 0, sizeof(*mask));
  payload->id = 0;

  head->p_m = mask;
  head->p_mlen = 1;
  head->p_b = 0;
  head->p_left = head;
  head->p_right = head;
  mask->pm_mask = 0;
  mask->pm_data = payload;

  return head;
}

static struct ptree *make_node(unsigned long key, unsigned long mask_bits,
                               uint32_t id)
{
  struct ptree *node = (struct ptree *)malloc(sizeof(*node));
  struct ptree_mask *mask = (struct ptree_mask *)malloc(sizeof(*mask));
  struct route_payload *payload =
    (struct route_payload *)malloc(sizeof(*payload));

  memset(node, 0, sizeof(*node));
  memset(mask, 0, sizeof(*mask));
  payload->id = id;

  node->p_key = key;
  node->p_m = mask;
  node->p_mlen = 1;
  mask->pm_mask = mask_bits;
  mask->pm_data = payload;

  return node;
}

static const struct {
  unsigned long key;
  unsigned long mask;
  uint32_t id;
} routes[] = {
  {0x0A000000UL, 0xFF000000UL, 1},
  {0x0A010000UL, 0xFFFF0000UL, 2},
  {0x0A010100UL, 0xFFFFFF00UL, 3},
  {0x0A020000UL, 0xFFFF0000UL, 4},
  {0x0A020200UL, 0xFFFFFF00UL, 5},
  {0xAC100000UL, 0xFFF00000UL, 6},
  {0xAC101000UL, 0xFFFFF000UL, 7},
  {0xC0A80000UL, 0xFFFF0000UL, 8},
  {0xC0A80100UL, 0xFFFFFF00UL, 9},
  {0xC0A80180UL, 0xFFFFFF80UL, 10},
  {0x64400000UL, 0xFFC00000UL, 11},
  {0x64401000UL, 0xFFFFF000UL, 12}
};

static const unsigned long lookups[] = {
  0x0A010164UL, 0x0A0101AAUL, 0x0A02027FUL, 0x0A020201UL,
  0xAC101234UL, 0xAC101ABCUL, 0xC0A80182UL, 0xC0A801FEUL,
  0x64401234UL, 0x6440FFFFUL, 0x0A030101UL, 0xC0A80201UL,
  0x01010101UL, 0x7F000001UL, 0x0A010164UL, 0xC0A80182UL
};

int main(void)
{
  struct ptree *head = make_head();
  volatile uintptr_t checksum = 0;

  for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
    struct ptree *node = make_node(routes[i].key, routes[i].mask, routes[i].id);
    pat_insert(node, head);
  }

  roi_start();
  for (int round = 0; round < 200; ++round) {
    for (size_t i = 0; i < sizeof(lookups) / sizeof(lookups[0]); ++i) {
      struct ptree *result = pat_search(lookups[i], head);
      if (result != NULL && result->p_m != NULL && result->p_m->pm_data != NULL) {
        struct route_payload *payload =
          (struct route_payload *)result->p_m->pm_data;
        checksum += payload->id;
      }
      checksum += (uintptr_t)(result != NULL);
    }
  }
  roi_end();

  if (checksum == 0xdeadbeefU)
    return 1;
  return 0;
}
