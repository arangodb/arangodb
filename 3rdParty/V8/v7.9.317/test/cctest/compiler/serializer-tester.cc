// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Serializer tests don't make sense in lite mode, as it doesn't gather
// IC feedback.
#ifndef V8_LITE_MODE

#include "test/cctest/compiler/serializer-tester.h"

#include "src/api/api-inl.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/serializer-for-background-compilation.h"
#include "src/compiler/zone-stats.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

SerializerTester::SerializerTester(const char* source)
    : canonical_(main_isolate()) {
  // The tests only make sense in the context of concurrent compilation.
  FLAG_concurrent_inlining = true;
  // The tests don't make sense when optimizations are turned off.
  FLAG_opt = true;
  // We need the IC to feed it to the serializer.
  FLAG_use_ic = true;
  // We need manual control over when a given function is optimized.
  FLAG_always_opt = false;
  // We need allocation of executable memory for the compilation.
  FLAG_jitless = false;
  FLAG_allow_natives_syntax = true;

  std::string function_string = "(function() { ";
  function_string += source;
  function_string += " })();";
  Handle<JSFunction> function = Handle<JSFunction>::cast(v8::Utils::OpenHandle(
      *v8::Local<v8::Function>::Cast(CompileRun(function_string.c_str()))));
  uint32_t flags = i::OptimizedCompilationInfo::kInliningEnabled |
                   i::OptimizedCompilationInfo::kFunctionContextSpecializing |
                   i::OptimizedCompilationInfo::kAccessorInliningEnabled |
                   i::OptimizedCompilationInfo::kLoopPeelingEnabled |
                   i::OptimizedCompilationInfo::kBailoutOnUninitialized |
                   i::OptimizedCompilationInfo::kAllocationFoldingEnabled |
                   i::OptimizedCompilationInfo::kSplittingEnabled |
                   i::OptimizedCompilationInfo::kAnalyzeEnvironmentLiveness;
  Optimize(function, main_zone(), main_isolate(), flags, &broker_);
  function_ = JSFunctionRef(broker(), function);
}

TEST(SerializeEmptyFunction) {
  SerializerTester tester(
      "function f() {}; %EnsureFeedbackVectorForFunction(f); return f;");
  JSFunctionRef function = tester.function();
  CHECK(
      function.shared().IsSerializedForCompilation(function.feedback_vector()));
}

// This helper function allows for testing whether an inlinee candidate
// was properly serialized. It expects that the top-level function (that is
// run through the SerializerTester) will return its inlinee candidate.
void CheckForSerializedInlinee(const char* source, int argc = 0,
                               Handle<Object> argv[] = {}) {
  SerializerTester tester(source);
  JSFunctionRef f = tester.function();
  CHECK(f.shared().IsSerializedForCompilation(f.feedback_vector()));

  MaybeHandle<Object> g_obj = Execution::Call(
      tester.isolate(), tester.function().object(),
      tester.isolate()->factory()->undefined_value(), argc, argv);
  Handle<Object> g;
  CHECK(g_obj.ToHandle(&g));

  CHECK_WITH_MSG(
      g->IsJSFunction(),
      "The return value of the outer function must be a function too");
  Handle<JSFunction> g_func = Handle<JSFunction>::cast(g);

  SharedFunctionInfoRef g_sfi(tester.broker(),
                              handle(g_func->shared(), tester.isolate()));
  FeedbackVectorRef g_fv(tester.broker(),
                         handle(g_func->feedback_vector(), tester.isolate()));
  CHECK(g_sfi.IsSerializedForCompilation(g_fv));
}

