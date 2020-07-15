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

int main(int argc, char** argv) {

  InitInterpreter();

  EvalContext ctx;
  VPackBuilder result;

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
		["if", 
			["list",
				["eq?", 
					["varref", "v"], 
					["varref", "S"]],
				["list", "set", "distance", 0]
			]
		]
  )aql");

	std::cout << "ArangoLISP Interpreter Executing" << std::endl;
  std::cout << " " << program->toJson() << std::endl;

  Evaluate(ctx, program->slice(), result);

  std::cout << " ArangoLISP executed, result " << result.toJson() << std::endl;

	return 0;
}
