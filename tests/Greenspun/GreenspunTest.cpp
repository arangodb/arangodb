#include <iostream>

#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"
#include "Pregel/Algos/AIR/Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"
#include "./structs/EvalContext.h"

/* YOLO */

namespace arangodb::basics::VelocyPackHelper {

int compare(arangodb::velocypack::Slice, arangodb::velocypack::Slice, bool,
            arangodb::velocypack::Options const*,
            arangodb::velocypack::Slice const*, arangodb::velocypack::Slice const*) {
  std::cerr << "WARNING! YOU ARE ABOUT TO SHOOT YOURSELF IN THE FOOT!" << std::endl;
  return 0;
}

}  // namespace arangodb::basics::VelocyPackHelper


#include <numeric>
int TRI_Levenshtein(std::string const& lhs, std::string const& rhs) {
  int const lhsLength = static_cast<int>(lhs.size());
  int const rhsLength = static_cast<int>(rhs.size());

  int* col = new int[lhsLength + 1];
  int start = 1;
  // fill with initial values
  std::iota(col + start, col + lhsLength + 1, start);

  for (int x = start; x <= rhsLength; ++x) {
    col[0] = x;
    int last = x - start;
    for (int y = start; y <= lhsLength; ++y) {
      int const save = col[y];
      col[y] = (std::min)({
                              col[y] + 1,                                // deletion
                              col[y - 1] + 1,                            // insertion
                              last + (lhs[y - 1] == rhs[x - 1] ? 0 : 1)  // substitution
                          });
      last = save;
    }
  }

  // fetch final value
  int result = col[lhsLength];
  // free memory
  delete[] col;

  return result;
}


int main(int argc, char** argv) {
  InitInterpreter();

  MyEvalContext ctx;
  VPackBuilder result;

  auto v = arangodb::velocypack::Parser::fromJson(R"aql("aNodeId")aql");
  auto S = arangodb::velocypack::Parser::fromJson(R"aql("anotherNodeId")aql");

  auto program = arangodb::velocypack::Parser::fromJson(R"aql(
  ["+", 3,
    ["if",
      [["eq?", ["+", 12, 2], 2], 3],
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
