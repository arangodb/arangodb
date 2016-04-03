// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Error constructor creates own message property
info: >
  19.5.1.1 Error ( message )

  ...
  4.
    ...
    c. Let msgDesc be the PropertyDescriptor{[[Value]]: msg, [[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true}.
    d. Let status be DefinePropertyOrThrow(O, "message", msgDesc).
es6id: 19.5.1.1
includes: [propertyHelper.js]
---*/

var message = "my-message";
var error = new Error(message);

verifyEqualTo(error, "message", message);
verifyNotEnumerable(error, "message");
verifyWritable(error, "message");
verifyConfigurable(error, "message");
