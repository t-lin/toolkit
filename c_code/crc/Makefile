LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O3 -Wall
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast

BINNAME = test_crc

all: $(BINNAME)

$(BINNAME): test_crc.cpp crc.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

debug: CXXFLAGS += -DDEBUG -g
debug: all

clean:
	rm -f $(BINNAME)

