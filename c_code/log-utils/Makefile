#CPPFLAGS += -I/usr/local/include
LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O3 -Wall
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast

all: test_logutils 

debug: CXXFLAGS += -DDEBUG -g
debug: all

test_logutils: test_logutils.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f test_logutils

