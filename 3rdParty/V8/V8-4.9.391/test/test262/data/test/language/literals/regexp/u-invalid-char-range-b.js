// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Invalid character class range with `u` flag (upper set size > 1)
info: >
    21.2.2.15.1 Runtime Semantics: CharacterRange Abstract Operation

    1. If A does not contain exactly one character or B does not contain
       exactly one character, throw a SyntaxError exception.
es6id: 21.2.2.15.1
negative: SyntaxError
---*/

/[a-\w]/u;
