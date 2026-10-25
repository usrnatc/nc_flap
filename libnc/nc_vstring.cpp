#include "nc_vstring.h"
#include "nc_memory.h"
#include "nc_simd.h"
#include "nc_arena.h"
#include "nc_file.h"

#if NC_SIMD_LEVEL >= 8
    #define VMASK_ALL 0xFFFFFFFFFFFFFFFFULL

    typedef v512 VChunk8;
    typedef u64 VMask8;
#elif NC_SIMD_LEVEL >= 6
    #define VMASK_ALL 0xFFFFFFFFU

    typedef v256 VChunk8;
    typedef u32 VMask8;
#elif NC_SIMD_LEVEL >= 2
    #define VMASK_ALL 0xFFFFU

    typedef v128 VChunk8;
    typedef u16 VMask8;
#elif defined(NC_CPU_NEON)
    #error nc_vstring: NEON chunk layer not implemented
#else
    #error nc_vstring: SWAR chunk layer not implemented
#endif

#define VMASK_EVEN ((VMask8) 0x5555555555555555ull)
#define VMASK_ODD  ((VMask8) 0xAAAAAAAAAAAAAAAAull)
#define VMASK_HIGH ((VMask8) ((VMask8) 1 << (VCHUNK_WIDTH - 1)))

#define V_UTF8_TOO_SHORT      (1 << 0)
#define V_UTF8_TOO_LONG       (1 << 1)
#define V_UTF8_OVERLONG_3     (1 << 2)
#define V_UTF8_TOO_LARGE      (1 << 3)
#define V_UTF8_SURROGATE      (1 << 4)
#define V_UTF8_OVERLONG_2     (1 << 5)
#define V_UTF8_TOO_LARGE_1000 (1 << 6)
#define V_UTF8_OVERLONG_4     (1 << 6)
#define V_UTF8_TWO_CONTS      (1 << 7)
#define V_UTF8_CARRY          (V_UTF8_TOO_SHORT | V_UTF8_TOO_LONG | V_UTF8_TWO_CONTS)

union VClass {
    struct {
        VMask8 Space;
        VMask8 Digit;
        VMask8 Alpha;
        VMask8 Slash;
        VMask8 Hex;
        VMask8 Ident;
        VMask8 Quote;
        VMask8 Escape;
    };

    VMask8 M[8];
};

struct VNibbleTable {
    u8 Lo[16];
    u8 Hi[16];
};

struct VNeedleData {
    Str8         Needle;
    u8           Head;
    u8           Tail;
    u32          TailOffset;
    StrMatchKind Method;
};

struct VNeedleSetData {
    Str8         Needles[V_NEEDLE_SET_MAX];
    u32          Count;
    u32          MinLength;
    StrMatchKind Method;
    VNibbleTable Fingerprint[3];
};

struct VPatternData {
    u64          Occurrence[256];
    u64          Accept;
    u32          Length;
    StrMatchKind Method;
};

struct VKeyIndex {
    u64*  Prefix;
    u64*  Hash;
    Str8* Key;
    u64   Count;
};

struct VScanState {
    u64 PrevWasInString;
    u64 PrevWasEscape;
};

struct VStructural {
    VMask8 Quote;
    VMask8 Escape;
    VMask8 InString;
    VMask8 Structural;
    VMask8 Space;
};

STATIC_ASSERT(sizeof(VNeedleData) <= sizeof(VNeedle), VNeedleDataSizeCheck);
STATIC_ASSERT(sizeof(VNeedleSetData) <= sizeof(VNeedleSet), VNeedleSetDataSizeCheck);
STATIC_ASSERT(sizeof(VPatternData) <= sizeof(VPattern), VPatternDataSizeCheck);
STATIC_ASSERT(sizeof(VMask8) * 8 == VCHUNK_WIDTH, VMask8SizeCheck);

const VClassTable V_CLASS_TABLE_ASCII = {
    {
        0x16, 
        0x3C, 
        0x3C, 
        0x3C, 
        0x3C, 
        0x3C, 
        0x3C, 
        0x1C,
        0x1C, 
        0x1D, 
        0x19, 
        0x09, 
        0x09, 
        0x09, 
        0x08, 
        0x48 
    },
    { 
        0x01, 
        0x00, 
        0x02, 
        0x04, 
        0x28, 
        0x50, 
        0x28, 
        0x10,
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00 
    },
    { 
        0x03, 
        0x04, 
        0x18, 
        0x00, 
        0x24, 
        0x5C, 
        0x00, 
        0x00 
    }
};

const VClassTable V_CLASS_TABLE_PATH = {
    {
        0x60, 
        0x40, 
        0x40, 
        0x40, 
        0x40, 
        0x40, 
        0x40, 
        0x40,
        0x40, 
        0x50, 
        0x10, 
        0x10, 
        0x12, 
        0x10, 
        0x00, 
        0x01 
    },
    { 
        0x10, 
        0x00, 
        0x21, 
        0x40, 
        0x00, 
        0x02, 
        0x00, 
        0x00,
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00 
    },
    { 
        0x30, 
        0x40, 
        0x00, 
        0x03, 
        0x00, 
        0x00, 
        0x00, 
        0x02 
    }
};

const VClassTable V_CLASS_TABLE_JSON = {
    { 
        0x82, 
        0x80, 
        0x84, 
        0x80, 
        0x80, 
        0x80, 
        0x80, 
        0x80,
        0x80, 
        0x81, 
        0x21, 
        0x10, 
        0x48, 
        0x11, 
        0x00, 
        0x00 
    },
    { 
        0x01, 
        0x00, 
        0x46, 
        0xA0, 
        0x00, 
        0x18, 
        0x00, 
        0x10,
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00, 
        0x00 
    },
    { 
        0x03, 
        0x80, 
        0x00, 
        0x00, 
        0x00, 
        0x70, 
        0x04, 
        0x08 
    }
};

global const u8 V_HEX_ENCODE_LOWER[16] = {
    '0', 
    '1', 
    '2', 
    '3', 
    '4', 
    '5', 
    '6', 
    '7',
    '8', 
    '9', 
    'a', 
    'b', 
    'c', 
    'd', 
    'e', 
    'f'
};

global const u8 V_HEX_ENCODE_UPPER[16] = {
    '0', 
    '1', 
    '2', 
    '3', 
    '4', 
    '5', 
    '6', 
    '7',
    '8', 
    '9', 
    'A', 
    'B', 
    'C', 
    'D', 
    'E', 
    'F'
};

global const u8 V_HEX_DECODE_HI[16] = {
    0x00, 
    0x00, 
    0x00, 
    0xD0, 
    0xC9, 
    0x00, 
    0xA9, 
    0x00,
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00
};

global const u8 V_UTF8_BYTE1_HIGH[16] = {
    V_UTF8_TOO_LONG, 
    V_UTF8_TOO_LONG, 
    V_UTF8_TOO_LONG, 
    V_UTF8_TOO_LONG,
    V_UTF8_TOO_LONG, 
    V_UTF8_TOO_LONG, 
    V_UTF8_TOO_LONG, 
    V_UTF8_TOO_LONG,
    V_UTF8_TWO_CONTS, 
    V_UTF8_TWO_CONTS, 
    V_UTF8_TWO_CONTS, 
    V_UTF8_TWO_CONTS,
    V_UTF8_TOO_SHORT | V_UTF8_OVERLONG_2,
    V_UTF8_TOO_SHORT,
    V_UTF8_TOO_SHORT | V_UTF8_OVERLONG_3 | V_UTF8_SURROGATE,
    V_UTF8_TOO_SHORT | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000 | V_UTF8_OVERLONG_4
};

global const u8 V_UTF8_BYTE1_LOW[16] = {
    V_UTF8_CARRY | V_UTF8_OVERLONG_3 | V_UTF8_OVERLONG_2 | V_UTF8_OVERLONG_4,
    V_UTF8_CARRY | V_UTF8_OVERLONG_2,
    V_UTF8_CARRY,
    V_UTF8_CARRY,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000 | V_UTF8_SURROGATE,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000,
    V_UTF8_CARRY | V_UTF8_TOO_LARGE | V_UTF8_TOO_LARGE_1000
};

global const u8 V_UTF8_BYTE2_HIGH[16] = {
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT,
    V_UTF8_TOO_SHORT,
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT,
    V_UTF8_TOO_SHORT,
    V_UTF8_TOO_LONG | V_UTF8_OVERLONG_2 | V_UTF8_TWO_CONTS | V_UTF8_OVERLONG_3 | V_UTF8_TOO_LARGE_1000 | V_UTF8_OVERLONG_4,
    V_UTF8_TOO_LONG | V_UTF8_OVERLONG_2 | V_UTF8_TWO_CONTS | V_UTF8_OVERLONG_3 | V_UTF8_TOO_LARGE,
    V_UTF8_TOO_LONG | V_UTF8_OVERLONG_2 | V_UTF8_TWO_CONTS | V_UTF8_SURROGATE  | V_UTF8_TOO_LARGE,
    V_UTF8_TOO_LONG | V_UTF8_OVERLONG_2 | V_UTF8_TWO_CONTS | V_UTF8_SURROGATE  | V_UTF8_TOO_LARGE,
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT, 
    V_UTF8_TOO_SHORT
};

global const u8  V_DIGIT_WEIGHTS_1_10[16] = { 
    10, 
    1, 
    10, 
    1, 
    10, 
    1, 
    10, 
    1, 
    10, 
    1, 
    10, 
    1, 
    10, 
    1, 
    10, 
    1 
};

global const u16 V_DIGIT_WEIGHTS_1_100[8] = { 
    100, 
    1, 
    100, 
    1, 
    100, 
    1, 
    100, 
    1 
};

global const u16 V_DIGIT_WEIGHTS_1_10000[8] = { 
    10000, 
    1, 
    10000, 
    1, 
    10000, 
    1, 
    10000, 
    1 
};

global const u8 V_MASK_SPREAD_SHUFFLE[32] = {
    0, 
    0, 
    0,
    0, 
    0, 
    0, 
    0, 
    0, 
    1, 
    1, 
    1, 
    1, 
    1, 
    1, 
    1, 
    1,
    2, 
    2, 
    2, 
    2, 
    2, 
    2, 
    2, 
    2, 
    3, 
    3, 
    3, 
    3, 
    3, 
    3, 
    3, 
    3
};

global const u8 V_MASK_LANE_BITS[32] = {
    1, 
    2, 
    4, 
    8, 
    16, 
    32, 
    64, 
    128, 
    1, 
    2, 
    4, 
    8, 
    16, 
    32, 
    64, 
    128,
    1, 
    2, 
    4, 
    8, 
    16, 
    32, 
    64, 
    128, 
    1, 
    2, 
    4, 
    8, 
    16, 
    32, 
    64, 
    128
};

global const u8 V_DIGIT_ALIGN_SHUFFLE[32] = {
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF,
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF,
    0x00,
    0x01,
    0x02,
    0x03,
    0x04,
    0x05,
    0x06,
    0x07,
    0x08,
    0x09,
    0x0A,
    0x0B,
    0x0C,
    0x0D,
    0x0E,
    0x0F
};

global const u8 V_TAIL_MASK_BYTES[32] = {
    0xFF, 
    0xFF, 
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF,
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF, 
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00
};

INTERNAL u32
VClassIndex(VClassKind Kind)
{
    ASSERT_POW2(Kind);

    return Ctz64(Kind);
}

INTERNAL VMask8
VMaskTail(u64 Remaining)
{
    VMask8 Result = VMASK_ALL;

    if (Remaining < VCHUNK_WIDTH)
        Result = (VMask8) (((u64) 1 << Remaining) - 1);

    return Result;
}

INTERNAL u32
VMaskFirst(VMask8 Mask)
{
    return Mask
        ? Ctz64(Mask)
        : VCHUNK_WIDTH;
}

INTERNAL u32
VMaskLast(VMask8 Mask)
{
    return Mask
        ? (63 - Clz64(Mask))
        : VCHUNK_WIDTH;
}

INTERNAL u32
VMaskCount(VMask8 Mask)
{
    return PopCount64(Mask);
}

INTERNAL u32
VMaskPop(VMask8* Mask)
{
    u32 Result = Ctz64(*Mask);

    *Mask &= (VMask8) (*Mask - 1);

    return Result;
}

INTERNAL VMask8
VMaskAfter(VMask8 Mask, u32 Lane)
{
    return (Lane + 1 >= VCHUNK_WIDTH)
        ? 0
        : (VMask8) (Mask & (VMASK_ALL << (Lane + 1)));
}

INTERNAL VMask8
VMaskBefore(VMask8 Mask, u32 Lane)
{
    return (VMask8) (Mask & (((VMask8) 1 << Lane) - 1));
}

