// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat holey sloppy arguments
includes: [compareArray.js]
---*/
var args = (function(a) { return arguments; })(1,2,3);
delete args[1];
args[Symbol.isConcatSpreadable] = true;
assert(compareArray([1, void 0, 3, 1, void 0, 3], [].concat(args, args)));
