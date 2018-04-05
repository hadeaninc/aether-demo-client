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

typedef enum {
  EVENT_CURSOR_MOVE = 0,
  EVENT_MOUSE_CLICK = 1,
  EVENT_DEL_AGENT = 2,
} aether_event_type_t;

typedef enum {
  BUTTON_PRESSED  = 0,
  BUTTON_RELEASED = 1,
} aether_button_action_t;

typedef struct __attribute__((packed)) {
  float x;
  float y;
} aether_screen_pos_t;

typedef struct __attribute__((packed)) {
  uint8_t button;
  uint8_t action;
  aether_screen_pos_t position;
} aether_mouse_click_t;

typedef struct __attribute__((packed)) {
  aether_screen_pos_t position;
} aether_cursor_move_t;

typedef struct __attribute__((packed)) {
  uint32_t id;
} aether_del_agent_t;

typedef struct __attribute__((packed)) {
  aether_event_type_t type;
  union {
    aether_mouse_click_t mouse_click;
    aether_cursor_move_t cursor_move;
    aether_del_agent_t del_agent;
  };
} aether_event_t;
