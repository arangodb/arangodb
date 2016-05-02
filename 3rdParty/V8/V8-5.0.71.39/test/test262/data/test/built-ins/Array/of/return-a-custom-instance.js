// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 22.1.2.3
description: >
  Returns an instance from a custom constructor.
info: >
  Array.of ( ...items )

  ...
  4. If IsConstructor(C) is true, then
    a. Let A be Construct(C, «len»).
  ...
  11. Return A.
---*/

function Coop() {}

var coop = Array.of.call(Coop, 'Mike', 'Rick', 'Leo');

assert.sameValue(
  coop.length, 3,
  'Sets a length property with the number of arguments'
);
assert.sameValue(
  coop[0], 'Mike',
  'Sets each argument in order as integer properties - #1 argument'
);
assert.sameValue(
  coop[1], 'Rick',
  'Sets each argument in order as integer properties - #2 argument'
);
assert.sameValue(
  coop[2], 'Leo',
  'Sets each argument in order as integer properties - #3 argument'
);
assert(coop instanceof Coop, 'Returns an instance from a custom constructor');
