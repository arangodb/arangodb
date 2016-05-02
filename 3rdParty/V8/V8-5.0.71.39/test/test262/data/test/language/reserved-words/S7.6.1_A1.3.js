// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "false" token can not be used as identifier
es5id: 7.6.1_A1.3
description: Checking if execution of "false=1" fails
negative: ReferenceError
---*/

false = 1;
