LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O3 -Wall
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast

BINNAME = test_serialutils

all: $(BINNAME)

debug: CXXFLAGS += -DDEBUG -g
debug: all

$(BINNAME): test_serialutils.cpp serial_utils.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME) 

