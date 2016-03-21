// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.1
description: >
  Rest parameters can not have a default
negative: SyntaxError
---*/

function fn(...args = [1]) {}
