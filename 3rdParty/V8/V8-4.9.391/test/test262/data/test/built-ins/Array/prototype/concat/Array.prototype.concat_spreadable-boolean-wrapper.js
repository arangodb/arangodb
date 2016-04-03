// Copyright (c) 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/*---
es6id: 22.1.3.1_3
description: Array.prototype.concat Symbol.isConcatSpreadable boolean wrapper
includes: [compareArray.js]
---*/
var bool = new Boolean(true)
// Boolean wrapper objects are not concat-spreadable by default
assert(compareArray([bool], [].concat(bool)));

// Boolean wrapper objects may be individually concat-spreadable
bool[Symbol.isConcatSpreadable] = true;
bool.length = 3;
bool[0] = 1, bool[1] = 2, bool[2] = 3;
assert(compareArray([1, 2, 3], [].concat(bool)));

Boolean.prototype[Symbol.isConcatSpreadable] = true;
// Boolean wrapper objects may be concat-spreadable
assert(compareArray([], [].concat(new Boolean(true))));
Boolean.prototype[0] = 1;
Boolean.prototype[1] = 2;
Boolean.prototype[2] = 3;
Boolean.prototype.length = 3;
assert(compareArray([1,2,3], [].concat(new Boolean(true))));

// Boolean values are never concat-spreadable
assert(compareArray([true], [].concat(true)));
delete Boolean.prototype[Symbol.isConcatSpreadable];
delete Boolean.prototype[0];
delete Boolean.prototype[1];
delete Boolean.prototype[2];
delete Boolean.prototype.length;
