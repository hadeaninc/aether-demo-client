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

#include <argp.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

struct arguments {
    char *host;
    char *port;
    uint64_t workers;
    uint64_t ticks;
    uint64_t tickrate;
    uint64_t cell_level;
    bool realtime;
};
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = (struct arguments*)state->input;
    switch (key) {
        case 'w':
            arguments->workers = atoi(arg);
            break;
        case 't':
            arguments->ticks = atoi(arg);
            break;
        case 'r':
            arguments->tickrate = atoi(arg);
            break;
        case 's':
            arguments->cell_level = atoi(arg);
            break;
        case 'b':
            arguments->realtime = false;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num == 0) {
                arguments->host = arg;
            } else if (state->arg_num == 1) {
                arguments->port = arg;
            } else if (state->arg_num >= 2) {
                argp_usage(state);
            }
            break;
        case ARGP_KEY_END:
            if (state->arg_num < 2) {
                argp_usage(state);
            }
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

void argument_parse(int argc, char **argv, struct arguments *arguments) {
    static char doc[] = "Aether Engine Demo\v"
        "TODO fill in docs\n\n"
        "Contact <patrick@hadean.com>";
    static char args_doc[] = "HOST PORT";
    static struct argp_option options[] = {
        {"workers",     'w', "WORKERS",     0, "Number of workers to spawn"},
        {"ticks",       't', "TICKS",       0, "Number of ticks to execute"},
        {"tickrate",    'r', "TICKRATE",    0, "Number of ticks to execute per second"},
        {"cell-level",  's', "CELL_LEVEL",  0, "Initial cell level to spawn workers"},
        {"batch",       'b', 0,             0, "Don't execute ticks in realtime"},
        {0}
    };
    static struct argp argp = {options, parse_opt, args_doc, doc};
    argp_parse(&argp, argc, argv, 0, 0, arguments);
}
