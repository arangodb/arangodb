// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.indexOf(searchString, position)
es5id: 15.5.4.7_A1_T11
description: Instance is Date(0) object
---*/

var __instance = new Date(0);

__instance.indexOf = String.prototype.indexOf;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if ((__instance.getTimezoneOffset()>0 ? __instance.indexOf('31') : __instance.indexOf('01')) !== 8) {
  $ERROR('#1: __instance = new Date(0); __instance.indexOf = String.prototype.indexOf;  (__instance.getTimezoneOffset()>0 ? __instance.indexOf(\'31\') : __instance.indexOf(\'01\')) === 8. Actual: '+(__instance.getTimezoneOffset()>0 ? __instance.indexOf('31') : __instance.indexOf('01')) ); 
}
//
//////////////////////////////////////////////////////////////////////////////
