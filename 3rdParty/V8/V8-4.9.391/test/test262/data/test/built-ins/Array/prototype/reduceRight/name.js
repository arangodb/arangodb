// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 22.1.3.19
description: >
  Array.prototype.reduceRight.name is "reduceRight".
info: >
  Array.prototype.reduceRight ( callbackfn [ , initialValue ] )

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(Array.prototype.reduceRight.name, "reduceRight");

verifyNotEnumerable(Array.prototype.reduceRight, "name");
verifyNotWritable(Array.prototype.reduceRight, "name");
verifyConfigurable(Array.prototype.reduceRight, "name");
