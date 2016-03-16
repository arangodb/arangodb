// Copyright (C) 2013 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The GeneratorPrototype intrinsic should define a `next` property that is
    non-enumerable, writable, and configurable (as per section 17).
includes: [propertyHelper.js]
es6id: 25.3.1
---*/

function* g() {}
var GeneratorPrototype = Object.getPrototypeOf(g).prototype;

verifyNotEnumerable(GeneratorPrototype, 'next');
verifyWritable(GeneratorPrototype, 'next');
verifyConfigurable(GeneratorPrototype, 'next');
