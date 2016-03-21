// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.4.4.14-1-8
description: Array.prototype.indexOf applied to String object
---*/

        var obj = new String("null");

assert.sameValue(Array.prototype.indexOf.call(obj, 'l'), 2, 'Array.prototype.indexOf.call(obj, "l")');
