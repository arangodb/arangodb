// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-filter=*

function foo() {
  with ({ value:"fooed" }) { return value; }
}

%OptimizeFunctionOnNextCall(foo);
assertEquals("fooed", foo());
assertOptimized(foo);

function bar() {
  with ({ value:"bared" }) { return value; }
}

assertEquals("bared", bar());
%OptimizeFunctionOnNextCall(bar);
assertEquals("bared", bar());
assertOptimized(bar);
