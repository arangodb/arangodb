// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "implements" token can not be used as identifier in strict code
es5id: 7.6.1.2_A1.15
description: Checking if execution of "implements=1" fails in strict code
negative: SyntaxError
flags: [onlyStrict]
---*/

var implements = 1;
