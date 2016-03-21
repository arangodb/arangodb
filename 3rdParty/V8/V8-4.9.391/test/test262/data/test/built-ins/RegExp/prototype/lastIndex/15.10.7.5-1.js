// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.10.7.5-1
description: RegExp.prototype.lastIndex is of type Undefined
---*/

assert.sameValue(typeof(RegExp.prototype.lastIndex), 'undefined', 'typeof(RegExp.prototype.lastIndex)');
