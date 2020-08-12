
VELOCYPACK_SRC_DIR := ../../3rdParty/velocypack/src
VELOCYPACK_OBJ_DIR := obj/velocypack
VELOCYPACK_SRC_FILES := $(wildcard $(VELOCYPACK_SRC_DIR)/*.cpp)
VELOCYPACK_OBJ_FILES := $(patsubst $(VELOCYPACK_SRC_DIR)/%.cpp,$(VELOCYPACK_OBJ_DIR)/%.o,$(VELOCYPACK_SRC_FILES))

LINENOISE_SRC_DIR := ../../3rdParty/linenoise-ng/src
LINENOISE_OBJ_DIR := obj/linenoise
LINENOISE_SRC_FILES := $(wildcard $(LINENOISE_SRC_DIR)/*.cpp)
LINENOISE_OBJ_FILES := $(patsubst $(LINENOISE_SRC_DIR)/%.cpp,$(LINENOISE_OBJ_DIR)/%.o,$(LINENOISE_SRC_FILES))

EVALUATOR_SRC_DIR := ../../arangod/Pregel/Algos/AIR/Greenspun/
EVALUATOR_OBJ_DIR := obj
EVALUATOR_SRC_FILES := $(wildcard $(EVALUATOR_SRC_DIR)/*.cpp)
EVALUATOR_OBJ_FILES := $(patsubst $(EVALUATOR_SRC_DIR)/%.cpp,$(EVALUATOR_OBJ_DIR)/%.o,$(EVALUATOR_SRC_FILES))

UTILITIES_SRC_DIR := ../../lib/Utilities/
UTILITIES_OBJ_DIR := obj
UTILITIES_SRC_FILES := LineEditor.cpp LinenoiseShell.cpp ShellBase.cpp
UTILITIES_OBJ_FILES := $(patsubst %.cpp,$(UTILITIES_OBJ_DIR)/%.o,$(UTILITIES_SRC_FILES))

LDFLAGS :=
CPPFLAGS :=
CXXFLAGS := -g -std=c++17 -I ../../3rdParty/velocypack/include -I ../../3rdParty/linenoise-ng/include -I ../../lib -I ../../arangod

default: dirs primitives repl

dirs:
	mkdir -p $(VELOCYPACK_OBJ_DIR)
	mkdir -p $(LINENOISE_OBJ_DIR)
	mkdir -p $(EVALUATOR_OBJ_DIR)


primitives: $(VELOCYPACK_OBJ_FILES) $(EVALUATOR_OBJ_FILES) obj/xxhash.o obj/PrimitivesTest.o
	g++ $(LDFLAGS) -o $@ $^

test: $(VELOCYPACK_OBJ_FILES) $(EVALUATOR_OBJ_FILES) obj/xxhash.o obj/GreenspunTest.o
	g++ $(LDFLAGS) -o $@ $^

repl: $(VELOCYPACK_OBJ_FILES) $(LINENOISE_OBJ_FILES) $(EVALUATOR_OBJ_FILES) $(UTILITIES_OBJ_FILES) obj/xxhash.o obj/REPL.o
	g++ $(LDFLAGS) -o $@ $^

$(VELOCYPACK_OBJ_DIR)/%.o: $(VELOCYPACK_SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(LINENOISE_OBJ_DIR)/%.o: $(LINENOISE_SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(EVALUATOR_OBJ_DIR)/%.o: $(EVALUATOR_SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(UTILITIES_OBJ_DIR)/%.o: $(UTILITIES_SRC_DIR)/%.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

obj/xxhash.o: ../../lib/Basics/xxhash.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

obj/PrimitivesTest.o: PrimitivesTest.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
obj/GreenspunTest.o: GreenspunTest.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
obj/REPL.o: REPL.cpp
	g++ $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<


clean:
	rm -rf $(VELOCYPACK_OBJ_DIR)
	rm -rf $(EVALUATOR_OBJ_DIR)
	rm -rf $(LINENOISE_OBJ_DIR)
	rm primitives repl
