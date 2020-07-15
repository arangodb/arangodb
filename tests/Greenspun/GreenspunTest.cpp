#include <iostream>

#include "Interpreter.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"


int main(int argc, char** argv) {

  InitInterpreter();

  EvalContext ctx;
  VPackBuilder result;

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
  [ "banana", 5, 6, ["banana", 11, 12], -8 ]
  )aql");

	std::cout << "ArangoLISP Interpreter Executing" << std::endl;
  std::cout << " " << program->toJson() << std::endl;

  Evaluate(ctx, program->slice(), result);

  std::cout << " ArangoLISP executed, result " << result.toJson() << std::endl;

	return 0;
}
