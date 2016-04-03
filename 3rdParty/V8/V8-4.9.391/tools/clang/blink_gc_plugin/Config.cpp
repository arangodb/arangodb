// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "Config.h"

#include <cassert>

#include "clang/AST/AST.h"

using namespace clang;

bool Config::IsTemplateInstantiation(CXXRecordDecl* record) {
  ClassTemplateSpecializationDecl* spec =
      dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
  if (!spec)
    return false;
  switch (spec->getTemplateSpecializationKind()) {
    case TSK_ImplicitInstantiation:
    case TSK_ExplicitInstantiationDefinition:
      return true;
    case TSK_Undeclared:
    case TSK_ExplicitSpecialization:
      return false;
    // TODO: unsupported cases.
    case TSK_ExplicitInstantiationDeclaration:
      return false;
  }
  assert(false && "Unknown template specialization kind");
  return false;
}
