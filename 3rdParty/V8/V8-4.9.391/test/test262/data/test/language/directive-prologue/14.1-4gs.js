// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 14.1-4gs
description: >
    StrictMode - a Use Strict Directive followed by a strict mode
    violation
negative: SyntaxError
flags: [raw]
---*/

"use strict";
throw new Error("This code should not execute");
eval = 42;
