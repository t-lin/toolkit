LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O3 -Wall -Werror
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast

BINNAME = test_channel

all: $(BINNAME)

debug: CXXFLAGS += -DDEBUG -g
debug: all

$(BINNAME): test_channel.cpp channel.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME) 

