// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.3-9
description: >
    7.3 - ES5 recognizes the character <LS> (\u2028) as a
    NonEscapeCharacter
---*/


assert.throws(SyntaxError, function() {
            eval("var prop = \\u2028;");
});
