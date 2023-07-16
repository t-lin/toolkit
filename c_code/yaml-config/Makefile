LDFLAGS += -lgtest -lpthread -lyaml-cpp
CXXFLAGS += -std=gnu++17 -O3 -Wall
CXXFLAGS += -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wmisleading-indentation -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast
CXXFLAGS += -DYAML_CONFIG_PATH=\"config.yaml\" -DYAML_TEST_CONFIG_PATH=\"test_config.yaml\"

BINNAME = test_yamlconfig

all: $(BINNAME)

debug: CXXFLAGS += -DDEBUG -g
debug: all

$(BINNAME): test_yamlconfig.cpp yaml_config.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME) 

