// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 20.1.1.1
description: Invalid octal literals yield NaN
info: >
    OctalIntegerLiteral ::
      0o OctalDigits
      0O OctalDigits
    OctalDigits ::
      OctalDigit
      OctalDigits OctalDigit
    OctalDigit :: one of
      0 1 2 3 4 5 6 7
---*/

assert.sameValue(Number('0o8'), NaN, 'invalid digit');
assert.sameValue(Number('00o0'), NaN, 'leading zero');
assert.sameValue(Number('0o'), NaN, 'omitted digits');
