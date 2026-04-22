#include <stddef.h>
#include <stdint.h>

void bmha_init(const char *pattern);
char *bmha_search(const char *string, const int stringlen);

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

static const char *const patterns[] = {
  "Kur", "gent", "lass", "suns", "for", "long", "have", "where",
  "pense", "pow", "Yo", "and", "faded", "you", "way", "possibili",
  "fat", "imag", "wor", "kind", "idle", "scare", "people", "yourself",
  "Forget", "succeed", "life", "Maybe", "congratulate", "greatest",
  "beauty", "Northern", "Politicians", "support", "trust", "careful"
};

static const char *const texts[] = {
  "Kurt Vonneguts commencement speech became internet folklore for years.",
  "Urgent messages rarely sound gentle when written in haste.",
  "A glass of water on the table catches late afternoon light.",
  "Use sunscreen every day because the suns rays do not negotiate.",
  "Look for joy in ordinary places before looking elsewhere.",
  "How long you have known someone changes the way advice lands.",
  "We have all stood somewhere between panic and confidence.",
  "No one knows where certainty goes when the room gets quiet.",
  "Suspense makes nonsense of plans when you are tired.",
  "Knowledge grows slowly and then all at once with practice.",
  "Young people are usually told to hurry before they understand why.",
  "You and I both know that memory edits the rough parts first.",
  "Colors fade and then surprise you by returning in spring light.",
  "If you feel ugly, beauty magazines are the wrong tribunal.",
  "Either way you choose, chance gets a vote in the ending.",
  "Every possibility feels obvious after it already happened.",
  "The old cat slept on the fat red cushion by the warm radiator.",
  "Imagination makes ordinary corners of a room feel much larger.",
  "Words matter most when they are almost but not quite right.",
  "Be kind because nearly everyone is improvising in public.",
  "Idle worry is still work, just badly directed work.",
  "Dont let people scare you out of your own careful thinking.",
  "People remember how you made the room feel, not your outline.",
  "Yourself is the hardest person to address honestly.",
  "Forget the lottery mindset and do the boring things repeatedly.",
  "Small routines succeed where heroic intentions evaporate.",
  "Life gets noisier unless you make a home for attention.",
  "Maybe youll marry, maybe you wont, maybe youll dance at forty.",
  "Congratulate yourself lightly and forgive yourself quickly.",
  "The greatest instrument youll ever own is your own body.",
  "Beauty is not a debt you owe to strangers passing by.",
  "Northern cities harden you if you never learn to leave them.",
  "Politicians will disappoint you on a schedule of their own.",
  "Support from friends is fragile if you never practice returning it.",
  "Trust takes longer to build than almost anything worth having.",
  "Be careful with advice because nostalgia edits the cost."
};

int main(void)
{
  volatile uintptr_t checksum = 0;

  roi_start();
  for (int round = 0; round < 200; ++round) {
    for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); ++i) {
      bmha_init(patterns[i]);
      char *hit = bmha_search(texts[i], (int)__builtin_strlen(texts[i]));
      checksum += (uintptr_t)(hit != NULL);
      if (hit != NULL)
        checksum += (uintptr_t)(hit - texts[i]);
    }
  }
  roi_end();

  if (checksum == 0xdeadbeefU)
    return 1;
  return 0;
}
