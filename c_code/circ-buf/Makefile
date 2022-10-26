#CPPFLAGS +=
LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O0 -g -Wall

BINNAME = test-circ-buf

all: $(BINNAME)

$(BINNAME): test-circ-buf.cpp circ-buf.hpp helpers.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

#debug:  main.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g main.cpp -o $(BINNAME) $(LDFLAGS)

#static: main.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static main.cpp -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME)

