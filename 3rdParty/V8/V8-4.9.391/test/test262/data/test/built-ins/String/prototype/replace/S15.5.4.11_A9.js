// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The String.prototype.replace.length property does not have the attribute
    DontDelete
es5id: 15.5.4.11_A9
description: >
    Checking if deleting the String.prototype.replace.length property
    fails
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#0
if (!(String.prototype.replace.hasOwnProperty('length'))) {
  $ERROR('#0: String.prototype.replace.hasOwnProperty(\'length\') return true. Actual: '+String.prototype.replace.hasOwnProperty('length'));
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (!delete String.prototype.replace.length) {
  $ERROR('#1: delete String.prototype.replace.length return true!');
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (String.prototype.replace.hasOwnProperty('length')) {
  $ERROR('#2: delete String.prototype.replace.length; String.prototype.replace.hasOwnProperty(\'length\') return false. Actual: '+String.prototype.replace.hasOwnProperty('length'));
}
//
//////////////////////////////////////////////////////////////////////////////
