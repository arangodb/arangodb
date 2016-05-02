// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    'y' entry's presence is determined by `y` flag
es6id: 21.2.5.3
info: >
    21.2.5.3 get RegExp.prototype.flags

    16. Let sticky be ToBoolean(Get(R, "sticky")).
    17. ReturnIfAbrupt(sticky).
    18. If sticky is true, append "y" as the last code unit of result.
---*/

var flags;

flags = /./.flags;
assert.sameValue(flags.length, 0);

flags = /./y.flags;
assert.sameValue(flags.length, 1);
assert.sameValue(flags[0], 'y');
