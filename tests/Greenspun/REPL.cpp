
#include <iostream>

#include "Greenspun/EvalResult.h"
#include "Greenspun/Interpreter.h"
#include "Greenspun/Primitives.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "Utilities/Completer.h"
#include "Utilities/LineEditor.h"
#include "Utilities/ShellBase.h"

/* YOLO */

using namespace arangodb;

greenspun::EvalResult Func_thisId(greenspun::Machine& ctx,
                                  VPackSlice const params, VPackBuilder& result) {
  result.add(VPackValue("V/1"));
  return {};
}

std::unordered_map<std::string, VPackBuilder> variableValues;

greenspun::EvalResult Func_VarSet(greenspun::Machine& ctx,
                                  VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 2) {
    return greenspun::EvalError("expected two parameters");
  }
  VPackSlice name = params.at(0);
  VPackSlice value = params.at(1);
  if (!name.isString()) {
    return greenspun::EvalError("expected string as first parameter, found: " +
                                name.toJson());
  }

  auto nameStr = name.copyString();
  VPackBuilder& builder = variableValues[nameStr];
  builder.clear();
  builder.add(value);

  ctx.setVariable(nameStr, builder.slice());
  return {};
}

void AddSomeFunctions(greenspun::Machine& m) {
  m.setFunction("this-id", Func_thisId);
  m.setFunction("var-set!", Func_VarSet);
}

struct LispCompleter : arangodb::Completer {
  bool isComplete(std::string const& source, size_t /*lineno*/) final {
    int openBrackets = 0;
    int openBraces = 0;

    enum line_parse_state_e {
      NORMAL,           // start
      DOUBLE_QUOTE,     // from NORMAL: seen a single "
      DOUBLE_QUOTE_ESC  // from DOUBLE_QUOTE: seen a backslash
    };

    char const* ptr = source.c_str();
    char const* end = ptr + source.length();
    line_parse_state_e state = NORMAL;

    while (ptr < end) {
      if (state == DOUBLE_QUOTE) {
        if (*ptr == '\\') {
          state = DOUBLE_QUOTE_ESC;
        } else if (*ptr == '"') {
          state = NORMAL;
        }

        ++ptr;
      } else if (state == DOUBLE_QUOTE_ESC) {
        state = DOUBLE_QUOTE;
        ptr++;
      } else {
        switch (*ptr) {
          case '"':
            state = DOUBLE_QUOTE;
            break;

          case '[':
            ++openBrackets;
            break;

          case ']':
            --openBrackets;
            break;

          case '{':
            ++openBraces;
            break;

          case '}':
            --openBraces;
            break;

          case '\\':
            ++ptr;
            break;
        }

        ++ptr;
      }
    }

    return (openBrackets <= 0 && openBraces <= 0);
  }
  std::vector<std::string> alternatives(char const*) override { return {}; }
};

struct LispLineEditor : arangodb::LineEditor {
  LispLineEditor(std::string const& history) : LineEditor() {
    _shell = arangodb::ShellBase::buildShell(history, new LispCompleter());
  }
};

int main(int argc, char** argv) {
  greenspun::Machine m;
  InitMachine(m);
  AddSomeFunctions(m);
  m.setPrintCallback(
      [](std::string const& msg) { std::cout << msg << std::endl; });

  LispLineEditor lineEditor(".arangolisphist");
  lineEditor.open(true);

  while (true) {
    arangodb::ShellBase::EofType eof;
    auto line = lineEditor.prompt("air> ", "air> ", eof);
    lineEditor.addHistory(line);

    if (line.empty() && (eof == arangodb::ShellBase::EOF_ABORT ||
                         eof == arangodb::ShellBase::EOF_FORCE_ABORT)) {
      break;
    }

    try {
      auto program = arangodb::velocypack::Parser::fromJson(line);

      VPackBuilder result;
      auto res =
          greenspun::Evaluate(m, program->slice(), result).mapError([](greenspun::EvalError& err) {
            err.wrapMessage("at top-level");
          });
      if (res.fail()) {
        std::cerr << "error: " << res.error().toString() << std::endl;
      } else {
        if (result.slice().isNone()) {
          std::cout << " (no result)" << std::endl;
        } else {
          std::cout << " = " << result.toJson() << std::endl;
        }
      }
    } catch (VPackException const& e) {
      std::cerr << "VPack failed: " << e.what() << std::endl;
      continue;
    }
  }
  return 0;
}
