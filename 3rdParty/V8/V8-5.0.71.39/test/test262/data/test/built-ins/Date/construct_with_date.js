// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Date constructor called with Date object
info: >
  20.3.2.2 Date ( value )

  ...
  3. If NewTarget is not undefined, then
    a. If Type(value) is Object and value has a [[DateValue]] internal slot, then
      i. Let tv be thisTimeValue(value).
es6id: 20.3.2.2
---*/

var dateValue = 1438560000000;

var oldDate = new Date(dateValue);
oldDate.toString = function() {
  $ERROR("toString() method called");
};
oldDate.valueOf = function() {
  $ERROR("valueOf() method called");
};

var newDate = new Date(oldDate);

assert.sameValue(newDate.getTime(), dateValue, "Same date value");
