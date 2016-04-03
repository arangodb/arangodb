// Copyright 2015 Microsoft Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
description: Testing descriptor property of Object.assign
includes:
    - propertyHelper.js
es6id: 19.1.2.1
---*/

verifyWritable(Object, "assign");
verifyNotEnumerable(Object, "assign");
verifyConfigurable(Object, "assign");
