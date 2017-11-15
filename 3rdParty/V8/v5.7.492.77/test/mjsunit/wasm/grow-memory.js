// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --stress-compaction

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

var kPageSize = 0x10000;
var kV8MaxPages = 16384;

function genGrowMemoryBuilder() {
  var builder = new WasmModuleBuilder();
  builder.addFunction("grow_memory", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprGrowMemory, kMemoryZero])
      .exportFunc();
  builder.addFunction("load", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem, 0, 0])
      .exportFunc();
  builder.addFunction("store", kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem, 0, 0,
                kExprGetLocal, 1])
      .exportFunc();
  builder.addFunction("load16", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem16U, 0, 0])
      .exportFunc();
  builder.addFunction("store16", kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem16, 0, 0,
                kExprGetLocal, 1])
      .exportFunc();
  builder.addFunction("load8", kSig_i_i)
      .addBody([kExprGetLocal, 0, kExprI32LoadMem8U, 0, 0])
      .exportFunc();
  builder.addFunction("store8", kSig_i_ii)
      .addBody([kExprGetLocal, 0, kExprGetLocal, 1, kExprI32StoreMem8, 0, 0,
                kExprGetLocal, 1])
      .exportFunc();
  return builder;
}

// TODO(gdeepti): Generate tests programatically for all the sizes instead of
// current implementation.
function testGrowMemoryReadWrite32() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 4); offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize - 3; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(3));

  for (offset = kPageSize; offset <= 4*kPageSize -4; offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 4*kPageSize - 3; offset < 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(15));

  for (offset = 4*kPageSize - 3; offset <= 4*kPageSize + 4; offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - 4; offset+=4) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 3; offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testGrowMemoryReadWrite32();

function testGrowMemoryReadWrite16() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load16(offset); }
  function poke(value) { return module.exports.store16(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 2); offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize - 1; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(3));

  for (offset = kPageSize; offset <= 4*kPageSize -2; offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 4*kPageSize - 1; offset < 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(15));

  for (offset = 4*kPageSize - 2; offset <= 4*kPageSize + 4; offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - 2; offset+=2) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 1; offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testGrowMemoryReadWrite16();

function testGrowMemoryReadWrite8() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load8(offset); }
  function poke(value) { return module.exports.store8(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= kPageSize - 1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = kPageSize; offset < kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(1, growMem(3));

  for (offset = kPageSize; offset <= 4*kPageSize -1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 4*kPageSize; offset < 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  assertEquals(4, growMem(15));

  for (offset = 4*kPageSize; offset <= 4*kPageSize + 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize - 10; offset <= 19*kPageSize - 1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }
  for (offset = 19*kPageSize; offset < 19*kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testGrowMemoryReadWrite8();

function testGrowMemoryZeroInitialSize() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(0, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 3; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }

  offset = 3*kPageSize;
  for (var i = 1; i < 4; i++) {
    assertTraps(kTrapMemOutOfBounds, poke);
    assertEquals(i, growMem(1));
  }
  poke(20);
  assertEquals(20, peek());
}

testGrowMemoryZeroInitialSize();

function testGrowMemoryZeroInitialSize32() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(0, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 4; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 3; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testGrowMemoryZeroInitialSize32();

function testGrowMemoryZeroInitialSize16() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(0, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load16(offset); }
  function poke(value) { return module.exports.store16(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 2; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize - 1; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testGrowMemoryZeroInitialSize16();

function testGrowMemoryZeroInitialSize8() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(0, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset;
  function peek() { return module.exports.load8(offset); }
  function poke(value) { return module.exports.store8(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  assertTraps(kTrapMemOutOfBounds, peek);
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(0, growMem(1));

  for(offset = 0; offset <= kPageSize - 1; offset++) {
    poke(20);
    assertEquals(20, peek());
  }

  for(offset = kPageSize; offset <= kPageSize + 5; offset++) {
    assertTraps(kTrapMemOutOfBounds, peek);
  }
}

testGrowMemoryZeroInitialSize8();

function testGrowMemoryTrapMaxPagesZeroInitialMemory() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(0, kV8MaxPages, false);
  var module = builder.instantiate();
  var maxPages = 16385;
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(-1, growMem(maxPages));
}

testGrowMemoryTrapMaxPagesZeroInitialMemory();

function testGrowMemoryTrapMaxPages() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, 1, false);
  var module = builder.instantiate();
  var maxPages = 16384;
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(-1, growMem(maxPages));
}

testGrowMemoryTrapMaxPages();

function testGrowMemoryTrapsWithNonSmiInput() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(0, kV8MaxPages, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  // The parameter of grow_memory is unsigned. Therefore -1 stands for
  // UINT32_MIN, which cannot be represented as SMI.
  assertEquals(-1, growMem(-1));
};

testGrowMemoryTrapsWithNonSmiInput();

function testGrowMemoryCurrentMemory() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  builder.addFunction("memory_size", kSig_i_v)
      .addBody([kExprMemorySize, kMemoryZero])
      .exportFunc();
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  function MemSize() { return module.exports.memory_size(); }
  assertEquals(1, MemSize());
  assertEquals(1, growMem(1));
  assertEquals(2, MemSize());
}

testGrowMemoryCurrentMemory();

function testGrowMemoryPreservesDataMemOp32() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 4); offset+=4) {
    poke(100000 - offset);
    assertEquals(100000 - offset, peek());
  }

  assertEquals(1, growMem(3));

  for(offset = 0; offset <= (kPageSize - 4); offset+=4) {
    assertEquals(100000 - offset, peek());
  }
}

testGrowMemoryPreservesDataMemOp32();

function testGrowMemoryPreservesDataMemOp16() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load16(offset); }
  function poke(value) { return module.exports.store16(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 2); offset+=2) {
    poke(65535 - offset);
    assertEquals(65535 - offset, peek());
  }

  assertEquals(1, growMem(3));

  for(offset = 0; offset <= (kPageSize - 2); offset+=2) {
    assertEquals(65535 - offset, peek());
  }
}

