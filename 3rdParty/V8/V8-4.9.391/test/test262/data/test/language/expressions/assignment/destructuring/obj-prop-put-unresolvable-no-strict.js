// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Outside of strict mode, if the the assignment target is an unresolvable
    reference, a new `var` binding should be created in the environment record.
es6id: 12.14.5.4
flags: [noStrict]
---*/

{
  ({ x: unresolvable } = {});
}

assert.sameValue(unresolvable, undefined);
