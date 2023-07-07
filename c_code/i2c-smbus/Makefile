LDFLAGS += -lgtest -lpthread -li2c
CXXFLAGS += -std=gnu++17 -O3 -Wall
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast

BINNAME = test_i2cutils

all: $(BINNAME)

$(BINNAME): test_i2cutils.cpp i2c_utils.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $(BINNAME) $(LDFLAGS)

i2c_utils.o: i2c_utils.cpp i2c_utils.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@ $(LDFLAGS)

debug: CXXFLAGS += -DDEBUG -g
debug: all

clean:
	rm -f $(BINNAME)

