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

#ifdef __cplusplus
extern "C" {
#endif

// This needs to make sense when zeroed
struct repclient_msgbuf {
    size_t pos;
    size_t len; // actually 'endpos'
    size_t cap;
    uint8_t *buf;
};

enum REPCLIENT_MODE {
    live, record, playback,
};

struct repclient_state {
    uint64_t num_conns;
    struct repclient_msgbuf *msgbufs;
    int sockfd;
    int recfd;
    enum REPCLIENT_MODE mode;
    struct timespec start_time;
    float current_packet_time;
    struct repclient_msgbuf playback_buf;

    struct __attribute__((packed)) multiplexer_header {
        uint64_t id;
        uint64_t len;
    } cur_header;
    size_t cur_header_got;
    struct repclient_msgbuf outbuf;
};

struct repclient_state repclient_init(const char *host, const char *port);
struct repclient_state repclient_init_record(const char *host, const char *port, const char *path);
struct repclient_state repclient_init_playback(const char *path);
void repclient_destroy(struct repclient_state *s);
void *repclient_tick(struct repclient_state *s, uint64_t *worker_id, size_t *msg_size);
void repclient_send_message(struct repclient_state *s, const void *data, size_t length);

#ifdef __cplusplus
}
#endif
