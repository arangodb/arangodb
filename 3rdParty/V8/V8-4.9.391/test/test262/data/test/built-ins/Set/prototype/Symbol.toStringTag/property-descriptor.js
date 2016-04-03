// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
  description: >
      `Object.prototype.getOwnPropertyDescriptor` should reflect the value and
      writability of the @@toStringTag attribute.
  includes: [propertyHelper.js]
  es6id: 23.2.3.12
 ---*/

var SetProto = Object.getPrototypeOf(new Set());

assert.sameValue(
  SetProto[Symbol.toStringTag],
  'Set',
  "The value of `SetProto[Symbol.toStringTag]` is `'Set'`"
);

verifyNotEnumerable(SetProto, Symbol.toStringTag);
verifyNotWritable(SetProto, Symbol.toStringTag);
verifyConfigurable(SetProto, Symbol.toStringTag);
