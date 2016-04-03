// Copyright (C) Copyright 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 14.5.15
description: >
    Function `name` attribute not inferred in presence of static `name` method
info: >
    ClassDeclaration : class BindingIdentifier ClassTail

    [...]
    4. Let hasNameProperty be HasOwnProperty(value, "name").
    5. ReturnIfAbrupt(hasNameProperty).
    6. If hasNameProperty is false, then perform SetFunctionName(value,
       className).
includes: [propertyHelper.js]
---*/

class A {
  static name() {
    $ERROR('Static method should not be executed during definition');
  }
}

assert.sameValue(typeof A.name, 'function');

var attr = 'name';
class B {
  static [attr]() {
    $ERROR(
      'Static method defined via computed property should not be executed ' +
      'during definition'
    );
  }
}

assert.sameValue(typeof B.name, 'function');

class C {
  static get name() {
    $ERROR('Static `get` accessor should not be executed during definition');
  }
}

assert.throws(Test262Error, function() {
  C.name;
});

class D {
  static set name(_) {
    $ERROR('Static `set` accessor should not be executed during definition');
  }
}

assert.sameValue(D.name, undefined);

class E {
  static *name() {
    $ERROR('Static GeneratorMethod should not be executed during definition');
  }
}

assert.sameValue(typeof E.name, 'function');
