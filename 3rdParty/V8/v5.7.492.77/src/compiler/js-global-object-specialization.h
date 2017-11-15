// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GLOBAL_OBJECT_SPECIALIZATION_H_
#define V8_COMPILER_JS_GLOBAL_OBJECT_SPECIALIZATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class JSOperatorBuilder;
class SimplifiedOperatorBuilder;
class TypeCache;

// Specializes a given JSGraph to a given global object, potentially constant
// folding some {JSLoadGlobal} nodes or strength reducing some {JSStoreGlobal}
// nodes.
class JSGlobalObjectSpecialization final : public AdvancedReducer {
 public:
  JSGlobalObjectSpecialization(Editor* editor, JSGraph* jsgraph,
                               Handle<JSGlobalObject> global_object,
                               CompilationDependencies* dependencies);

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSLoadGlobal(Node* node);
  Reduction ReduceJSStoreGlobal(Node* node);

  struct ScriptContextTableLookupResult;
  bool LookupInScriptContextTable(Handle<Name> name,
                                  ScriptContextTableLookupResult* result);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;
  JSOperatorBuilder* javascript() const;
  SimplifiedOperatorBuilder* simplified() const;
  Handle<JSGlobalObject> global_object() const { return global_object_; }
  CompilationDependencies* dependencies() const { return dependencies_; }

  JSGraph* const jsgraph_;
  Handle<JSGlobalObject> const global_object_;
  CompilationDependencies* const dependencies_;
  TypeCache const& type_cache_;

  DISALLOW_COPY_AND_ASSIGN(JSGlobalObjectSpecialization);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_GLOBAL_OBJECT_SPECIALIZATION_H_
