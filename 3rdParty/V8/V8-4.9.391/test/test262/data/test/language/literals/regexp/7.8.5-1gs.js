// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 7.8.5-1gs
description: Empty literal RegExp should result in a SyntaxError
negative: SyntaxError
---*/

throw NotEarlyError;
var re = //;
