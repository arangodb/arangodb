// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.3.1.4
description: >
    A TypeError should be thrown from GeneratorValidate (25.3.3.2) if the
    context of `throw` does not defined the [[GeneratorState]] internal slot.
---*/

function* g() {}
var GeneratorPrototype = Object.getPrototypeOf(g).prototype;

assert.throws(TypeError, function() { GeneratorPrototype.throw.call(1); });
assert.throws(TypeError, function() { GeneratorPrototype.throw.call({}); });
assert.throws(TypeError, function() { GeneratorPrototype.throw.call(function() {}); });
assert.throws(TypeError, function() { GeneratorPrototype.throw.call(g); });
assert.throws(TypeError, function() { GeneratorPrototype.throw.call(g.prototype); });
