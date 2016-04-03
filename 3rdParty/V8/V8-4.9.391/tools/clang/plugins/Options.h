// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_OPTIONS_H_
#define TOOLS_CLANG_PLUGINS_OPTIONS_H_

namespace chrome_checker {

struct Options {
  Options()
      : check_base_classes(false),
        enforce_in_pdf(false),
        enforce_in_thirdparty_webkit(false),
        check_enum_last_value(false),
        with_ast_visitor(false),
        check_templates(false) {}

  bool check_base_classes;
  bool enforce_in_pdf;
  bool enforce_in_thirdparty_webkit;  // Use in Blink code itself
  bool check_enum_last_value;
  bool with_ast_visitor;
  bool check_templates;
  bool follow_macro_expansion = false;
};

}  // namespace chrome_checker

#endif  // TOOLS_CLANG_PLUGINS_OPTIONS_H_
