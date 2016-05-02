// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Result of appying "typeof" operator to string is "string"
es5id: 11.4.3_A3.5
es6id: 12.5.6.1
description: typeof (string value) === "string"
---*/

assert.sameValue(
  typeof "1",
  "string",
  '#1: typeof "1" === "string". Actual: ' + (typeof "1")
);

assert.sameValue(
  typeof "NaN",
  "string",
  '#2: typeof "NaN" === "string". Actual: ' + (typeof "NaN")
);

assert.sameValue(
  typeof "Infinity",
  "string",
  '#3: typeof "Infinity" === "string". Actual: ' + (typeof "Infinity")
);

assert.sameValue(
  typeof "",
  "string",
  '#4: typeof "" === "string". Actual: ' + (typeof "")
);

assert.sameValue(
  typeof "true",
  "string",
  '#5: typeof "true" === "string". Actual: ' + (typeof "true")
);

assert.sameValue(
  typeof Date(),
  "string",
  '#6: typeof Date() === "string". Actual: ' + (typeof Date())
);
