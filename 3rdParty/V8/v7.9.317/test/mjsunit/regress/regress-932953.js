// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function NonExtensibleBetweenSetterAndGetter() {
  o = {};
  o.x = 42;
  o.__defineGetter__('y', function() {});
  Object.preventExtensions(o);
  o.__defineSetter__('y', function() {});
  o.x = 0.1;
})();

(function InterleavedIntegrityLevel() {
  o = {};
  o.x = 42;
  o.__defineSetter__('y', function() {});
  Object.preventExtensions(o);
  o.__defineGetter__('y', function() {
    return 44;
  });
  Object.seal(o);
  o.x = 0.1;
  assertEquals(44, o.y);
})();

(function TryUpdateRepeatedIntegrityLevel() {
  function C() {
    this.x = 0;
    this.x = 1;
    Object.preventExtensions(this);
    Object.seal(this);
  }

  const o1 = new C();
  const o2 = new C();
  const o3 = new C();

  function f(o) {
    return o.x;
  }

  // Warm up the IC.
  ;
  %PrepareFunctionForOptimization(f);
  f(o1);
  f(o1);
  f(o1);

  // Reconfigure to double field.
  o3.x = 0.1;

  // Migrate o2 to the new shape.
  f(o2);

  %OptimizeFunctionOnNextCall(f);
  f(o1);

  assertTrue(%HaveSameMap(o1, o2));
  assertTrue(%HaveSameMap(o1, o3));
})();
