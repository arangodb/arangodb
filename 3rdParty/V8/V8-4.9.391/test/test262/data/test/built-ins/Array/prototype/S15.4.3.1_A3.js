// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Array.prototype property has the attribute DontDelete
es5id: 15.4.3.1_A3
description: Checking if deleting the Array.prototype property fails
includes: [propertyHelper.js]
---*/

//CHECK#1
if (Array.hasOwnProperty('prototype') !== true) {
	$ERROR('#1: Array.hasOwnProperty(\'prototype\') === true. Actual: ' + (Array.hasOwnProperty('prototype')));
}

verifyNotConfigurable(Array, "prototype");

//CHECK#2
try {
  if((delete Array.prototype) !== false){
    $ERROR('#2: Array.prototype has the attribute DontDelete');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}

//CHECK#3
if (Array.hasOwnProperty('prototype') !== true) {
	$ERROR('#3: delete Array.prototype; Array.hasOwnProperty(\'prototype\') === true. Actual: ' + (Array.hasOwnProperty('prototype')));
}

//CHECK#4
if (Array.prototype === undefined) {
  $ERROR('#4: delete Array.prototype; Array.prototype !== undefined');
}
