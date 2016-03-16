// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.4.1-2-2
description: >
    delete operator returns true when deleting returned value from a
    function
---*/

  var bIsFooCalled = false;
  var foo = function(){bIsFooCalled = true;};

  var d = delete foo();

assert.sameValue(d, true, 'd');
assert.sameValue(bIsFooCalled, true, 'bIsFooCalled');
