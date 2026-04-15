#include "CacheConfig.hpp"
#include <gtest/gtest.h>

using namespace GR740Cache;

// ── 하드웨어 상수 검증 ────────────────────────────────────────────────────
// GR740 UM v2.7: 2MiB L2, 4-way, 32B line → 16384 sets

TEST(GR740Constants, Values) {
    EXPECT_EQ(LINE_SIZE,   32u);
    EXPECT_EQ(NUM_WAYS,     4u);
    EXPECT_EQ(NUM_SETS,  16384u);
    EXPECT_EQ(TOTAL_LINES, NUM_WAYS * NUM_SETS);  // 65536
}

// ── block_id: addr >> 5 ───────────────────────────────────────────────────

TEST(BlockId, AlignedAddresses) {
    EXPECT_EQ(block_id(0),     0u);
    EXPECT_EQ(block_id(32),    1u);   // 정확히 한 캐시 라인
    EXPECT_EQ(block_id(64),    2u);
    EXPECT_EQ(block_id(16384), 512u); // 0x4000
}

TEST(BlockId, UnalignedAddresses) {
    // 같은 캐시 라인 안에 있으면 block_id 동일
    EXPECT_EQ(block_id(0),  block_id(31));
    EXPECT_EQ(block_id(32), block_id(63));
    EXPECT_NE(block_id(31), block_id(32));
}

// ── set_index: block_id % NUM_SETS ───────────────────────────────────────

TEST(SetIndex, BasicMapping) {
    EXPECT_EQ(set_index(0),      0u);
    EXPECT_EQ(set_index(32),     1u);  // block_id=1 → set 1
    EXPECT_EQ(set_index(16384),  512u); // block_id=512 → set 512
}

TEST(SetIndex, WrapAround) {
    // NUM_SETS * LINE_SIZE = 16384 * 32 = 524288 bytes → set 0 으로 wrap
    const uint64_t one_cache = static_cast<uint64_t>(NUM_SETS) * LINE_SIZE;
    EXPECT_EQ(set_index(0),        set_index(one_cache));
    EXPECT_EQ(set_index(32),       set_index(one_cache + 32));
    EXPECT_EQ(set_index(one_cache - 32), NUM_SETS - 1);
}

TEST(SetIndex, TwoCacheWraps) {
    const uint64_t two_cache = 2ULL * NUM_SETS * LINE_SIZE;
    EXPECT_EQ(set_index(0), set_index(two_cache));
}

// ── 캐시 라인 경계 케이스 ─────────────────────────────────────────────────

TEST(CacheConfig, LastByteOfLine) {
    // 주소 31(0x1F)과 0은 같은 캐시 라인
    EXPECT_EQ(block_id(0), block_id(31));
    EXPECT_EQ(set_index(0), set_index(31));
}

TEST(CacheConfig, FirstByteNextLine) {
    // 주소 32는 다음 캐시 라인
    EXPECT_NE(block_id(0), block_id(32));
    EXPECT_NE(set_index(0), set_index(32));
}