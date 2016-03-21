// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 26.2.2.1.1
description: The `name` property of Proxy Revocation functions
info: >
  A Proxy revocation function is an anonymous function.

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.
---*/

var revocationFunction = Proxy.revocable({}, {}).revoke;

assert.sameValue(Object.prototype.hasOwnProperty.call(revocationFunction, "name"), false);
