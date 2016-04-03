// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.3.4.5-13.b-3
description: >
    Function.prototype.bind, 'length' set to remaining number of
    expected args (all args prefilled)
---*/

  function foo(x, y) { }
  var o = {};
  
  var bf = foo.bind(o, 42, 101);

assert.sameValue(bf.length, 0, 'bf.length');
