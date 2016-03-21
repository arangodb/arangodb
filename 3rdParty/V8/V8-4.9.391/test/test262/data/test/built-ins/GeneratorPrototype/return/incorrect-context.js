// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.1.3
description: >
    A TypeError should be thrown from GeneratorValidate (25.3.3.2) if the
    context of `return` does not defined the [[GeneratorState]] internal slot.
---*/

function* g() {}
var GeneratorPrototype = Object.getPrototypeOf(g).prototype;

assert.throws(TypeError, function() { GeneratorPrototype.return.call(1); });
assert.throws(TypeError, function() { GeneratorPrototype.return.call({}); });
assert.throws(TypeError, function() { GeneratorPrototype.return.call(function() {}); });
assert.throws(TypeError, function() { GeneratorPrototype.return.call(g); });
assert.throws(TypeError, function() { GeneratorPrototype.return.call(g.prototype); });
