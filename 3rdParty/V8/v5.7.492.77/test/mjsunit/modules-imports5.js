// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// MODULE

import {a} from "modules-skip-3.js";
export var b = 20;
assertEquals(42, a+b);
