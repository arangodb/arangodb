// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_REWRITER_H_
#define V8_PARSING_REWRITER_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

class AstValueFactory;
class Isolate;
class ParseInfo;
class Parser;
class DeclarationScope;
class Scope;

class Rewriter {
 public:
  // Rewrite top-level code (ECMA 262 "programs") so as to conservatively
  // include an assignment of the value of the last statement in the code to
  // a compiler-generated temporary variable wherever needed.
  //
  // Assumes code has been parsed and scopes have been analyzed.  Mutates the
  // AST, so the AST should not continue to be used in the case of failure.
  V8_EXPORT_PRIVATE static bool Rewrite(ParseInfo* info);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_REWRITER_H_
