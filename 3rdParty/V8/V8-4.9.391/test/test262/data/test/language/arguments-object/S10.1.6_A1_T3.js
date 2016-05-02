// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The activation object is initialised with a property with name arguments
    and attributes {DontDelete}
es5id: 10.1.6_A1_T3
description: Checking function which returns "this"
flags: [noStrict]
---*/

function f1() {
  if (delete arguments) {
    $ERROR("#1: Function parameters have attribute {DontDelete}" + arguments);
  }
  return arguments;
}

f1();
