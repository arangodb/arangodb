// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat strict arguments
includes: [compareArray.js]
---*/
var args = (function(a, b, c) { "use strict"; return arguments; })(1,2,3);
args[Symbol.isConcatSpreadable] = true;
assert(compareArray([].concat(args, args), [1, 2, 3, 1, 2, 3]));

Object.defineProperty(args, "length", { value: 6 });
assert(compareArray([].concat(args), [1, 2, 3, void 0, void 0, void 0]));
