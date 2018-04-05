/*
   Copyright 2018 Hadean Supercomputing Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <stdint.h>
#include <assert.h>
#include <immintrin.h>

#include <vector.hh>

extern "C" {

struct morton2d_32 {
  uint64_t value;
};

/** \brief Morton dimension masks.
These bitwise masks define the bit pattern for each dimension.
*/
static const uint64_t __morton_2_x_mask = 0x5555555555555555;
static const uint64_t __morton_2_y_mask = 0xaaaaaaaaaaaaaaaa;

static const uint64_t __morton_3_x_mask = 0x1249249249249249;
static const uint64_t __morton_3_y_mask = 0x2492492492492492;
static const uint64_t __morton_3_z_mask = 0x4924924924924924;

static uint32_t compact_bits_2(uint64_t x) {
    x &= 0x5555555555555555;
    x = (x ^ (x >>  1))  & 0x3333333333333333;
    x = (x ^ (x >>  2))  & 0x0f0f0f0f0f0f0f0f;
    x = (x ^ (x >>  4))  & 0x00ff00ff00ff00ff;
    x = (x ^ (x >>  8))  & 0x0000ffff0000ffff;
    x = (x ^ (x >>  16)) & 0x00000000ffffffff;
    return (uint32_t) x;
}

static uint64_t expand_bits_2(const uint32_t x32) {
    uint64_t x = x32;
    x = (x ^ (x << 16)) & 0x0000ffff0000ffff;
    x = (x ^ (x << 8))  & 0x00ff00ff00ff00ff;
    x = (x ^ (x << 4))  & 0x0f0f0f0f0f0f0f0f;
    x = (x ^ (x << 2))  & 0x3333333333333333;
    x = (x ^ (x << 1))  & 0x5555555555555555;
    return x;
}

static void morton2d_32_encode(struct morton2d_32 *code, uint32_t x, uint32_t y) {
#ifdef __AVX2__
    code->value =
        _pdep_u64(x, __morton_2_x_mask) |
        _pdep_u64(y, __morton_2_y_mask);
#else
    code->value = (expand_bits_2(y) << 1) | expand_bits_2(x);
#endif
}

static void morton2d_32_decode(const struct morton2d_32 *code, uint32_t *x, uint32_t *y) {
#ifdef __AVX2__
    *x = (uint32_t) _pext_u64(code->value, __morton_2_x_mask);
    *y = (uint32_t) _pext_u64(code->value, __morton_2_y_mask);
#else
    *x = compact_bits_2(code->value);
    *y = compact_bits_2(code->value >> 1);
#endif
}

static void morton2d_32_add(struct morton2d_32 *left, const struct morton2d_32 *right) {
    uint64_t x = (left->value | ~__morton_2_x_mask) + (right->value & __morton_2_x_mask);
    uint64_t y = (left->value | ~__morton_2_y_mask) + (right->value & __morton_2_y_mask);
    left->value = (x & __morton_2_x_mask) | (y & __morton_2_y_mask);
}

static void morton2d_32_sub(struct morton2d_32 *left, const struct morton2d_32 *right) {
    uint64_t x = (left->value & __morton_2_x_mask) - (right->value & __morton_2_x_mask);
    uint64_t y = (left->value & __morton_2_y_mask) - (right->value & __morton_2_y_mask);
    left->value = (x & __morton_2_x_mask) | (y & __morton_2_y_mask);
}

struct morton3d_21 {
  uint64_t value;
};

static void morton3d_21_encode(struct morton3d_21 *code, uint32_t x, uint32_t y, uint32_t z) {
#ifdef __AVX2__
    code->value =
        _pdep_u64(x, __morton_3_x_mask) |
        _pdep_u64(y, __morton_3_y_mask) |
        _pdep_u64(z, __morton_3_z_mask);
#else
    assert(false && "morton_3_encode not implemented for non-AVX2 capable processors.");
#endif
}

static void morton3d_21_decode(const struct morton3d_21 *code, uint32_t *x, uint32_t *y, uint32_t *z) {
#ifdef __AVX2__
    *x = _pext_u64(code->value, __morton_3_x_mask);
    *y = _pext_u64(code->value, __morton_3_y_mask);
    *z = _pext_u64(code->value, __morton_3_z_mask);
#else
    assert(false && "morton_3_decode not implemented for non-AVX2 capable processors.");
#endif
}

static void morton3d_21_add(struct morton3d_21 *left, const struct morton3d_21 *right) {
    uint64_t x = (left->value | ~__morton_3_x_mask) + (right->value & __morton_3_x_mask);
    uint64_t y = (left->value | ~__morton_3_y_mask) + (right->value & __morton_3_y_mask);
    uint64_t z = (left->value | ~__morton_3_z_mask) + (right->value & __morton_3_z_mask);
    left->value = (x & __morton_3_x_mask) | (y & __morton_3_y_mask) | (z & __morton_3_z_mask);
}

static void morton3d_21_sub(struct morton3d_21 *left, const struct morton3d_21 *right) {
    uint64_t x = (left->value & __morton_3_x_mask) - (right->value & __morton_3_x_mask);
    uint64_t y = (left->value & __morton_3_y_mask) - (right->value & __morton_3_y_mask);
    uint64_t z = (left->value & __morton_3_z_mask) - (right->value & __morton_3_z_mask);
    left->value = (x & __morton_3_x_mask) | (y & __morton_3_y_mask) | (z & __morton_3_z_mask);
}

static vec2f morton_2_decode(const uint64_t m) {
    morton2d_32 code = { m };
    uint32_t x, y;
    morton2d_32_decode(&code, &x, &y);
    vec2f result;
    result.x = (int32_t) x;
    result.y = (int32_t) y;
    return result;
}

static uint64_t morton_2_encode(vec2f v) {
    const uint32_t x = (uint32_t) ((int32_t) floorf(v.x));
    const uint32_t y = (uint32_t) ((int32_t) floorf(v.y));
    morton2d_32 code;
    morton2d_32_encode(&code, x, y);
    return code.value;
}

static uint64_t morton_2_add(const uint64_t _m, const uint64_t _i) {
    morton2d_32 m = { _m };
    const morton2d_32 i = { _i };
    morton2d_32_add(&m, &i);
    return m.value;
}

static uint64_t morton_2_sub(const uint64_t _m, const uint64_t _i) {
    morton2d_32 m = { _m };
    const morton2d_32 i = { _i };
    morton2d_32_sub(&m, &i);
    return m.value;
}

static vec3f morton_3_decode(const uint64_t m) {
    morton3d_21 code = { m };
    uint32_t x, y, z;
    morton3d_21_decode(&code, &x, &y, &z);
    vec3f result;
    result.x = x;
    result.y = y;
    result.z = z;
    return result;
}

static uint64_t morton_3_encode(vec3f v) {
    const uint32_t x = (uint32_t) ((int32_t) floorf(v.x));
    const uint32_t y = (uint32_t) ((int32_t) floorf(v.y));
    const uint32_t z = (uint32_t) ((int32_t) floorf(v.z));
    morton3d_21 code;
    morton3d_21_encode(&code, x, y, z);
    return code.value;
}

static uint64_t morton_3_add(const uint64_t _m, const uint64_t _i) {
    morton3d_21 m = { _m };
    const morton3d_21 i = { _i };
    morton3d_21_add(&m, &i);
    return m.value;
}

static uint64_t morton_3_sub(const uint64_t _m, const uint64_t _i) {
    morton3d_21 m = { _m };
    const morton3d_21 i = { _i };
    morton3d_21_sub(&m, &i);
    return m.value;
}

}
