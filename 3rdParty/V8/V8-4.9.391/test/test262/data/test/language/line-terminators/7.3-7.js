// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.3-7
description: >
    7.3 - ES5 recognizes the character <LS> (\u2028) as terminating
    regular expression literals
---*/


assert.throws(SyntaxError, function() {
            eval("var regExp =  /[\u2028]/");
});
