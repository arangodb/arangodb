// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 10.5-1gs
description: Strict Mode - arguments cannot be assigned to in a strict function
negative: SyntaxError
flags: [onlyStrict]
---*/

throw NotEarlyError;

function f_10_5_1_gs(){
    arguments = 7;
}
