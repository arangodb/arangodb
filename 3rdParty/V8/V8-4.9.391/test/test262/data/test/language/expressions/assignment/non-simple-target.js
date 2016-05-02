// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    It is an early Reference Error if LeftHandSideExpression is neither an
    ObjectLiteral nor an ArrayLiteral and IsValidSimpleAssignmentTarget of
    LeftHandSideExpression is false.
es6id: 12.14.1
description: Assignment with non-simple target
negative: ReferenceError
---*/

1 = 1;
