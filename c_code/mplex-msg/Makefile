#CPPFLAGS += -I/usr/local/include
LDFLAGS += -lpcap -ltins -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O3 -Wall
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast

all: test_MsgFramev0 test_MsgGroupv0

debug: CXXFLAGS += -DDEBUG -g
debug: all

test_MsgFramev0: test_MsgFramev0.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

test_MsgGroupv0: test_MsgGroupv0.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f test_MsgFramev0 test_MsgGroupv0

