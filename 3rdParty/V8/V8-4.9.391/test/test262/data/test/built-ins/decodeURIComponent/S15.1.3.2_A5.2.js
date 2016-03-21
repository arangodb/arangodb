// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The length property of decodeURIComponent does not have the attribute
    DontDelete
es5id: 15.1.3.2_A5.2
description: Checking use hasOwnProperty, delete
---*/

//CHECK#1
if (decodeURIComponent.hasOwnProperty('length') !== true) {
  $ERROR('#1: decodeURIComponent.hasOwnProperty(\'length\') === true. Actual: ' + (decodeURIComponent.hasOwnProperty('length')));
}

delete decodeURIComponent.length;

//CHECK#2
if (decodeURIComponent.hasOwnProperty('length') !== false) {
  $ERROR('#2: delete decodeURIComponent.length; decodeURIComponent.hasOwnProperty(\'length\') === false. Actual: ' + (decodeURIComponent.hasOwnProperty('length')));
}

//CHECK#3
if (decodeURIComponent.length === undefined) {
  $ERROR('#3: delete decodeURIComponent.length; decodeURIComponent.length !== undefined');
}
