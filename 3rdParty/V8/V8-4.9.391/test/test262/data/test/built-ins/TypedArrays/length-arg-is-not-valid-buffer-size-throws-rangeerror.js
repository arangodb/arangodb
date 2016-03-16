// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-length
description: >
  Throws a RangeError when length argument is not a valid buffer size
info: >
  22.2.4.2 TypedArray ( length )

  This description applies only if the TypedArray function is called with at
  least one argument and the Type of the first argument is not Object.

  ...
  8. Return ? AllocateTypedArray(constructorName, NewTarget,
  %TypedArrayPrototype%, elementLength).

  22.2.4.2.1 Runtime Semantics: AllocateTypedArray (constructorName, newTarget,
  defaultProto [ , length ])

  6. Else,
    a. Perform ? AllocateTypedArrayBuffer(obj, length).
  ...

  22.2.4.2.2 Runtime Semantics: AllocateTypedArrayBuffer ( O, length )

  ...
  7. Let data be ? AllocateArrayBuffer(%ArrayBuffer%, byteLength).
  ...


  24.1.1.1 AllocateArrayBuffer ( constructor, byteLength )

  ...
  3. Let block be ? CreateByteDataBlock(byteLength).
  ...

  6.2.6.1 CreateByteDataBlock (size)

  ...
  2. Let db be a new Data Block value consisting of size bytes. If it is
  impossible to create such a Data Block, throw a RangeError exception.
  ...

includes: [testTypedArray.js]
---*/

var length = Math.pow(2, 53);

testWithTypedArrayConstructors(function(TA) {
  assert.throws(RangeError, function() {
    new TA(length);
  })
});
