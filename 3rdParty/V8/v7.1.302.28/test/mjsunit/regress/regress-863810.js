// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-liftoff --no-wasm-tier-up --no-future --debug-code

load('test/mjsunit/wasm/wasm-constants.js');
load('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('main', kSig_i_v)
  .addBody([
    kExprI64Const, 0xa3, 0x82, 0x83, 0x86, 0x8c, 0xd8, 0xae, 0xb5, 0x40,
    kExprI32ConvertI64,
    kExprI32Const, 0x00,
    kExprI32Sub,
  ]).exportFunc();
const instance = builder.instantiate();
print(instance.exports.main(1, 2, 3));
