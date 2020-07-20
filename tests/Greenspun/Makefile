
VELOCYPACK_SRC_DIR := ../../3rdParty/velocypack/src
VELOCYPACK_OBJ_DIR := obj/velocypack
VELOCYPACK_SRC_FILES := $(wildcard $(VELOCYPACK_SRC_DIR)/*.cpp)
VELOCYPACK_OBJ_FILES := $(patsubst $(VELOCYPACK_SRC_DIR)/%.cpp,$(VELOCYPACK_OBJ_DIR)/%.o,$(VELOCYPACK_SRC_FILES))

EVALUATOR_SRC_DIR := ../../arangod/Pregel/Algos/VertexAccumulators/Greenspun/
EVALUATOR_OBJ_DIR := obj
EVALUATOR_SRC_FILES := $(wildcard $(EVALUATOR_SRC_DIR)/*.cpp)
EVALUATOR_OBJ_FILES := $(patsubst $(EVALUATOR_SRC_DIR)/%.cpp,$(EVALUATOR_OBJ_DIR)/%.o,$(EVALUATOR_SRC_FILES))

LDFLAGS :=
CPPFLAGS :=
CXXFLAGS := -g -std=c++17 -I ../../3rdParty/velocypack/include -I ../../lib -I ../../arangod

default: dirs eval

dirs:
	mkdir -p $(VELOCYPACK_OBJ_DIR)
	mkdir -p $(EVALUATOR_OBJ_DIR)


eval: $(VELOCYPACK_OBJ_FILES) $(EVALUATOR_OBJ_FILES) obj/xxhash.o obj/PrimitivesTest.o
	g++ $(LDFLAGS) -o $@ $^

test: $(VELOCYPACK_OBJ_FILES) $(EVALUATOR_OBJ_FILES) obj/xxhash.o obj/GreenspunTest.o
	g++ $(LDFLAGS) -o $@ $^

$(VELOCYPACK_OBJ_DIR)/%.o: $(VELOCYPACK_SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(EVALUATOR_OBJ_DIR)/%.o: $(EVALUATOR_SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

obj/xxhash.o: ../../lib/Basics/xxhash.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

obj/PrimitivesTest.o: PrimitivesTest.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
obj/GreenspunTest.o: GreenspunTest.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<


clean:
	rm -rf $(VELOCYPACK_OBJ_DIR)
	rm -rf $(EVALUATOR_OBJ_DIR)
