// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.4
description: Instances has the own property length
info: >
  21.1.4 Properties of String Instances

  ...

  String instances have a length property, and a set of enumerable properties
  with integer indexed names.
includes: [propertyHelper.js]
---*/

class S extends String {}

var s1 = new S();
assert.sameValue(s1.length, 0);

verifyNotWritable(s1, 'length');
verifyNotEnumerable(s1, 'length');
verifyNotConfigurable(s1, 'length');

var s2 = new S('test262');
assert.sameValue(s2.length, 7);

verifyNotWritable(s2, 'length');
verifyNotEnumerable(s2, 'length');
verifyNotConfigurable(s2, 'length');