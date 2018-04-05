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
#include <deque>
#include <ctime>
#include <cassert>
#include <algorithm>
#include <stdio.h>

template<typename T>
class statistics {
    static constexpr double interval = 0.5;
    typedef T sample_t;
    double history_length;
    std::deque<T> previous;
    timespec start_time;
    uint64_t last_tick;

public:
    statistics(const double _num_seconds) : history_length(_num_seconds), last_tick(0) {
        assert(history_length > 0.0);
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        previous.push_front(sample_t());
    }

    statistics &operator+=(const sample_t &sample) {
        timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        const double delta =
            static_cast<double>(current_time.tv_sec - start_time.tv_sec) +
            static_cast<double>(current_time.tv_nsec - start_time.tv_nsec) / 1e9;
        const uint64_t tick = static_cast<uint64_t>(delta / interval);

        while (previous.empty() || last_tick < tick) {
            previous.push_front(sample_t());
            ++last_tick;
        }

        previous.front() += sample;
        while (previous.size() > (history_length / interval)) {
            previous.pop_back();
        }
        return *this;
    }

    sample_t get_sample_total(const double duration) {
        assert(duration <= this->history_length);
        size_t num_added = 0;
        double current_duration = 0.0;

        sample_t result;
        while (num_added < previous.size() && current_duration < duration) {
            current_duration += interval;
            result += previous[num_added];
            ++num_added;
        }

        return result;
    }

    sample_t get_sample_per_second(const double num_seconds) {
        sample_t stat = get_sample_total(num_seconds);
        stat /= std::min(num_seconds, previous.size() * interval);
        return stat;
    }
};
