// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.9.5.44-0-2
description: Date.prototype.toJSON must exist as a function taking 1 parameter
---*/

assert.sameValue(Date.prototype.toJSON.length, 1, 'Date.prototype.toJSON.length');
