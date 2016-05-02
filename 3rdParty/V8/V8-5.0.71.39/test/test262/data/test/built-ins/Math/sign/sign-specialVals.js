// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Math.sign with special values
es6id: 20.2.2.29
---*/

assert.sameValue(Math.sign(NaN), Number.NaN,
    "Math.sign produces incorrect output for NaN");
assert.sameValue(1/Math.sign(-0), Number.NEGATIVE_INFINITY,
    "Math.sign produces incorrect output for -0");
assert.sameValue(1/Math.sign(0), Number.POSITIVE_INFINITY,
    "Math.sign produces incorrect output for 0");
