#include <iostream>

#include "Pregel/Algos/VertexAccumulators/Greenspun/Interpreter.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"


/* YOLO */

namespace arangodb::basics::VelocyPackHelper {

int compare(arangodb::velocypack::Slice, arangodb::velocypack::Slice, bool, arangodb::velocypack::Options const*, arangodb::velocypack::Slice const*, arangodb::velocypack::Slice const*) {
   return 0;
}

}

struct MyEvalContext : EvalContext {

  std::string const& getThisId() const override {
    std::abort();
  }

  VPackSlice getDocumentById(std::string_view id) const override {
    std::abort();
  }

  VPackSlice getAccumulatorValue(std::string_view id) const override {
    return VPackSlice::zeroSlice();
  }

  void updateAccumulator(std::string_view accumId, std::string_view vertexId, VPackSlice value) override {
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

  ctx.variables["v"] = v->slice();
  ctx.variables["S"] = S->slice();

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
    ["if", [
      [
        ["eq?", ["varref", "v"], ["varref", "S"]],
        ["set", "distance", 0]
      ]
    ]]
  )aql");

	std::cout << "ArangoLISP Interpreter Executing" << std::endl;
  std::cout << " " << program->toJson() << std::endl;

  Evaluate(ctx, program->slice(), result);

  std::cout << " ArangoLISP executed, result " << result.toJson() << std::endl;

	return 0;
}
