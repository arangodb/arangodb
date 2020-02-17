// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ARGUMENTS_INL_H_
#define V8_EXECUTION_ARGUMENTS_INL_H_

#include "src/execution/arguments.h"

#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"  // TODO(jkummerow): Just smi-inl.h.

namespace v8 {
namespace internal {

template <class S>
Handle<S> Arguments::at(int index) const {
  return Handle<S>::cast(at<Object>(index));
}

int Arguments::smi_at(int index) const {
  return Smi::ToInt(Object(*address_of_arg_at(index)));
}

double Arguments::number_at(int index) const { return (*this)[index].Number(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ARGUMENTS_INL_H_
