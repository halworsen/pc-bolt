/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */

#include <inttypes.h>
#include <stdio.h>

#include "interface.hh"

#define SCOREMAX 31
#define ROUNDMAX 32
#define BADSCORE 0

// Should not be changed without changing all other
// infrastructure surrounding the Recent requests table!
static const size_t rrTableSize = 256;

static Addr recentRequestTable[rrTableSize];

static size_t offsetList[] = {
    1,   2,   3,   4,   5,   6,   8,   9,   10,  12,  15,  16,  18,
    20,  24,  25,  27,  30,  32,  36,  40,  45,  48,  50,  54,  60,
    64,  72,  75,  80,  81,  90,  96,  100, 108, 120, 125, 128, 135,
    144, 150, 160, 162, 180, 192, 200, 216, 225, 240, 243, 250, 256};

static const uint8_t offsetListSize =
    sizeof(offsetList) / (sizeof(offsetList[0]));

static uint64_t scoreList[offsetListSize];

// Null out scores
void clearScores() {
  for (int i = 0; i < offsetListSize; ++i) {
    scoreList[i] = 0;
  }
}

// TODO: Figure out proper way to initialize prefetcher
static size_t currentOffset = 0;

/**
 * @brief Calculates index in Recent Request table for the given address.
 *
 * @param address Address to look up
 * @return index in Recent Request table for the given address
 */
static uint8_t RRHash(Addr address) {
  // TODO: Make an actual hash function
  uint8_t bitmask = 0b11111111;
  // And lower 8 bits with the next 8 bits
  return (address & bitmask) & ((address >> 8) & bitmask);
}

/**
 * Called before any calls to prefetch_access.
 * This is the place to initialize data structures.
 */
void prefetch_init(void) {
  // DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");

  // Null out recent request table
  for (int i = 0; i < rrTableSize; ++i) {
    recentRequestTable[i] = 0;
  }
  clearScores();
}

unsigned int roundCount = 0;
unsigned int offsetIndex = 0;
void prefetch_access(AccessStat stat) {
  // currentOffset == 0 implies no prefetching
  if ((currentOffset != 0)) {
    // Cache miss or prefetch hit
    DPRINTF(HWPrefetch,"Miss: %d, Prefetch bit: %d\n", stat.miss, get_prefetch_bit(stat.mem_addr));
    if (stat.miss || (!stat.miss && get_prefetch_bit(stat.mem_addr))) {
      DPRINTF(HWPrefetch, "Performed prefetch\n");
      Addr pf_addr = stat.mem_addr + BLOCK_SIZE * currentOffset;
      issue_prefetch(pf_addr);
    }
  }

  size_t testOffset = offsetList[offsetIndex];
  Addr testAddress = stat.mem_addr - testOffset * BLOCK_SIZE;
  bool isInRR = false;
  for (int i = 0; i < rrTableSize; ++i){
    if (recentRequestTable[i] == testAddress){
      isInRR = true;
      break;
    }
  }
  if (isInRR){
    scoreList[offsetIndex] += 1;
    if (scoreList[offsetIndex] >= SCOREMAX){
      // Scoremax reached, trigger new training phase
      roundCount = ROUNDMAX-1;
      offsetIndex = offsetListSize-1;
    }
  }

  offsetIndex = (offsetIndex + 1) % offsetListSize;
  if (offsetIndex == 0) {
    roundCount = (roundCount + 1) % ROUNDMAX;
    if (roundCount == 0) {
      // Find best offset and start new training phase

      DPRINTF(HWPrefetch, "Ending training phase\n");
      size_t bestOffset = 0;
      uint64_t bestScore = 0;
      for (int i = 0; i < offsetListSize; ++i) {
        if (scoreList[i] >= bestScore && scoreList[i] > BADSCORE) {
          bestOffset = offsetList[i];
          bestScore = scoreList[i];
          DPRINTF(HWPrefetch, "Current best offset: %d\n", bestOffset);
        }
      }
      currentOffset = bestOffset;
      DPRINTF(HWPrefetch, "Chose new offset: %d\n", currentOffset);
      clearScores();
    }
  }
}

void prefetch_complete(Addr addr) {
  /*
   * Called when a block requested by the prefetcher has been loaded.
   */
  set_prefetch_bit(addr);
  uint8_t rrIndex = RRHash(addr);
  recentRequestTable[rrIndex] = addr - currentOffset * BLOCK_SIZE;
}
