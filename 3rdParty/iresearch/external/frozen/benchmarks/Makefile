ifndef GOOGLE_BENCHMARK_PREFIX
    $(error GOOGLE_BENCHMARK_PREFIX is undefined)
endif

CXXFLAGS=-O2
override CXXFLAGS+= -std=c++17 #17 for boyer_moore_searcher

LIBS=-lbenchmark -lpthread
LDFLAGS=-L$(GOOGLE_BENCHMARK_PREFIX)/lib

CPPFLAGS=-I../include -I$(GOOGLE_BENCHMARK_PREFIX)/include


all:bench
	./$<

bench: bench_main.o bench_str_set.o bench_str_unordered_set.o bench_int_set.o bench_int_unordered_set.o bench_str_search.o
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

clean:
	$(RM) *.o bench bench_str_unordered_set.cpp bench_int_unordered_set.cpp

bench_str_unordered_set.cpp:bench_str_set.cpp
	sed -e 's/set/unordered_set/g' -e 's/Set/UnorderedSet/g' $< > $@

bench_int_unordered_set.cpp:bench_int_set.cpp
	sed -e 's/set/unordered_set/g' -e 's/Set/UnorderedSet/g' $< > $@
