// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The RegExp.prototype property has the attribute DontDelete
es5id: 15.10.5.1_A3
description: Checking if deleting the RegExp.prototype property fails
includes: [propertyHelper.js]
---*/

//CHECK#0
if (RegExp.hasOwnProperty('prototype') !== true) {
	$ERROR('#0: RegExp.hasOwnProperty(\'prototype\') === true');
}

verifyNotConfigurable(RegExp, "prototype");

//CHECK#1
try {
  if (delete RegExp.prototype !== false) {
    $ERROR('#1: delete RegExp.prototype === false');
  }
} catch (e) {
  if (e instanceof Test262Error) throw e;
  assert(e instanceof TypeError);
}

//CHECK#2
if (RegExp.hasOwnProperty('prototype') !== true) {
	$ERROR('#2: delete RegExp.prototype; RegExp.hasOwnProperty(\'prototype\') === true');
}
