include ../../makefile.inc

all: bin/client

CXXFLAGS += -I../common/src/

bin/client: obj/client.o $(REP_CLIENT_LIB) $(SIM_CLIENT_LIB)
	@mkdir -p bin
	$(CXX) $^ -o $@ -lm -lGLEW -lGL -lglfw -L/usr/lib/x86_64-linux-gnu

obj/%.o: src/%.cc
	@mkdir -p obj
	$(CXX) $< -c -o $@ $(CXXFLAGS) -I$(COMMON_INC_DIR) -I$(REP_INC_DIR)

.PHONY: all

-include bin/*.d obj/*.d
