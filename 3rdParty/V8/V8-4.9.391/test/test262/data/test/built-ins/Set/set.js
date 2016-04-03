// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 23.2.1.1
description: >
    Set ( [ iterable ] )

    17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

verifyNotEnumerable(this, "Set");
verifyWritable(this, "Set");
verifyConfigurable(this, "Set");
