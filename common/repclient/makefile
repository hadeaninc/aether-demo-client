include ../../makefile.inc

all: obj/librepclient.a

obj/librepclient.a: obj/repclient.o
	@mkdir -p obj
	ar rcs $@ $^

obj/%.o: src/%.cc
	@mkdir -p obj
	$(CXX) $< -c -o $@ $(CXXFLAGS) -I$(COMMON_INC_DIR) -I$(REP_INC_DIR)

.PHONY: all

-include obj/*.d
