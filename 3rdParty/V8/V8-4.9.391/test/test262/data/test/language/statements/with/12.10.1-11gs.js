// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 12.10.1-11gs
description: Strict Mode - SyntaxError is thrown when using with statement
negative: SyntaxError
flags: [onlyStrict]
---*/

throw NotEarlyError;
with ({}) { }
