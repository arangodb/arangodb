// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TYPED_OPTIMIZATION_H_
#define V8_COMPILER_TYPED_OPTIMIZATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;
class Isolate;

namespace compiler {

// Forward declarations.
class CompilationDependencies;
class JSGraph;
class SimplifiedOperatorBuilder;
class TypeCache;

class V8_EXPORT_PRIVATE TypedOptimization final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  TypedOptimization(Editor* editor, CompilationDependencies* dependencies,
                    JSGraph* jsgraph, JSHeapBroker* js_heap_broker);
  ~TypedOptimization() override;

  const char* reducer_name() const override { return "TypedOptimization"; }

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceConvertReceiver(Node* node);
  Reduction ReduceCheckHeapObject(Node* node);
  Reduction ReduceCheckMaps(Node* node);
  Reduction ReduceCheckNumber(Node* node);
  Reduction ReduceCheckString(Node* node);
  Reduction ReduceCheckEqualsInternalizedString(Node* node);
  Reduction ReduceCheckEqualsSymbol(Node* node);
  Reduction ReduceLoadField(Node* node);
  Reduction ReduceNumberFloor(Node* node);
  Reduction ReduceNumberRoundop(Node* node);
  Reduction ReduceNumberSilenceNaN(Node* node);
  Reduction ReduceNumberToUint8Clamped(Node* node);
  Reduction ReducePhi(Node* node);
  Reduction ReduceReferenceEqual(Node* node);
  Reduction ReduceStringComparison(Node* node);
  Reduction ReduceStringLength(Node* node);
  Reduction ReduceSameValue(Node* node);
  Reduction ReduceSelect(Node* node);
  Reduction ReduceSpeculativeToNumber(Node* node);
  Reduction ReduceCheckNotTaggedHole(Node* node);
  Reduction ReduceTypeOf(Node* node);
  Reduction ReduceToBoolean(Node* node);

  Reduction TryReduceStringComparisonOfStringFromSingleCharCode(
      Node* comparison, Node* from_char_code, Type constant_type,
      bool inverted);
  Reduction TryReduceStringComparisonOfStringFromSingleCharCodeToConstant(
      Node* comparison, const StringRef& string, bool inverted);
  const Operator* NumberComparisonFor(const Operator* op);

  SimplifiedOperatorBuilder* simplified() const;
  Factory* factory() const;
  Graph* graph() const;
  Isolate* isolate() const;

  CompilationDependencies* dependencies() const { return dependencies_; }
  JSGraph* jsgraph() const { return jsgraph_; }
  JSHeapBroker* js_heap_broker() const { return js_heap_broker_; }

  CompilationDependencies* const dependencies_;
  JSGraph* const jsgraph_;
  JSHeapBroker* js_heap_broker_;
  Type const true_type_;
  Type const false_type_;
  TypeCache const& type_cache_;

  DISALLOW_COPY_AND_ASSIGN(TypedOptimization);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_TYPED_OPTIMIZATION_H_
