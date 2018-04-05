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
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define CHECK_NAN_2D(v) {assert(!isnan((v).x) && !isnan((v).y));}
#define CHECK_INF_2D(v) {assert(!isinf((v).x) && !isinf((v).y));}
#define CHECK_NAN_INF_2D(v) {CHECK_NAN_2D(v);CHECK_INF_2D(v);}

#define CHECK_NAN_3D(v) {assert(!isnan((v).x) && !isnan((v).y) && !isnan((v).z));}
#define CHECK_INF_3D(v) {assert(!isinf((v).x) && !isinf((v).y) && !isinf((v).z));}
#define CHECK_NAN_INF_3D(v) {CHECK_NAN_3D(v);CHECK_INF_3D(v);}

typedef struct vec2f {
    float x, y;
} vec2f;

static vec2f vec2f_new(float x, float y) {
    vec2f v;
    v.x = x;
    v.y = y;
    return v;
}

typedef struct vec3f {
    float x, y, z;
} vec3f;

static void vec2f_add_scaled(vec2f *y, float alpha, const vec2f *x) {
    y->x += x->x * alpha;
    y->y += x->y * alpha;
}

static void vec3f_add_scaled(vec3f *y, float alpha, const vec3f *x) {
    y->x += x->x * alpha;
    y->y += x->y * alpha;
    y->z += x->z * alpha;
}

static void vec2f_scale(float alpha, vec2f *x) {
    x->x *= alpha;
    x->y *= alpha;
}

static void vec3f_scale(float alpha, vec3f *x) {
    x->x *= alpha;
    x->y *= alpha;
    x->z *= alpha;
}

static float vec2f_dot(const vec2f *x, const vec2f *y) {
    return x->x * y->x + x->y * y->y;
}

static float vec3f_dot(const vec3f *x, const vec3f *y) {
    return x->x * y->x + x->y * y->y + x->z * y->z;
}

static bool vec2f_in_range(const vec2f *x, const vec2f *y, float cutoff) {
  vec2f offset = *x;
  vec2f_add_scaled(&offset, -1.0, y);
  const double distance_sq = vec2f_dot(&offset, &offset);
  return distance_sq <= (cutoff * cutoff);
}

static bool vec3f_in_range(const vec3f *x, const vec3f *y, float cutoff) {
  vec3f offset = *x;
  vec3f_add_scaled(&offset, -1.0, y);
  const double distance_sq = vec3f_dot(&offset, &offset);
  return distance_sq <= (cutoff * cutoff);
}

static float vec3f_norm2(const vec3f *x) {
    return sqrtf(vec3f_dot(x,x));
}

static float vec2f_norm2(const vec2f *x) {
    return sqrtf(vec2f_dot(x,x));
}

static void vec2f_normalize(vec2f *x) {
    const float norm = vec2f_norm2(x);
    assert(norm != 0.0f);
    vec2f_scale(1.0f / norm, x);
}

static void vec3f_normalize(vec3f *x) {
    const float norm = vec3f_norm2(x);
    assert(norm != 0.0f);
    vec3f_scale(1.0f / norm, x);
}

//Distance between position vectors.
static float vec2f_dist(const vec2f *p0, const vec2f *p1) {
    vec2f delta;
    delta.x = p1->x - p0->x;
    delta.y = p1->y - p0->y;
    return vec2f_norm2(&delta);
}

static float vec3f_dist(const vec3f *p0, const vec3f *p1) {
    vec3f delta;
    delta.x = p1->x - p0->x;
    delta.y = p1->y - p0->y;
    delta.z = p1->z - p0->z;
    return vec3f_norm2(&delta);
}
