// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    This test should be run without any built-ins being added/augmented.
    The initial value of [[Configurable]] on JSON is true. This means we
    should be able to delete (8.6.2.5) the stringify and parse properties.
es5id: 15.12.3-0-3
description: JSON.stringify must be deletable (configurable)
---*/

  var o = JSON;
  var desc = Object.getOwnPropertyDescriptor(o, "stringify");

assert.sameValue(desc.configurable, true, 'desc.configurable');
