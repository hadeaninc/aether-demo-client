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

#include <vector.hh>
#include <morton.hh>
#include <colour.hh>


enum cell_status {
    CELL_ALIVE = 0,
    CELL_DYING = 1
};

//The coordinate of an octree cell.
struct __attribute__((packed)) net_tree_cell {
    uint64_t code;
    uint64_t level;
};

struct __attribute__((packed)) net_point {
    uint32_t net_encoded_position;
    uint32_t net_encoded_color;
    uint32_t id;
};

struct __attribute__((packed)) client_stats {
  uint64_t num_agents;
  uint64_t num_agents_ghost;
};

struct __attribute__((packed)) client_message {
    struct net_tree_cell cell;
    uint64_t num_points;
    uint64_t cell_status;
    struct client_stats stats;
    struct net_point points[];
};

static uint8_t float_to_u8(float v) {
    v = v < 0.0 ? 0.0 : v;
    v = v > 1.0 ? 1.0 : v;
    return (uint8_t) round(v * 255.0);
}

static float u8_to_float(uint8_t v) {
    return  (v * 1.0) / 255.0;
}

static uint32_t net_encode_color(struct colour c) {
    uint32_t result = 0;
    result += ((uint32_t) float_to_u8(c.r)) << 16;
    result += ((uint32_t) float_to_u8(c.g)) << 8;
    result += ((uint32_t) float_to_u8(c.b));
    return result;
}

static struct colour net_decode_color(uint32_t c) {
    struct colour result;
    result.r = u8_to_float((c >> 16) & 255);
    result.g = u8_to_float((c >> 8) & 255);
    result.b = u8_to_float(c & 255);
    return result;
}

static uint32_t net_encode_position_2f(vec2f v, struct net_tree_cell cell) {
    uint64_t size = 1 << cell.level;
    vec2f c = morton_2_decode(cell.code);
    v.x -= c.x;
    v.y -= c.y;
    v.x /= size;
    v.y /= size;
    v.x *= (1 << 10);
    v.y *= (1 << 10);
    return
        (((uint32_t)v.x) <<  0) |
        (((uint32_t)v.y) << 10);
}

static uint32_t net_encode_position_3f(vec3f v, struct net_tree_cell cell) {
    uint64_t size = 1 << cell.level;
    vec3f c = morton_3_decode(cell.code);
    v.x -= c.x;
    v.y -= c.y;
    v.z -= c.z;
    v.x /= size;
    v.y /= size;
    v.z /= size;
    v.x *= (1 << 10);
    v.y *= (1 << 10);
    v.z *= (1 << 10);
    return
        (((uint32_t)v.x) <<  0) |
        (((uint32_t)v.y) << 10) |
        (((uint32_t)v.z) << 20);
}

static vec2f net_decode_position_2f(uint32_t p, struct net_tree_cell cell) {
    uint64_t size = 1 << cell.level;
    vec2f c = morton_2_decode(cell.code);
    vec2f v = {0};
    v.x = (p >>  0) & ((1 << 10) - 1);
    v.y = (p >> 10) & ((1 << 10) - 1);
    v.x /= (1 << 10);
    v.y /= (1 << 10);
    v.x *= size;
    v.y *= size;
    v.x += c.x;
    v.y += c.y;
    return v;
}

static vec3f net_decode_position_3f(uint32_t p, struct net_tree_cell cell) {
    uint64_t size = 1 << cell.level;
    vec3f c = morton_3_decode(cell.code);
    vec3f v = {0};
    v.x = (p >>  0) & ((1 << 10) - 1);
    v.y = (p >> 10) & ((1 << 10) - 1);
    v.z = (p >> 20) & ((1 << 10) - 1);
    v.x /= (1 << 10);
    v.y /= (1 << 10);
    v.z /= (1 << 10);
    v.x *= size;
    v.y *= size;
    v.z *= size;
    v.x += c.x;
    v.y += c.y;
    v.z += c.z;
    return v;
}
