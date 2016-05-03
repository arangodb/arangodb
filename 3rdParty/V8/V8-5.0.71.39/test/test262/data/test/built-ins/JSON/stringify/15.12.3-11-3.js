// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.12.3-11-3
description: A JSON.stringify correctly works on top level string values.
---*/

assert.sameValue(JSON.stringify("a string"), '"a string"', 'JSON.stringify("a string")');
