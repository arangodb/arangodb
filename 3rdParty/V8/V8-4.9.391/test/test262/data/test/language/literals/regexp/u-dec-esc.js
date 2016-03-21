// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: DecimalEscape used with `u` flag
info: >
    DecimalEscape is not allowed when the `u` flag is set (regardless of Annex
    B extensions--see ES6 section B.1.4).
es6id: 21.2.1
negative: SyntaxError
---*/

/\1/u;
