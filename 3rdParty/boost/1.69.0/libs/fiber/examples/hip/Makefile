HIP_PATH := /opt/rocm/hip

HIPCC := $(HIP_PATH)/bin/hipcc

CPPFLAGS := -O2 -std=c++11
LDFLAGS := -L/usr/local/lib
INCLUDES := -I/usr/local/include -I$(HIP_PATH)/include
LIBRARIES := -lboost_fiber -lboost_context -lboost_system -lboost_filesystem

all: build

build: single_stream multiple_streams

single_stream.o:single_stream.cpp
	$(HIPCC) $(INCLUDES) $(CPPFLAGS) -o $@ -c $<

single_stream: single_stream.o
	$(HIPCC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

multiple_streams.o:multiple_streams.cpp
	$(HIPCC) $(INCLUDES) $(CPPFLAGS) -o $@ -c $<

multiple_streams: multiple_streams.o
	$(HIPCC) $(LDFLAGS) -o $@ $+ $(LIBRARIES)

clean:
	rm -f single_stream single_stream.o multiple_streams multiple_streams.o

clobber: clean
