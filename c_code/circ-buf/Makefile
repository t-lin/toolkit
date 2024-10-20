#CPPFLAGS +=
LDFLAGS += -lgtest -lpthread
CXXFLAGS += -std=gnu++17 -O0 -g -Wall -Wsign-compare -Wconversion -Wpedantic
CXX=clang++-10

BINNAME = test-bounded-fifo

all: $(BINNAME)

$(BINNAME): test-bounded-fifo.cpp bounded-fifo.hpp helpers.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $(BINNAME) $(LDFLAGS)

clean:
	rm -f $(BINNAME) *.gcda *.gcno

# Build for test coverage info
coverage: CXXFLAGS += -fprofile-arcs -ftest-coverage
coverage: clean $(BINNAME)

lcov-clean:
	lcov --zerocounters --directory .

COMPILER=$(shell $(CXX) --version | grep -Eo "clang")
ifeq ($(COMPILER), clang)
	GCOV_TOOL = --gcov-tool ./llvm-gcov.sh
endif

LCOV_DIR = ./lcov
LCOV_FILE = coverage.info
lcov: *.gcda *.gcno
	mkdir -p $(LCOV_DIR)
	rm -f $(LCOV_DIR)/$(LCOV_FILE)
	lcov $(GCOV_TOOL) --capture --base-directory . --directory . --no-external --output-file $(LCOV_DIR)/$(LCOV_FILE)
	genhtml $(LCOV_DIR)/$(LCOV_FILE) --legend --output-directory $(LCOV_DIR)
