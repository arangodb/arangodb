// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 24.3.2
description: >
  JSON.stringify.name is "stringify".
info: >
  JSON.stringify ( value [ , replacer [ , space ] ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(JSON.stringify.name, "stringify");

verifyNotEnumerable(JSON.stringify, "name");
verifyNotWritable(JSON.stringify, "name");
verifyConfigurable(JSON.stringify, "name");
