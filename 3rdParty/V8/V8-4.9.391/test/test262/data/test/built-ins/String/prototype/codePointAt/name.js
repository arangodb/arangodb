// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.3.3
description: >
  String.prototype.codePointAt.name value and descriptor.
info: >
  21.1.3.3 String.prototype.codePointAt ( pos )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  String.prototype.codePointAt.name, 'codePointAt',
  'The value of `String.prototype.codePointAt.name` is `"codePointAt"`'
);

verifyNotEnumerable(String.prototype.codePointAt, 'name');
verifyNotWritable(String.prototype.codePointAt, 'name');
verifyConfigurable(String.prototype.codePointAt, 'name');
