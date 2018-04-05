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
#include <time.h>

static struct timespec timer_get() {
    struct timespec time_struct;
    clock_gettime(CLOCK_REALTIME, &time_struct);
    return time_struct;
}
static float timer_diff(struct timespec stop_time, struct timespec start_time) {
    return
        (float)((int64_t)stop_time.tv_sec - (int64_t)start_time.tv_sec) +
        (float)((int64_t)stop_time.tv_nsec - (int64_t)start_time.tv_nsec) / 1e9;
}
static struct timespec timer_add(struct timespec start_time, uint64_t nanos) {
    start_time.tv_nsec += nanos;
    start_time.tv_sec += start_time.tv_nsec / 1e9;
    start_time.tv_nsec %= 1000000000ULL;
    return start_time;
}
static struct timespec timer_sub(struct timespec *a, struct timespec *b) {
    struct timespec result;
    if (a->tv_nsec < b->tv_nsec) {
        result.tv_sec = a->tv_sec - b->tv_sec - 1;
        result.tv_nsec = a->tv_nsec - b->tv_nsec + 1000000000;
    } else {
        result.tv_sec = a->tv_sec - b->tv_sec;
        result.tv_nsec = a->tv_nsec - b->tv_nsec;
    }
    return result;
}
static bool timer_gt(struct timespec *a, struct timespec *b) {
    return a->tv_sec > b->tv_sec || (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec);
}

static void timer_sleep_until(struct timespec target_time) {
#ifdef TIMER_ABSTIME
    while (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target_time, &target_time) != 0);
#else
    struct timespec current_time = timer_get();
    if (!timer_gt(&current_time, &target_time)) {
        struct timespec rel_time = timer_sub(&target_time, &current_time);
        while (nanosleep(&rel_time, &rel_time) != 0);
    }
#endif
}
