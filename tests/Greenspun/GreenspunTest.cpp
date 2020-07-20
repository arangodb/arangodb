#include <iostream>

#include "Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h"
#include "Pregel/Algos/VertexAccumulators/Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

/* YOLO */

namespace arangodb::basics::VelocyPackHelper {

int compare(arangodb::velocypack::Slice, arangodb::velocypack::Slice, bool,
            arangodb::velocypack::Options const*,
            arangodb::velocypack::Slice const*, arangodb::velocypack::Slice const*) {
  std::cerr << "WARNING! YOU ARE ABOUT TO SHOOT YOURSELF IN THE FOOT!" << std::endl;
  return 0;
}

}  // namespace arangodb::basics::VelocyPackHelper

struct MyEvalContext : PrimEvalContext {
  std::string const& getThisId() const override { std::abort(); }

  void getAccumulatorValue(std::string_view id, VPackBuilder& result) const override {
    std::abort();
  }

  void updateAccumulator(std::string_view accumId, std::string_view vertexId,
                         VPackSlice value) override {
    std::abort();
  }

  void setAccumulator(std::string_view accumId, VPackSlice value) override {
    std::abort();
  }

  void enumerateEdges(std::function<void(VPackSlice edge, VPackSlice vertex)> cb) const override {
    std::abort();
  }
};

int main(int argc, char** argv) {
  InitInterpreter();

  MyEvalContext ctx;
  VPackBuilder result;

  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
  ["+", 3,
    ["if",
      [["eq?", ["foobar", "hello", 2], 2], 3],
      [true, 1]
    ]
  ]
  )aql");

  std::cout << "ArangoLISP Interpreter Executing" << std::endl;
  std::cout << " " << program->toJson() << std::endl;

  auto res = Evaluate(ctx, program->slice(), result).wrapError([](EvalError& err) { err.wrapMessage("at top-level"); });
  if (res.fail()) {
    std::cerr << "Evaluate failed: " << res.error().toString() << std::endl;
  } else {
    std::cout << " ArangoLISP executed, result " << result.toJson() << std::endl;
  }

  return 0;
}