INTERNAL VMask8
VMaskPrefixXOR(VMask8 Mask, u64* Carry)
{
    VMask8 Result = 0;

#if defined(NC_CPU_PCLMUL)
    v128 Product = _mm_clmulepi64_si128(
        _mm_set_epi64x(0, (i64) Mask),
        _mm_set1_epi8((u8) 0xFF),
        0
    );

    Result = (VMask8) _mm_cvtsi128_si64(Product);
#else
    Result = Mask;
    Result ^= (VMask8) (Result << 1);
    Result ^= (VMask8) (Result << 2);
    Result ^= (VMask8) (Result << 4);
    Result ^= (VMask8) (Result << 8);

    #if VCHUNK_WIDTH > 16
        Result ^= (VMask8) (Result << 16);
    #endif

    #if VCHUNK_WIDTH > 32
        Result ^= (VMask8) (Result << 32);
    #endif
#endif

    Result ^= (VMask8) *Carry;
    *Carry = (Result & VMASK_HIGH) ? ~0ULL : 0ULL;

    return Result;
}

INTERNAL VMask8
VMaskShiftIn(VMask8 Mask, u32 Bits, u64* Carry)
{
    VMask8 Result = (VMask8) ((Mask << Bits) | (VMask8) *Carry);

    *Carry = (u64) (Mask >> (VCHUNK_WIDTH - Bits));

    return Result;
}

