#ifndef QUASAR_STD_SIMD_H
#define QUASAR_STD_SIMD_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * simd.h — Quasar portable SIMD vector API.
 *
 * Provides 18 fixed-width vector types (128, 256, 512-bit) for integer
 * and floating-point elements, with 20+ operations per type.  The
 * implementation uses GCC/Clang vector extensions when available and
 * falls back to portable scalar code otherwise.
 *
 * No architecture-specific intrinsics are exposed in the public API.
 * Users write once and the backend is selected automatically at
 * compile time.
 */

/* ── Backend Detection ────────────────────────────────────────────────
 *
 * QUASAR_SIMD_LEVEL encodes the highest available SIMD instruction set.
 * QUASAR_SIMD_WIDTH is the native vector register width in bytes, or a
 * reasonable struct size for the scalar fallback.
 *
 * Users should NOT test QUASAR_SIMD_LEVEL directly; use the vector
 * types and operations which are always available.
 */

#define QUASAR_SIMD_NONE   0
#define QUASAR_SIMD_SSE2   1
#define QUASAR_SIMD_AVX    2
#define QUASAR_SIMD_AVX2   3
#define QUASAR_SIMD_AVX512 4
#define QUASAR_SIMD_NEON   5

#if defined(__AVX512F__)
	#define QUASAR_SIMD_LEVEL QUASAR_SIMD_AVX512
	#define QUASAR_SIMD_WIDTH 64
#elif defined(__AVX2__)
	#define QUASAR_SIMD_LEVEL QUASAR_SIMD_AVX2
	#define QUASAR_SIMD_WIDTH 32
#elif defined(__AVX__)
	#define QUASAR_SIMD_LEVEL QUASAR_SIMD_AVX
	#define QUASAR_SIMD_WIDTH 32
#elif defined(__SSE2__)
	#define QUASAR_SIMD_LEVEL QUASAR_SIMD_SSE2
	#define QUASAR_SIMD_WIDTH 16
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
	#define QUASAR_SIMD_LEVEL QUASAR_SIMD_NEON
	#define QUASAR_SIMD_WIDTH 16
#else
	#define QUASAR_SIMD_LEVEL QUASAR_SIMD_NONE
	#define QUASAR_SIMD_WIDTH 16
#endif

/*
 * Vector extensions: GCC and Clang support __attribute__((vector_size))
 * which allows operator overloading on vector types.  When available,
 * most operations compile to single SIMD instructions.
 */
#if defined(__GNUC__) || defined(__clang__)
	#define QUASAR_HAS_VECTOR_EXT 1
#else
	#define QUASAR_HAS_VECTOR_EXT 0
#endif

/*
 * Alignment hint helper.  Tells the compiler a pointer is aligned to a
 * given boundary so it can emit aligned load/store instructions.
 * Non-GCC compilers simply pass the pointer through.
 */
#if defined(__GNUC__) || defined(__clang__)
	#define QUASAR_ASSUME_ALIGNED(p, a) __builtin_assume_aligned((p), (a))
#else
	#define QUASAR_ASSUME_ALIGNED(p, a) (p)
#endif

/* ── Vector Type Definitions ──────────────────────────────────────────
 *
 * Each type name encodes the element count and type:
 *   vec_NiM  — N elements of signed integer width M bits
 *   vec_NfM  — N elements of float width M bits
 *
 * With GCC vector extensions the types map directly to hardware vector
 * registers.  The aligned(N) attribute guarantees proper stack alignment.
 *
 * Without vector extensions each type is a plain struct with a .d array;
 * operations fall back to element-wise loops.
 */

#if QUASAR_HAS_VECTOR_EXT

/* 128-bit vectors */
typedef int8_t   vec_16i8  __attribute__((vector_size(16), aligned(16)));
typedef int16_t  vec_8i16  __attribute__((vector_size(16), aligned(16)));
typedef int32_t  vec_4i32  __attribute__((vector_size(16), aligned(16)));
typedef int64_t  vec_2i64  __attribute__((vector_size(16), aligned(16)));
typedef float    vec_4f32  __attribute__((vector_size(16), aligned(16)));
typedef double   vec_2f64  __attribute__((vector_size(16), aligned(16)));

/* 256-bit vectors */
typedef int8_t   vec_32i8  __attribute__((vector_size(32), aligned(32)));
typedef int16_t  vec_16i16 __attribute__((vector_size(32), aligned(32)));
typedef int32_t  vec_8i32  __attribute__((vector_size(32), aligned(32)));
typedef int64_t  vec_4i64  __attribute__((vector_size(32), aligned(32)));
typedef float    vec_8f32  __attribute__((vector_size(32), aligned(32)));
typedef double   vec_4f64  __attribute__((vector_size(32), aligned(32)));

/* 512-bit vectors */
typedef int8_t   vec_64i8  __attribute__((vector_size(64), aligned(64)));
typedef int16_t  vec_32i16 __attribute__((vector_size(64), aligned(64)));
typedef int32_t  vec_16i32 __attribute__((vector_size(64), aligned(64)));
typedef int64_t  vec_8i64  __attribute__((vector_size(64), aligned(64)));
typedef float    vec_16f32 __attribute__((vector_size(64), aligned(64)));
typedef double   vec_8f64  __attribute__((vector_size(64), aligned(64)));

#else /* !QUASAR_HAS_VECTOR_EXT — scalar fallback structs */

/* 128-bit vectors */
typedef struct { int8_t   d[16]; } vec_16i8;
typedef struct { int16_t  d[8];  } vec_8i16;
typedef struct { int32_t  d[4];  } vec_4i32;
typedef struct { int64_t  d[2];  } vec_2i64;
typedef struct { float    d[4];  } vec_4f32;
typedef struct { double   d[2];  } vec_2f64;

/* 256-bit vectors */
typedef struct { int8_t   d[32]; } vec_32i8;
typedef struct { int16_t  d[16]; } vec_16i16;
typedef struct { int32_t  d[8];  } vec_8i32;
typedef struct { int64_t  d[4];  } vec_4i64;
typedef struct { float    d[8];  } vec_8f32;
typedef struct { double   d[4];  } vec_4f64;

/* 512-bit vectors */
typedef struct { int8_t   d[64]; } vec_64i8;
typedef struct { int16_t  d[32]; } vec_32i16;
typedef struct { int32_t  d[16]; } vec_16i32;
typedef struct { int64_t  d[8];  } vec_8i64;
typedef struct { float    d[16]; } vec_16f32;
typedef struct { double   d[8];  } vec_8f64;

/*
 * Conversion helpers for the scalar fallback: construct a vector struct
 * from a raw element array.  Useful when the compiler cannot resolve the
 * type automatically (e.g. passing a literal to vec_broadcast where the
 * source type differs from the destination element type).
 */

