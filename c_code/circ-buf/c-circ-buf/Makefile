#CPPFLAGS +=
#LDFLAGS +=
CXXFLAGS += -std=gnu++17 -O3 -Wall

BINNAME = circ-buf

#all:  main.cpp
all:  circ-buf.c
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

#debug:  main.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g main.cpp -o $(BINNAME) $(LDFLAGS)

#static: main.cpp
#	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static main.cpp -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME)

