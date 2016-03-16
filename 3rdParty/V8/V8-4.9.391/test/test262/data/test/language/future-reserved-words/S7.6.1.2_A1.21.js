// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "package" token can not be used as identifier in strict code
es5id: 7.6.1.2_A1.21
description: Checking if execution of "package=1" fails in strict code
negative: SyntaxError
flags: [onlyStrict]
---*/

var package = 1;
