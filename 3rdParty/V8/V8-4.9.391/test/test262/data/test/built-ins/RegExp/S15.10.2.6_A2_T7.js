// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    The production Assertion :: ^ evaluates by returning an internal
    AssertionTester closure that takes a State argument x and performs the ...
es5id: 15.10.2.6_A2_T7
description: Execute /^..^e/.test("ab\ncde") and check results
---*/

var __executed = /^..^e/.test("ab\ncde");

//CHECK#1
if (__executed) {
	$ERROR('#1: /^..^e/.test("ab\\ncde") === false');
}
