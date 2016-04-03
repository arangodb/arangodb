// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.4-4-2
description: Object.getOwnPropertyNames returns array of property names (Object)
includes: [arrayContains.js]
---*/

  var result = Object.getOwnPropertyNames(Object);
  var expResult = ["getPrototypeOf", "getOwnPropertyDescriptor", "getOwnPropertyNames", "create", "defineProperty", "defineProperties", "seal", "freeze", "preventExtensions", "isSealed", "isFrozen", "isExtensible", "keys", "prototype", "length"];

assert(arrayContains(result, expResult), 'arrayContains(result, expResult) !== true');