#if NC_SIMD_LEVEL >= 8
    INTERNAL VChunk8
    VChunkLoad(u8 const* Ptr)
    {
        return _mm512_loadu_si512((void const*) Ptr);
    }

    INTERNAL VChunk8
    VChunkLoadTail(u8 const* Ptr, u64 Remaining)
    {
        return _mm512_maskz_loadu_epi8(
            VMaskTail(Remaining), 
            (void const*) Ptr
        );
    }

    INTERNAL VChunk8
    VChunkSet(u8 Byte)
    {
        return _mm512_set1_epi8(Byte);
    }

    INTERNAL VChunk8
    VChunkZero(void)
    {
        return _mm512_setzero_si512();
    }

    INTERNAL void
    VChunkStore(u8* Dst, VChunk8 Chunk)
    {
        _mm512_storeu_si512((void*) Dst, Chunk);
    }

    INTERNAL void
    VChunkStoreTail(u8* Dst, VChunk8 Chunk, u64 Count)
    {
        _mm512_mask_storeu_epi8(
            (void*) Dst, 
            VMaskTail(Count), 
            Chunk
        );
    }

    INTERNAL VChunk8
    VChunkAND(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_and_si512(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkOR(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_or_si512(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkXOR(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_xor_si512(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkADD(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_add_epi8(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkSUB(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_sub_epi8(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkSUBSAT(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_subs_epu8(ChunkA, ChunkB);
    }

    INTERNAL VMask8
    VChunkEq(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm512_cmpeq_epi8_mask(ChunkA, ChunkB);
    }

    INTERNAL VMask8
    VChunkEQ(VChunk8 Chunk, u8 Byte)
    {
        return _mm512_cmpeq_epi8_mask(Chunk, VChunkSet(Byte));
    }

    INTERNAL VMask8
    VChunkRange(VChunk8 Chunk, u8 Lo, u8 Hi)
    {
        VChunk8 Shifted = _mm512_subs_epu8(Chunk, VChunkSet(Lo));

        return _mm512_cmple_epu8_mask(Shifted, VChunkSet((u8) (Hi - Lo)));
    }

    INTERNAL VMask8
    VChunkTest(VChunk8 Chunk, u8 Bits)
    {
        return _mm512_test_epi8_mask(Chunk, VChunkSet(Bits));
    }

    INTERNAL VMask8
    VChunkToMask(VChunk8 Chunk)
    {
        return _mm512_movepi8_mask(Chunk);
    }

    INTERNAL VChunk8
    VChunkFromMask(VMask8 Mask)
    {
        return _mm512_movm_epi8(Mask);
    }

    INTERNAL VMask8
    VChunkHiBits(VChunk8 Chunk)
    {
        return _mm512_movepi8_mask(Chunk);
    }

    INTERNAL VChunk8
    VChunkFold(VChunk8 Chunk)
    {
        VMask8 Upper = VChunkRange(Chunk, 'A', 'Z');

        return _mm512_mask_add_epi8(Chunk, Upper, Chunk, VChunkSet(0x20));
    }

    INTERNAL VChunk8
    VChunkSlashFold(VChunk8 Chunk)
    {
        VMask8 Back = VChunkEQ(Chunk, '\\');

        return _mm512_mask_mov_epi8(Chunk, Back, VChunkSet('/'));
    }

    INTERNAL VChunk8
    VChunkBlend(VChunk8 ChunkA, VChunk8 ChunkB, VMask8 TakeB)
    {
        return _mm512_mask_blend_epi8(TakeB, ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkTable(u8 const Table[16])
    {
        return _mm512_broadcast_i32x4(
            _mm_loadu_si128((v128 const*) Table)
        );
    }

    INTERNAL VChunk8
    VChunkLookup(VChunk8 Chunk, u8 const Table[16])
    {
        VChunk8 Index = _mm512_and_si512(Chunk, VChunkSet(0x0F));

        return _mm512_shuffle_epi8(VChunkTable(Table), Index);
    }

    INTERNAL VChunk8
    VChunkLookupHi(VChunk8 Chunk, u8 const Table[16])
    {
        VChunk8 Index = _mm512_and_si512(
            _mm512_srli_epi16(Chunk, 4), 
            VChunkSet(0x0F)
        );

        return _mm512_shuffle_epi8(VChunkTable(Table), Index);
    }

    INTERNAL VChunk8
    VChunkShiftRight(VChunk8 Prev, VChunk8 Chunk, u32 Bytes)
    {
        ASSERT(
            Bytes == 1 || 
            Bytes == 2 || 
            Bytes == 3
        );

        VChunk8 Result = {};
        VChunk8 Lanes = _mm512_alignr_epi32(Chunk, Prev, 12);

        switch (Bytes) {
            default: {
                Result = _mm512_alignr_epi8(Chunk, Lanes, 13);
            } break;

            case 1: {
                Result = _mm512_alignr_epi8(Chunk, Lanes, 15);
            } break;

            case 2: {
                Result = _mm512_alignr_epi8(Chunk, Lanes, 14);
            } break;
        }

        return Result;
    }
#elif NC_SIMD_LEVEL >= 6
    INTERNAL VChunk8
    VChunkLoad(u8 const* Ptr)
    {
        return _mm256_loadu_si256((v256 const*) Ptr);
    }

    INTERNAL VChunk8
    VChunkLoadTail(u8 const* Ptr, u64 Remaining)
    {
        VChunk8 Result = {};

        if (Remaining >= VCHUNK_WIDTH) {
            Result = VChunkLoad(Ptr);
        } else if (
            (
                (uptr) Ptr & (MEM_INFO.PageSize - 1)
            ) <= MEM_INFO.PageSize - VCHUNK_WIDTH
        ) {
            Result = VChunkLoad(Ptr);
        } else {
            ALIGNAS(VCHUNK_WIDTH) u8 Buffer[VCHUNK_WIDTH] = {};

            MemCpy(Buffer, Ptr, Remaining);
            Result = _mm256_load_si256((v256 const*) Buffer);
        }

        return Result;
    }

    INTERNAL VChunk8
    VChunkSet(u8 Byte)
    {
        return _mm256_set1_epi8((char) Byte);
    }

    INTERNAL VChunk8
    VChunkZero(void)
    {
        return _mm256_setzero_si256();
    }

    INTERNAL void
    VChunkStore(u8* Dst, VChunk8 Chunk)
    {
        _mm256_storeu_si256((v256*) Dst, Chunk);
    }

    INTERNAL void
    VChunkStoreTail(u8* Dst, VChunk8 Chunk, u64 Count)
    {
        if (Count >= VCHUNK_WIDTH) {
            VChunkStore(Dst, Chunk);
        } else {
            ALIGNAS(VCHUNK_WIDTH) u8 Buffer[VCHUNK_WIDTH];

            _mm256_store_si256((v256*) Buffer, Chunk);
            MemCpy(Dst, Buffer, Count);
        }
    }

    INTERNAL VChunk8
    VChunkAND(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm256_and_si256(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkOR(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm256_or_si256(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkXOR(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm256_xor_si256(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkADD(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm256_add_epi8(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkSUB(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm256_sub_epi8(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkSUBSAT(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm256_subs_epu8(ChunkA, ChunkB);
    }

    INTERNAL VMask8
    VChunkToMask(VChunk8 Chunk)
    {
        return (VMask8) _mm256_movemask_epi8(Chunk);
    }

    INTERNAL VChunk8
    VChunkFromMask(VMask8 Mask)
    {
        VChunk8 Spread = _mm256_shuffle_epi8(
            _mm256_set1_epi32((i32) Mask),
            _mm256_loadu_si256((v256 const*) V_MASK_SPREAD_SHUFFLE)
        );
        VChunk8 Bits = _mm256_loadu_si256((v256 const*) V_MASK_LANE_BITS);

        return _mm256_cmpeq_epi8(_mm256_and_si256(Spread, Bits), Bits);
    }

    INTERNAL VChunk8
    __VChunkRangeV(VChunk8 Chunk, u8 Lo, u8 Hi)
    {
        VChunk8 Shifted = _mm256_subs_epu8(Chunk, VChunkSet(Lo));
        VChunk8 Limit = VChunkSet((u8) (Hi - Lo));

        return _mm256_cmpeq_epi8(_mm256_max_epu8(Shifted, Limit), Limit);
    }

    INTERNAL VMask8
    VChunkEQ(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return VChunkToMask(_mm256_cmpeq_epi8(ChunkA, ChunkB));
    }

    INTERNAL VMask8
    VChunkEQ(VChunk8 Chunk, u8 Byte)
    {
        return VChunkEQ(Chunk, VChunkSet(Byte));
    }

    INTERNAL VMask8
    VChunkRange(VChunk8 Chunk, u8 Lo, u8 Hi)
    {
        return VChunkToMask(__VChunkRangeV(Chunk, Lo, Hi));
    }

    INTERNAL VMask8
    VChunkTest(VChunk8 Chunk, u8 Bits)
    {
        VChunk8 Masked = _mm256_and_si256(Chunk, VChunkSet(Bits));
        VChunk8 IsZero = _mm256_cmpeq_epi8(Masked, VChunkZero());

        return (VMask8) (VChunkToMask(IsZero) ^ VMASK_ALL);
    }

    INTERNAL VMask8
    VChunkHiBits(VChunk8 Chunk)
    {
        return VChunkToMask(Chunk);
    }

    INTERNAL VChunk8
    VChunkFold(VChunk8 Chunk)
    {
        VChunk8 Upper = __VChunkRangeV(Chunk, 'A', 'Z');

        return _mm256_or_si256(
            Chunk, 
            _mm256_and_si256(Upper, VChunkSet(0x20))
        );
    }

    INTERNAL VChunk8
    VChunkSlashFold(VChunk8 Chunk)
    {
        VChunk8 Back = _mm256_cmpeq_epi8(Chunk, VChunkSet('\\'));

        return _mm256_blendv_epi8(Chunk, VChunkSet('/'), Back);
    }

    INTERNAL VChunk8
    VChunkBlend(VChunk8 ChunkA, VChunk8 ChunkB, VMask8 TakeB)
    {
        return _mm256_blendv_epi8(ChunkA, ChunkB, VChunkFromMask(TakeB));
    }

    INTERNAL VChunk8
    VChunkTable(u8 const Table[16])
    {
        return _mm256_broadcastsi128_si256(
            _mm_loadu_si128((v128 const*) Table)
        );
    }

    INTERNAL VChunk8
    VChunkLookup(VChunk8 Chunk, u8 const Table[16])
    {
        VChunk8 Index = _mm256_and_si256(Chunk, VChunkSet(0x0F));

        return _mm256_shuffle_epi8(VChunkTable(Table), Index);
    }

    INTERNAL VChunk8
    VChunkLookupHi(VChunk8 Chunk, u8 const Table[16])
    {
        VChunk8 Index = _mm256_and_si256(
            _mm256_srli_epi16(Chunk, 4), 
            VChunkSet(0x0F)
        );

        return _mm256_shuffle_epi8(VChunkTable(Table), Index);
    }

    INTERNAL VChunk8
    VChunkShiftRight(VChunk8 Prev, VChunk8 Chunk, u32 Bytes)
    {
        VChunk8 Result = {};

        ASSERT(
            Bytes == 1 || 
            Bytes == 2 || 
            Bytes == 3
        );

        VChunk8 Lanes = _mm256_permute2x128_si256(Prev, Chunk, 0x21);

        switch (Bytes) {
            default: {
                Result = _mm256_alignr_epi8(Chunk, Lanes, 13);
            } break;

            case 1: {
                Result = _mm256_alignr_epi8(Chunk, Lanes, 15);
            } break;

            case 2: {
                Result = _mm256_alignr_epi8(Chunk, Lanes, 14);
            } break;
        }

        return Result;
    }
#elif NC_SIMD_LEVEL >= 2
    INTERNAL VChunk8
    VChunkLoad(u8 const* Ptr)
    {
        return _mm_loadu_si128((v128 const*) Ptr);
    }

    INTERNAL VChunk8
    VChunkLoadTail(u8 const* Ptr, u64 Remaining)
    {
        VChunk8 Result = {};

        if (Remaining >= VCHUNK_WIDTH) {
            Result = VChunkLoad(Ptr);
        } else if (
            (
                (uptr) Ptr & (MEM_INFO.PageSize - 1)
            ) <= MEM_INFO.PageSize - VCHUNK_WIDTH
        ) {
            Result = VChunkLoad(Ptr);
        } else {
            ALIGNAS(VCHUNK_WIDTH) u8 Buffer[VCHUNK_WIDTH] = {};

            MemCpy(Buffer, Ptr, Remaining);
            Result = _mm_load_si128((v128 const*) Buffer);
        }

        return Result;
    }

    INTERNAL VChunk8
    VChunkSet(u8 Byte)
    {
        return _mm_set1_epi8((char) Byte);
    }

    INTERNAL VChunk8
    VChunkZero(void)
    {
        return _mm_setzero_si128();
    }

    INTERNAL void
    VChunkStore(u8* Dst, VChunk8 Chunk)
    {
        _mm_storeu_si128((v128*) Dst, Chunk);
    }

    INTERNAL void
    VChunkStoreTail(u8* Dst, VChunk8 Chunk, u64 Count)
    {
        if (Count >= VCHUNK_WIDTH) {
            VChunkStore(Dst, Chunk);
        } else {
            ALIGNAS(VCHUNK_WIDTH) u8 Buffer[VCHUNK_WIDTH];

            _mm_store_si128((v128*) Buffer, Chunk);
            MemCpy(Dst, Buffer, Count);
        }
    }

    INTERNAL VChunk8
    VChunkAND(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm_and_si128(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkOR(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm_or_si128(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkXOR(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm_xor_si128(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkADD(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm_add_epi8(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkSUB(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm_sub_epi8(ChunkA, ChunkB);
    }

    INTERNAL VChunk8
    VChunkSUBSAT(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return _mm_subs_epu8(ChunkA, ChunkB);
    }

    INTERNAL VMask8
    VChunkToMask(VChunk8 Chunk)
    {
        return (VMask8) _mm_movemask_epi8(Chunk);
    }

    INTERNAL VChunk8
    VChunkFromMask(VMask8 Mask)
    {
        VChunk8 Spread = _mm_shuffle_epi8(
            _mm_set1_epi16((i16) Mask),
            _mm_loadu_si128((v128 const*) V_MASK_SPREAD_SHUFFLE)
        );
        VChunk8 Bits = _mm_loadu_si128((v128 const*) V_MASK_LANE_BITS);

        return _mm_cmpeq_epi8(_mm_and_si128(Spread, Bits), Bits);
    }

    INTERNAL VChunk8
    __VChunkRangeV(VChunk8 Chunk, u8 Lo, u8 Hi)
    {
        VChunk8 Shifted = _mm_subs_epu8(Chunk, VChunkSet(Lo));
        VChunk8 Limit = VChunkSet((u8) (Hi - Lo));

        return _mm_cmpeq_epi8(_mm_max_epu8(Shifted, Limit), Limit);
    }

    INTERNAL VMask8
    VChunkEQ(VChunk8 ChunkA, VChunk8 ChunkB)
    {
        return VChunkToMask(_mm_cmpeq_epi8(ChunkA, ChunkB));
    }

    INTERNAL VMask8
    VChunkEQ(VChunk8 Chunk, u8 Byte)
    {
        return VChunkEq(Chunk, VChunkSet(Byte));
    }

    INTERNAL VMask8
    VChunkRange(VChunk8 Chunk, u8 Lo, u8 Hi)
    {
        return VChunkToMask(__VChunkRangeV(Chunk, Lo, Hi));
    }

    INTERNAL VMask8
    VChunkTest(VChunk8 Chunk, u8 Bits)
    {
        VChunk8 Masked = _mm_and_si128(Chunk, VChunkSet(Bits));
        VChunk8 IsZero = _mm_cmpeq_epi8(Masked, VChunkZero());

        return (VMask8) (VChunkToMask(IsZero) ^ VMASK_ALL);
    }

    INTERNAL VMask8
    VChunkHighBits(VChunk8 Chunk)
    {
        return VChunkToMask(Chunk);
    }

    INTERNAL VChunk8
    VChunkFold(VChunk8 Chunk)
    {
        VChunk8 Upper = __VChunkRangeV(Chunk, 'A', 'Z');

        return _mm_or_si128(Chunk, _mm_and_si128(Upper, VChunkSet(0x20)));
    }

    INTERNAL VChunk8
    VChunkSlashFold(VChunk8 Chunk)
    {
        VChunk8 Back = _mm_cmpeq_epi8(Chunk, VChunkSet('\\'));

        return _mm_blendv_epi8(Chunk, VChunkSet('/'), Back);
    }

    INTERNAL VChunk8
    VChunkBlend(VChunk8 ChunkA, VChunk8 ChunkB, VMask8 TakeB)
    {
        return _mm_blendv_epi8(ChunkA, ChunkB, VChunkFromMask(TakeB));
    }

    INTERNAL VChunk8
    VChunkTable(u8 const Table[16])
    {
        return _mm_loadu_si128((v128 const*) Table);
    }

    INTERNAL VChunk8
    VChunkLookup(VChunk8 Chunk, u8 const Table[16])
    {
        VChunk8 Index = _mm_and_si128(Chunk, VChunkSet(0x0F));

        return _mm_shuffle_epi8(VChunkTable(Table), Index);
    }

    INTERNAL VChunk8
    VChunkLookupHi(VChunk8 Chunk, u8 const Table[16])
    {
        VChunk8 Index = _mm_and_si128(
            _mm_srli_epi16(Chunk, 4), 
            VChunkSet(0x0F)
        );

        return _mm_shuffle_epi8(VChunkTable(Table), Index);
    }

    INTERNAL VChunk8
    VChunkShiftRight(VChunk8 Prev, VChunk8 Chunk, u32 Bytes)
    {
        ASSERT(
            Bytes == 1 || 
            Bytes == 2 || 
            Bytes == 3
        );

        switch (Bytes) {
            default: {
                Result = _mm_alignr_epi8(Chunk, Lanes, 13);
            } break;

            case 1: {
                Result = _mm_alignr_epi8(Chunk, Lanes, 15);
            } break;

            case 2: {
                Result = _mm_alignr_epi8(Chunk, Lanes, 14);
            } break;
        }
    }
#endif

INTERNAL VChunk8
VChunkLoad(VStr8 String, u64 Offset)
{
    ASSERT(Offset < VSTR_PADDED_SIZE(String.Size));

    return VChunkLoad(String.Str + Offset);
}

INTERNAL VChunk8
VChunkLoad(Str8 String, u64 Offset)
{
    ASSERT(Offset < String.Size);

    return VChunkLoadTail(String.Str + Offset, String.Size - Offset);
}

INTERNAL VMask8
VChunkValid(VStr8 String, u64 Offset)
{
    return VMaskTail(String.Size - Offset);
}

INTERNAL VMask8
VChunkLT(VChunk8 Chunk, u8 Byte)
{
    ASSERT(Byte);

    return VChunkRange(Chunk, 0, Byte - 1);
}

INTERNAL VChunk8
__VChunkRaw(
    VChunk8 Chunk, 
    u8 const Lo[16], 
    u8 const Hi[16]
) {
    return VChunkAND(
        VChunkLookup(Chunk, Lo), 
        VChunkLookupHi(Chunk, Hi)
    );
}

INTERNAL VMask8
VChunkClass(VChunk8 Chunk, VClassTable const* Table, VClassKind Kind)
{
    VChunk8 Raw = __VChunkRaw(Chunk, Table->Lo, Table->Hi);

    return VChunkTest(Raw, Table->Combine[VClassIndex(Kind)]);
}

INTERNAL VClass
VChunkClassify(VChunk8 Chunk, VClassTable const* Table)
{
    VClass Result = {};
    VChunk8 Raw = __VChunkRaw(Chunk, Table->Lo, Table->Hi);

    for (u32 Index = 0; Index < 8; ++Index) {
        Result.M[Index] = Table->Combine[Index] 
            ? VChunkTest(Raw, Table->Combine[Index]) 
            : 0;
    }

    return Result;
}

INTERNAL VMask8
VChunkNibbleMatch(VChunk8 Chunk, VNibbleTable const* Table)
{
    return VChunkTest(
        __VChunkRaw(Chunk, Table->Lo, Table->Hi), 
        0xFF
    );
}

INTERNAL VChunk8
VChunkNibbleBuckets(VChunk8 Chunk, VNibbleTable const* Table)
{
    return __VChunkRaw(Chunk, Table->Lo, Table->Hi);
}

INTERNAL VMask8
VMaskEscaped(VMask8 Backslash, u64* PrevEscape)
{
    Backslash = (VMask8) (Backslash & ~(VMask8) *PrevEscape);

    VMask8 FollowsEscape = (VMask8) ((Backslash << 1) | (VMask8) *PrevEscape);
    VMask8 OddStarts = (VMask8) (Backslash & ~VMASK_EVEN & ~FollowsEscape);
    VMask8 EvenStarts = (VMask8) (OddStarts + Backslash);

    *PrevEscape = !!(EvenStarts < OddStarts);

    VMask8 Invert = (VMask8) (EvenStarts << 1);

    return (VMask8) ((VMASK_EVEN ^ Invert) & FollowsEscape);
}

INTERNAL VStructural
VStructuralScan(VChunk8 Chunk, VClassTable const* Table, VScanState* State)
{
    VStructural Result = {};
    VClass Class = VChunkClassify(Chunk, Table);

    Result.Escape = VMaskEscaped(Class.Escape, &State->PrevWasEscape);
    Result.Quote = (VMask8) (Class.Quote & ~Result.Escape);
    Result.InString = VMaskPrefixXOR(Result.Quote, &State->PrevWasInString);
    Result.Structural = (VMask8) (Class.Ident & ~Result.InString);
    Result.Space = (VMask8) (Class.Space & ~Result.InString);

    return Result;
}

INTERNAL VNeedleData*
VNeedleGet(VNeedle* Needle)
{
    return (VNeedleData*) Needle->Memory;
}

INTERNAL VNeedleData const*
VNeedleGet(VNeedle const* Needle)
{
    return (VNeedleData const*) Needle->Memory;
}

INTERNAL VNeedleSetData*
VNeedleSetGet(VNeedleSet* Set)
{
    return (VNeedleSetData*) Set->Memory;
}

INTERNAL VNeedleSetData const*
VNeedleSetGet(VNeedleSet const* Set)
{
    return (VNeedleSetData const*) Set->Memory;
}

INTERNAL VPatternData*
VPatternGet(VPattern* Pattern)
{
    return (VPatternData*) Pattern->Memory;
}

INTERNAL VPatternData const*
VPatternGet(VPattern const* Pattern)
{
    return (VPatternData const*) Pattern->Memory;
}

INTERNAL u64
VKeyPrefix(Str8 Key)
{
    u64 Result = 0;

    MemCpy(&Result, Key.Str, MIN(Key.Size, (u64) V_KEY_PREFIX_SIZE));

    return Result;
}

INTERNAL VChunk8
__VChunkPrep(VChunk8 Chunk, StrMatchKind Method)
{
    if (Method & STR_MATCH_ALL_CASES)
        Chunk = VChunkFold(Chunk);

    if (Method & STR_MATCH_SKIP_SLASH)
        Chunk = VChunkSlashFold(Chunk);

    return Chunk;
}

INTERNAL u8
__VBytePrep(u8 Byte, StrMatchKind Method)
{
    if ((Method & STR_MATCH_ALL_CASES) && IsUpper(Byte))
        Byte = ToLower(Byte);

    if ((Method & STR_MATCH_SKIP_SLASH) && Byte == '\\')
        Byte = '/';

    return Byte;
}

INTERNAL VChunk8
__VChunkUnfold(VChunk8 Chunk)
{
    VMask8 Lower = VChunkRange(Chunk, 'a', 'z');

    return VChunkBlend(
        Chunk, 
        VChunkSUB(Chunk, VChunkSet(0x20)), 
        Lower
    );
}

INTERNAL u8*
__VPushPadded(Arena* MemPool, u64 Size)
{
    u8* Result = ArenaPushArrayEx(
        MemPool, 
        u8, 
        VSTR_PADDED_SIZE(Size), 
        VCHUNK_WIDTH
    );

    MemSet(Result + Size, 0, VCHUNK_WIDTH);

    return Result;
}

INTERNAL u64
__VStrFindLastNotClass(VStr8 String, VClassTable const* Table, VClassKind Kind)
{
    u64 Result = String.Size;

    if (String.Size) {
        u64 Offset = (String.Size - 1) & ~((u64) VCHUNK_WIDTH - 1);

        for (;;) {
            VChunk8 Chunk = VChunkLoad(String, Offset);
            VMask8 Hits = (VMask8) (
                ~VChunkClass(Chunk, Table, Kind) & 
                VChunkValid(String, Offset)
            );

            if (Hits) {
                Result = Offset + VMaskLast(Hits);
                break;
            }

            if (!Offset)
                break;

            Offset -= VCHUNK_WIDTH;
        }
    }

    return Result;
}

INTERNAL u64
__VStrFindNthByte(VStr8 String, u8 Byte, u64 N)
{
    u64 Result = String.Size;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunk8 Chunk = VChunkLoad(String, Offset);
        VMask8 Hits = (VMask8) (
            VChunkEQ(Chunk, Byte) & 
            VChunkValid(String, Offset)
        );
        u32 Count = VMaskCount(Hits);

        if (N < Count) {
            while (N--)
                VMaskPop(&Hits);

            Result = Offset + VMaskFirst(Hits);
            break;
        }

        N -= Count;
    }

    return Result;
}

INTERNAL void
__VRangeListPush(Arena* MemPool, FMRangeList* List, r1u64 Range)
{
    FMRangeNode* Node = ArenaPushStruct(MemPool, FMRangeNode);

    Node->Range = Range;
    Node->Next = NULL;

    if (List->Tail)
        List->Tail->Next = Node;
    else
        List->Head = Node;

    List->Tail = Node;
    List->Count += 1;
    List->TotalDimensions += Length(Range);
}

#if NC_SIMD_LEVEL >= 8
    INTERNAL void
    __VChunkStoreWide16(u16* Dst, VChunk8 Chunk)
    {
        _mm512_storeu_si512(
            (void*) (Dst +  0), 
            _mm512_cvtepu8_epi16(
                _mm512_castsi512_si256(Chunk)
            )
        );
        _mm512_storeu_si512(
            (void*) (Dst + 32), 
            _mm512_cvtepu8_epi16(
                _mm512_extracti64x4_epi64(
                    Chunk, 
                    1
                )
            )
        );
    }
#elif NC_SIMD_LEVEL >= 6
    INTERNAL void
    __VChunkStoreWide16(u16* Dst, VChunk8 Chunk)
    {
        _mm256_storeu_si256(
            (v256*) (Dst +  0), 
            _mm256_cvtepu8_epi16(
                _mm256_castsi256_si128(Chunk)
            )
        );
        _mm256_storeu_si256(
            (v256*) (Dst + 16), 
            _mm256_cvtepu8_epi16(
                _mm256_extracti128_si256(
                    Chunk, 
                    1
                )
            )
        );
    }
#else
    INTERNAL void
    __VChunkStoreWide16(u16* Dst, VChunk8 Chunk)
    {
        _mm_storeu_si128(
            (v128*) (Dst + 0), 
            _mm_cvtepu8_epi16(Chunk)
        );
        _mm_storeu_si128(
            (v128*) (Dst + 8), 
            _mm_cvtepu8_epi16(
                _mm_srli_si128(
                    Chunk, 
                    8
                )
            )
        );
    }
#endif

API_INTERNAL VStr8 
VStr(Arena* MemPool, Str8 String)
{
    VStr8 Result = {};

    Result.Size = String.Size;
    Result.Str = __VPushPadded(MemPool, Result.Size);
    MemCpy(Result.Str, String.Str, Result.Size);

    return Result;
}

API_INTERNAL VStr8 
VStrUnsafe(Str8 String)
{
    VStr8 Result = {};

    Result.Size = String.Size;
    Result.Str = String.Str;

    return Result;
}

API_INTERNAL VStr8 
VStrUnsafe(u8* CString, u64 Size)
{
    VStr8 Result = {};

    Result.Size = Size;
    Result.Str = CString;

    return Result;
}

API_INTERNAL Str8 
Str(VStr8 String)
{
    Str8 Result = Str(String.Str, String.Size);

    return Result;
}

API_INTERNAL VStr8 
VStrSub(VStr8 String, r1u64 Range)
{
    u64 Min = MIN(Range.Min, String.Size);
    u64 Max = MIN(Range.Max, String.Size);

    if (Max < Min) 
        Max = Min;

    return VStrUnsafe(String.Str + Min, Max - Min);
}

API_INTERNAL VStr8 
VStrSkip(VStr8 String, u64 Min)
{
    return VStrSub(String, Rng(Min, String.Size));
}

API_INTERNAL VStr8 
VStrChop(VStr8 String, u64 NMax)
{
    VStr8 Result = VStrSub(
        String,
        Rng((u64) 0, String.Size - MIN(NMax, String.Size))
    );

    return Result;
}

API_INTERNAL VStr8 
ArenaPushVStrCpy(Arena* MemPool, Str8 String)
{
    return VStr(MemPool, String);
}

API_INTERNAL VStr8 
ArenaPushVStrCat(Arena* MemPool, Str8 StringA, Str8 StringB)
{
    VStr8 Result = {};

    Result.Size = StringA.Size + StringB.Size;
    Result.Str = __VPushPadded(MemPool, Result.Size);
    MemCpy(Result.Str, StringA.Str, StringA.Size);
    MemCpy(Result.Str + StringA.Size, StringB.Str, StringB.Size);

    return Result;
}

API_INTERNAL VStr8 
ArenaPushVStrFmt(Arena* MemPool, char* Fmt, ...)
{
    va_list Args;

    va_start(Args, Fmt);

    VStr8 Result = ArenaPushVStrFmtV(MemPool, Fmt, Args);

    va_end(Args);

    return Result;
}

API_INTERNAL VStr8 
ArenaPushVStrFmtV(Arena* MemPool, char const* Fmt, va_list Args)
{
    VStr8 Result = {};
    va_list ArgsCopy;

    va_copy(ArgsCopy, Args);
    Result.Size = (u64) VSCPrintf(Fmt, ArgsCopy);
    va_end(ArgsCopy);
    Result.Str = __VPushPadded(MemPool, Result.Size);
    VSNPrintf((char*) Result.Str, Result.Size + 1, Fmt, Args);

    return Result;
}

API_INTERNAL VStr8Array 
VStrArrayFromList(Arena* MemPool, Str8List* List)
{
    VStr8Array Result = {};

    Result.Data = ArenaPushArray(MemPool, VStr8, List->Count);

    for (Str8Node* Node = List->Head; Node; Node = Node->Next)
        Result.Data[Result.Count++] = VStr(MemPool, Node->String);

    return Result;
}

API_INTERNAL VStr8 
ReadVFileContents(Arena* MemPool, Str8 FilePath)
{
    VStr8 Result = {};
    SystemAccessKind Access = (SystemAccessKind) (
        SYS_ACCESS_READ | 
        SYS_ACCESS_SHARE_READ
    );
    Handle FileHandle = SystemOpenFile(Access, FilePath);

    if (FileHandle != EMPTY_HANDLE_VALUE) {
        FileProperties Props = SystemGetFileProperties(FileHandle);
        u64 PreviousOffset = ArenaGetPosition(MemPool);

        Result.Str = __VPushPadded(MemPool, Props.Size);
        Result.Size = SystemReadFile(
            FileHandle,
            Rng((u64) 0, Props.Size),
            Result.Str
        );

        if (Result.Size < Props.Size) {
            ArenaPopTo(MemPool, PreviousOffset);
            Result.Str = __VPushPadded(MemPool, Result.Size);
        }

        SystemCloseFile(FileHandle);
    }

    return Result;
}

API_INTERNAL VClassTable 
VClassTableAlloc(void)
{
    VClassTable Result = {};

    return Result;
}

INTERNAL b32
__VClassTablePushProduct(
    VClassTable* Table, 
    VClassKind Kind, 
    u16 LoSet, 
    u16 HiSet
) {
    u8 Used = 0;

    for (u32 Index = 0; Index < 8; ++Index)
        Used |= Table->Combine[Index];

    if (Used == U8_MAX)
        return FALSE;
 
    u8 Bit = (u8) (1 << Ctz64((u8) ~Used));

    for (u32 Nibble = 0; Nibble < 16; ++Nibble) {
        if (LoSet & (1 << Nibble))
            Table->Lo[Nibble] |= Bit;

        if (HiSet & (1 << Nibble))
            Table->Hi[Nibble] |= Bit;
    }

    Table->Combine[VClassIndex(Kind)] |= Bit;

    return TRUE;
}

API_INTERNAL b32 
TablePush(VClassTable* Table, VClassKind Kind, u8 Char)
{
    return __VClassTablePushProduct(
        Table,
        Kind,
        (u16) (1 << (Char & 0x0F)),
        (u16) (1 << (Char >> 4))
    );
}

API_INTERNAL b32 
TablePush(VClassTable* Table, VClassKind Kind, u8 Lo, u8 Hi)
{
    b32 Result = TRUE;
    u32 HiLo = Lo >> 4;
    u32 HiHi = Hi >> 4;

    if (Hi < Lo)
        return FALSE;

    if (HiLo == HiHi) {
        u16 LoSet = (u16) (
            ((1 << ((Hi & 0x0F) + 1)) - 1) &
            ~((1 << (Lo & 0x0F)) - 1)
        );

        Result = __VClassTablePushProduct(
            Table, 
            Kind, 
            LoSet, 
            (u16) (1 << HiLo)
        );
    } else {
        u16 FirstLo = (u16) (0xFFFF & ~((1 << (Lo & 0x0F)) - 1));
        u16 LastLo = (u16) ((1 << ((Hi & 0x0F) + 1)) - 1);

        Result = Result && __VClassTablePushProduct(
            Table, 
            Kind, 
            FirstLo, 
            (u16) (1 << HiLo)
        );
        Result = Result && __VClassTablePushProduct(
            Table, 
            Kind, 
            LastLo, 
            (u16) (1 << HiHi)
        );

        if (HiHi - HiLo > 1) {
            u16 MidHi = (u16) (((1 << HiHi) - 1) & ~((1 << (HiLo + 1)) - 1));

            Result = (
                Result && 
                __VClassTablePushProduct(Table, Kind, 0xFFFF, MidHi)
            );
        }
    }

    return Result;
}

API_INTERNAL b32 
TablePush(VClassTable* Table, VClassKind Kind, Str8 Chars)
{
    b32 Result = TRUE;
    u16 LoSets[16] = {};
    u16 HiUsed = 0;

    for (u64 Index = 0; Index < Chars.Size; ++Index) {
        u8 Char = Chars.Str[Index];

        LoSets[Char >> 4] |= (u16) (1 << (Char & 0x0F));
        HiUsed |= (u16) (1 << (Char >> 4));
    }

    for (u32 Nibble = 0; Nibble < 16 && Result; ++Nibble) {
        if (HiUsed & (1 << Nibble)) {
            Result = __VClassTablePushProduct(
                Table, 
                Kind, 
                LoSets[Nibble], 
                (u16) (1 << Nibble)
            );
        }
    }

    return Result;
}

INTERNAL VMask8
__VCursorHits(VCursor* Cursor)
{
    VChunk8 Chunk = VChunkLoad(Cursor->String, Cursor->Chunk);
    VMask8 Hits = Cursor->Table
        ? VChunkClass(Chunk, Cursor->Table, Cursor->Kind)
        : VChunkEQ(Chunk, Cursor->Byte);

    return (VMask8) (Hits & VChunkValid(Cursor->String, Cursor->Chunk));
}

API_INTERNAL VCursor 
VCursorBegin(VStr8 String, u8 Byte)
{
    VCursor Result = {};

    Result.String = String;
    Result.Byte = Byte;
    Result.Mask = String.Size 
        ? __VCursorHits(&Result) 
        : 0;

    return Result;
}

API_INTERNAL VCursor 
VCursorBegin(VStr8 String, VClassTable const* Table, VClassKind Kind)
{
    VCursor Result = {};

    Result.String = String;
    Result.Table = Table;
    Result.Kind = Kind;
    Result.Mask = String.Size 
        ? __VCursorHits(&Result) 
        : 0;

    return Result;
}

API_INTERNAL b32 
VCursorNext(VCursor* Cursor, OUT u64* Offset)
{
    while (!Cursor->Mask) {
        Cursor->Chunk += VCHUNK_WIDTH;

        if (Cursor->Chunk >= Cursor->String.Size)
            return FALSE;

        Cursor->Mask = __VCursorHits(Cursor);
    }

    VMask8 Mask = (VMask8) Cursor->Mask;

    *Offset = Cursor->Chunk + VMaskPop(&Mask);
    Cursor->Mask = Mask;

    return TRUE;
}

API_INTERNAL b32 
VCursorNextToken(VCursor* Cursor, OUT Str8* Token)
{
    return VCursorNextToken(Cursor, FALSE, Token);
}

API_INTERNAL b32 
VCursorNextToken(VCursor* Cursor, b32 KeepEmpties, OUT Str8* Token)
{
    for (;;) {
        if (Cursor->TokenOffset > Cursor->String.Size)
            return FALSE;

        u64 Offset = 0;
        b32 Found = VCursorNext(Cursor, &Offset);
        u64 End = Found 
            ? Offset 
            : Cursor->String.Size;
        Str8 Result = Str(
            Cursor->String.Str + Cursor->TokenOffset, 
            End - Cursor->TokenOffset
        );

        Cursor->TokenOffset = Found 
            ? Offset + 1 
            : Cursor->String.Size + 1;

        if (Result.Size || KeepEmpties) {
            *Token = Result;

            return TRUE;
        }
    }
}

API_INTERNAL Str8List 
StrSplit(
    Arena* MemPool, 
    VStr8 String, 
    u8 const* Splits, 
    u32 SplitsCount, 
    b32 KeepEmpties
) {
    Str8List Result = {};
    VClassTable Table = VClassTableAlloc();
    VCursor Cursor = {};
    Str8 Token = {};

    if (SplitsCount == 1) {
        Cursor = VCursorBegin(String, Splits[0]);
    } else if (
        TablePush(
            &Table, 
            V_CLASS_KIND_SPACE, 
            Str((u8*) Splits, SplitsCount)
        )
    ) {
        Cursor = VCursorBegin(String, &Table, V_CLASS_KIND_SPACE);
    } else {
        return StrSplit(
            MemPool, 
            Str(String), 
            Splits, 
            SplitsCount, 
            KeepEmpties
        );
    }

    while (VCursorNextToken(&Cursor, KeepEmpties, &Token))
        ListPush(MemPool, &Result, Token);

    return Result;
}

API_INTERNAL Str8List 
StrSplitLines(Arena* MemPool, VStr8 String)
{
    Str8List Result = {};
    VCursor Cursor = VCursorBegin(String, '\n');
    Str8 Line = {};

    while (VCursorNextToken(&Cursor, TRUE, &Line)) {
        if (Line.Size && Line.Str[Line.Size - 1] == '\r')
            Line.Size -= 1;

        ListPush(MemPool, &Result, Line);
    }

    return Result;
}

API_INTERNAL u64 
VStrFindByte(VStr8 String, u8 Byte, u64 StartOffset)
{
    for (
        u64 Offset = StartOffset; 
        Offset < String.Size; 
        Offset += VCHUNK_WIDTH
    ) {
        VMask8 Hits = (VMask8) (
            VChunkEQ(
                VChunkLoad(String, Offset), 
                Byte
            ) & VChunkValid(String, Offset)
        );

        if (Hits)
            return Offset + VMaskFirst(Hits);
    }

    return String.Size;
}

API_INTERNAL u64 
VStrFindLastByte(VStr8 String, u8 Byte)
{
    if (String.Size) {
        u64 Offset = (String.Size - 1) & ~((u64) VCHUNK_WIDTH - 1);

        for (;;) {
            VMask8 Hits = (VMask8) (
                VChunkEQ(
                    VChunkLoad(String, Offset), 
                    Byte
                ) & VChunkValid(String, Offset)
            );

            if (Hits)
                return Offset + VMaskLast(Hits);

            if (!Offset)
                break;

            Offset -= VCHUNK_WIDTH;
        }
    }

    return String.Size;
}

API_INTERNAL u64 
VStrFindClass(
    VStr8 String, 
    VClassTable const* Table, 
    VClassKind Kind, 
    u64 StartOffset
) {
    for (
        u64 Offset = StartOffset; 
        Offset < String.Size; 
        Offset += VCHUNK_WIDTH
    ) {
        VMask8 Hits = (VMask8) (
            VChunkClass(
                VChunkLoad(String, Offset), 
                Table, 
                Kind
            ) & VChunkValid(String, Offset)
        );

        if (Hits)
            return Offset + VMaskFirst(Hits);
    }

    return String.Size;
}

API_INTERNAL u64 
VStrFindNotClass(
    VStr8 String, 
    VClassTable const* Table, 
    VClassKind Kind, 
    u64 StartOffset
) {
    for (
        u64 Offset = StartOffset; 
        Offset < String.Size; 
        Offset += VCHUNK_WIDTH
    ) {
        VMask8 Hits = (VMask8) (
            ~VChunkClass(
                VChunkLoad(String, Offset), 
                Table, 
                Kind
            ) & VChunkValid(String, Offset)
        );

        if (Hits)
            return Offset + VMaskFirst(Hits);
    }

    return String.Size;
}

API_INTERNAL u64 
VStrFindLastClass(
    VStr8 String, 
    VClassTable const* Table, 
    VClassKind Kind, 
    u64 StartOffset
) {
    if (String.Size) {
        u64 Offset = (String.Size - 1) & ~((u64) VCHUNK_WIDTH - 1);

        for (;;) {
            VMask8 Hits = (VMask8) (
                VChunkClass(
                    VChunkLoad(String, Offset), 
                    Table, 
                    Kind
                ) & VChunkValid(String, Offset)
            );

            if (Hits)
                return Offset + VMaskLast(Hits);

            if (!Offset)
                break;

            Offset -= VCHUNK_WIDTH;
        }
    }

    return String.Size;
}

API_INTERNAL u64 
VStrCountByte(VStr8 String, u8 Byte)
{
    u64 Result = 0;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        Result += VMaskCount(
            (VMask8) (
                VChunkEQ(
                    VChunkLoad(String, Offset), 
                    Byte
                ) & VChunkValid(String, Offset)
            )
        );
    }

    return Result;
}

API_INTERNAL u64 
VStrCountClass(VStr8 String, VClassTable const* Table, VClassKind Kind)
{
    u64 Result = 0;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        Result += VMaskCount(
            (VMask8) (
                VChunkClass(
                    VChunkLoad(String, Offset), 
                    Table, 
                    Kind
                ) & VChunkValid(String, Offset)
            )
        );
    }

    return Result;
}

API_INTERNAL VNeedle 
VNeedleFromStr(Str8 Needle, StrMatchKind Kind)
{
    VNeedle Result = {};
    VNeedleData* Data = VNeedleGet(&Result);

    Data->Needle = Needle;
    Data->Method = Kind;

    if (Needle.Size) {
        Data->Head = __VBytePrep(Needle.Str[0], Kind);
        Data->Tail = __VBytePrep(Needle.Str[Needle.Size - 1], Kind);
        Data->TailOffset = SafeTruncateU64(Needle.Size - 1);
    }

    return Result;
}

API_INTERNAL u64 
VStrFind(VStr8 String, VNeedle const* Needle, u64 StartOffset)
{
    VNeedleData const* Data = VNeedleGet(Needle);
    StrMatchKind Verify = (StrMatchKind) (
        Data->Method & 
        (
            STR_MATCH_ALL_CASES | STR_MATCH_SKIP_SLASH
        )
    );
    u64 Result = String.Size;

    if (!Data->Needle.Size)
        return MIN(StartOffset, String.Size);

    if (
        String.Size < Data->Needle.Size || 
        StartOffset + Data->Needle.Size > String.Size
    ) {
        return String.Size;
    }

    for (
        u64 Offset = StartOffset; 
        Offset + Data->TailOffset < String.Size; 
        Offset += VCHUNK_WIDTH
    ) {
        VChunk8 First = __VChunkPrep(
            VChunkLoad(String, Offset), 
            Data->Method
        );
        VChunk8 Last = __VChunkPrep(
            VChunkLoad(String, Offset + Data->TailOffset), 
            Data->Method
        );
        VMask8 Valid = VMaskTail(
            String.Size - Offset - Data->TailOffset
        );
        VMask8 Candidates = (VMask8) (
            VChunkEQ(First, Data->Head) & 
            VChunkEQ(Last, Data->Tail) & 
            Valid
        );

        while (Candidates) {
            u64 Hit = Offset + VMaskPop(&Candidates);
            Str8 Window = Str(String.Str + Hit, Data->Needle.Size);

            if (StrMatch(Window, Data->Needle, Verify)) {
                Result = Hit;

                if (!(Data->Method & STR_MATCH_LAST))
                    return Result;
            }
        }
    }

    return Result;
}

API_INTERNAL u64 
StrFindSubStr(
    VStr8 String, 
    Str8 SubString, 
    u64 StartOffset, 
    StrMatchKind Kind
) {
    VNeedle Needle = VNeedleFromStr(SubString, Kind);

    return VStrFind(String, &Needle, StartOffset);
}

API_INTERNAL VNeedleSet 
VNeedleSetFromStrs(Str8 const* Needles, u32 Count, StrMatchKind Kind)
{
    VNeedleSet Result = {};
    VNeedleSetData* Data = VNeedleSetGet(&Result);

    ASSERT(Count && Count <= V_NEEDLE_SET_MAX);

    Data->Count = Count;
    Data->Method = Kind;
    Data->MinLength = (u32) -1;

    for (u32 Index = 0; Index < Count; ++Index) {
        Data->Needles[Index] = Needles[Index];
        Data->MinLength = SafeTruncateU64(
            MIN(
                (u64) Data->MinLength, 
                Needles[Index].Size
            )
        );
    }

    ASSERT(Data->MinLength);

    u32 FingerprintCount = MIN(Data->MinLength, 3U);

    for (u32 Position = 0; Position < FingerprintCount; ++Position) {
        for (u32 Index = 0; Index < Count; ++Index) {
            u8 Byte = __VBytePrep(Needles[Index].Str[Position], Kind);

            Data->Fingerprint[Position].Lo[Byte & 0x0F] |= (u8) (1 << Index);
            Data->Fingerprint[Position].Hi[Byte >> 4] |= (u8) (1 << Index);
        }
    }

    return Result;
}

VNeedleMatch 
VNeedleSetFind(VStr8 String, VNeedleSet const* Set, u64 StartOffset)
{
    VNeedleSetData const* Data = VNeedleSetGet(Set);
    StrMatchKind Verify = (StrMatchKind) (
        Data->Method & 
        (
            STR_MATCH_ALL_CASES | STR_MATCH_SKIP_SLASH
        )
    );
    u32 FingerprintCount = MIN(Data->MinLength, 3u);
    VNeedleMatch Result = {};

    Result.Offset = String.Size;
    Result.Needle = V_NEEDLE_SET_MAX;

    for (
        u64 Offset = StartOffset; 
        Offset + Data->MinLength <= String.Size; 
        Offset += VCHUNK_WIDTH
    ) {
        VChunk8 Buckets = VChunkNibbleBuckets(
            __VChunkPrep(
                VChunkLoad(String, Offset), 
                Data->Method
            ), 
            &Data->Fingerprint[0]
        );

        for (u32 Position = 1; Position < FingerprintCount; ++Position) {
            VChunk8 Shifted = __VChunkPrep(
                VChunkLoad(String, Offset + Position), 
                Data->Method
            );

            Buckets = VChunkAND(
                Buckets, 
                VChunkNibbleBuckets(Shifted, &Data->Fingerprint[Position])
            );
        }

        VMask8 Valid = VMaskTail(String.Size - Offset - Data->MinLength + 1);
        VMask8 Candidates = (VMask8) (VChunkTest(Buckets, U8_MAX) & Valid);

        while (Candidates) {
            u64 Hit = Offset + VMaskPop(&Candidates);
            u64 Remaining = String.Size - Hit;

            for (u32 Index = 0; Index < Data->Count; ++Index) {
                Str8 Needle = Data->Needles[Index];

                if (
                    Needle.Size <= Remaining && 
                    StrMatch(
                        Str(String.Str + Hit, Needle.Size), 
                        Needle, 
                        Verify
                    )
                ) {
                    Result.Offset = Hit;
                    Result.Needle = Index;

                    return Result;
                }
            }
        }
    }

    return Result;
}

API_INTERNAL u32 
VNeedleSetMatch(VNeedleSet const* Set, Str8 String)
{
    u32 Result = V_NEEDLE_SET_MAX;
    VNeedleSetData const* Data = VNeedleSetGet(Set);
    StrMatchKind Verify = (StrMatchKind) (
        Data->Method & 
        (
            STR_MATCH_ALL_CASES | STR_MATCH_SKIP_SLASH
        )
    );

    for (u32 Index = 0; Index < Data->Count; ++Index) {
        if (
            Data->Needles[Index].Size == String.Size && 
            StrMatch(String, Data->Needles[Index], Verify)
        ) {
            Result = Index;
            break;
        }
    }

    return Result;
}

INTERNAL b32
__VStrMatchPrefix(
    VStr8 StringA,
    u8 const* StringB,
    u64 Length,
    StrMatchKind Kind
) {
    b32 Result = TRUE;

    for (u64 Offset = 0; Offset < Length; Offset += VCHUNK_WIDTH) {
        VChunk8 ChunkA = __VChunkPrep(
            VChunkLoad(StringA, Offset), 
            Kind
        );
        VChunk8 ChunkB = __VChunkPrep(
            VChunkLoadTail(StringB + Offset, Length - Offset), 
            Kind
        );
        VMask8 Valid = VMaskTail(Length - Offset);

        if ((VChunkEQ(ChunkA, ChunkB) | ~Valid) != VMASK_ALL) {
            Result = FALSE;
            break;
        }
    }

    return Result;
}

API_INTERNAL b32 
StrMatch(VStr8 StringA, VStr8 StringB, StrMatchKind Kind)
{
    return StrMatch(StringA, Str(StringB), Kind);
}

API_INTERNAL b32 
StrMatch(VStr8 StringA, Str8 StringB, StrMatchKind Kind)
{    if (Kind & STR_MATCH_JUNK_AFTER) {
        if (StringA.Size < StringB.Size)
            return FALSE;
    } else if (StringA.Size != StringB.Size) {
        return FALSE;
    }

    return __VStrMatchPrefix(StringA, StringB.Str, StringB.Size, Kind);
}

INTERNAL i32
__VStrCmp(VStr8 StringA, VStr8 StringB, StrMatchKind Kind)
{
    u64 Length = MIN(StringA.Size, StringB.Size);

    for (u64 Offset = 0; Offset < Length; Offset += VCHUNK_WIDTH) {
        VChunk8 ChunkA = __VChunkPrep(VChunkLoad(StringA, Offset), Kind);
        VChunk8 ChunkB = __VChunkPrep(VChunkLoad(StringB, Offset), Kind);
        VMask8 Diff = (VMask8) (
            ~VChunkEQ(ChunkA, ChunkB) & 
            VMaskTail(Length - Offset)
        );

        if (Diff) {
            u64 Index = Offset + VMaskFirst(Diff);
            u8 ByteA = __VBytePrep(StringA.Str[Index], Kind);
            u8 ByteB = __VBytePrep(StringB.Str[Index], Kind);

            return (i32) ByteA - (i32) ByteB;
        }
    }

    return (StringA.Size < StringB.Size) 
        ? -1 
        : (StringA.Size > StringB.Size) 
            ? 1 
            : 0;
}

API_INTERNAL i32 
VStrCmp(VStr8 StringA, VStr8 StringB)
{
    return __VStrCmp(StringA, StringB, 0);
}

API_INTERNAL i32 
VStrCmpFold(VStr8 StringA, VStr8 StringB)
{
    return __VStrCmp(StringA, StringB, STR_MATCH_ALL_CASES);
}

API_INTERNAL u64 
VStrMatchPrefix(VStr8 StringA, VStr8 StringB)
{
    u64 Length = MIN(StringA.Size, StringB.Size);

    for (u64 Offset = 0; Offset < Length; Offset += VCHUNK_WIDTH) {
        VMask8 Diff = (VMask8) (
            ~VChunkEQ(
                VChunkLoad(StringA, Offset), 
                VChunkLoad(StringB, Offset)
            ) & VMaskTail(Length - Offset)
        );

        if (Diff)
            return Offset + VMaskFirst(Diff);
    }

    return Length;
}

API_INTERNAL u64 
VStrMatchSuffix(VStr8 StringA, VStr8 StringB)
{
    u64 Length = MIN(StringA.Size, StringB.Size);
    u64 Matched = 0;
    u8 const* EndA = StringA.Str + StringA.Size;
    u8 const* EndB = StringB.Str + StringB.Size;

    while (Matched + VCHUNK_WIDTH <= Length) {
        u64 Back = Matched + VCHUNK_WIDTH;
        VMask8 Diff = (VMask8) ~VChunkEQ(
            VChunkLoad(EndA - Back), 
            VChunkLoad(EndB - Back)
        );

        if (Diff)
            return Matched + (VCHUNK_WIDTH - 1 - VMaskLast(Diff));

        Matched = Back;
    }

    while (
        Matched < Length && 
        EndA[-(i64) Matched - 1] == EndB[-(i64) Matched - 1]
    ) {
        ++Matched;
    }

    return Matched;
}

API_INTERNAL b32 
PathMatchNormalised(VStr8 PathA, VStr8 PathB)
{
    return StrMatch(PathA, PathB, PathMatchKindFromSystem());
}

API_INTERNAL u64 
VStrFindInArray(VStr8Array Strings, Str8 String)
{
    u64 Prefix = VKeyPrefix(String);

    for (u64 Index = 0; Index < Strings.Count; ++Index) {
        VStr8 Candidate = Strings.Data[Index];

        if (
            Candidate.Size == String.Size && 
            VKeyPrefix(Str(Candidate)) == Prefix && 
            StrMatch(Candidate, String, 0)
        ) {
            return Index;
        }
    }

    return Strings.Count;
}

API_INTERNAL VPattern 
VPatternFromStr(Str8 Pattern, StrMatchKind Kind)
{
    VPattern Result = {};
    VPatternData* Data = VPatternGet(&Result);
    u32 Length = SafeTruncateU64(
        MIN(
            Pattern.Size, 
            (u64) V_PATTERN_MAX
        )
    );

    Data->Length = Length;
    Data->Method = Kind;
    Data->Accept = Length 
        ? ((u64) 1 << (Length - 1)) 
        : 0;

    for (u32 Index = 0; Index < Length; ++Index) {
        u8 Byte = Pattern.Str[Index];
        u64 Bit = (u64) 1 << Index;

        Data->Occurrence[Byte] |= Bit;

        if (Kind & STR_MATCH_ALL_CASES) {
            Data->Occurrence[ToLower(Byte)] |= Bit;
            Data->Occurrence[ToUpper(Byte)] |= Bit;
        }

        if (Kind & STR_MATCH_SKIP_SLASH && IsSlash(Byte)) {
            Data->Occurrence['/'] |= Bit;
            Data->Occurrence['\\'] |= Bit;
        }
    }

    return Result;
}

API_INTERNAL u64 
VPatternFind(VStr8 String, VPattern const* Pattern, u64 StartOffset)
{
    VPatternData const* Data = VPatternGet(Pattern);
    u64 State = 0;

    if (!Data->Length)
        return MIN(StartOffset, String.Size);

    for (u64 Offset = StartOffset; Offset < String.Size; ++Offset) {
        State = ((State << 1) | 1) & Data->Occurrence[String.Str[Offset]];

        if (State & Data->Accept)
            return Offset + 1 - Data->Length;
    }

    return String.Size;
}

API_INTERNAL u64 
VPatternFindApprox(
    VStr8 String, 
    VPattern const* Pattern, 
    u32 MaxErrors, 
    u64 StartOffset
) {
    VPatternData const* Data = VPatternGet(Pattern);
    u64 State[V_PATTERN_MAX_ERRORS + 1] = {};
    u32 Errors = MIN(MaxErrors, (u32) V_PATTERN_MAX_ERRORS);

    if (!Data->Length)
        return MIN(StartOffset, String.Size);

    if (Errors >= Data->Length)
        Errors = Data->Length - 1;

    for (u32 Level = 0; Level <= Errors; ++Level)
        State[Level] = ((u64) 1 << Level) - 1;

    for (u64 Offset = StartOffset; Offset < String.Size; ++Offset) {
        u64 Occurrence = Data->Occurrence[String.Str[Offset]];
        u64 PrevOld = State[0];
        u64 PrevNew = ((State[0] << 1) | 1) & Occurrence;

        State[0] = PrevNew;

        for (u32 Level = 1; Level <= Errors; ++Level) {
            u64 Old = State[Level];
            u64 New = (
                (((Old << 1) | 1) & Occurrence) | 
                PrevOld | 
                (PrevOld << 1) | 
                (PrevNew << 1)
            );

            State[Level] = New;
            PrevOld = Old;
            PrevNew = New;
        }

        if (State[Errors] & Data->Accept) {
            return (Offset + 1 >= Data->Length) 
                ? (Offset + 1 - Data->Length) 
                : 0;
        }
    }

    return String.Size;
}

API_INTERNAL u32 
VEditDistance(VPattern const* Pattern, Str8 String)
{
    VPatternData const* Data = VPatternGet(Pattern);
    u32 Length = Data->Length;
    u64 Pv = ~(u64) 0;
    u64 Mv = 0;
    u32 Score = Length;
    u64 Top = Length ? ((u64) 1 << (Length - 1)) : 0;

    if (!Length)
        return (u32) String.Size;

    for (u64 Offset = 0; Offset < String.Size; ++Offset) {
        u64 Eq = Data->Occurrence[String.Str[Offset]];
        u64 Xv = Eq | Mv;
        u64 Xh = (((Eq & Pv) + Pv) ^ Pv) | Eq;
        u64 Ph = Mv | ~(Xh | Pv);
        u64 Mh = Pv & Xh;

        if (Ph & Top)
            ++Score;
        else if (Mh & Top)
            --Score;

        Ph = (Ph << 1) | 1;
        Mh = Mh << 1;
        Pv = Mh | ~(Xv | Ph);
        Mv = Ph & Xv;
    }

    return Score;
}

API_INTERNAL u32 
VEditDistance(Str8 StringA, Str8 StringB)
{
    Str8 Short = (StringA.Size <= StringB.Size) 
        ? StringA 
        : StringB;
    Str8 Long = (StringA.Size <= StringB.Size) 
        ? StringB 
        : StringA;

    if (Short.Size > V_PATTERN_MAX)
        return (u32) -1;

    VPattern Pattern = VPatternFromStr(Short, 0);

    return VEditDistance(&Pattern, Long);
}

API_INTERNAL FMRangeList 
FuzzyFind(Arena* MemPool, Str8 Needle, VStr8 String)
{
    FMRangeList Result = {};
    TempArena Scratch = GetScratch(&MemPool, 1);
    u8 Space = ' ';
    Str8List Parts = StrSplit(Scratch.MemPool, Needle, &Space, 1, FALSE);

    Result.StrPartCount = Parts.Count;

    for (Str8Node* Node = Parts.Head; Node; Node = Node->Next) {
        VNeedle Part = VNeedleFromStr(Node->String, STR_MATCH_ALL_CASES);
        u64 Offset = 0;

        while ((Offset = VStrFind(String, &Part, Offset)) < String.Size) {
            __VRangeListPush(
                MemPool, 
                &Result, 
                Rng(Offset, Offset + Node->String.Size)
            );
            Offset += Node->String.Size;
        }
    }

    ReleaseScratch(Scratch);

    return Result;
}

API_INTERNAL i32 
VFuzzyScore(Str8 Needle, Str8 Candidate, VFuzzyKind Kind)
{
    i32 Score = 0;
    u64 NeedleOffset = 0;
    i64 LastMatch = -1;
    b32 Fold = (Kind & V_FUZZY_ALL_CASES) != 0;

    if (!Needle.Size)
        return 0;

    for (
        u64 Offset = 0; 
        Offset < Candidate.Size && NeedleOffset < Needle.Size; 
        ++Offset
    ) {
        u8 CandidateByte = Candidate.Str[Offset];
        u8 NeedleByte = Needle.Str[NeedleOffset];

        if (Fold) {
            CandidateByte = ToLower(CandidateByte);
            NeedleByte = ToLower(NeedleByte);
        }

        if (CandidateByte != NeedleByte) {
            if (LastMatch >= 0)
                Score -= 1;

            continue;
        }

        Score += 16;

        if (!Offset && (Kind & V_FUZZY_PREFIX_BONUS))
            Score += 32;

        if (
            LastMatch >= 0 && 
            (i64) Offset == LastMatch + 1 && 
            (Kind & V_FUZZY_CONTIGUOUS)
        ) {
            Score += 24;
        }

        if (Offset > 0 && (Kind & V_FUZZY_BOUNDARY_BONUS)) {
            u8 Prev = Candidate.Str[Offset - 1];
            b32 Boundary = (
                IsSpace(Prev) || 
                Prev == '_' || 
                Prev == '-' || 
                IsSlash(Prev) || 
                Prev == '.' || 
                (
                    IsLower(Prev) && 
                    IsUpper(Candidate.Str[Offset])
                )
            );

            if (Boundary)
                Score += 16;
        }

        LastMatch = (i64) Offset;
        ++NeedleOffset;
    }

    if (NeedleOffset < Needle.Size)
        return -1;

    Score -= (i32) MIN(
        Candidate.Size - Needle.Size, 
        (u64) 64
    );

    return Score;
}

API_INTERNAL void 
VFuzzyScore(
    Str8 Needle, 
    Str8 const* Candidates, 
    u64 Count, 
    VFuzzyKind Kind, 
    OUT i32* Scores
) {
    for (u64 Index = 0; Index < Count; ++Index)
        Scores[Index] = VFuzzyScore(Needle, Candidates[Index], Kind);
}

API_INTERNAL u64 
VStrFindEndOfStr(VStr8 String, u64 StartOffset)
{
    VScanState State = {};

    State.PrevWasInString = ~0ull;

    for (
        u64 Offset = StartOffset; 
        Offset < String.Size; 
        Offset += VCHUNK_WIDTH
    ) {
        VStructural Scan = VStructuralScan(
            VChunkLoad(String, Offset), 
            &V_CLASS_TABLE_JSON, 
            &State
        );
        VMask8 Hits = (VMask8) (Scan.Quote & VChunkValid(String, Offset));

        if (Hits)
            return Offset + VMaskFirst(Hits);
    }

    return String.Size;
}

API_INTERNAL u64 
VStrSkipSpace(VStr8 String, u64 StartOffset)
{

    return VStrFindNotClass(
        String, 
        &V_CLASS_TABLE_ASCII, 
        V_CLASS_KIND_SPACE, 
        StartOffset
    );
}

API_INTERNAL b32 
VStrNeedsEscape(VStr8 String)
{
    b32 Result = FALSE;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunk8 Chunk = VChunkLoad(String, Offset);
        VMask8 Hits = (VMask8) (
            VChunkClass(
                Chunk, 
                &V_CLASS_TABLE_JSON, 
                V_CLASS_KIND_QUOTE
            ) | 
            VChunkClass(Chunk, &V_CLASS_TABLE_JSON, V_CLASS_KIND_ESCAPE) | 
            VChunkLT(Chunk, 0x20)
        );

        if (Hits & VChunkValid(String, Offset)) {
            Result = TRUE;
            break;
        }
    }

    return Result;
}

INTERNAL u64
__VUTF8TailIncomplete(VStr8 String)
{
    for (u64 Back = 1; Back <= 3 && Back <= String.Size; ++Back) {
        u8 Byte = String.Str[String.Size - Back];
        u64 Needed = (Byte >= 0xF0) 
            ? 4 
            : (Byte >= 0xE0) 
                ? 3 
                : (Byte >= 0xC0) 
                    ? 2 
                    : 1;

        if (Needed > Back)
            return String.Size - Back;

        if (Byte < 0x80 || Byte >= 0xC0)
            break;
    }

    return String.Size;
}

API_INTERNAL VUTF8Info 
VUTF8Inspect(VStr8 String)
{
    VUTF8Info Result = {};
    VChunk8 Prev = VChunkZero();
    u64 Continuations = 0;

    Result.IsValid = TRUE;
    Result.IsASCII = TRUE;
    Result.ErrorOffset = String.Size;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunk8 Chunk = VChunkLoad(String, Offset);
        VMask8 Valid = VChunkValid(String, Offset);
        VMask8 High = (VMask8) (VChunkHiBits(Chunk) & Valid);

        if (High) {
            Result.IsASCII = FALSE;
            Continuations += VMaskCount(
                (VMask8) (VChunkRange(Chunk, 0x80, 0xBF) & Valid)
            );
        }

        if (High || VChunkHiBits(Prev)) {
            VChunk8 Prev1 = VChunkShiftRight(Prev, Chunk, 1);
            VChunk8 Prev2 = VChunkShiftRight(Prev, Chunk, 2);
            VChunk8 Prev3 = VChunkShiftRight(Prev, Chunk, 3);
            VChunk8 Special = VChunkAND(
                VChunkAND(
                    VChunkLookupHi(Prev1, V_UTF8_BYTE1_HIGH), 
                    VChunkLookup(Prev1, V_UTF8_BYTE1_LOW)
                ),
                VChunkLookupHi(Chunk, V_UTF8_BYTE2_HIGH)
            );
            VChunk8 Lead3 = VChunkSUBSAT(Prev2, VChunkSet(0xDF));
            VChunk8 Lead4 = VChunkSUBSAT(Prev3, VChunkSet(0xEF));
            VMask8 MustContinue = VChunkTest(VChunkOR(Lead3, Lead4), 0xFF);
            VMask8 Errors = (VMask8) (
                (
                    VChunkTest(Special, 0x7F) | 
                    (
                        VChunkTest(Special, 0x80) ^ 
                        MustContinue
                    )
                ) & Valid
            );

            if (Errors) {
                Result.IsValid = FALSE;
                Result.ErrorOffset = Offset + VMaskFirst(Errors);
                break;
            }
        }

        Prev = Chunk;
    }

    if (Result.IsValid) {
        u64 Incomplete = __VUTF8TailIncomplete(String);

        if (Incomplete < String.Size) {
            Result.IsValid = FALSE;
            Result.ErrorOffset = Incomplete;
        }
    }

    Result.CodePoints = String.Size - Continuations;

    return Result;
}

API_INTERNAL b32 
VUTF8Validate(VStr8 String)
{

    return VUTF8Inspect(String).IsValid;
}

API_INTERNAL b32 
VIsASCII(VStr8 String)
{
    b32 Result = TRUE;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        if (
            VChunkHiBits(VChunkLoad(String, Offset)) & 
            VChunkValid(String, Offset)
        ) {
            Result = FALSE;
            break;
        }
    }

    return Result;
}

API_INTERNAL u64 
VCodePointCount(VStr8 String)
{
    u64 Continuations = 0;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        Continuations += VMaskCount(
            (VMask8) (
                VChunkRange(
                    VChunkLoad(String, Offset), 
                    0x80, 
                    0xBF
                ) &
                VChunkValid(String, Offset)
            )
        );
    }

    return String.Size - Continuations;
}

API_INTERNAL u64 
VUTF8SnapToLead(VStr8 String, u64 Offset)
{
    if (Offset >= String.Size)
        return String.Size;

    while (Offset && (String.Str[Offset] & 0xC0) == 0x80)
        --Offset;

    return Offset;
}

API_INTERNAL u64 
VUTF8NextLead(VStr8 String, u64 Offset)
{
    if (Offset >= String.Size)
        return String.Size;

    ++Offset;

    while (Offset < String.Size && (String.Str[Offset] & 0xC0) == 0x80)
        ++Offset;

    return Offset;
}

API_INTERNAL u64 
VUTF8PrevLead(VStr8 String, u64 Offset)
{
    u64 Result = (
        !!Offset * 
        VUTF8SnapToLead(String, MIN(Offset, String.Size) - 1)
    );

    return Result;
}

API_INTERNAL u64 
VUTF8OffsetFromCodePoint(VStr8 String, u64 CodePointIndex)
{
    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VMask8 Leads = (VMask8) (
            ~VChunkRange(
                VChunkLoad(String, Offset), 
                0x80, 
                0xBF
            ) &
            VChunkValid(String, Offset)
        );
        u32 Count = VMaskCount(Leads);

        if (CodePointIndex < Count) {
            while (CodePointIndex--)
                VMaskPop(&Leads);

            return Offset + VMaskFirst(Leads);
        }

        CodePointIndex -= Count;
    }

    return String.Size;
}

API_INTERNAL Str16 
Str16FromStr8(Arena* MemPool, VStr8 String)
{
    Str16 Result = {};

    if (!VIsASCII(String))
        return Str16FromStr8(MemPool, Str(String));

    Result.Size = String.Size;
    Result.Str = ArenaPushArray(MemPool, u16, String.Size + VCHUNK_WIDTH);

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH)
        __VChunkStoreWide16(Result.Str + Offset, VChunkLoad(String, Offset));

    Result.Str[Result.Size] = 0;

    return Result;
}

API_INTERNAL Str32 
Str32FromStr8(Arena* MemPool, VStr8 String)
{

    return Str32FromStr8(MemPool, Str(String));
}

API_INTERNAL VStr8 
ToLower(Arena* MemPool, VStr8 String)
{
    VStr8 Result = {};

    Result.Size = String.Size;
    Result.Str = __VPushPadded(MemPool, String.Size);

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunkStore(
            Result.Str + Offset, 
            VChunkFold(VChunkLoad(String, Offset))
        );
    }

    return Result;
}

API_INTERNAL VStr8 
ToUpper(Arena* MemPool, VStr8 String)
{
    VStr8 Result = {};

    Result.Size = String.Size;
    Result.Str = __VPushPadded(MemPool, String.Size);

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunkStore(
            Result.Str + Offset, 
            __VChunkUnfold(VChunkLoad(String, Offset))
        );
    }

    return Result;
}

#if defined(NC_CPU_AESNI)
    INTERNAL u128
    __VHashAES(u64 Seed, VStr8 String)
    {
        const v128 Key0 = _mm_set_epi64x(
            (i64) 0x9E3779B97F4A7C15ull, 
            (i64) 0xBF58476D1CE4E5B9ull
        );
        const v128 Key1 = _mm_set_epi64x(
            (i64) 0x94D049BB133111EBull, 
            (i64) 0x2545F4914F6CDD1Dull
        );
        v128 State = _mm_set_epi64x((i64) Seed, (i64) String.Size);
        u64 Offset = 0;

        for (; Offset + 16 <= String.Size; Offset += 16) {
            v128 Block = _mm_loadu_si128((v128 const*) (String.Str + Offset));

            State = _mm_aesenc_si128(_mm_xor_si128(State, Block), Key0);
        }

        if (Offset < String.Size) {
            v128 Block = _mm_loadu_si128((v128 const*) (String.Str + Offset));
            v128 Keep = _mm_loadu_si128(
                (v128 const*) (V_TAIL_MASK_BYTES + 16 - (String.Size - Offset))
            );

            State = _mm_aesenc_si128(
                _mm_xor_si128(State, _mm_and_si128(Block, Keep)), 
                Key0
            );
        }

        State = _mm_aesenc_si128(State, Key1);
        State = _mm_aesenc_si128(State, Key0);
        State = _mm_aesenc_si128(State, Key1);

        u128 Result = {};

        Result.U64[0] = _mm_cvtsi128_si64(State);
        Result.U64[1] = _mm_extract_epi64(State, 1);

        return Result;
    }
#endif

API_INTERNAL u64 
Hash(VStr8 String)
{
    return Hash(0, String);
}

API_INTERNAL u64 
Hash(u64 Seed, VStr8 String)
{
#if defined(NC_CPU_AESNI)
    return __VHashAES(Seed, String).V[0];
#else
    return Hash(Seed, Str(String));
#endif
}

API_INTERNAL u128 
BigHash(VStr8 String)
{
    return BigHash(0, String);
}

API_INTERNAL u128 
BigHash(u64 Seed, VStr8 String)
{
#if defined(NC_CPU_AESNI)
    return __VHashAES(Seed, String);
#else
    return BigHash(Seed, Str(String));
#endif
}

API_INTERNAL u32 
VCRC32C(u32 Seed, VStr8 String)
{
    u32 Crc = ~Seed;
    u64 Offset = 0;

#if defined(NC_CPU_SSE4_2)
    u64 Wide = (u64) Crc;

    for (; Offset + 8 <= String.Size; Offset += 8) {
        Wide = _mm_crc32_u64(
            Wide, 
            *(u64 const*) (String.Str + Offset)
        );
    }

    Crc = (u32) Wide;

    for (; Offset < String.Size; ++Offset)
        Crc = _mm_crc32_u8(Crc, String.Str[Offset]);
#else
    for (; Offset < String.Size; ++Offset) {
        Crc ^= String.Str[Offset];

        for (u32 Bit = 0; Bit < 8; ++Bit)
            Crc = (Crc >> 1) ^ (0x82F63B78U & (u32) -(i32) (Crc & 1));
    }
#endif

    return ~Crc;
}

INTERNAL VDigits
__VDigitsBase10(VStr8 String)
{
    VDigits Result = {};
    VChunk8 Chunk = VChunkLoad(String, 0);
    VMask8 Digits = (VMask8) (
        VChunkRange(Chunk, '0', '9') & 
        VChunkValid(String, 0)
    );
    u64 Count = VMaskFirst((VMask8) ~Digits);

    if (Count == VCHUNK_WIDTH) {
        while (Count < String.Size && IsDigit(String.Str[Count]))
            ++Count;
    }

    Result.Count = (u32) Count;

    if (!Count)
        return Result;

    u32 Head = SafeTruncateU64(
        MIN(
            Count, 
            (u64) 16
        )
    );
    v128 Raw = _mm_sub_epi8(
        _mm_loadu_si128((v128 const*) String.Str), 
        _mm_set1_epi8('0')
    );
v128 Aligned = _mm_shuffle_epi8(
        Raw, 
        _mm_loadu_si128((v128 const*) (V_DIGIT_ALIGN_SHUFFLE + Head))
    );
    v128 Pairs = _mm_maddubs_epi16(
        Aligned, 
        _mm_loadu_si128((v128 const*) V_DIGIT_WEIGHTS_1_10)
    );
    v128 Quads = _mm_madd_epi16(
        Pairs, 
        _mm_loadu_si128((v128 const*) V_DIGIT_WEIGHTS_1_100)
    );
    v128 Packed = _mm_packus_epi32(Quads, Quads);
    v128 Octets = _mm_madd_epi16(
        Packed, 
        _mm_loadu_si128((v128 const*) V_DIGIT_WEIGHTS_1_10000)
    );
    u64 Hi = (u32) _mm_cvtsi128_si32(Octets);
    u64 Lo = (u32) _mm_extract_epi32(Octets, 1);

    Result.Value = Hi * 100000000ULL + Lo;

    for (u64 Index = 16; Index < Count; ++Index) {
        u64 Digit = String.Str[Index] - '0';

        if (Result.Value > ((u64) -1 - Digit) / 10) {
            Result.HasOverflow = TRUE;
            Result.Value = 0;
            break;
        }

        Result.Value = Result.Value * 10 + Digit;
    }

    return Result;
}

API_INTERNAL VDigits 
VDigitsFromStr(VStr8 String, u32 Base)
{
    VDigits Result = {};

    if (Base == 10)
        return __VDigitsBase10(String);

    if (Base == 16) {
        VChunk8 Chunk = VChunkLoad(String, 0);
        VMask8 Hex = (VMask8) (
            VChunkClass(
                Chunk, 
                &V_CLASS_TABLE_ASCII, 
                V_CLASS_KIND_HEX
            ) &
            VChunkValid(String, 0)
        );
        u64 Count = VMaskFirst((VMask8) ~Hex);

        if (Count == VCHUNK_WIDTH) {
            while (Count < String.Size && IsDigit(String.Str[Count], 16))
                ++Count;
        }

        Result.Count = (u32) Count;
        Result.HasOverflow = Count > 16;

        for (u64 Index = 0; Index < MIN(Count, (u64) 16); ++Index) {
            u8 Byte = String.Str[Index];

            Result.Value = (
                (Result.Value << 4) | 
                (u8) (Byte + V_HEX_DECODE_HI[Byte >> 4])
            );
        }

        return Result;
    }

    for (
        u64 Index = 0; 
        Index < String.Size && IsDigit(String.Str[Index], Base); 
        ++Index
    ) {
        u8 Byte = String.Str[Index];
        u64 Digit = IsAlpha(Byte) 
            ? (u64) (ToLower(Byte) - 'a' + 10) 
            : (u64) (Byte - '0');

        if (Result.Value > ((u64) -1 - Digit) / Base)
            Result.HasOverflow = TRUE;

        Result.Value = Result.Value * Base + Digit;
        Result.Count += 1;
    }

    return Result;
}

API_INTERNAL u64 
U64FromStr(VStr8 String, u32 Base)
{
    return VDigitsFromStr(String, Base).Value;
}

API_INTERNAL i64 
I64FromStr(VStr8 String, u32 Base)
{
    Str8 Tail = {};
    i8 Sign = SignFromStr(Str(String), &Tail);
    u64 Magnitude = VDigitsFromStr(VStrUnsafe(Tail), Base).Value;

    return (i64) Magnitude * (i64) Sign;
}

API_INTERNAL f64 
F64FromStr(VStr8 String)
{

    return F64FromStr(Str(String));
}

API_INTERNAL b32 
StrIsInteger(VStr8 String, u32 Base)
{
    Str8 Tail = {};

    SignFromStr(Str(String), &Tail);

    if (!Tail.Size)
        return FALSE;

    return VDigitsFromStr(VStrUnsafe(Tail), Base).Count == Tail.Size;
}

API_INTERNAL u32 
VStrFromU64(u8* Dst, u64 Value)
{
    u8 Scratch[20];
    u32 Count = 0;

    do {
        Scratch[Count++] = (u8) ('0' + Value % 10);
        Value /= 10;
    } while (Value);

    for (u32 Index = 0; Index < Count; ++Index)
        Dst[Index] = Scratch[Count - 1 - Index];

    return Count;
}

API_INTERNAL u32 
VStrFromI64(u8* Dst, i64 Value)
{
    u32 Count = 0;
    u64 Magnitude = (Value < 0) 
        ? (u64) (-(Value + 1)) + 1 
        : (u64) Value;

    if (Value < 0)
        Dst[Count++] = '-';

    return Count + VStrFromU64(Dst + Count, Magnitude);
}

API_INTERNAL i32 
VStrFromU64Hex(u8* Dst, u64 Value, b32 MakeUpper)
{
    u8 const* Table = MakeUpper ? 
        V_HEX_ENCODE_UPPER : 
        V_HEX_ENCODE_LOWER;
    u32 Count = Value 
        ? (16 - (Clz64(Value) >> 2)) 
        : 1;

    for (u32 Index = 0; Index < Count; ++Index)
        Dst[Index] = Table[(Value >> (4 * (Count - 1 - Index))) & 0x0F];

    return Count;
}

API_INTERNAL u64 
VLineCount(VStr8 String)
{
    u64 Result = !!String.Size * (VStrCountByte(String, '\n') + 1);

    return Result;
}

API_INTERNAL TextPoint 
VTextPointFromOffset(VStr8 String, u64 EndOffset)
{
    TextPoint Result = TxtPt(1, 1);
    u64 LineStart = 0;

    EndOffset = MIN(EndOffset, String.Size);

    for (u64 Offset = 0; Offset < EndOffset; Offset += VCHUNK_WIDTH) {
        VMask8 Newlines = (VMask8) (
            VChunkEQ(
                VChunkLoad(String, Offset), 
                '\n'
            ) &
            VMaskTail(EndOffset - Offset)
        );

        if (Newlines) {
            Result.Line += VMaskCount(Newlines);
            LineStart = Offset + VMaskLast(Newlines) + 1;
        }
    }

    Result.Column = (i64) (EndOffset - LineStart) + 1;

    return Result;
}

API_INTERNAL u64 
VOffsetFromTextPoint(VStr8 String, TextPoint Point)
{
    r1u64 LineRange = VLineRange(String, Point.Line);
    u64 Column = (Point.Column > 0) * (Point.Column - 1);

    return MIN(LineRange.Min + Column, LineRange.Max);
}

API_INTERNAL r1u64 
VLineRange(VStr8 String, i64 Line)
{
    u64 Start = 0;

    if (Line > 1) {
        u64 PrevNewline = __VStrFindNthByte(String, '\n', (u64) (Line - 2));

        if (PrevNewline >= String.Size)
            return Rng(String.Size, String.Size);

        Start = PrevNewline + 1;
    }

    return Rng(Start, VStrFindByte(String, '\n', Start));
}

API_INTERNAL VStr8 
VStrLine(VStr8 String, i64 Line)
{
    return VStrSub(String, VLineRange(String, Line));
}

API_INTERNAL Str8TextPointPair 
Str8TextPointPairFromStr(VStr8 String)
{
    return Str8TextPointPairFromStr(Str(String));
}

API_INTERNAL VStr8 
StrSkipChopWhitespace(VStr8 String)
{
    u64 First = VStrFindNotClass(
        String, 
        &V_CLASS_TABLE_ASCII, 
        V_CLASS_KIND_SPACE, 
        0
    );

    if (First >= String.Size)
        return VStrUnsafe(String.Str, 0);

    u64 Last = __VStrFindLastNotClass(
        String, 
        &V_CLASS_TABLE_ASCII, 
        V_CLASS_KIND_SPACE
    );

    return VStrSub(String, Rng(First, Last + 1));
}

API_INTERNAL VStr8 
StrChopLastSlash(VStr8 String)
{
    u64 Slash = VStrFindLastClass(
        String, 
        &V_CLASS_TABLE_PATH, 
        V_CLASS_KIND_SLASH, 
        0
    );

    return (Slash < String.Size) 
        ? VStrSub(String, Rng((u64) 0, Slash)) 
        : String;
}

API_INTERNAL VStr8 
StrSkipLastSlash(VStr8 String)
{
    u64 Slash = VStrFindLastClass(
        String, 
        &V_CLASS_TABLE_PATH, 
        V_CLASS_KIND_SLASH, 
        0
    );

    return (Slash < String.Size) 
        ? VStrSkip(String, Slash + 1) 
        : String;
}

API_INTERNAL VStr8 
StrChopLastDot(VStr8 String)
{
    u64 Dot = VStrFindLastByte(String, '.');

    return (Dot < String.Size) 
        ? VStrSub(String, Rng((u64) 0, Dot)) 
        : String;
}

API_INTERNAL VStr8 
StrSkipLastDot(VStr8 String)
{
    u64 Dot = VStrFindLastByte(String, '.');

    return (Dot < String.Size) 
        ? VStrSkip(String, Dot + 1) 
        : String;
}

API_INTERNAL VStr8 
StrTrimLastSlash(VStr8 String)
{
    if (String.Size && IsSlash(String.Str[String.Size - 1]))
        String.Size -= 1;

    return String;
}

API_INTERNAL VStr8 
PathConvertSlashes(Arena* MemPool, VStr8 Path, PathStyle Style)
{
    VStr8 Result = {};
    b32 ToBack = (Style == PATH_STYLE_WIN_ABSOLUTE);

    Result.Size = Path.Size;
    Result.Str = __VPushPadded(MemPool, Path.Size);

    for (u64 At = 0; At < Path.Size; At += VCHUNK_WIDTH) {
        VChunk8 Chunk = VChunkLoad(Path, At);

        if (ToBack)
            Chunk = VChunkBlend(Chunk, VChunkSet('\\'), VChunkEQ(Chunk, '/'));
        else
            Chunk = VChunkSlashFold(Chunk);

        VChunkStore(Result.Str + At, Chunk);
    }

    return Result;
}

INTERNAL VStr8
__VStrEscapeSplit(
    Arena* MemPool, 
    VStr8 String, 
    u8 Trigger, 
    Str8 (*Scalar)(Arena*, Str8)
) {
    u64 First = String.Size;

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunk8 Chunk = VChunkLoad(String, Offset);
        VMask8 Hits = Trigger
            ? VChunkEQ(Chunk, Trigger)
            : (VMask8) (
                VChunkClass(Chunk, &V_CLASS_TABLE_JSON, V_CLASS_KIND_QUOTE) | 
                VChunkClass(Chunk, &V_CLASS_TABLE_JSON, V_CLASS_KIND_ESCAPE) | 
                VChunkLT(Chunk, 0x20)
            );

        Hits = (VMask8) (Hits & VChunkValid(String, Offset));

        if (Hits) {
            First = Offset + VMaskFirst(Hits);
            break;
        }
    }

    if (First >= String.Size)
        return VStr(MemPool, Str(String));

    TempArena Scratch = GetScratch(&MemPool, 1);
    Str8 Tail = Scalar(
        Scratch.MemPool, 
        Str(String.Str + First, String.Size - First)
    );
    VStr8 Result = ArenaPushVStrCat(
        MemPool, 
        Str(String.Str, First), 
        Tail
    );

    ReleaseScratch(Scratch);

    return Result;
}

API_INTERNAL VStr8 
RawFromEscapedStr8(Arena* MemPool, VStr8 String)
{
    return __VStrEscapeSplit(MemPool, String, '\\', RawFromEscapedStr8);
}

API_INTERNAL VStr8 
EscapedFromRawStr8(Arena* MemPool, VStr8 String)
{
    return __VStrEscapeSplit(MemPool, String, 0, EscapedFromRawStr8);
}

API_INTERNAL VStr8 
VStrReplaceByte(Arena* MemPool, VStr8 String, u8 StartOffset, u8 EndOffset)
{
    VStr8 Result = VStr(MemPool, Str(String));

    VStrReplaceByte(Result, StartOffset, EndOffset);

    return Result;
}

API_INTERNAL void 
VStrReplaceByte(VStr8 String, u8 StartOffset, u8 EndOffset)
{
    VChunk8 Replacement = VChunkSet(EndOffset);

    for (u64 Offset = 0; Offset < String.Size; Offset += VCHUNK_WIDTH) {
        VChunk8 Chunk = VChunkLoad(String, Offset);
        VMask8 Hits = VChunkEQ(Chunk, StartOffset);

        if (Hits) {
            VChunkStore(
                String.Str + Offset, 
                VChunkBlend(Chunk, Replacement, Hits)
            );
        }
    }
}

#if defined(NC_LANG_CPP)
    INTERNAL b32
    operator==(VStr8 StringA, VStr8 StringB)
    {
        return StrMatch(StringA, StringB, 0);
    }

    INTERNAL b32
    operator!=(VStr8 StringA, VStr8 StringB)
    {
        return !StrMatch(StringA, StringB, 0);
    }

    INTERNAL b32
    operator<(VStr8 StringA, VStr8 StringB)
    {
        return VStrCmp(StringA, StringB) < 0;
    }
#endif
