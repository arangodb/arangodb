
#include <iostream>

#include "Pregel/Algos/AIR/Greenspun/Interpreter.h"
#include "Pregel/Algos/AIR/Greenspun/Primitives.h"
#include "Pregel/Algos/AIR/Greenspun/EvalResult.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "Utilities/Completer.h"
#include "Utilities/LineEditor.h"
#include "Utilities/ShellBase.h"

/* YOLO */

using namespace arangodb;
namespace arangodb::basics::VelocyPackHelper {

int compare(arangodb::velocypack::Slice, arangodb::velocypack::Slice, bool,
            arangodb::velocypack::Options const*,
            arangodb::velocypack::Slice const*, arangodb::velocypack::Slice const*) {
  std::cerr << "WARNING! YOU ARE ABOUT TO SHOOT YOURSELF IN THE FOOT!" << std::endl;
  return 0;
}

}  // namespace arangodb::basics::VelocyPackHelper

greenspun::EvalResult Func_thisId(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  result.add(VPackValue("V/1"));
  return {};
}

std::unordered_map<std::string, VPackBuilder> variableValues;

greenspun::EvalResult Func_VarSet(greenspun::Machine& ctx, VPackSlice const params, VPackBuilder& result) {
  if (params.length() != 2) {
    return greenspun::EvalError("expected two parameters");
  }
  VPackSlice name = params.at(0);
  VPackSlice value = params.at(1);
  if (!name.isString()) {
    return greenspun::EvalError("expected string as first parameter, found: " + name.toJson());
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
  m.setFunction("var-set", Func_VarSet);
}

std::string TRI_HomeDirectory() {
  char const* result = getenv("HOME");

  if (result == nullptr) {
    return std::string(".");
  }

  return std::string(result);
}

namespace arangodb::basics::StringUtils {
bool isPrefix(std::string const& str, std::string const& prefix) {
  if (prefix.length() > str.length()) {
    return false;
  } else if (prefix.length() == str.length()) {
    return str == prefix;
  } else {
    return str.compare(0, prefix.length(), prefix) == 0;
  }
}
}  // namespace arangodb::basics::StringUtils

struct LispCompleter : arangodb::Completer {
  bool isComplete(std::string const& source, size_t /*lineno*/) final {
    int openParen = 0;
    int openBrackets = 0;
    int openBraces = 0;
    int openStrings = 0;  // only used for template strings, which can be multi-line
    int openComments = 0;

    enum line_parse_state_e {
      NORMAL,            // start
      NORMAL_1,          // from NORMAL: seen a single /
      DOUBLE_QUOTE,      // from NORMAL: seen a single "
      DOUBLE_QUOTE_ESC,  // from DOUBLE_QUOTE: seen a backslash
      SINGLE_QUOTE,      // from NORMAL: seen a single '
      SINGLE_QUOTE_ESC,  // from SINGLE_QUOTE: seen a backslash
      BACKTICK,          // from NORMAL: seen a single `
      BACKTICK_ESC,      // from BACKTICK: seen a backslash
      MULTI_COMMENT,     // from NORMAL_1: seen a *
      MULTI_COMMENT_1,   // from MULTI_COMMENT, seen a *
      SINGLE_COMMENT     // from NORMAL_1; seen a /
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
      } else if (state == SINGLE_QUOTE) {
        if (*ptr == '\\') {
          state = SINGLE_QUOTE_ESC;
        } else if (*ptr == '\'') {
          state = NORMAL;
        }

        ++ptr;
      } else if (state == SINGLE_QUOTE_ESC) {
        state = SINGLE_QUOTE;
        ptr++;
      } else if (state == BACKTICK) {
        if (*ptr == '\\') {
          state = BACKTICK_ESC;
        } else if (*ptr == '`') {
          state = NORMAL;
          --openStrings;
        }

        ++ptr;
      } else if (state == BACKTICK_ESC) {
        state = BACKTICK;
        ptr++;
      } else if (state == MULTI_COMMENT) {
        if (*ptr == '*') {
          state = MULTI_COMMENT_1;
        }

        ++ptr;
      } else if (state == MULTI_COMMENT_1) {
        if (*ptr == '/') {
          state = NORMAL;
          --openComments;
        }

        ++ptr;
      } else if (state == SINGLE_COMMENT) {
        ++ptr;

        if (ptr == end || *ptr == '\n') {
          state = NORMAL;
          --openComments;
        }
      } else if (state == NORMAL_1) {
        switch (*ptr) {
          case '/':
            state = SINGLE_COMMENT;
            ++openComments;
            ++ptr;
            break;

          case '*':
            state = MULTI_COMMENT;
            ++openComments;
            ++ptr;
            break;

          default:
            state = NORMAL;  // try again, do not change ptr
            break;
        }
      } else {
        switch (*ptr) {
          case '"':
            state = DOUBLE_QUOTE;
            break;

          case '\'':
            state = SINGLE_QUOTE;
            break;

          case '`':
            state = BACKTICK;
            ++openStrings;
            break;

          case '/':
            state = NORMAL_1;
            break;

          case '(':
            ++openParen;
            break;

          case ')':
            --openParen;
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

    return (openParen <= 0 && openBrackets <= 0 && openBraces <= 0 &&
            openStrings <= 0 && openComments <= 0);
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

  LispLineEditor lineEditor(".arangolisphist");
  lineEditor.open(true);

  while (true) {
    arangodb::ShellBase::EofType eof;
    auto line = lineEditor.prompt("aql> ", "aql> ", eof);
    lineEditor.addHistory(line);

    if (line.empty() && (eof == arangodb::ShellBase::EOF_ABORT ||
                         eof == arangodb::ShellBase::EOF_FORCE_ABORT)) {
      break;
    }

    try {
      auto program = arangodb::velocypack::Parser::fromJson(line);

      VPackBuilder result;
      auto res = greenspun::Evaluate(m, program->slice(), result).mapError([](greenspun::EvalError& err) {
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