TEST(SerializeInlinedClosure) {
  CheckForSerializedInlinee(
      "function f() {"
      "  function g(){ return g; }"
      "  %EnsureFeedbackVectorForFunction(g);"
      "  return g();"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeInlinedFunction) {
  CheckForSerializedInlinee(
      "function g() {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "function f() {"
      "  g(); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeCallUndefinedReceiver) {
  CheckForSerializedInlinee(
      "function g(a,b,c) {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "function f() {"
      "  g(1,2,3); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeCallUndefinedReceiver2) {
  CheckForSerializedInlinee(
      "function g(a,b) {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "function f() {"
      "  g(1,2); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeCallProperty) {
  CheckForSerializedInlinee(
      "let obj = {"
      "  g: function g(a,b,c) {}"
      "};"
      "%EnsureFeedbackVectorForFunction(obj.g);"
      "function f() {"
      "  obj.g(1,2,3); return obj.g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeCallProperty2) {
  CheckForSerializedInlinee(
      "let obj = {"
      "  g: function g(a,b) {}"
      "};"
      "%EnsureFeedbackVectorForFunction(obj.g);"
      "function f() {"
      "  obj.g(1,2); return obj.g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeCallAnyReceiver) {
  CheckForSerializedInlinee(
      "let obj = {"
      "  g: function g() {}"
      "};"
      "%EnsureFeedbackVectorForFunction(obj.g);"
      "function f() {"
      "  with(obj) {"
      "    g(); return g;"
      "  };"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeCallWithSpread) {
  CheckForSerializedInlinee(
      "function g(args) {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "const arr = [1,2,3];"
      "function f() {"
      "  g(...arr); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

// The following test causes the CallIC of `g` to turn megamorphic,
// thus allowing us to test if we forward arguments hints (`callee` in this
// example) and correctly serialize the inlining candidate `j`.
TEST(SerializeCallArguments) {
  CheckForSerializedInlinee(
      "function g(callee) { callee(); };"
      "function h() {};"
      "function i() {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "g(h); g(i);"
      "function f() {"
      "  function j() {};"
      "  g(j);"
      "  return j;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "var j = f();"
      "%EnsureFeedbackVectorForFunction(j);"
      "f(); return f;");
}

TEST(SerializeConstruct) {
  CheckForSerializedInlinee(
      "function g() {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "function f() {"
      "  new g(); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeConstructWithSpread) {
  CheckForSerializedInlinee(
      "function g(a, b, c) {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "const arr = [1, 2];"
      "function f() {"
      "  new g(0, ...arr); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeConstructSuper) {
  CheckForSerializedInlinee(
      "class A {};"
      "class B extends A { constructor() { super(); } };"
      "%EnsureFeedbackVectorForFunction(A);"
      "%EnsureFeedbackVectorForFunction(B);"
      "function f() {"
      "  new B(); return A;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "f(); return f;");
}

TEST(SerializeConditionalJump) {
  CheckForSerializedInlinee(
      "function g(callee) { callee(); };"
      "function h() {};"
      "function i() {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "let a = true;"
      "g(h); g(i);"
      "function f() {"
      "  function q() {};"
      "  if (a) g(q);"
      "  return q;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "var q = f();"
      "%EnsureFeedbackVectorForFunction(q);"
      "f(); return f;");
}

TEST(SerializeUnconditionalJump) {
  CheckForSerializedInlinee(
      "function g(callee) { callee(); };"
      "function h() {};"
      "function i() {};"
      "%EnsureFeedbackVectorForFunction(g);"
      "%EnsureFeedbackVectorForFunction(h);"
      "%EnsureFeedbackVectorForFunction(i);"
      "let a = false;"
      "g(h); g(i);"
      "function f() {"
      "  function p() {};"
      "  function q() {};"
      "  if (a) q();"
      "  else g(p);"
      "  return p;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "var p = f();"
      "%EnsureFeedbackVectorForFunction(p);"
      "f(); return f;");
}

TEST(MergeJumpTargetEnvironment) {
  CheckForSerializedInlinee(
      "function f() {"
      "  let g;"
      "  while (true) {"
      "    if (g === undefined) {g = ()=>1; break;} else {g = ()=>2; break};"
      "  };"
      "  g(); return g;"
      "};"
      "%EnsureFeedbackVectorForFunction(f);"
      "%EnsureFeedbackVectorForFunction(f());"
      "f(); return f;");  // Two calls to f to make g() megamorhpic.
}

TEST(BoundFunctionTarget) {
  CheckForSerializedInlinee(
      "function apply(foo, arg) { return foo(arg); };"
      "%EnsureFeedbackVectorForFunction(apply);"
      "function test() {"
      "  const lambda = (a) => a;"
      "  %EnsureFeedbackVectorForFunction(lambda);"
      "  let bound = apply.bind(null, lambda).bind(null, 42);"
      "  %TurbofanStaticAssert(bound() == 42); return apply;"
      "};"
      "%EnsureFeedbackVectorForFunction(test);"
      "test(); return test;");
}

TEST(BoundFunctionArguments) {
  CheckForSerializedInlinee(
      "function apply(foo, arg) { return foo(arg); };"
      "%EnsureFeedbackVectorForFunction(apply);"
      "function test() {"
      "  const lambda = (a) => a;"
      "  %EnsureFeedbackVectorForFunction(lambda);"
      "  let bound = apply.bind(null, lambda).bind(null, 42);"
      "  %TurbofanStaticAssert(bound() == 42); return lambda;"
      "};"
      "%EnsureFeedbackVectorForFunction(test);"
      "test(); return test;");
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_LITE_MODE
