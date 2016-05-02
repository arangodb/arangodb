// Copyright 2011 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Call replaceValue passing undefined as the this value
es5id: 15.5.4.11_A12
description: replaceValue tests that its this value is undefined
flags: [noStrict]
---*/

var global = this;
'x'.replace(/x/, function() {
  "use strict";

  if (this === global) {
    $ERROR('#1: String replace leaks global');
  }
  if (this !== undefined) {
    $ERROR('#2: replaceValue should be called with this===undefined. ' +
          'Actual: ' + this);
  }
  return 'y';
});
