#ifndef fodi_math_h
#define fodi_math_h

#include <math.h>
#include <stdint.h>

// A union to let us reinterpret a double as raw bits and back.
typedef union
{
  uint64_t bits64;
  uint32_t bits32[2];
  double num;
} FodiDoubleBits;

#define FODI_DOUBLE_QNAN_POS_MIN_BITS (UINT64_C(0x7FF8000000000000))
#define FODI_DOUBLE_QNAN_POS_MAX_BITS (UINT64_C(0x7FFFFFFFFFFFFFFF))

#define FODI_DOUBLE_NAN (fodiDoubleFromBits(FODI_DOUBLE_QNAN_POS_MIN_BITS))

static inline double fodiDoubleFromBits(uint64_t bits)
{
  FodiDoubleBits data;
  data.bits64 = bits;
  return data.num;
}

static inline uint64_t fodiDoubleToBits(double num)
{
  FodiDoubleBits data;
  data.num = num;
  return data.bits64;
}

#endif
