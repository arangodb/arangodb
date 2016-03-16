// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-2-5
description: delete operator returns true when deleting a non-reference (obj)
---*/

  var d = delete {a:0} ;

assert.sameValue(d, true, 'd');
