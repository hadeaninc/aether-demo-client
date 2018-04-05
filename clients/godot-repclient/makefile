include ../../makefile.inc

lib/libaether_repclient.so: $(wildcard *.cc) $(REP_CLIENT_LIB)
	$(CXX) $^ -fPIC -shared -o $@ $(CXXFLAGS) -I$(COMMON_INC_DIR) -I$(REP_INC_DIR)