static inline vec_16i8  vec_16i8_from_i8x16(const int8_t d[16])   { vec_16i8  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_8i16  vec_8i16_from_i16x8(const int16_t d[8])   { vec_8i16  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_4i32  vec_4i32_from_i32x4(const int32_t d[4])   { vec_4i32  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_2i64  vec_2i64_from_i64x2(const int64_t d[2])   { vec_2i64  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_4f32  vec_4f32_from_f32x4(const float d[4])     { vec_4f32  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_2f64  vec_2f64_from_f64x2(const double d[2])    { vec_2f64  r; memcpy(r.d, d, sizeof(r.d)); return r; }

static inline vec_32i8  vec_32i8_from_i8x32(const int8_t d[32])   { vec_32i8  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_16i16 vec_16i16_from_i16x16(const int16_t d[16]){ vec_16i16 r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_8i32  vec_8i32_from_i32x8(const int32_t d[8])   { vec_8i32  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_4i64  vec_4i64_from_i64x4(const int64_t d[4])   { vec_4i64  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_8f32  vec_8f32_from_f32x8(const float d[8])     { vec_8f32  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_4f64  vec_4f64_from_f64x4(const double d[4])    { vec_4f64  r; memcpy(r.d, d, sizeof(r.d)); return r; }

static inline vec_64i8  vec_64i8_from_i8x64(const int8_t d[64])   { vec_64i8  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_32i16 vec_32i16_from_i16x32(const int16_t d[32]){ vec_32i16 r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_16i32 vec_16i32_from_i32x16(const int32_t d[16]){ vec_16i32 r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_8i64  vec_8i64_from_i64x8(const int64_t d[8])   { vec_8i64  r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_16f32 vec_16f32_from_f32x16(const float d[16])  { vec_16f32 r; memcpy(r.d, d, sizeof(r.d)); return r; }
static inline vec_8f64  vec_8f64_from_f64x8(const double d[8])    { vec_8f64  r; memcpy(r.d, d, sizeof(r.d)); return r; }

#endif /* QUASAR_HAS_VECTOR_EXT */

/* ── Operation-Generating Macros (GCC Vector Extensions) ──────────────
 *
 * When GCC/Clang vector extensions are available, these macros generate
 * functions that use native C operators on vector types.  The compiler
 * maps each operator to the corresponding SIMD instruction.
 *
 * Each macro takes four arguments:
 *   T      — vector type (e.g. vec_4i32)
 *   P      — function-name prefix (same as T by convention)
 *   S      — scalar element type (e.g. int32_t); unused in vector path
 *   N      — element count; unused in vector path
 *
 * The S and N arguments exist so call sites are identical across the
 * vector and scalar code paths.
 */

#if QUASAR_HAS_VECTOR_EXT

/* ── Arithmetic: add, sub, mul (all types) ── */
#define QUASAR_SIMD_ARITH_V(T, P)                                \
	static inline T P##_add(T a, T b) { return (a) + (b); } \
	static inline T P##_sub(T a, T b) { return (a) - (b); } \
	static inline T P##_mul(T a, T b) { return (a) * (b); }

/* ── Arithmetic with division: integer types ── */
#define QUASAR_SIMD_ARITH_INT_V(T, P)                            \
	QUASAR_SIMD_ARITH_V(T, P)                                \
	static inline T P##_div(T a, T b) { return (a) / (b); }

/* ── Arithmetic with division: float types ── */
#define QUASAR_SIMD_ARITH_FLT_V(T, P)                            \
	QUASAR_SIMD_ARITH_V(T, P)                                \
	static inline T P##_div(T a, T b) { return (a) / (b); }

/* ── Bitwise: integer types only ── */
#define QUASAR_SIMD_BITWISE_V(T, P)                              \
	static inline T P##_and(T a, T b) { return (a) & (b); } \
	static inline T P##_or(T a, T b)  { return (a) | (b); } \
	static inline T P##_xor(T a, T b) { return (a) ^ (b); } \
	static inline T P##_not(T a)      { return ~(a); }

/* ── Comparison: all types ──
 *
 * Returns a mask vector: all-bits-set (-1 / NaN) for true lanes,
 * all-bits-zero for false lanes.  This is the natural result of
 * C comparison operators on GCC vector types.
 */
#define QUASAR_SIMD_COMPARE_V(T, P)                              \
	static inline T P##_eq(T a, T b) { return (a) == (b); } \
	static inline T P##_ne(T a, T b) { return (a) != (b); } \
	static inline T P##_lt(T a, T b) { return (a) < (b);  } \
	static inline T P##_gt(T a, T b) { return (a) > (b);  } \
	static inline T P##_le(T a, T b) { return (a) <= (b); } \
	static inline T P##_ge(T a, T b) { return (a) >= (b); }

/* ── Memory: load and store (all types) ──
 *
 * load/load_aligned read a vector from memory.
 * store/store_aligned write a vector to memory.
 *
 * Aligned variants assume the pointer satisfies the vector's natural
 * alignment (16/32/64 bytes).  Misaligned pointers cause undefined
 * behaviour — it is the caller's responsibility.
 *
 * Unaligned variants use memcpy which the compiler lowers to a single
 * unaligned load/store instruction on architectures that support it.
 */
#define QUASAR_SIMD_MEM_V(T, P, S, N)                                 \
	static inline T P##_load(const S *p)                           \
	{                                                              \
		T r;                                                   \
		memcpy(&r, (p), sizeof(T));                            \
		return r;                                              \
	}                                                              \
	static inline T P##_load_aligned(const S *p)                   \
	{                                                              \
		return *(const T *)QUASAR_ASSUME_ALIGNED((p), sizeof(T)); \
	}                                                              \
	static inline void P##_store(S *p, T v)                        \
	{                                                              \
		memcpy((p), &v, sizeof(T));                            \
	}                                                              \
	static inline void P##_store_aligned(S *p, T v)                \
	{                                                              \
		*(T *)QUASAR_ASSUME_ALIGNED((p), sizeof(T)) = v;       \
	}

/* ── Utility: initialisation (all types) ──
 *
 * zero()      — all lanes set to 0.
 * broadcast() — all lanes set to the same scalar value.
 * set()       — alias for broadcast (semantic intent: "fill with value").
 */
#define QUASAR_SIMD_UTIL_V(T, P, S, N)               \
	static inline T P##_zero(void)               \
	{                                            \
		T r = {0};                           \
		return r;                            \
	}                                            \
	static inline T P##_broadcast(S v)           \
	{                                            \
		return (T){} + (v);                  \
	}                                            \
	static inline T P##_set(S v)                 \
	{                                            \
		return P##_broadcast(v);             \
	}

/* ── Element-wise min / max — integer types ──
 *
 * Per-element minimum and maximum of two vectors.  Uses bitwise select
 * because GCC does not provide a built-in per-element ternary for vector
 * types.  The pattern (lt & a) | (~lt & b) selects the smaller element:
 *
 *   If a < b then lt = all-bits-set → (all-bits & a) | (0 & b) = a
 *   If a ≥ b then lt = 0           → (0 & a) | (all-bits & b) = b
 *
 * Bitwise operators are valid on integer vector types in GCC extensions.
 */
#define QUASAR_SIMD_MINMAX_V(T, P)                             \
	static inline T P##_min_elem(T a, T b)                 \
	{                                                      \
		T lt = (a) < (b);                              \
		return (lt & (a)) | (~lt & (b));               \
	}                                                      \
	static inline T P##_max_elem(T a, T b)                 \
	{                                                      \
		T gt = (a) > (b);                              \
		return (gt & (a)) | (~gt & (b));               \
	}

/*
 * ── Element-wise min / max — float types ──
 *
 * Bitwise operators are NOT valid on float vector types in GCC extensions
 * (the C standard forbids & | ~ on floating-point types even in vectors).
 * We use memcpy to reinterpret the float vectors as their integer-width
 * counterparts, perform the bitwise select there, and reinterpret back.
 * All memcpy calls are eliminated by the compiler at any optimisation
 * level — this produces the same machine code as direct bitwise ops.
 */
#define QUASAR_SIMD_MINMAX_FLT_V(T, P, IT)                     \
	static inline T P##_min_elem(T a, T b)                 \
	{                                                      \
		T lt = (a) < (b);                              \
		IT lt_i, a_i, b_i;                             \
		memcpy(&lt_i, &lt, sizeof(IT));                \
		memcpy(&a_i, &a, sizeof(IT));                  \
		memcpy(&b_i, &b, sizeof(IT));                  \
		IT r_i = (lt_i & a_i) | (~lt_i & b_i);         \
		T r; memcpy(&r, &r_i, sizeof(T));              \
		return r;                                      \
	}                                                      \
	static inline T P##_max_elem(T a, T b)                 \
	{                                                      \
		T gt = (a) > (b);                              \
		IT gt_i, a_i, b_i;                             \
		memcpy(&gt_i, &gt, sizeof(IT));                \
		memcpy(&a_i, &a, sizeof(IT));                  \
		memcpy(&b_i, &b, sizeof(IT));                  \
		IT r_i = (gt_i & a_i) | (~gt_i & b_i);         \
		T r; memcpy(&r, &r_i, sizeof(T));              \
		return r;                                      \
	}

#else /* !QUASAR_HAS_VECTOR_EXT — scalar fallback macros */

/* ── Arithmetic — scalar fallback ── */
#define QUASAR_SIMD_ARITH_S(T, P, S, N)                  \
	static inline T P##_add(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] + b.d[_i];        \
		return r;                                \
	}                                                \
	static inline T P##_sub(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] - b.d[_i];        \
		return r;                                \
	}                                                \
	static inline T P##_mul(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] * b.d[_i];        \
		return r;                                \
	}

/* ── Arithmetic with division — integer scalar fallback ── */
#define QUASAR_SIMD_ARITH_INT_S(T, P, S, N)              \
	QUASAR_SIMD_ARITH_S(T, P, S, N)                  \
	static inline T P##_div(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] / b.d[_i];        \
		return r;                                \
	}

/* ── Arithmetic with division — float scalar fallback ── */
#define QUASAR_SIMD_ARITH_FLT_S(T, P, S, N)              \
	QUASAR_SIMD_ARITH_S(T, P, S, N)                  \
	static inline T P##_div(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] / b.d[_i];        \
		return r;                                \
	}

/* ── Bitwise — integer scalar fallback ── */
#define QUASAR_SIMD_BITWISE_S(T, P, S, N)                \
	static inline T P##_and(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] & b.d[_i];        \
		return r;                                \
	}                                                \
	static inline T P##_or(T a, T b)                 \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] | b.d[_i];        \
		return r;                                \
	}                                                \
	static inline T P##_xor(T a, T b)                \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = a.d[_i] ^ b.d[_i];        \
		return r;                                \
	}                                                \
	static inline T P##_not(T a)                     \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = ~a.d[_i];              \
		return r;                                \
	}

/* ── Comparison — integer scalar fallback ──
 *
 * Returns a mask vector: each lane is all-bits-set (-1 as that integer
 * type) for true, zero for false.  Casting -1 to the integer type
 * gives the correct bit pattern (0xFF for i8, 0xFFFF for i16, etc.).
 */
#define QUASAR_SIMD_COMPARE_INT_S(T, P, S, N)                \
	static inline T P##_eq(T a, T b)                     \
	{                                                    \
		T r;                                         \
		for (int _i = 0; _i < (N); _i++)             \
			r.d[_i] = a.d[_i] == b.d[_i]         \
				? (S)(-1) : (S)0;               \
		return r;                                    \
	}                                                    \
	static inline T P##_ne(T a, T b)                     \
	{                                                    \
		T r;                                         \
		for (int _i = 0; _i < (N); _i++)             \
			r.d[_i] = a.d[_i] != b.d[_i]         \
				? (S)(-1) : (S)0;               \
		return r;                                    \
	}                                                    \
	static inline T P##_lt(T a, T b)                     \
	{                                                    \
		T r;                                         \
		for (int _i = 0; _i < (N); _i++)             \
			r.d[_i] = a.d[_i] < b.d[_i]          \
				? (S)(-1) : (S)0;               \
		return r;                                    \
	}                                                    \
	static inline T P##_gt(T a, T b)                     \
	{                                                    \
		T r;                                         \
		for (int _i = 0; _i < (N); _i++)             \
			r.d[_i] = a.d[_i] > b.d[_i]          \
				? (S)(-1) : (S)0;               \
		return r;                                    \
	}                                                    \
	static inline T P##_le(T a, T b)                     \
	{                                                    \
		T r;                                         \
		for (int _i = 0; _i < (N); _i++)             \
			r.d[_i] = a.d[_i] <= b.d[_i]         \
				? (S)(-1) : (S)0;               \
		return r;                                    \
	}                                                    \
	static inline T P##_ge(T a, T b)                     \
	{                                                    \
		T r;                                         \
		for (int _i = 0; _i < (N); _i++)             \
			r.d[_i] = a.d[_i] >= b.d[_i]         \
				? (S)(-1) : (S)0;               \
		return r;                                    \
	}

/*
 * ── Comparison — float scalar fallback ──
 *
 * For float types we cannot cast -1 to get an all-bits-set value
 * (that would be -1.0f, not 0xFFFFFFFF).  Instead we construct the
 * mask via memcpy from an integer with all bits set.  The compiler
 * optimises the memcpy away and uses the raw bit pattern.
 */
#define QUASAR_SIMD_COMPARE_FLT_S(T, P, S, N, IW)                \
	static inline T P##_eq(T a, T b)                         \
	{                                                        \
		T r; S _mask; IW _bits = (IW)(-1);              \
		memcpy(&_mask, &_bits, sizeof(S));              \
		for (int _i = 0; _i < (N); _i++)                 \
			r.d[_i] = a.d[_i] == b.d[_i]             \
				? _mask : (S)0;                      \
		return r;                                        \
	}                                                        \
	static inline T P##_ne(T a, T b)                         \
	{                                                        \
		T r; S _mask; IW _bits = (IW)(-1);              \
		memcpy(&_mask, &_bits, sizeof(S));              \
		for (int _i = 0; _i < (N); _i++)                 \
			r.d[_i] = a.d[_i] != b.d[_i]             \
				? _mask : (S)0;                      \
		return r;                                        \
	}                                                        \
	static inline T P##_lt(T a, T b)                         \
	{                                                        \
		T r; S _mask; IW _bits = (IW)(-1);              \
		memcpy(&_mask, &_bits, sizeof(S));              \
		for (int _i = 0; _i < (N); _i++)                 \
			r.d[_i] = a.d[_i] < b.d[_i]              \
				? _mask : (S)0;                      \
		return r;                                        \
	}                                                        \
	static inline T P##_gt(T a, T b)                         \
	{                                                        \
		T r; S _mask; IW _bits = (IW)(-1);              \
		memcpy(&_mask, &_bits, sizeof(S));              \
		for (int _i = 0; _i < (N); _i++)                 \
			r.d[_i] = a.d[_i] > b.d[_i]              \
				? _mask : (S)0;                      \
		return r;                                        \
	}                                                        \
	static inline T P##_le(T a, T b)                         \
	{                                                        \
		T r; S _mask; IW _bits = (IW)(-1);              \
		memcpy(&_mask, &_bits, sizeof(S));              \
		for (int _i = 0; _i < (N); _i++)                 \
			r.d[_i] = a.d[_i] <= b.d[_i]             \
				? _mask : (S)0;                      \
		return r;                                        \
	}                                                        \
	static inline T P##_ge(T a, T b)                         \
	{                                                        \
		T r; S _mask; IW _bits = (IW)(-1);              \
		memcpy(&_mask, &_bits, sizeof(S));              \
		for (int _i = 0; _i < (N); _i++)                 \
			r.d[_i] = a.d[_i] >= b.d[_i]             \
				? _mask : (S)0;                      \
		return r;                                        \
	}

/* ── Memory — scalar fallback ── */
#define QUASAR_SIMD_MEM_S(T, P, S, N)                      \
	static inline T P##_load(const S *p)               \
	{                                                  \
		T r; memcpy(&r, (p), sizeof(T));           \
		return r;                                  \
	}                                                  \
	static inline T P##_load_aligned(const S *p)       \
	{                                                  \
		T r; memcpy(&r, (p), sizeof(T));           \
		return r;                                  \
	}                                                  \
	static inline void P##_store(S *p, T v)            \
	{                                                  \
		memcpy((p), &v, sizeof(T));                \
	}                                                  \
	static inline void P##_store_aligned(S *p, T v)    \
	{                                                  \
		memcpy((p), &v, sizeof(T));                \
	}

/* ── Utility — scalar fallback ── */
#define QUASAR_SIMD_UTIL_S(T, P, S, N)                  \
	static inline T P##_zero(void)                   \
	{                                                \
		T r; memset(&r, 0, sizeof(T));           \
		return r;                                \
	}                                                \
	static inline T P##_set(S v)                     \
	{                                                \
		T r;                                     \
		for (int _i = 0; _i < (N); _i++)         \
			r.d[_i] = v;                         \
		return r;                                \
	}                                                \
	static inline T P##_broadcast(S v)               \
	{                                                \
		return P##_set(v);                       \
	}

/*
 * ── Element-wise min / max — scalar fallback ──
 *
 * For the scalar path we use a simple ternary.  This is semantically
 * equivalent to the bitwise-select pattern used in the vector path:
 * for each lane, the smaller (or larger) of the two values is chosen.
 */
#define QUASAR_SIMD_MINMAX_S(T, P, S, N)                      \
	static inline T P##_min_elem(T a, T b)                 \
	{                                                      \
		T r;                                           \
		for (int _i = 0; _i < (N); _i++)               \
			r.d[_i] = a.d[_i] < b.d[_i]            \
				? a.d[_i] : b.d[_i];              \
		return r;                                      \
	}                                                      \
	static inline T P##_max_elem(T a, T b)                 \
	{                                                      \
		T r;                                           \
		for (int _i = 0; _i < (N); _i++)               \
			r.d[_i] = a.d[_i] > b.d[_i]            \
				? a.d[_i] : b.d[_i];              \
		return r;                                      \
	}

#endif /* QUASAR_HAS_VECTOR_EXT */

/* ── Dispatch Macros ─────────────────────────────────────────────────
 *
 * These select the vector or scalar implementation based on
 * QUASAR_HAS_VECTOR_EXT.  Call sites use the same four-argument
 * form regardless of the active backend; unused arguments in the
 * vector path are silently ignored by the preprocessor.
 */

#if QUASAR_HAS_VECTOR_EXT

#define QUASAR_SIMD_ARITH(T, P, S, N)         QUASAR_SIMD_ARITH_V(T, P)
#define QUASAR_SIMD_ARITH_INT(T, P, S, N)     QUASAR_SIMD_ARITH_INT_V(T, P)
#define QUASAR_SIMD_ARITH_FLT(T, P, S, N)     QUASAR_SIMD_ARITH_FLT_V(T, P)
#define QUASAR_SIMD_BITWISE(T, P, S, N)       QUASAR_SIMD_BITWISE_V(T, P)
#define QUASAR_SIMD_COMPARE(T, P, S, N)       QUASAR_SIMD_COMPARE_V(T, P)
#define QUASAR_SIMD_MEM(T, P, S, N)           QUASAR_SIMD_MEM_V(T, P, S, N)
#define QUASAR_SIMD_UTIL(T, P, S, N)          QUASAR_SIMD_UTIL_V(T, P, S, N)
#define QUASAR_SIMD_MINMAX(T, P, S, N)        QUASAR_SIMD_MINMAX_V(T, P)
#define QUASAR_SIMD_MINMAX_FLT(T, P, S, N, IT)    QUASAR_SIMD_MINMAX_FLT_V(T, P, IT)

/* Single dispatch for compare (same macro for int and float) */
#define QUASAR_SIMD_COMPARE_INT(T, P, S, N)   QUASAR_SIMD_COMPARE_V(T, P)
#define QUASAR_SIMD_COMPARE_FLT(T, P, S, N, IW)   QUASAR_SIMD_COMPARE_V(T, P)

#else /* !QUASAR_HAS_VECTOR_EXT */

#define QUASAR_SIMD_ARITH(T, P, S, N)         QUASAR_SIMD_ARITH_S(T, P, S, N)
#define QUASAR_SIMD_ARITH_INT(T, P, S, N)     QUASAR_SIMD_ARITH_INT_S(T, P, S, N)
#define QUASAR_SIMD_ARITH_FLT(T, P, S, N)     QUASAR_SIMD_ARITH_FLT_S(T, P, S, N)
#define QUASAR_SIMD_BITWISE(T, P, S, N)       QUASAR_SIMD_BITWISE_S(T, P, S, N)
#define QUASAR_SIMD_MEM(T, P, S, N)           QUASAR_SIMD_MEM_S(T, P, S, N)
#define QUASAR_SIMD_UTIL(T, P, S, N)          QUASAR_SIMD_UTIL_S(T, P, S, N)
#define QUASAR_SIMD_MINMAX(T, P, S, N)        QUASAR_SIMD_MINMAX_S(T, P, S, N)
#define QUASAR_SIMD_MINMAX_FLT(T, P, S, N, IT)    QUASAR_SIMD_MINMAX_S(T, P, S, N)

/* Scalar compare dispatches: separate int and float paths.
 * The IW (integer-width) parameter is passed through to construct
 * all-bits-set mask values: uint32_t for float, uint64_t for double. */
#define QUASAR_SIMD_COMPARE_INT(T, P, S, N)   QUASAR_SIMD_COMPARE_INT_S(T, P, S, N)
#define QUASAR_SIMD_COMPARE_FLT(T, P, S, N, IW)   QUASAR_SIMD_COMPARE_FLT_S(T, P, S, N, IW)

#endif /* QUASAR_HAS_VECTOR_EXT */

/*
 * ── Type Instantiation ───────────────────────────────────────────────
 *
 * Each of the 18 vector types gets its full set of operations
 * instantiated via the dispatch macros above.  The naming convention
 * is strictly type-prefixed:
 *
 *   vec_4i32_add, vec_4i32_sub, vec_4i32_mul, vec_4i32_div,
 *   vec_4i32_and, vec_4i32_or,  vec_4i32_xor, vec_4i32_not,
 *   vec_4i32_eq,  vec_4i32_ne,  vec_4i32_lt,  vec_4i32_gt,
 *   vec_4i32_le,  vec_4i32_ge,
 *   vec_4i32_load, vec_4i32_load_aligned,
 *   vec_4i32_store, vec_4i32_store_aligned,
 *   vec_4i32_zero, vec_4i32_set, vec_4i32_broadcast,
 *   vec_4i32_min_elem, vec_4i32_max_elem,
 *   vec_4i32_sum, vec_4i32_min, vec_4i32_max   (reductions)
 *
 * Float types omit the bitwise operations.
 */

/* ── 128-bit Integer Vectors ─────────────────────────────────────── */

QUASAR_SIMD_ARITH_INT(vec_16i8, vec_16i8, int8_t, 16)
QUASAR_SIMD_BITWISE(vec_16i8, vec_16i8, int8_t, 16)
QUASAR_SIMD_COMPARE_INT(vec_16i8, vec_16i8, int8_t, 16)
QUASAR_SIMD_MEM(vec_16i8, vec_16i8, int8_t, 16)
QUASAR_SIMD_UTIL(vec_16i8, vec_16i8, int8_t, 16)
QUASAR_SIMD_MINMAX(vec_16i8, vec_16i8, int8_t, 16)

QUASAR_SIMD_ARITH_INT(vec_8i16, vec_8i16, int16_t, 8)
QUASAR_SIMD_BITWISE(vec_8i16, vec_8i16, int16_t, 8)
QUASAR_SIMD_COMPARE_INT(vec_8i16, vec_8i16, int16_t, 8)
QUASAR_SIMD_MEM(vec_8i16, vec_8i16, int16_t, 8)
QUASAR_SIMD_UTIL(vec_8i16, vec_8i16, int16_t, 8)
QUASAR_SIMD_MINMAX(vec_8i16, vec_8i16, int16_t, 8)

QUASAR_SIMD_ARITH_INT(vec_4i32, vec_4i32, int32_t, 4)
QUASAR_SIMD_BITWISE(vec_4i32, vec_4i32, int32_t, 4)
QUASAR_SIMD_COMPARE_INT(vec_4i32, vec_4i32, int32_t, 4)
QUASAR_SIMD_MEM(vec_4i32, vec_4i32, int32_t, 4)
QUASAR_SIMD_UTIL(vec_4i32, vec_4i32, int32_t, 4)
QUASAR_SIMD_MINMAX(vec_4i32, vec_4i32, int32_t, 4)

QUASAR_SIMD_ARITH_INT(vec_2i64, vec_2i64, int64_t, 2)
QUASAR_SIMD_BITWISE(vec_2i64, vec_2i64, int64_t, 2)
QUASAR_SIMD_COMPARE_INT(vec_2i64, vec_2i64, int64_t, 2)
QUASAR_SIMD_MEM(vec_2i64, vec_2i64, int64_t, 2)
QUASAR_SIMD_UTIL(vec_2i64, vec_2i64, int64_t, 2)
QUASAR_SIMD_MINMAX(vec_2i64, vec_2i64, int64_t, 2)

/* ── 128-bit Float Vectors ───────────────────────────────────────── */

QUASAR_SIMD_ARITH_FLT(vec_4f32, vec_4f32, float, 4)
QUASAR_SIMD_COMPARE_FLT(vec_4f32, vec_4f32, float, 4, uint32_t)
QUASAR_SIMD_MEM(vec_4f32, vec_4f32, float, 4)
QUASAR_SIMD_UTIL(vec_4f32, vec_4f32, float, 4)
QUASAR_SIMD_MINMAX_FLT(vec_4f32, vec_4f32, float, 4, vec_4i32)

QUASAR_SIMD_ARITH_FLT(vec_2f64, vec_2f64, double, 2)
QUASAR_SIMD_COMPARE_FLT(vec_2f64, vec_2f64, double, 2, uint64_t)
QUASAR_SIMD_MEM(vec_2f64, vec_2f64, double, 2)
QUASAR_SIMD_UTIL(vec_2f64, vec_2f64, double, 2)
QUASAR_SIMD_MINMAX_FLT(vec_2f64, vec_2f64, double, 2, vec_4i32)

/* ── 256-bit Integer Vectors ─────────────────────────────────────── */

QUASAR_SIMD_ARITH_INT(vec_32i8, vec_32i8, int8_t, 32)
QUASAR_SIMD_BITWISE(vec_32i8, vec_32i8, int8_t, 32)
QUASAR_SIMD_COMPARE_INT(vec_32i8, vec_32i8, int8_t, 32)
QUASAR_SIMD_MEM(vec_32i8, vec_32i8, int8_t, 32)
QUASAR_SIMD_UTIL(vec_32i8, vec_32i8, int8_t, 32)
QUASAR_SIMD_MINMAX(vec_32i8, vec_32i8, int8_t, 32)

QUASAR_SIMD_ARITH_INT(vec_16i16, vec_16i16, int16_t, 16)
QUASAR_SIMD_BITWISE(vec_16i16, vec_16i16, int16_t, 16)
QUASAR_SIMD_COMPARE_INT(vec_16i16, vec_16i16, int16_t, 16)
QUASAR_SIMD_MEM(vec_16i16, vec_16i16, int16_t, 16)
QUASAR_SIMD_UTIL(vec_16i16, vec_16i16, int16_t, 16)
QUASAR_SIMD_MINMAX(vec_16i16, vec_16i16, int16_t, 16)

QUASAR_SIMD_ARITH_INT(vec_8i32, vec_8i32, int32_t, 8)
QUASAR_SIMD_BITWISE(vec_8i32, vec_8i32, int32_t, 8)
QUASAR_SIMD_COMPARE_INT(vec_8i32, vec_8i32, int32_t, 8)
QUASAR_SIMD_MEM(vec_8i32, vec_8i32, int32_t, 8)
QUASAR_SIMD_UTIL(vec_8i32, vec_8i32, int32_t, 8)
QUASAR_SIMD_MINMAX(vec_8i32, vec_8i32, int32_t, 8)

QUASAR_SIMD_ARITH_INT(vec_4i64, vec_4i64, int64_t, 4)
QUASAR_SIMD_BITWISE(vec_4i64, vec_4i64, int64_t, 4)
QUASAR_SIMD_COMPARE_INT(vec_4i64, vec_4i64, int64_t, 4)
QUASAR_SIMD_MEM(vec_4i64, vec_4i64, int64_t, 4)
QUASAR_SIMD_UTIL(vec_4i64, vec_4i64, int64_t, 4)
QUASAR_SIMD_MINMAX(vec_4i64, vec_4i64, int64_t, 4)

/* ── 256-bit Float Vectors ───────────────────────────────────────── */

QUASAR_SIMD_ARITH_FLT(vec_8f32, vec_8f32, float, 8)
QUASAR_SIMD_COMPARE_FLT(vec_8f32, vec_8f32, float, 8, uint32_t)
QUASAR_SIMD_MEM(vec_8f32, vec_8f32, float, 8)
QUASAR_SIMD_UTIL(vec_8f32, vec_8f32, float, 8)
QUASAR_SIMD_MINMAX_FLT(vec_8f32, vec_8f32, float, 8, vec_8i32)

QUASAR_SIMD_ARITH_FLT(vec_4f64, vec_4f64, double, 4)
QUASAR_SIMD_COMPARE_FLT(vec_4f64, vec_4f64, double, 4, uint64_t)
QUASAR_SIMD_MEM(vec_4f64, vec_4f64, double, 4)
QUASAR_SIMD_UTIL(vec_4f64, vec_4f64, double, 4)
QUASAR_SIMD_MINMAX_FLT(vec_4f64, vec_4f64, double, 4, vec_8i32)

/* ── 512-bit Integer Vectors ─────────────────────────────────────── */

QUASAR_SIMD_ARITH_INT(vec_64i8, vec_64i8, int8_t, 64)
QUASAR_SIMD_BITWISE(vec_64i8, vec_64i8, int8_t, 64)
QUASAR_SIMD_COMPARE_INT(vec_64i8, vec_64i8, int8_t, 64)
QUASAR_SIMD_MEM(vec_64i8, vec_64i8, int8_t, 64)
QUASAR_SIMD_UTIL(vec_64i8, vec_64i8, int8_t, 64)
QUASAR_SIMD_MINMAX(vec_64i8, vec_64i8, int8_t, 64)

QUASAR_SIMD_ARITH_INT(vec_32i16, vec_32i16, int16_t, 32)
QUASAR_SIMD_BITWISE(vec_32i16, vec_32i16, int16_t, 32)
QUASAR_SIMD_COMPARE_INT(vec_32i16, vec_32i16, int16_t, 32)
QUASAR_SIMD_MEM(vec_32i16, vec_32i16, int16_t, 32)
QUASAR_SIMD_UTIL(vec_32i16, vec_32i16, int16_t, 32)
QUASAR_SIMD_MINMAX(vec_32i16, vec_32i16, int16_t, 32)

QUASAR_SIMD_ARITH_INT(vec_16i32, vec_16i32, int32_t, 16)
QUASAR_SIMD_BITWISE(vec_16i32, vec_16i32, int32_t, 16)
QUASAR_SIMD_COMPARE_INT(vec_16i32, vec_16i32, int32_t, 16)
QUASAR_SIMD_MEM(vec_16i32, vec_16i32, int32_t, 16)
QUASAR_SIMD_UTIL(vec_16i32, vec_16i32, int32_t, 16)
QUASAR_SIMD_MINMAX(vec_16i32, vec_16i32, int32_t, 16)

QUASAR_SIMD_ARITH_INT(vec_8i64, vec_8i64, int64_t, 8)
QUASAR_SIMD_BITWISE(vec_8i64, vec_8i64, int64_t, 8)
QUASAR_SIMD_COMPARE_INT(vec_8i64, vec_8i64, int64_t, 8)
QUASAR_SIMD_MEM(vec_8i64, vec_8i64, int64_t, 8)
QUASAR_SIMD_UTIL(vec_8i64, vec_8i64, int64_t, 8)
QUASAR_SIMD_MINMAX(vec_8i64, vec_8i64, int64_t, 8)

/* ── 512-bit Float Vectors ───────────────────────────────────────── */

QUASAR_SIMD_ARITH_FLT(vec_16f32, vec_16f32, float, 16)
QUASAR_SIMD_COMPARE_FLT(vec_16f32, vec_16f32, float, 16, uint32_t)
QUASAR_SIMD_MEM(vec_16f32, vec_16f32, float, 16)
QUASAR_SIMD_UTIL(vec_16f32, vec_16f32, float, 16)
QUASAR_SIMD_MINMAX_FLT(vec_16f32, vec_16f32, float, 16, vec_16i32)

QUASAR_SIMD_ARITH_FLT(vec_8f64, vec_8f64, double, 8)
QUASAR_SIMD_COMPARE_FLT(vec_8f64, vec_8f64, double, 8, uint64_t)
QUASAR_SIMD_MEM(vec_8f64, vec_8f64, double, 8)
QUASAR_SIMD_UTIL(vec_8f64, vec_8f64, double, 8)
QUASAR_SIMD_MINMAX_FLT(vec_8f64, vec_8f64, double, 8, vec_16i32)

/*
 * ── Reductions ──────────────────────────────────────────────────────
 *
 * Reductions collapse a vector to a single scalar value:
 *
 *   sum  — sum of all elements
 *   min  — minimum element
 *   max  — maximum element
 *
 * With GCC vector extensions we use pairwise shuffle + accumulate
 * (a log₂(N) tree reduction).  For i8 and i16 types we use a loop
 * with i32 accumulation to avoid overflow.
 *
 * Without vector extensions all reductions use a straightforward
 * element-wise loop.
 *
 * Float reductions follow IEEE 754 semantics: NaN propagation and
 * signed zero behaviour depend on the underlying C operators used
 * in the reduction tree.
 */

/*
 * Reductions use straightforward element access for maximum portability.
 * The compiler can still auto-vectorise these loops when profitable.
 *
 * For narrow integer types (i8, i16) sums are accumulated into i32 to
 * prevent overflow (e.g. 64 × 127 = 8128, which fits in i32).
 */

/* ── vec_16i8 reductions ─────────────────────────────────────────── */

static inline int32_t vec_16i8_sum(vec_16i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 16; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 16; i++) s += v.d[i];
	return s;
#endif
}

static inline int8_t vec_16i8_min(vec_16i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int8_t m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int8_t m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int8_t vec_16i8_max(vec_16i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int8_t m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int8_t m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_8i16 reductions ─────────────────────────────────────────── */

static inline int32_t vec_8i16_sum(vec_8i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 8; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 8; i++) s += v.d[i];
	return s;
#endif
}

static inline int16_t vec_8i16_min(vec_8i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int16_t m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int16_t m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int16_t vec_8i16_max(vec_8i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int16_t m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int16_t m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_4i32 reductions ─────────────────────────────────────────── */

static inline int32_t vec_4i32_sum(vec_4i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	return p[0] + p[1] + p[2] + p[3];
#else
	return v.d[0] + v.d[1] + v.d[2] + v.d[3];
#endif
}

static inline int32_t vec_4i32_min(vec_4i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t m = p[0];
	if (p[1] < m) m = p[1];
	if (p[2] < m) m = p[2];
	if (p[3] < m) m = p[3];
	return m;
#else
	int32_t m = v.d[0];
	if (v.d[1] < m) m = v.d[1];
	if (v.d[2] < m) m = v.d[2];
	if (v.d[3] < m) m = v.d[3];
	return m;
#endif
}

static inline int32_t vec_4i32_max(vec_4i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t m = p[0];
	if (p[1] > m) m = p[1];
	if (p[2] > m) m = p[2];
	if (p[3] > m) m = p[3];
	return m;
#else
	int32_t m = v.d[0];
	if (v.d[1] > m) m = v.d[1];
	if (v.d[2] > m) m = v.d[2];
	if (v.d[3] > m) m = v.d[3];
	return m;
#endif
}

/* ── vec_2i64 reductions ─────────────────────────────────────────── */

static inline int64_t vec_2i64_sum(vec_2i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	return p[0] + p[1];
#else
	return v.d[0] + v.d[1];
#endif
}

static inline int64_t vec_2i64_min(vec_2i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	return p[0] < p[1] ? p[0] : p[1];
#else
	return v.d[0] < v.d[1] ? v.d[0] : v.d[1];
#endif
}

static inline int64_t vec_2i64_max(vec_2i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	return p[0] > p[1] ? p[0] : p[1];
#else
	return v.d[0] > v.d[1] ? v.d[0] : v.d[1];
#endif
}

/* ── vec_4f32 reductions ─────────────────────────────────────────── */

static inline float vec_4f32_sum(vec_4f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	return p[0] + p[1] + p[2] + p[3];
#else
	return v.d[0] + v.d[1] + v.d[2] + v.d[3];
#endif
}

static inline float vec_4f32_min(vec_4f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float m = p[0];
	if (p[1] < m) m = p[1];
	if (p[2] < m) m = p[2];
	if (p[3] < m) m = p[3];
	return m;
#else
	float m = v.d[0];
	if (v.d[1] < m) m = v.d[1];
	if (v.d[2] < m) m = v.d[2];
	if (v.d[3] < m) m = v.d[3];
	return m;
#endif
}

static inline float vec_4f32_max(vec_4f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float m = p[0];
	if (p[1] > m) m = p[1];
	if (p[2] > m) m = p[2];
	if (p[3] > m) m = p[3];
	return m;
#else
	float m = v.d[0];
	if (v.d[1] > m) m = v.d[1];
	if (v.d[2] > m) m = v.d[2];
	if (v.d[3] > m) m = v.d[3];
	return m;
#endif
}

/* ── vec_2f64 reductions ─────────────────────────────────────────── */

static inline double vec_2f64_sum(vec_2f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	return p[0] + p[1];
#else
	return v.d[0] + v.d[1];
#endif
}

static inline double vec_2f64_min(vec_2f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	return p[0] < p[1] ? p[0] : p[1];
#else
	return v.d[0] < v.d[1] ? v.d[0] : v.d[1];
#endif
}

static inline double vec_2f64_max(vec_2f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	return p[0] > p[1] ? p[0] : p[1];
#else
	return v.d[0] > v.d[1] ? v.d[0] : v.d[1];
#endif
}

/* ── vec_32i8 reductions ─────────────────────────────────────────── */

static inline int32_t vec_32i8_sum(vec_32i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 32; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 32; i++) s += v.d[i];
	return s;
#endif
}

static inline int8_t vec_32i8_min(vec_32i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int8_t m = p[0];
	for (int i = 1; i < 32; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int8_t m = v.d[0];
	for (int i = 1; i < 32; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int8_t vec_32i8_max(vec_32i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int8_t m = p[0];
	for (int i = 1; i < 32; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int8_t m = v.d[0];
	for (int i = 1; i < 32; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_16i16 reductions ────────────────────────────────────────── */

static inline int32_t vec_16i16_sum(vec_16i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 16; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 16; i++) s += v.d[i];
	return s;
#endif
}

static inline int16_t vec_16i16_min(vec_16i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int16_t m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int16_t m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int16_t vec_16i16_max(vec_16i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int16_t m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int16_t m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_8i32 reductions ─────────────────────────────────────────── */

static inline int32_t vec_8i32_sum(vec_8i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 8; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 8; i++) s += v.d[i];
	return s;
#endif
}

static inline int32_t vec_8i32_min(vec_8i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int32_t m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int32_t vec_8i32_max(vec_8i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int32_t m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_4i64 reductions ─────────────────────────────────────────── */

static inline int64_t vec_4i64_sum(vec_4i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	return p[0] + p[1] + p[2] + p[3];
#else
	return v.d[0] + v.d[1] + v.d[2] + v.d[3];
#endif
}

static inline int64_t vec_4i64_min(vec_4i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	int64_t m = p[0];
	if (p[1] < m) m = p[1];
	if (p[2] < m) m = p[2];
	if (p[3] < m) m = p[3];
	return m;
#else
	int64_t m = v.d[0];
	if (v.d[1] < m) m = v.d[1];
	if (v.d[2] < m) m = v.d[2];
	if (v.d[3] < m) m = v.d[3];
	return m;
#endif
}

static inline int64_t vec_4i64_max(vec_4i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	int64_t m = p[0];
	if (p[1] > m) m = p[1];
	if (p[2] > m) m = p[2];
	if (p[3] > m) m = p[3];
	return m;
#else
	int64_t m = v.d[0];
	if (v.d[1] > m) m = v.d[1];
	if (v.d[2] > m) m = v.d[2];
	if (v.d[3] > m) m = v.d[3];
	return m;
#endif
}

/* ── vec_8f32 reductions ─────────────────────────────────────────── */

static inline float vec_8f32_sum(vec_8f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float s = 0.0f;
	for (int i = 0; i < 8; i++) s += p[i];
	return s;
#else
	float s = 0.0f;
	for (int i = 0; i < 8; i++) s += v.d[i];
	return s;
#endif
}

static inline float vec_8f32_min(vec_8f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	float m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline float vec_8f32_max(vec_8f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	float m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_4f64 reductions ─────────────────────────────────────────── */

static inline double vec_4f64_sum(vec_4f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	return p[0] + p[1] + p[2] + p[3];
#else
	return v.d[0] + v.d[1] + v.d[2] + v.d[3];
#endif
}

static inline double vec_4f64_min(vec_4f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	double m = p[0];
	if (p[1] < m) m = p[1];
	if (p[2] < m) m = p[2];
	if (p[3] < m) m = p[3];
	return m;
#else
	double m = v.d[0];
	if (v.d[1] < m) m = v.d[1];
	if (v.d[2] < m) m = v.d[2];
	if (v.d[3] < m) m = v.d[3];
	return m;
#endif
}

static inline double vec_4f64_max(vec_4f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	double m = p[0];
	if (p[1] > m) m = p[1];
	if (p[2] > m) m = p[2];
	if (p[3] > m) m = p[3];
	return m;
#else
	double m = v.d[0];
	if (v.d[1] > m) m = v.d[1];
	if (v.d[2] > m) m = v.d[2];
	if (v.d[3] > m) m = v.d[3];
	return m;
#endif
}

/* ── vec_64i8 reductions ─────────────────────────────────────────── */

static inline int32_t vec_64i8_sum(vec_64i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 64; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 64; i++) s += v.d[i];
	return s;
#endif
}

static inline int8_t vec_64i8_min(vec_64i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int8_t m = p[0];
	for (int i = 1; i < 64; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int8_t m = v.d[0];
	for (int i = 1; i < 64; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int8_t vec_64i8_max(vec_64i8 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int8_t *p = (const int8_t *)&v;
	int8_t m = p[0];
	for (int i = 1; i < 64; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int8_t m = v.d[0];
	for (int i = 1; i < 64; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_32i16 reductions ────────────────────────────────────────── */

static inline int32_t vec_32i16_sum(vec_32i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 32; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 32; i++) s += v.d[i];
	return s;
#endif
}

static inline int16_t vec_32i16_min(vec_32i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int16_t m = p[0];
	for (int i = 1; i < 32; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int16_t m = v.d[0];
	for (int i = 1; i < 32; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int16_t vec_32i16_max(vec_32i16 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int16_t *p = (const int16_t *)&v;
	int16_t m = p[0];
	for (int i = 1; i < 32; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int16_t m = v.d[0];
	for (int i = 1; i < 32; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_16i32 reductions ────────────────────────────────────────── */

static inline int32_t vec_16i32_sum(vec_16i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t s = 0;
	for (int i = 0; i < 16; i++) s += p[i];
	return s;
#else
	int32_t s = 0;
	for (int i = 0; i < 16; i++) s += v.d[i];
	return s;
#endif
}

static inline int32_t vec_16i32_min(vec_16i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int32_t m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int32_t vec_16i32_max(vec_16i32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int32_t *p = (const int32_t *)&v;
	int32_t m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int32_t m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_8i64 reductions ─────────────────────────────────────────── */

static inline int64_t vec_8i64_sum(vec_8i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	int64_t s = 0;
	for (int i = 0; i < 8; i++) s += p[i];
	return s;
#else
	int64_t s = 0;
	for (int i = 0; i < 8; i++) s += v.d[i];
	return s;
#endif
}

static inline int64_t vec_8i64_min(vec_8i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	int64_t m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	int64_t m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline int64_t vec_8i64_max(vec_8i64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const int64_t *p = (const int64_t *)&v;
	int64_t m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	int64_t m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_16f32 reductions ────────────────────────────────────────── */

static inline float vec_16f32_sum(vec_16f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float s = 0.0f;
	for (int i = 0; i < 16; i++) s += p[i];
	return s;
#else
	float s = 0.0f;
	for (int i = 0; i < 16; i++) s += v.d[i];
	return s;
#endif
}

static inline float vec_16f32_min(vec_16f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	float m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline float vec_16f32_max(vec_16f32 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const float *p = (const float *)&v;
	float m = p[0];
	for (int i = 1; i < 16; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	float m = v.d[0];
	for (int i = 1; i < 16; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

/* ── vec_8f64 reductions ─────────────────────────────────────────── */

static inline double vec_8f64_sum(vec_8f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	double s = 0.0;
	for (int i = 0; i < 8; i++) s += p[i];
	return s;
#else
	double s = 0.0;
	for (int i = 0; i < 8; i++) s += v.d[i];
	return s;
#endif
}

static inline double vec_8f64_min(vec_8f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	double m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] < m) m = p[i];
	return m;
#else
	double m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] < m) m = v.d[i];
	return m;
#endif
}

static inline double vec_8f64_max(vec_8f64 v)
{
#if QUASAR_HAS_VECTOR_EXT
	const double *p = (const double *)&v;
	double m = p[0];
	for (int i = 1; i < 8; i++)
		if (p[i] > m) m = p[i];
	return m;
#else
	double m = v.d[0];
	for (int i = 1; i < 8; i++)
		if (v.d[i] > m) m = v.d[i];
	return m;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* QUASAR_STD_SIMD_H */
