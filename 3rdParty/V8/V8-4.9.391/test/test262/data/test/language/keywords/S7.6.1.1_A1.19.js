// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "throw" token can not be used as identifier
es5id: 7.6.1.1_A1.19
description: Checking if execution of "throw=1" fails
negative: SyntaxError
---*/

throw = 1;
