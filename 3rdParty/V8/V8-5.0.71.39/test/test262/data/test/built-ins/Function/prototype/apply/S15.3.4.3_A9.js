// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The Function.prototype.apply.length property does not have the attribute
    DontDelete
es5id: 15.3.4.3_A9
description: >
    Checking if deleting the Function.prototype.apply.length property
    fails
---*/

//CHECK#0
if (!(Function.prototype.apply.hasOwnProperty('length'))) {
  $ERROR('#0: the Function.prototype.apply has length property');
}

//CHECK#1
if (!delete Function.prototype.apply.length) {
  $ERROR('#1: The Function.prototype.apply.length property does not have the attributes DontDelete');
}

//CHECK#2
if (Function.prototype.apply.hasOwnProperty('length')) {
  $ERROR('#2: The Function.prototype.apply.length property does not have the attributes DontDelete');
}
