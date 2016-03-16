// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 23.2.5.2.1
description: >
  %SetIteratorPrototype%.next.name is "next".
info: >
  %SetIteratorPrototype%.next ( )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

var SetIteratorProto = Object.getPrototypeOf(new Set().values());

assert.sameValue(SetIteratorProto.next.name, "next");

verifyNotEnumerable(SetIteratorProto.next, "name");
verifyNotWritable(SetIteratorProto.next, "name");
verifyConfigurable(SetIteratorProto.next, "name");
