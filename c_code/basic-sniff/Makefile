LIBTINS = $(HOME)/libtins
CPPFLAGS += -I$(LIBTINS)/include
LDFLAGS += -L$(LIBTINS)/lib -ltins -lpcap
CXXFLAGS += -std=c++14 -O3 -Wall

BINNAME = blek

all:  main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) main.cpp -o $(BINNAME) $(LDFLAGS)

debug:  main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g main.cpp -o $(BINNAME) $(LDFLAGS)

clean:
	rm $(BINNAME)

