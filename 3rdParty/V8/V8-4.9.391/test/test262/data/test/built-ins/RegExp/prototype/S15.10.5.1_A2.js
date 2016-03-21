// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype property has the attribute DontEnum
es5id: 15.10.5.1_A2
description: Checking if enumerating the RegExp.prototype property fails
---*/

//CHECK#0
if (RegExp.hasOwnProperty('prototype') !== true) {
	$ERROR('#0: RegExp.hasOwnProperty(\'prototype\') === true');
}

 //CHECK#1
if (RegExp.propertyIsEnumerable('prototype') !== false) {
	$ERROR('#1: RegExp.propertyIsEnumerable(\'prototype\') === false');
}

 //CHECK#2
var count=0;
for (var p in RegExp){
	if (p==="prototype") count++;
}

if (count !== 0) {
	$ERROR('#2: count=0; for (p in RegExp){ if (p==="prototype") count++; } count === 0. Actual: ' + (count));
}
