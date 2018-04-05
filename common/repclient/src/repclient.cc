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

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <tcp.hh>
#include "repclient.hh"
#include <timer.hh>

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))

#define MIN_SLACK_BYTES 128
#define MIN_BUF_SIZE 1024

static void msgbuf_reserve(struct repclient_msgbuf *const msgbuf, const size_t bytes) {
    if (msgbuf->cap < bytes) {
        msgbuf->cap = MAX(msgbuf->cap * 2, MAX(bytes, MIN_BUF_SIZE));
        msgbuf->buf = (uint8_t *) realloc(msgbuf->buf, msgbuf->cap);
        assert(msgbuf->buf);
    }
}

static void *consume_message(struct repclient_msgbuf *msgbuf, size_t *length) {
    typedef uint32_t prefix_t;
    size_t offset = msgbuf->pos;
    while(true) {
        const size_t remaining = msgbuf->len - offset;
        if (remaining < sizeof(prefix_t)) {
            return NULL;
        }

        const prefix_t segment_size = *((prefix_t*)(msgbuf->buf + offset));
        if (segment_size == 0) {
            break;
        } else {
            offset += segment_size + sizeof(prefix_t);
            if (offset > msgbuf->len)
                return NULL;
        }
    }

    size_t header_pos = msgbuf->pos;
    size_t dst_pos = msgbuf->pos;
    while(true) {
        const prefix_t segment_size = *((prefix_t*)(msgbuf->buf + header_pos));
        if (segment_size == 0) {
            void *const msg = msgbuf->buf + msgbuf->pos;
            *length = dst_pos - msgbuf->pos;
            msgbuf->pos = header_pos + sizeof(prefix_t);
            return msg;
        }
        memmove(msgbuf->buf + dst_pos, msgbuf->buf + header_pos + sizeof(prefix_t), segment_size);
        dst_pos += segment_size;
        header_pos += segment_size + sizeof(prefix_t);
    }
}

struct repclient_state repclient_init(const char *host, const char *port) {
    struct repclient_state ret = {0};
    ret.mode = live;
    ret.num_conns = 16;
    ret.msgbufs = (repclient_msgbuf *) calloc(ret.num_conns, sizeof(struct repclient_msgbuf));
    assert(ret.msgbufs);
    ret.sockfd = connect_to_host_port_with_timeout(host, port);
    assert(ret.sockfd >= 0);

