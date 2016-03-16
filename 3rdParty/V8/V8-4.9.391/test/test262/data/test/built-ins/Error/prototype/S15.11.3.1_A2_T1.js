// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Error.prototype property has the attributes {DontEnum}
es5id: 15.11.3.1_A2_T1
description: Checking if enumerating the Error.prototype property fails
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#0
if (!(Error.hasOwnProperty('prototype'))) {
  $ERROR('#0: Error.hasOwnProperty(\'prototype\') return true. Actual: '+Error.hasOwnProperty('prototype'));
}
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// CHECK#1
if (Error.propertyIsEnumerable('prototype')) {
  $ERROR('#1: Error.propertyIsEnumerable(\'prototype\') return false. Actual: '+Error.propertyIsEnumerable('prototype'));
}
//
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// CHECK#2
var cout=0;

for (var p in Error){
  if (p==="prototype") cout++;
}

if (cout !== 0) {
  $ERROR('#2: cout === 0. Actual: '+cout );
}
//
//////////////////////////////////////////////////////////////////////////////
