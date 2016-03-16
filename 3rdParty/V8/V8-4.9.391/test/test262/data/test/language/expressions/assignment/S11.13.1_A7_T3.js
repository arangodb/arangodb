// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Assignment Operator evaluates its operands from left to right.
description: >
    The left-hand side expression is evaluated before the right-hand side.
    Left-hand side expression is MemberExpression: base[prop]. Evaluating
    ToPropertyKey(prop) throws an error.
---*/

function DummyError() { }

assert.throws(DummyError, function() {
  var base = {};
  var prop = {
    toString: function() {
      throw new DummyError();
    }
  };
  var expr = function() {
    $ERROR("right-hand side expression evaluated");
  };

  base[prop] = expr();
});
