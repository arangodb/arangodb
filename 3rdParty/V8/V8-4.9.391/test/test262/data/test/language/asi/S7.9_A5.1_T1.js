// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Check Postfix Increment Operator for automatic semicolon insertion
es5id: 7.9_A5.1_T1
description: Try use Variable \n ++ construction
negative: SyntaxError
---*/

//CHECK#1
var x = 0;
x
++;
$ERROR('#1: Check Postfix Increment Operator for automatic semicolon insertion');
