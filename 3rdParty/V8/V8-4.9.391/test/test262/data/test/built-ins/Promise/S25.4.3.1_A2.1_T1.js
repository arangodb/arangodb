// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise throws TypeError when 'this' is not Object
es6id: S25.4.3.1_A2.1_T1
author: Sam Mikes
description: Promise.call("non-object") throws TypeError
---*/

assert.throws(TypeError, function() {
  Promise.call("non-object", function () {});
});
