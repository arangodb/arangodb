// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.10.7.5-2
description: RegExp.prototype.lastIndex is not present
---*/

  var d = Object.getOwnPropertyDescriptor(RegExp.prototype, 'lastIndex');

assert.sameValue(d, undefined, 'd');
