// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 14.1-5gs
description: >
    StrictMode - a Use Strict Directive embedded in a directive
    prologue followed by a strict mode violation
negative: SyntaxError
flags: [raw]
---*/

"a";
"use strict";
"c";
throw new Error("This code should not execute");
eval = 42;