testGrowMemoryPreservesDataMemOp16();

function testGrowMemoryPreservesDataMemOp8() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset, val = 0;
  function peek() { return module.exports.load8(offset); }
  function poke(value) { return module.exports.store8(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  for(offset = 0; offset <= (kPageSize - 1); offset++, val++) {
    poke(val);
    assertEquals(val, peek());
    if (val == 255) val = 0;
  }

  assertEquals(1, growMem(3));

  val = 0;

  for(offset = 0; offset <= (kPageSize - 1); offset++, val++) {
    assertEquals(val, peek());
    if (val == 255) val = 0;
  }
}

testGrowMemoryPreservesDataMemOp8();

function testGrowMemoryOutOfBoundsOffset() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, kV8MaxPages, false);
  var module = builder.instantiate();
  var offset, val;
  function peek() { return module.exports.load(offset); }
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }

  offset = 3*kPageSize + 4;
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(1, growMem(1));
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(2, growMem(1));
  assertTraps(kTrapMemOutOfBounds, poke);

  assertEquals(3, growMem(1));

  for (offset = 3*kPageSize; offset <= 4*kPageSize - 4; offset++) {
    poke(0xaced);
    assertEquals(0xaced, peek());
  }

  for (offset = 4*kPageSize - 3; offset <= 4*kPageSize + 4; offset++) {
    assertTraps(kTrapMemOutOfBounds, poke);
  }
}

testGrowMemoryOutOfBoundsOffset();

function testGrowMemoryOutOfBoundsOffset2() {
  var builder = new WasmModuleBuilder();
  builder.addMemory(16, 128, false);
  builder.addFunction("main", kSig_v_v)
      .addBody([
          kExprI32Const, 20,
          kExprI32Const, 29,
          kExprGrowMemory, kMemoryZero,
          kExprI32StoreMem, 0, 0xFF, 0xFF, 0xFF, 0x3a
          ])
      .exportAs("main");
  var module = builder.instantiate();
  assertTraps(kTrapMemOutOfBounds, module.exports.main);
}

testGrowMemoryOutOfBoundsOffset2();

function testGrowMemoryDeclaredMaxTraps() {
  var builder = genGrowMemoryBuilder();
  builder.addMemory(1, 16, false);
  var module = builder.instantiate();
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(1, growMem(5));
  assertEquals(6, growMem(5));
  assertEquals(-1, growMem(6));
}

testGrowMemoryDeclaredMaxTraps();

function testGrowMemoryDeclaredSpecMaxTraps() {
  // The spec maximum is higher than the internal V8 maximum. This test only
  // checks that grow_memory does not grow past the internally defined maximum
  // to reflect the currentl implementation.
  var builder = genGrowMemoryBuilder();
  var kSpecMaxPages = 65535;
  builder.addMemory(1, kSpecMaxPages, false);
  var module = builder.instantiate();
  function poke(value) { return module.exports.store(offset, value); }
  function growMem(pages) { return module.exports.grow_memory(pages); }
  assertEquals(1, growMem(20));
  assertEquals(-1, growMem(kV8MaxPages - 20));
}

testGrowMemoryDeclaredSpecMaxTraps();
