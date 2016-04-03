// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    RegExp.prototype.exec behavior depends on global property.
    Let global is true and let I = If ToInteger(lastIndex).
    Then if I<0 orI>length then set lastIndex to 0 and return null
es5id: 15.10.6.2_A5_T2
description: "Set lastIndex to 100 and call /(?:ab|cd)\\d?/g.exec(\"aacd22 \")"
---*/

var __re = /(?:ab|cd)\d?/g;
__re.lastIndex=100;
var __executed = __re.exec("aacd22 ");

//CHECK#1
if (__executed) {
	$ERROR('#1: __re = /(?:ab|cd)\\d?/g; __re.lastIndex=100; __executed = __re.exec("aacd22 "); __executed === true');
}

//CHECK#2
if (__re.lastIndex !== 0) {
	$ERROR('#2: __re = /(?:ab|cd)\\d?/g; __re.lastIndex=100; __executed = __re.exec("aacd22 "); __re.lastIndex === 0. Actual: ' + (__re.lastIndex));
}
