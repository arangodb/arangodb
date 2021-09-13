.DELETE_ON_ERROR:

OBJS := aligned_allocator.o nanobenchmark.o targets.o
IMAGE_OBJS := image.o
TEST_NAMES := arithmetic_test combine_test compare_test convert_test hwy_test logical_test memory_test swizzle_test
TESTS := $(foreach i, $(TEST_NAMES), bin/$(i))
ROOT_TEST_NAMES := aligned_allocator_test nanobenchmark_test
ROOT_TESTS := $(foreach i, $(ROOT_TEST_NAMES), bin/$(i))

CXXFLAGS += -I. -fmerge-all-constants -std=c++17 -O2 \
    -Wno-builtin-macro-redefined -D__DATE__="redacted" \
    -D__TIMESTAMP__="redacted" -D__TIME__="redacted"  \
    -Wall -Wextra -Wformat-security -Wno-unused-function \
    -Wnon-virtual-dtor -Woverloaded-virtual -Wvla

.PHONY: all
all: $(TESTS) $(ROOT_TESTS) benchmark test

.PHONY: clean
clean: ; @rm -rf $(OBJS) bin/ benchmark.o

$(OBJS): %.o: hwy/%.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(IMAGE_OBJS): %.o: contrib/image/%.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

benchmark: $(OBJS) hwy/examples/benchmark.cc
	mkdir -p bin && $(CXX) $(CXXFLAGS) $^ -o bin/benchmark

bin/%: hwy/%.cc $(OBJS)
	mkdir -p bin && $(CXX) $(CXXFLAGS) $< $(OBJS) -o $@ -lgtest -lgtest_main -lpthread

bin/%: hwy/tests/%.cc $(OBJS)
	mkdir -p bin && $(CXX) $(CXXFLAGS) $< $(OBJS) -o $@ -lgtest -lgtest_main -lpthread

bin/%: contrib/image/%.cc $(OBJS)
	mkdir -p bin && $(CXX) $(CXXFLAGS) $< $(OBJS) -o $@ -lgtest -lgtest_main -lpthread
bin/%: contrib/math/%.cc $(OBJS)
	mkdir -p bin && $(CXX) $(CXXFLAGS) $< $(OBJS) -o $@ -lgtest -lgtest_main -lpthread

.PHONY: test
test: $(TESTS) $(ROOT_TESTS)
	for name in $^; do echo ---------------------$$name && $$name; done
