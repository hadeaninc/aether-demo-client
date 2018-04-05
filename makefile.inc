HERE := $(abspath $(lastword $(MAKEFILE_LIST))/..)

COMMON_FLAGS := -lm -MMD -O3 -g -march=native -fPIC \
    -Wall -Wextra -Wpedantic \
    -std=c++11 \
    -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-function

COMMON_INC_DIR := $(HERE)/common/src
CXXFLAGS := $(COMMON_FLAGS) $(CXXFLAGS) -I$(COMMON_INC_DIR)
BINFLAGS := -lpthread -lprocess_c -ldl -lbsd
export BINFLAGS CXXFLAGS

REP_INC_DIR := $(HERE)/common/repclient/src
export REP_INC_DIR

REP_CLIENT_LIB := $(HERE)/common/repclient/obj/librepclient.a
export REP_CLIENT_LIB
