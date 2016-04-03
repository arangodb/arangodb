// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of encodeURI does not have the attribute DontDelete
es5id: 15.1.3.3_A5.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (encodeURI.hasOwnProperty('length') !== true) {
  $ERROR('#1: encodeURI.hasOwnProperty(\'length\') === true. Actual: ' + (encodeURI.hasOwnProperty('length')));
}

delete encodeURI.length;

//CHECK#2
if (encodeURI.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete encodeURI.length; encodeURI.hasOwnProperty(\'length\') === false. Actual: ' + (encodeURI.hasOwnProperty('length')));
}

//CHECK#3
if (encodeURI.length === undefined) {
  $ERROR('#3: delete encodeURI.length; encodeURI.length !== undefined');
}
