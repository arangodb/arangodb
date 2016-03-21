// Copyright (C) 2014 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 13.1
description: >
    Redeclaration error within strict mode function inside non-strict code.
negative: SyntaxError
flags: [noStrict]
---*/
(function() { 'use strict'; { const f = 1; var f; } })

