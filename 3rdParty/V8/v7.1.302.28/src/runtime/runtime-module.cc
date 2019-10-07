// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments-inl.h"
#include "src/counters.h"
#include "src/objects-inl.h"
#include "src/objects/js-promise.h"
#include "src/objects/module.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DynamicImportCall) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, specifier, 1);

  Handle<Script> script(Script::cast(function->shared()->script()), isolate);

  while (script->has_eval_from_shared()) {
    script =
        handle(Script::cast(script->eval_from_shared()->script()), isolate);
  }

  RETURN_RESULT_OR_FAILURE(
      isolate,
      isolate->RunHostImportModuleDynamicallyCallback(script, specifier));
}

RUNTIME_FUNCTION(Runtime_GetModuleNamespace) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_SMI_ARG_CHECKED(module_request, 0);
  Handle<Module> module(isolate->context()->module(), isolate);
  return *Module::GetModuleNamespace(isolate, module, module_request);
}

RUNTIME_FUNCTION(Runtime_GetImportMetaObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  Handle<Module> module(isolate->context()->module(), isolate);
  return *isolate->RunHostInitializeImportMetaObjectCallback(module);
}

}  // namespace internal
}  // namespace v8
