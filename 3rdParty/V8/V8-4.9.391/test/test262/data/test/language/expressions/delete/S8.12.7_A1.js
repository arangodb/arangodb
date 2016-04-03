// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    When the [[Delete]] method of O is called with property name P,
    and If the property has the DontDelete attribute, return false
es5id: 8.12.7_A1
description: Try to delete Math.E, that has the DontDelete attribute
flags: [noStrict]
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (delete Math.E !== false){
  $ERROR('#1: delete Math.E === false. Actual: ' + (delete Math.E));
};
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (Math.E === undefined){
  $ERROR('#2: delete Math.E; Math.E !== undefined');
};
//
//////////////////////////////////////////////////////////////////////////////
