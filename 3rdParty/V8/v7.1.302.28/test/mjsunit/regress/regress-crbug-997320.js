// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy --stress-lazy-source-positions
// Flags: --enable-lazy-source-positions

async(a, b = a) => {};