    const int flags = fcntl(ret.sockfd, F_GETFL, 0);
    assert(flags != -1);
    const int res = fcntl(ret.sockfd, F_SETFL, flags | O_NONBLOCK);
    assert(res == 0);
    return ret;
}
struct repclient_state repclient_init_record(const char *host, const char *port, const char *path) {
    struct repclient_state ret = repclient_init(host, port);
    ret.mode = record;
    ret.start_time = {0, 0};
    ret.recfd = open(path ? path : "aether_recording.dump", O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (ret.recfd == -1) {
        perror("open");
        exit(1);
    }
    return ret;
}
struct repclient_state repclient_init_playback(const char *path) {
    struct repclient_state ret = {0};
    ret.mode = playback;
    ret.start_time = timer_get();
    ret.recfd = open(path ? path : "aether_recording.dump", O_RDONLY);
    if (ret.recfd == -1) {
        perror("open");
        exit(1);
    }
    const int flags = fcntl(ret.recfd, F_GETFL, 0);
    assert(flags != -1);
    const int res = fcntl(ret.recfd, F_SETFL, flags | O_NONBLOCK);
    assert(res == 0);
    ret.playback_buf.cap = 4096;
    ret.playback_buf.buf = (uint8_t *) malloc(ret.playback_buf.cap * sizeof(*ret.playback_buf.buf));
    assert(ret.playback_buf.buf);
    return ret;
}

void repclient_destroy(struct repclient_state *s) {
    switch (s->mode) {
    case live: {
        free(s->msgbufs);
        free(s->outbuf.buf);
        // TODO: free the actual bufs
        close(s->sockfd);
    } break;
    case record: {
        free(s->msgbufs);
        free(s->outbuf.buf);
        // TODO: free the actual bufs
        close(s->sockfd);
        close(s->recfd);
    } break;
    case playback: {
        if (s->recfd != -1) {
            close(s->recfd);
        }
        free(s->playback_buf.buf);
    } break;
    default:
        abort();
    }
}

void repclient_send_message(struct repclient_state *s, const void *data, size_t length) {
  const size_t total_length = sizeof(uint32_t) + length;
  const size_t buf_length_new = s->outbuf.len + total_length;
  msgbuf_reserve(&s->outbuf, buf_length_new);
  assert(length <= UINT32_MAX);
  const uint32_t length_u32 = length;
  memcpy(s->outbuf.buf + s->outbuf.len, &length_u32, sizeof(uint32_t));
  s->outbuf.len += sizeof(uint32_t);
  memcpy(s->outbuf.buf + s->outbuf.len, data, length);
  s->outbuf.len += length;
  assert(s->outbuf.len == buf_length_new);
}

// Supports both file fds and socket fds, both blocking and nonblocking
static size_t try_fill_buf(int fd, void *buf, int wanted) {
    const ssize_t n = read(fd, buf, wanted);
    if (n > 0) {
        return n;
    } else if (n == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
        return 0;
    } else {
        perror("invalid read return");
        exit(EXIT_FAILURE);
    }
}

static void try_drain_interaction(struct repclient_state *s) {
    if (s->outbuf.len == 0) {
      return;
    }
    const ssize_t written = write(s->sockfd, s->outbuf.buf, s->outbuf.len);
    if (written < 0) {
      perror("invalid write return");
      exit(EXIT_FAILURE);
    } else if (written == 0) {
      assert(errno == EAGAIN || errno == EWOULDBLOCK);
    } else {
      s->outbuf.len -= written;
      memmove(s->outbuf.buf, s->outbuf.buf + written, s->outbuf.len);
    }
}

void write_all(int fd, void* data, size_t size) {
    size_t writebytes;
    ssize_t n;
    for (writebytes = 0; writebytes < size; writebytes += n) {
        n = write(fd, (char*)data + writebytes, size - writebytes);
        if (n == -1) {
            perror("write");
            exit(1);
        }
    }
    assert(writebytes == size);
}

void *__repclient_tick(struct repclient_state *s, uint64_t *worker_id, size_t *length);
// Although this code exits as soon as a read is too short, it should be asking the OS
// how many bytes are available and use this
void *repclient_tick(struct repclient_state *s, uint64_t *worker_id, size_t *length) {
    switch (s->mode) {
        case live: {
            return __repclient_tick(s, worker_id, length);
        } break;
        case record: {
            char* buf = (char *) __repclient_tick(s, worker_id, length);
            if (buf) {
                if (!s->start_time.tv_sec && !s->start_time.tv_nsec)
                    s->start_time = timer_get();
                s->current_packet_time = timer_diff(timer_get(), s->start_time);
                write_all(s->recfd, worker_id, sizeof(*worker_id));
                write_all(s->recfd, &s->current_packet_time, sizeof(s->current_packet_time));
                write_all(s->recfd, length, sizeof(*length));
                write_all(s->recfd, buf, *length);
            }
            return buf;
        } break;
        case playback: {
            // Note that the playback file could be truncated at any point, and we'd really like
            // to just return NULLs once we reach the 'end', even if it's not been correctly closed
            if (s->recfd == -1) {
                return NULL;
            }
            const size_t headersize = sizeof(*worker_id) + sizeof(s->current_packet_time) + sizeof(*length);
            struct repclient_msgbuf *playbuf = &s->playback_buf;
            msgbuf_reserve(playbuf, headersize);

            while (playbuf->len < headersize) {
                const size_t n = try_fill_buf(s->recfd, playbuf->buf + playbuf->len, headersize - playbuf->len);
                playbuf->len += n;
                if (n == 0) {
                    return NULL;
                }
            }
            memcpy(worker_id,               playbuf->buf, sizeof(*worker_id));
            memcpy(&s->current_packet_time, playbuf->buf + sizeof(*worker_id), sizeof(s->current_packet_time));
            memcpy(length,                  playbuf->buf + sizeof(*worker_id) + sizeof(s->current_packet_time), sizeof(*length));

            msgbuf_reserve(playbuf, headersize + *length);
            while (playbuf->len != *length + headersize) {
                const size_t n = try_fill_buf(s->recfd, playbuf->buf + playbuf->len, headersize + *length - playbuf->len);
                playbuf->len += n;
                if (n == 0) {
                    return NULL;
                }
            }
            if (timer_diff(timer_get(), s->start_time) < s->current_packet_time) {
                return NULL;
            } else {
                playbuf->len = 0;
                return playbuf->buf + headersize;
            }
        } break;
        default:
            abort();
    }
}
void *__repclient_tick(struct repclient_state *s, uint64_t *worker_id, size_t *length) {
    try_drain_interaction(s);
    while (true) {
        // try to recv the multiplexer header
        if (s->cur_header_got < sizeof(s->cur_header)) {
            const ssize_t wanted = sizeof(s->cur_header) - s->cur_header_got;
            const size_t n = try_fill_buf(s->sockfd, (uint8_t *)&s->cur_header + s->cur_header_got, wanted);
            s->cur_header_got += n;
            if (s->cur_header_got != sizeof(s->cur_header)) {
                return NULL;
            }
        }

        // Add new connection if necessary
        uint64_t wid = s->cur_header.id;
        if (wid + 1 > s->num_conns) {
            uint64_t old_size = s->num_conns;
            while (wid + 1 > s->num_conns)
                s->num_conns *= 2;
            s->msgbufs = (repclient_msgbuf *) realloc(s->msgbufs, s->num_conns * sizeof(struct repclient_msgbuf));
            assert(s->msgbufs);
            for (uint64_t i = old_size; i < s->num_conns; i++) {
                s->msgbufs[i] = repclient_msgbuf();
            }
        }
        assert(wid < s->num_conns);

        // Fill the client buffer
        struct repclient_msgbuf *msgbuf = &s->msgbufs[wid];
        if (s->cur_header.len) {
            // shift the buf back to the beginning since we're going to be receiving network data
            if (msgbuf->pos > 0) {
                memmove(msgbuf->buf, msgbuf->buf + msgbuf->pos, msgbuf->len - msgbuf->pos);
                msgbuf->len -= msgbuf->pos;
                msgbuf->pos = 0;
            }
            // Initialise buf for first time, or give enough slack to avoid reads of tiny numbers of bytes
            if (msgbuf->cap - msgbuf->len < MIN_SLACK_BYTES) {
                msgbuf_reserve(msgbuf, MAX(MIN_BUF_SIZE, msgbuf->cap * 2));
            }
            const int wanted = MIN(s->cur_header.len, msgbuf->cap - msgbuf->len);
            const size_t n = try_fill_buf(s->sockfd, msgbuf->buf + msgbuf->len, wanted);
            s->cur_header.len -= n;
            msgbuf->len += n;
        }

        // Return a message if present
        void *msg = consume_message(msgbuf, length);
        if (msg != NULL) {
            *worker_id = wid;
            return msg;
        } else if (s->cur_header.len > 0) {
            return NULL;
        } else {
            // no more messages to drain and no more multiplexer message to recv, prep for next multiplexer header
            s->cur_header_got = 0;
        }
    }
}
