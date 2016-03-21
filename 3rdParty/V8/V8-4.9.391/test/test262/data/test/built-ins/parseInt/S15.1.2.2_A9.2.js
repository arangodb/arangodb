// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of parseInt does not have the attribute DontDelete
es5id: 15.1.2.2_A9.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (parseInt.hasOwnProperty('length') !== true) {
  $ERROR('#1: parseInt.hasOwnProperty(\'length\') === true. Actual: ' + (parseInt.hasOwnProperty('length')));
}

delete parseInt.length;

//CHECK#2
if (parseInt.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete parseInt.length; parseInt.hasOwnProperty(\'length\') === false. Actual: ' + (parseInt.hasOwnProperty('length')));
}

//CHECK#3
if (parseInt.length === undefined) {
  $ERROR('#3: delete parseInt.length; parseInt.length !== undefined');
}
