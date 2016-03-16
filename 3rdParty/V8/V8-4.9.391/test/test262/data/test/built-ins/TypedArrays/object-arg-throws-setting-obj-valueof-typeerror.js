// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-object
description: >
  Throw TypeError from OrdinaryToPrimitive when setting a property
info: >
  22.2.4.4 TypedArray ( object )

  This description applies only if the TypedArray function is called with at
  least one argument and the Type of the first argument is Object and that
  object does not have either a [[TypedArrayName]] or an [[ArrayBufferData]]
  internal slot.

  ...
  8. Repeat, while k < len
    ...
    b. Let kValue be ? Get(arrayLike, Pk).
    c. Perform ? Set(O, Pk, kValue, true).
  ...

  9.4.5.5 [[Set]] ( P, V, Receiver)

  ...
  2. If Type(P) is String and if SameValue(O, Receiver) is true, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      i. Return ? IntegerIndexedElementSet(O, numericIndex, V).
  ...

  9.4.5.9 IntegerIndexedElementSet ( O, index, value )

  ...
  3. Let numValue be ? ToNumber(value).
  ...

  7.1.3 ToNumber ( argument )

  Object, Apply the following steps:

    1. Let primValue be ? ToPrimitive(argument, hint Number).
    2. Return ? ToNumber(primValue).

  7.1.1 ToPrimitive ( input [ , PreferredType ] )

  ...
  4. Let exoticToPrim be ? GetMethod(input, @@toPrimitive).
  5. If exoticToPrim is not undefined, then
    a. Let result be ? Call(exoticToPrim, input, « hint »).
    b. If Type(result) is not Object, return result.
    c. Throw a TypeError exception.
  ...
  7. Return ? OrdinaryToPrimitive(input, hint).

  OrdinaryToPrimitive

  ...
  5. For each name in methodNames in List order, do
    a. Let method be ? Get(O, name).
    b. If IsCallable(method) is true, then
      i. Let result be ? Call(method, O).
      ii. If Type(result) is not Object, return result.
  6. Throw a TypeError exception.
includes: [testTypedArray.js]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new Int8Array(1);
  var valueOf = 0;
  var toString = 0;

  sample.valueOf = function() {
    valueOf++;
    return {};
  };

  sample.toString = function() {
    toString++;
    return {};
  };

  assert.throws(TypeError, function() {
    new TA([8, sample]);
  }, "abrupt completion from ToNumber(sample)");

  assert.sameValue(valueOf, 1, "valueOf called once");
  assert.sameValue(toString, 1, "toString called once");
});
