// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.1.5-1gs
description: >
    Strict Mode - SyntaxError is thrown when 'eval' occurs as the
    Identifier in a PropertySetParameterList of a PropertyAssignment
    that is contained in strict code
negative: SyntaxError
flags: [onlyStrict]
---*/

throw NotEarlyError;
var obj = { set _11_1_5_1_fun(eval) {}};
