// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Single line comments can not contain LINE SEPARATOR (U+2028) inside
es5id: 7.3_A3.3_T2
description: Insert LINE SEPARATOR (\u2028) into begin of single line comment
negative: SyntaxError
---*/

// CHECK#1
eval("//\u2028 single line comment");
