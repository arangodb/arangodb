// Copyright 2011 Google Inc.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    A strict delete should either succeed, returning true, or it
    should fail by throwing a TypeError. Under no circumstances
    should a strict delete return false.
es5id: 11.4.1_A5
description: >
    See if a strict delete returns false when deleting a  non-standard
    property.
flags: [onlyStrict]
---*/

var reNames = Object.getOwnPropertyNames(RegExp);
for (var i = 0, len = reNames.length; i < len; i++) {
  var reName = reNames[i];
  if (reName !== 'prototype') {
    var deleted = 'unassigned';
    try {
      deleted = delete RegExp[reName];
    } catch (err) {
      if (!(err instanceof TypeError)) {
        $ERROR('#1: strict delete threw a non-TypeError: ' + err);
      }
      // fall through
    }
    if (deleted === false) {
      $ERROR('#2: Strict delete returned false');
    }
  }
}
