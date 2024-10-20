#CPPFLAGS +=
LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O0 -g -Wall -Wsign-compare -Wconversion -Wpedantic
CXX=clang++-10

BINNAME = test-bounded-fifo

all: $(BINNAME)

$(BINNAME): test-bounded-fifo.cpp bounded-fifo.hpp helpers.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

#debug:  main.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g main.cpp -o $(BINNAME) $(LDFLAGS)

#static: main.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static main.cpp -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME)

