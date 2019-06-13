CUDA_PATH := /opt/cuda

NVCC := $(CUDA_PATH)/bin/nvcc

CPPFLAGS := -O2 -std=c++11
LDFLAGS := -g -L/usr/local/lib
INCLUDES := -I/usr/local/include -I$(CUDA_PATH)/include
LIBRARIES := -lboost_fiber -lboost_context -lboost_system -lboost_filesystem

all: build

build: single_stream multiple_streams

single_stream.o:single_stream.cu
	$(NVCC) $(INCLUDES) $(CPPFLAGS) -o $@ -c $<

single_stream: single_stream.o
	$(NVCC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

multiple_streams.o:multiple_streams.cu
	$(NVCC) $(INCLUDES) $(CPPFLAGS) -o $@ -c $<

multiple_streams: multiple_streams.o
	$(NVCC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f single_stream single_stream.o multiple_streams multiple_streams.o

clobber: clean
