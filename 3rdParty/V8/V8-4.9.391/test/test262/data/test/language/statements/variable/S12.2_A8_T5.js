// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Only AssignmentExpression is admitted when variable is initialized
es5id: 12.2_A8_T5
description: Checking if execution of "var --x" fails
negative: SyntaxError
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
var --x;
//
//////////////////////////////////////////////////////////////////////////////
