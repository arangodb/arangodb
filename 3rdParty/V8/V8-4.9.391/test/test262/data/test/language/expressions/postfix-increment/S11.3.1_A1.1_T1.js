// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Line Terminator between LeftHandSideExpression and "++" is not allowed
es5id: 11.3.1_A1.1_T1
description: Checking Line Feed
negative: SyntaxError
---*/

//CHECK#1
eval("var x = 1; x\u000A++");
