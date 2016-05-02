// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    Being in function code, "this" and eval("this"), called as a functions,
    return the global object
es5id: 11.1.1_A3.1
description: Creating function which returns "this" or eval("this")
flags: [noStrict]
---*/

//CHECK#1
function MyFunction() {return this}
if (MyFunction() !== this) {
  $ERROR('#1: function MyFunction() {return this} MyFunction() === this. Actual: ' + (MyFunction()));
}

//CHECK#2
function MyFunction() {return eval("this")}
if (MyFunction() !== this) {
  $ERROR('#2: function MyFunction() {return eval("this")} MyFunction() === this. Actual: ' + (MyFunction()));
}
