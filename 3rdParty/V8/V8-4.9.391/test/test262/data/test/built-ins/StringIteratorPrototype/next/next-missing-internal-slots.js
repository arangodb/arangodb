// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 21.1.5.2.1 S 3
description: >
    If the `this` value does not have all of the internal slots of an String
    Iterator Instance (21.1.5.3), throw a `TypeError` exception.
---*/

var iterator = ''[Symbol.iterator]();
var object = Object.create(iterator);

assert.throws(TypeError, function() {
  object.next();
});
