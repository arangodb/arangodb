// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 14.2.1
description: >
    ArrowParameters[Yield] :
      BindingIdentifier[?Yield]
      ...

    No parameter named "arguments"

negative: SyntaxError
flags: [onlyStrict]
---*/
var af = arguments => 1;
