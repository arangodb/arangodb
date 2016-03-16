// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.5
description: >
    class strict mode: `with` disallowed
negative: SyntaxError
---*/

class C extends (function B() { with ({}); return B; }()) {}

