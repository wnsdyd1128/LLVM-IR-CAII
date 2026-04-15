#pragma once
#include <cstdint>
#include <unordered_set>
namespace GR740Cache
{

constexpr uint32_t LINE_SIZE = 32;
constexpr uint32_t NUM_WAYS = 4;
constexpr uint32_t NUM_SETS = 16384;
constexpr uint32_t TOTAL_LINES = NUM_WAYS * NUM_SETS;

inline uint32_t block_id(uint64_t addr)
{
  return addr >> 5;  // Divide by line size (32 bytes = 2^5)
}

inline uint32_t set_index(uint64_t addr)
{
  return block_id(addr) % NUM_SETS;  // Modulo number of sets
}

struct CacheBlock
{
  uint64_t block_id;
  uint32_t set_idx;

  bool operator==(const CacheBlock & other) const
  {
    return block_id == other.block_id;
  }
};
using CacheBlockSet = std::unordered_set<uint64_t>;
}  // namespace GR740Cache
