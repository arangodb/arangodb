// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 26.2.2.1.1
description: Proxy Revocation functions are not constructors
info: >
  17 ECMAScript Standard Built-in Objects:
    Built-in function objects that are not identified as constructors do not
    implement the [[Construct]] internal method unless otherwise specified
    in the description of a particular function.
---*/

var revocationFunction = Proxy.revocable({}, {}).revoke;

assert.sameValue(Object.prototype.hasOwnProperty.call(revocationFunction, "prototype"), false);
assert.throws(TypeError, function() { new revocationFunction(); });
