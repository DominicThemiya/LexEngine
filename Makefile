CXX ?= g++
CXXFLAGS ?= -std=c++17 -O1 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror

HEADERS := langdef.hpp lexer.hpp lexengine.hpp tests/harness.hpp
TEST_SOURCES := tests/tests.cpp tests/second_tu.cpp

.PHONY: test sanitize example clean

test: build/tests
	./build/tests $(FILTER)

sanitize: build/tests-sanitize
	./build/tests-sanitize $(FILTER)

build/tests: $(TEST_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(WARNINGS) -I. $(TEST_SOURCES) -o $@

build/tests-sanitize: $(TEST_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(WARNINGS) -fsanitize=address,undefined -fno-sanitize-recover=all -I. $(TEST_SOURCES) -o $@

clean:
	rm -rf build

example: build/calc

build/calc: examples/calc.cpp langdef.hpp lexer.hpp lexengine.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(WARNINGS) -I. examples/calc.cpp -o $@
