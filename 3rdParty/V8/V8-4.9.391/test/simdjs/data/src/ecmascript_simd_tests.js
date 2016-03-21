/*
  Copyright (C) 2013

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

function minNum(x, y) {
  return x != x ? y :
         y != y ? x :
         Math.min(x, y);
}

function maxNum(x, y) {
  return x != x ? y :
         y != y ? x :
         Math.max(x, y);
}

function sameValue(x, y) {
  if (x == y)
    return x != 0 || y != 0 || (1/x == 1/y);

  return x != x && y != y;
}

function sameValueZero(x, y) {
  if (x == y) return true;
  return x != x & y != y;
}

function binaryMul(a, b) { return a * b; }
var binaryImul;
if (typeof Math.imul !== 'undefined') {
  binaryImul = Math.imul;
} else {
  binaryImul = function(a, b) {
    var ah = (a >>> 16) & 0xffff;
    var al = a & 0xffff;
    var bh = (b >>> 16) & 0xffff;
    var bl = b & 0xffff;
    // the shift by 0 fixes the sign on the high part
    // the final |0 converts the unsigned value into a signed value
    return ((al * bl) + (((ah * bl + al * bh) << 16) >>> 0)|0);
  };
}

var _f32x4 = new Float32Array(4);
var _f64x2 = new Float64Array(_f32x4.buffer);
var _i32x4 = new Int32Array(_f32x4.buffer);
var _i16x8 = new Int16Array(_f32x4.buffer);
var _i8x16 = new Int8Array(_f32x4.buffer);
var _ui32x4 = new Uint32Array(_f32x4.buffer);
var _ui16x8 = new Uint16Array(_f32x4.buffer);
var _ui8x16 = new Uint8Array(_f32x4.buffer);

var float32x4 = {
  name: "Float32x4",
  fn: SIMD.Float32x4,
  floatLane: true,
  signed: true,
  numerical: true,
  lanes: 4,
  laneSize: 4,
  interestingValues: [0, -0, 1, -1, 1.414, 0x7F, -0x80, -0x8000, -0x80000000, 0x7FFF, 0x7FFFFFFF, Infinity, -Infinity, NaN],
  view: Float32Array,
  buffer: _f32x4,
  mulFn: binaryMul,
}

var int32x4 = {
  name: "Int32x4",
  fn: SIMD.Int32x4,
  intLane: true,
  signed: true,
  numerical: true,
  logical: true,
  lanes: 4,
  laneSize: 4,
  minVal: -0x80000000,
  maxVal: 0x7FFFFFFF,
  interestingValues: [0, 1, -1, 0x40000000, 0x7FFFFFFF, -0x80000000],
  view: Int32Array,
  buffer: _i32x4,
  mulFn: binaryImul,
}

var int16x8 = {
  name: "Int16x8",
  fn: SIMD.Int16x8,
  intLane: true,
  signed: true,
  numerical: true,
  logical: true,
  lanes: 8,
  laneSize: 2,
  laneMask: 0xFFFF,
  minVal: -0x8000,
  maxVal: 0x7FFF,
  interestingValues: [0, 1, -1, 0x4000, 0x7FFF, -0x8000],
  view: Int16Array,
  buffer: _i16x8,
  mulFn: binaryMul,
}

var int8x16 = {
  name: "Int8x16",
  fn: SIMD.Int8x16,
  intLane: true,
  signed: true,
  numerical: true,
  logical: true,
  lanes: 16,
  laneSize: 1,
  laneMask: 0xFF,
  minVal: -0x80,
  maxVal: 0x7F,
  interestingValues: [0, 1, -1, 0x40, 0x7F, -0x80],
  view: Int8Array,
  buffer: _i8x16,
  mulFn: binaryMul,
}

var uint32x4 = {
  name: "Uint32x4",
  fn: SIMD.Uint32x4,
  intLane: true,
  unsigned: true,
  numerical: true,
  logical: true,
  lanes: 4,
  laneSize: 4,
  minVal: 0,
  maxVal: 0xFFFFFFFF,
  interestingValues: [0, 1, 0x40000000, 0x7FFFFFFF, 0xFFFFFFFF],
  view: Uint32Array,
  buffer: _ui32x4,
  mulFn: binaryImul,
}

var uint16x8 = {
  name: "Uint16x8",
  fn: SIMD.Uint16x8,
  intLane: true,
  unsigned: true,
  numerical: true,
  logical: true,
  lanes: 8,
  laneSize: 2,
  laneMask: 0xFFFF,
  minVal: 0,
  maxVal: 0xFFFF,
  interestingValues: [0, 1, 0x4000, 0x7FFF, 0xFFFF],
  view: Uint16Array,
  buffer: _ui16x8,
  mulFn: binaryMul,
}

var uint8x16 = {
  name: "Uint8x16",
  fn: SIMD.Uint8x16,
  intLane: true,
  unsigned: true,
  numerical: true,
  logical: true,
  lanes: 16,
  laneSize: 1,
  laneMask: 0xFF,
  minVal: 0,
  maxVal: 0xFF,
  interestingValues: [0, 1, 0x40, 0x7F, 0xFF],
  view: Int8Array,
  buffer: _ui8x16,
  mulFn: binaryMul,
}

var bool32x4 = {
  name: "Bool32x4",
  fn: SIMD.Bool32x4,
  boolLane: true,
  lanes: 4,
  laneSize: 4,
  interestingValues: [true, false],
}

var bool16x8 = {
  name: "Bool16x8",
  fn: SIMD.Bool16x8,
  boolLane: true,
  lanes: 8,
  laneSize: 2,
  interestingValues: [true, false],
}

var bool8x16 = {
  name: "Bool8x16",
  fn: SIMD.Bool8x16,
  boolLane: true,
  lanes: 16,
  laneSize: 1,
  interestingValues: [true, false],
}

// Filter functions.
function isFloatType(type) { return type.floatLane; }
function isIntType(type) { return type.intLane; }
function isBoolType(type) { return type.boolLane; }
function isNumerical(type) { return type.numerical; }
function isLogical(type) { return type.logical; }
function isSigned(type) { return type.signed; }
function isSignedIntType(type) { return type.intLane && type.signed; }
function isUnsignedIntType(type) { return type.intLane && type.unsigned; }
function isSmallIntType(type) { return type.intLane && type.lanes >= 8; }
function isSmallUnsignedIntType(type) { return type.intLane && type.unsigned && type.lanes >= 8; }
function hasLoadStore123(type) { return !type.boolLane && type.lanes == 4; }

// Each SIMD type has a corresponding Boolean SIMD type, which is returned by
// relational ops.
float32x4.boolType = int32x4.boolType = uint32x4.boolType = bool32x4;
int16x8.boolType = uint16x8.boolType = bool16x8;
int8x16.boolType = uint8x16.boolType = bool8x16;

// SIMD fromTIMD types.
float32x4.from = [int32x4, uint32x4];
int32x4.from = [float32x4, uint32x4];
int16x8.from = [uint16x8];
int8x16.from = [uint8x16];
uint32x4.from = [float32x4, int32x4];
uint16x8.from = [int16x8];
uint8x16.from = [int8x16];

// SIMD fromBits types.
float32x4.fromBits = [int32x4, int16x8, int8x16, uint32x4, uint16x8, uint8x16];
int32x4.fromBits = [float32x4, int16x8, int8x16, uint32x4, uint16x8, uint8x16];
int16x8.fromBits = [float32x4, int32x4, int8x16, uint32x4, uint16x8, uint8x16];
int8x16.fromBits = [float32x4, int32x4, int16x8, uint32x4, uint16x8, uint8x16];
uint32x4.fromBits = [float32x4, int32x4, int16x8, int8x16, uint16x8, uint8x16];
uint16x8.fromBits = [float32x4, int32x4, int16x8, int8x16, uint32x4, uint8x16];
uint8x16.fromBits = [float32x4, int32x4, int16x8, int8x16, uint32x4, uint16x8];

var simdTypes = [float32x4,
                 int32x4, int16x8, int8x16,
                 uint32x4, uint16x8, uint8x16,
                 bool32x4, bool16x8, bool8x16];

if (typeof simdPhase2 !== 'undefined') {
  var float64x2 = {
    name: "Float64x2",
    fn: SIMD.Float64x2,
    floatLane: true,
    signed: true,
    numerical: true,
    lanes: 2,
    laneSize: 8,
    interestingValues: [0, -0, 1, -1, 1.414, 0x7F, -0x80, -0x8000, -0x80000000, 0x7FFF, 0x7FFFFFFF, Infinity, -Infinity, NaN],
    view: Float64Array,
    buffer: _f64x2,
    mulFn: binaryMul,
  }

  var bool64x2 = {
    name: "Bool64x2",
    fn: SIMD.Bool64x2,
    boolLane: true,
    lanes: 2,
    laneSize: 8,
    interestingValues: [true, false],
  }

  float64x2.boolType = bool64x2;

  float32x4.fromBits.push(float64x2);
  int32x4.fromBits.push(float64x2);
  int16x8.fromBits.push(float64x2);
  int8x16.fromBits.push(float64x2);
  uint32x4.fromBits.push(float64x2);
  uint16x8.fromBits.push(float64x2);
  uint8x16.fromBits.push(float64x2);

  float64x2.fromBits = [float32x4, int32x4, int16x8, int8x16,
                        uint32x4, uint16x8, uint8x16];

  int32x4.fromBits = [float32x4, int16x8, int8x16, uint32x4, uint16x8, uint8x16];
  int16x8.fromBits = [float32x4, int32x4, int8x16, uint32x4, uint16x8, uint8x16];
  int8x16.fromBits = [float32x4, int32x4, int16x8, uint32x4, uint16x8, uint8x16];
  uint32x4.fromBits = [float32x4, int32x4, int16x8, int8x16, uint16x8, uint8x16];
  uint16x8.fromBits = [float32x4, int32x4, int16x8, int8x16, uint32x4, uint8x16];
  uint8x16.fromBits = [float32x4, int32x4, int16x8, int8x16, uint32x4, uint16x8];

  simdTypes.push(float64x2);
  simdTypes.push(bool64x2);
}

// SIMD reference functions.

function simdConvert(type, value) {
  if (type.buffer === undefined) return !!value;  // bool types
  type.buffer[0] = value;
  return type.buffer[0];
}

// Reference implementation of toString.
function simdToString(type, value) {
  value = type.fn.check(value);
  var str = "SIMD." + type.name + "(";
  str += type.fn.extractLane(value, 0);
  for (var i = 1; i < type.lanes; i++) {
    str += ", " + type.fn.extractLane(value, i);
  }
  return str + ")";
}

// Reference implementation of toLocaleString.
function simdToLocaleString(type, value) {
  value = type.fn.check(value);
  var str = "SIMD." + type.name + "(";
  str += type.fn.extractLane(value, 0).toLocaleString();
  for (var i = 1; i < type.lanes; i++) {
    str += ", " + type.fn.extractLane(value, i).toLocaleString();
  }
  return str + ")";
}

// Utility functions.

// Create a value for testing, with vanilla lane values, i.e. [0, 1, 2, ..]
// for numeric types, [false, true, true, ..] for boolean types. These test
// values shouldn't contain NaNs or other "interesting" values.
function createTestValue(type) {
  var lanes = [];
  for (var i = 0; i < type.lanes; i++)
    lanes.push(i);
  return type.fn.apply(type.fn, lanes);
}

function createSplatValue(type, v) {
  var lanes = [];
  for (var i = 0; i < type.lanes; i++)
    lanes.push(v);
  return type.fn.apply(type.fn, lanes);
}

function checkValue(type, a, expect) {
  var ok = true;
  for (var i = 0; i < type.lanes; i++) {
    var v = type.fn.extractLane(a, i);
    var ev = simdConvert(type, expect(i));
    if (!sameValue(ev, v) && Math.abs(ev - v) >= 0.00001)
      ok = false;
  }
  if (!ok) {
    var lanes = [];
    for (var i = 0; i < type.lanes; i++)
      lanes.push(simdConvert(type, expect(i)));
    fail('expected SIMD.' + type.name + '(' + lanes + ') but found ' + a.toString());
  }
}

// Test methods for the different kinds of operations.

// Test the constructor and splat with the given lane values.
function testConstructor(type) {
  equal('function', typeof type.fn);
  equal('function', typeof type.fn.splat);
  for (var v of type.interestingValues) {
    var expected = simdConvert(type, v);
    var result = createSplatValue(type, v);
    checkValue(type, result, function(index) { return expected; });
    // splat.
    result = type.fn.splat(v);
    checkValue(type, result, function(index) { return expected; });
  }
}

function testCheck(type) {
  equal('function', typeof type.fn.check);
  // Other SIMD types shouldn't check for this type.
  var a = type.fn();
  for (var otherType of simdTypes) {
    if (otherType === type)
      equal(a, type.fn.check(a));
    else
      throws(function() { otherType.check(a); });
  }
  // Neither should other types.
  for (var x of [ {}, "", 0, 1, true, false, undefined, null, NaN, Infinity]) {
    throws(function() { type.fn.check(x); });
  }
}

function testReplaceLane(type) {
  equal('function', typeof type.fn.replaceLane);
  a = createTestValue(type);
  for (var v of type.interestingValues) {
    var expected = simdConvert(type, v);
    for (var i = 0; i < type.lanes; i++) {
      var result = type.fn.replaceLane(a, i, v);
      checkValue(type, result,
                 function(index) {
                   return index == i ? expected : type.fn.extractLane(a, index);
                 });
    }
  }

  function testIndexCheck(index) {
    throws(function() { type.fn.replaceLane(a, index, 0); });
  }
  testIndexCheck(type.lanes);
  testIndexCheck(13.37);
  testIndexCheck(null);
  testIndexCheck(undefined);
  testIndexCheck({});
  testIndexCheck(true);
  testIndexCheck('yo');
  testIndexCheck(-1);
  testIndexCheck(128);
}

// Compare unary op's behavior to ref op at each lane.
function testUnaryOp(type, op, refOp) {
  equal('function', typeof type.fn[op]);
  for (var v of type.interestingValues) {
    var expected = simdConvert(type, refOp(v));
    var a = type.fn.splat(v);
    var result = type.fn[op](a);
    checkValue(type, result, function(index) { return expected; });
  }
}

// Compare binary op's behavior to ref op at each lane with the Cartesian
// product of the given values.
function testBinaryOp(type, op, refOp) {
  equal('function', typeof type.fn[op]);
  var zero = type.fn();
  for (var av of type.interestingValues) {
    for (var bv of type.interestingValues) {
      var expected = simdConvert(type, refOp(simdConvert(type, av), simdConvert(type, bv)));
      var a = type.fn.splat(av);
      var b = type.fn.splat(bv);
      var result = type.fn[op](a, b);
      checkValue(type, result, function(index) { return expected; });
    }
  }
}

// Compare relational op's behavior to ref op at each lane with the Cartesian
// product of the given values.
function testRelationalOp(type, op, refOp) {
  equal('function', typeof type.fn[op]);
  var zero = type.fn();
  for (var av of type.interestingValues) {
    for (var bv of type.interestingValues) {
      var expected = refOp(simdConvert(type, av), simdConvert(type, bv));
      var a = type.fn.splat(av);
      var b = type.fn.splat(bv);
      var result = type.fn[op](a, b);
      checkValue(type.boolType, result, function(index) { return expected; });
    }
  }
}

// Compare shift op's behavior to ref op at each lane.
function testShiftOp(type, op, refOp) {
  equal('function', typeof type.fn[op]);
  var zero = type.fn();
  for (var v of type.interestingValues) {
    var s = type.laneSize * 8;
    for (var bits of [-1, 0, 1, 2, s - 1, s, s + 1]) {
      var expected = simdConvert(type, refOp(simdConvert(type, v), bits));
      var a = type.fn.splat(v);
      var result = type.fn[op](a, bits);
      checkValue(type, result, function(index) { return expected; });
    }
  }
}

function testFrom(toType, fromType, name) {
  equal('function', typeof toType.fn[name]);
  for (var v of fromType.interestingValues) {
    var fromValue = createSplatValue(fromType, v);
    v = simdConvert(fromType, v);
    if (toType.minVal !== undefined &&
        (v < toType.minVal || v > toType.maxVal)) {
      throws(function() { toType.fn[name](fromValue) });
    } else {
      v = simdConvert(toType, v);
      var result = toType.fn[name](fromValue);
      checkValue(toType, result, function(index) { return v; });
    }
  }
}

function testFromBits(toType, fromType, name) {
  equal('function', typeof toType.fn[name]);
  for (var v of fromType.interestingValues) {
    var fromValue = createSplatValue(fromType, v);
    var result = toType.fn[name](fromValue);
    for (var i = 0; i < fromType.lanes; i++)
      fromType.buffer[i] = fromType.fn.extractLane(fromValue, i);
    checkValue(toType, result, function(index) { return toType.buffer[index]; });
  }
}

function testAnyTrue(type) {
  equal('function', typeof type.fn.anyTrue);
  // All lanes 'false'.
  var a = type.fn.splat(false);
  ok(!type.fn.anyTrue(a));
  // One lane 'true'.
  for (var i = 0; i < type.lanes; i++) {
    a = type.fn.replaceLane(a, i, true);
    ok(type.fn.anyTrue(a));
  }
  // All lanes 'true'.
  a = type.fn.splat(true);
  ok(type.fn.anyTrue(a));
}

function testAllTrue(type) {
  equal('function', typeof type.fn.allTrue);
  // All lanes 'true'.
  var a = type.fn.splat(true);
  ok(type.fn.allTrue(a));
  // One lane 'false'.
  for (var i = 0; i < type.lanes; i++) {
    a = type.fn.replaceLane(a, i, false);
    ok(!type.fn.allTrue(a));
  }
  // All lanes 'false'.
  a = type.fn.splat(false);
  ok(!type.fn.allTrue(a));
}

function testSelect(type) {
  equal('function', typeof type.fn.select);
  // set a and b to values that are different for all numerical types.
  var av = 1;
  var bv = 2;
  var a = type.fn.splat(av);
  var b = type.fn.splat(bv);
  // test all selectors with a single 'true' lane.
  for (var i = 0; i < type.lanes; i++) {
    var selector = type.boolType.fn();
    selector = type.boolType.fn.replaceLane(selector, i, true);
    var result = type.fn.select(selector, a, b);
    checkValue(type, result, function(index) { return index == i ? av : bv; });
  }
}

function testSwizzle(type) {
  equal('function', typeof type.fn.swizzle);
  var a = createTestValue(type);  // 0, 1, 2, 3, 4, 5, 6, ...
  var indices = [];
  // Identity swizzle.
  for (var i = 0; i < type.lanes; i++) indices.push(i);
  var result = type.fn.swizzle.apply(type.fn, [a].concat(indices));
  checkValue(type, result, function(index) { return type.fn.extractLane(a, index); });
  // Reverse swizzle.
  indices.reverse();
  var result = type.fn.swizzle.apply(type.fn, [a].concat(indices));
  checkValue(type, result, function(index) { return type.fn.extractLane(a, type.lanes - index - 1); });

  function testIndexCheck(index) {
    for (var i = 0; i < type.lanes; i++) {
      var args = [a].concat(indices);
      args[i + 1] = index;
      throws(function() { type.fn.swizzle.apply(type.fn, args); });
    }
  }
  testIndexCheck(type.lanes);
  testIndexCheck(13.37);
  testIndexCheck(null);
  testIndexCheck(undefined);
  testIndexCheck({});
  testIndexCheck(true);
  testIndexCheck('yo');
  testIndexCheck(-1);
  testIndexCheck(128);
}

function testShuffle(type) {
  equal('function', typeof type.fn.shuffle);
  var indices = [];
  for (var i = 0; i < type.lanes; i++) indices.push(i);

  var a = type.fn.apply(type.fn, indices);            // 0, 1, 2, 3, 4 ...
  var b = type.fn.add(a, type.fn.splat(type.lanes));  // lanes, lanes+1 ...
  // All lanes from a.
  var result = type.fn.shuffle.apply(type.fn, [a, b].concat(indices));
  checkValue(type, result, function(index) { return type.fn.extractLane(a, index); });
  // One lane from b.
  for (var i = 0; i < type.lanes; i++) {
    var args = [a, b].concat(indices);
    args[2 + i] += type.lanes;
    var result = type.fn.shuffle.apply(type.fn, args);
    checkValue(type, result, function(index) {
      var val = index == i ? b : a;
      return type.fn.extractLane(val, index);
    });
  }
  // All lanes from b.
  for (var i = 0; i < type.lanes; i++) indices[i] += type.lanes;
  var result = type.fn.shuffle.apply(type.fn, [a, b].concat(indices));
  checkValue(type, result, function(index) { return type.fn.extractLane(b, index); });

  function testIndexCheck(index) {
    for (var i = 0; i < type.lanes; i++) {
      var args = [a, b].concat(indices);
      args[i + 2] = index;
      throws(function() { type.fn.shuffle.apply(type.fn, args); });
    }
  }
  testIndexCheck(2 * type.lanes);
  testIndexCheck(13.37);
  testIndexCheck(null);
  testIndexCheck(undefined);
  testIndexCheck({});
  testIndexCheck(true);
  testIndexCheck('yo');
  testIndexCheck(-1);
  testIndexCheck(128);
}

function testLoad(type, name, count) {
  var loadFn = type.fn[name];
  equal('function', typeof loadFn);
  var bufLanes = 2 * type.lanes;  // Test all alignments.
  var bufSize = bufLanes * type.laneSize + 8;  // Extra for over-alignment test.
  var ab = new ArrayBuffer(bufSize);
  var buf = new type.view(ab);
  for (var i = 0; i < bufLanes; i++) buf[i] = i; // Number buffer sequentially.
  // Test aligned loads.
  for (var i = 0; i < type.lanes; i++) {
    var a = loadFn(buf, i);
    checkValue(type, a, function(index) { return index < count ? i + index : 0; });
  }
  // Test the 2 possible over-alignments.
  var f64 = new Float64Array(ab);
  var stride = 8 / type.laneSize;
  for (var i = 0; i < 1; i++) {
    var a = loadFn(f64, i);
    checkValue(type, a, function(index) { return index < count ? stride * i + index : 0; });
  }
  // Test the 7 possible mis-alignments.
  var i8 = new Int8Array(ab);
  for (var misalignment = 1; misalignment < 8; misalignment++) {
    // Shift the buffer up by 1 byte.
    for (var i = i8.length - 1; i > 0; i--)
      i8[i] = i8[i - 1];
    var a = loadFn(i8, misalignment);
    checkValue(type, a, function(index) { return index < count ? i + index : 0; });
  }

  function testIndexCheck(buf, index) {
    throws(function () { loadFn(buf, index); });
  }
  testIndexCheck(buf, -1);
  testIndexCheck(buf, bufSize / type.laneSize - count + 1);
  testIndexCheck(buf.buffer, 1);
  testIndexCheck(buf, "a");
}

function testStore(type, name, count) {
  var storeFn = type.fn[name];
  equal('function', typeof storeFn);
  var bufLanes = 2 * type.lanes;  // Test all alignments.
  var bufSize = bufLanes * type.laneSize + 8;  // Extra for over-alignment test.
  var ab = new ArrayBuffer(bufSize);
  var buf = new type.view(ab);
  var a = createTestValue(type); // Value containing 0, 1, 2, 3 ...
  function checkBuffer(offset) {
    for (var i = 0; i < count; i++)
      if (buf[offset + i] != i) return false;
    return true;
  }
  // Test aligned stores.
  for (var i = 0; i < type.lanes; i++) {
    storeFn(buf, i, a);
    ok(checkBuffer(i));
  }
  // Test the 2 over-alignments.
  var f64 = new Float64Array(ab);
  var stride = 8 / type.laneSize;
  for (var i = 0; i < 1; i++) {
    storeFn(f64, i, a);
    ok(checkBuffer(stride * i));
  }
  // Test the 7 mis-alignments.
  var i8 = new Int8Array(ab);
  for (var misalignment = 1; misalignment < 8; misalignment++) {
    storeFn(i8, misalignment, a);
    // Shift the buffer down by misalignment.
    for (var i = 0; i < i8.length - misalignment; i++)
      i8[i] = i8[i + misalignment];
    ok(checkBuffer(0));
  }

  function testIndexCheck(buf, index) {
    throws(function () { storeFn(buf, index, type.fn()); });
  }
  testIndexCheck(buf, -1);
  testIndexCheck(buf, bufSize / type.laneSize - count + 1);
  testIndexCheck(buf.buffer, 1);
  testIndexCheck(buf, "a");
}

function testOperators(type) {
  var inst = createTestValue(type);
  throws(function() { Number(inst) });
  throws(function() { +inst });
  throws(function() { -inst });
  throws(function() { ~inst });
  throws(function() { Math.fround(inst) });
  throws(function() { inst|0} );
  throws(function() { inst&0 });
  throws(function() { inst^0 });
  throws(function() { inst>>>0 });
  throws(function() { inst>>0 });
  throws(function() { inst<<0 });
  throws(function() { (inst + inst) });
  throws(function() { inst - inst });
  throws(function() { inst * inst });
  throws(function() { inst / inst });
  throws(function() { inst % inst });
  throws(function() { inst < inst });
  throws(function() { inst > inst });
  throws(function() { inst <= inst });
  throws(function() { inst >= inst });
  throws(function() { inst(); });

  equal(inst[0], undefined);
  equal(inst.a, undefined);
  equal(!inst, false);
  equal(!inst, false);
  equal(inst ? 1 : 2, 1);
  equal(inst ? 1 : 2, 1);

  equal('function', typeof inst.toString);
  equal(inst.toString(), simdToString(type, inst));
  equal('function', typeof inst.toLocaleString);
  equal(inst.toLocaleString(), simdToLocaleString(type, inst));
  // TODO: test valueOf?
}

// Tests value semantics for a given type.
// TODO: more complete tests for Object wrappers, sameValue, sameValueZero, etc.
function testValueSemantics(type) {
  // Create a vanilla test value.
  var x = createTestValue(type);

  // Check against non-SIMD types.
  var otherTypeValues = [0, 1.275, NaN, Infinity, "string", null, undefined,
                         {}, function() {}];
  for (var other of simdTypes) {
    if (type !== other)
      otherTypeValues.push(createTestValue(other));
  }
  otherTypeValues.forEach(function(y) {
    equal(y == x, false);
    equal(x == y, false);
    equal(y != x, true);
    equal(x != y, true);
    equal(y === x, false);
    equal(x === y, false);
    equal(y !== x, true);
    equal(x !== y, true);
  });

  // Test that f(a, b) is the same as f(SIMD(a), SIMD(b)) for equality and
  // strict equality, at every lane.
  function test(a, b) {
    for (var i = 0; i < type.lanes; i++) {
      var aval = type.fn.replaceLane(x, i, a);
      var bval = type.fn.replaceLane(x, i, b);
      equal(a == b, aval == bval);
      equal(a === b, aval === bval);
    }
  }
  for (var a of type.interestingValues) {
    for (var b of type.interestingValues) {
      test(a, b);
    }
  }
}


simdTypes.forEach(function(type) {
  test(type.name + ' constructor', function() {
    testConstructor(type);
  });
  test(type.name + ' check', function() {
    testCheck(type);
  });
  test(type.name + ' operators', function() {
    testOperators(type);
  });
  // Note: This fails in the polyfill due to the lack of value semantics.
  test(type.name + ' value semantics', function() {
    testValueSemantics(type);
  });
  test(type.name + ' replaceLane', function() {
    testReplaceLane(type);
  });
});

simdTypes.filter(isNumerical).forEach(function(type) {
  test(type.name + ' equal', function() {
    testRelationalOp(type, 'equal', function(a, b) { return a == b; });
  });
  test(type.name + ' notEqual', function() {
    testRelationalOp(type, 'notEqual', function(a, b) { return a != b; });
  });
  test(type.name + ' lessThan', function() {
    testRelationalOp(type, 'lessThan', function(a, b) { return a < b; });
  });
  test(type.name + ' lessThanOrEqual', function() {
    testRelationalOp(type, 'lessThanOrEqual', function(a, b) { return a <= b; });
  });
  test(type.name + ' greaterThan', function() {
    testRelationalOp(type, 'greaterThan', function(a, b) { return a > b; });
  });
  test(type.name + ' greaterThanOrEqual', function() {
    testRelationalOp(type, 'greaterThanOrEqual', function(a, b) { return a >= b; });
  });
  test(type.name + ' add', function() {
    testBinaryOp(type, 'add', function(a, b) { return a + b; });
  });
  test(type.name + ' sub', function() {
    testBinaryOp(type, 'sub', function(a, b) { return a - b; });
  });
  test(type.name + ' mul', function() {
    testBinaryOp(type, 'mul', type.mulFn);
  });
  test(type.name + ' min', function() {
    testBinaryOp(type, 'min', Math.min);
  });
  test(type.name + ' max', function() {
    testBinaryOp(type, 'max', Math.max);
  });
  test(type.name + ' select', function() {
    testSelect(type);
  });
  test(type.name + ' swizzle', function() {
    testSwizzle(type);
  });
  test(type.name + ' shuffle', function() {
    testShuffle(type);
  });
  test(type.name + ' load', function() {
    testLoad(type, 'load', type.lanes);
  });
  test(type.name + ' store', function() {
    testStore(type, 'store', type.lanes);
  });
});

simdTypes.filter(hasLoadStore123).forEach(function(type) {
  test(type.name + ' load1', function() {
    testLoad(type, 'load1', 1);
  });
  test(type.name + ' load2', function() {
    testLoad(type, 'load2', 2);
  });
  test(type.name + ' load3', function() {
    testLoad(type, 'load3', 3);
  });
  test(type.name + ' store1', function() {
    testStore(type, 'store1', 1);
  });
  test(type.name + ' store1', function() {
    testStore(type, 'store2', 2);
  });
  test(type.name + ' store3', function() {
    testStore(type, 'store3', 3);
  });
});

simdTypes.filter(isLogical).forEach(function(type) {
  test(type.name + ' and', function() {
    testBinaryOp(type, 'and', function(a, b) { return a & b; });
  });
  test(type.name + ' or', function() {
    testBinaryOp(type, 'or', function(a, b) { return a | b; });
  });
  test(type.name + ' xor', function() {
    testBinaryOp(type, 'xor', function(a, b) { return a ^ b; });
  });
});

simdTypes.filter(isSigned).forEach(function(type) {
  test(type.name + ' neg', function() {
    testUnaryOp(type, 'neg', function(a) { return -a; });
  });
});

simdTypes.filter(isFloatType).forEach(function(type) {
  test(type.name + ' div', function() {
    testBinaryOp(type, 'div', function(a, b) { return a / b; });
  });
  test(type.name + ' abs', function() {
    testBinaryOp(type, 'abs', Math.abs);
  });
  test(type.name + ' minNum', function() {
    testBinaryOp(type, 'minNum', minNum);
  });
  test(type.name + ' maxNum', function() {
    testBinaryOp(type, 'maxNum', maxNum);
  });
  test(type.name + ' sqrt', function() {
    testUnaryOp(type, 'sqrt', function(a) { return Math.sqrt(a); });
  });
  test(type.name + ' reciprocalApproximation', function() {
    testUnaryOp(type, 'reciprocalApproximation', function(a) { return 1 / a; });
  });
  test(type.name + ' reciprocalSqrtApproximation', function() {
    testUnaryOp(type, 'reciprocalSqrtApproximation', function(a) { return 1 / Math.sqrt(a); });
  });
})

simdTypes.filter(isIntType).forEach(function(type) {
  test(type.name + ' not', function() {
    testUnaryOp(type, 'not', function(a) { return ~a; });
  });
  test(type.name + ' shiftLeftByScalar', function() {
    function shift(a, bits) {
      if (bits>>>0 >= type.laneSize * 8) return 0;
      return a << bits;
    }
    testShiftOp(type, 'shiftLeftByScalar', shift);
  });
});

simdTypes.filter(isSignedIntType).forEach(function(type) {
  test(type.name + ' shiftRightByScalar', function() {
    function shift(a, bits) {
      if (bits>>>0 >= type.laneSize * 8)
        bits = type.laneSize * 8 - 1;
      return a >> bits;
    }
    testShiftOp(type, 'shiftRightByScalar', shift);
  });
});

simdTypes.filter(isUnsignedIntType).forEach(function(type) {
  test(type.name + ' shiftRightByScalar', function() {
    function shift(a, bits) {
      if (bits>>>0 >= type.laneSize * 8) return 0;
      if (type.laneMask)
        a &= type.laneMask;
      return a >>> bits;
    }
    testShiftOp(type, 'shiftRightByScalar', shift);
  });
});

simdTypes.filter(isSmallIntType).forEach(function(type) {
  function saturate(type, a) {
    if (a < type.minVal) return type.minVal;
    if (a > type.maxVal) return type.maxVal;
    return a;
  }
  test(type.name + ' addSaturate', function() {
    testBinaryOp(type, 'addSaturate', function(a, b) { return saturate(type, a + b); });
  });
  test(type.name + ' subSaturate', function() {
    testBinaryOp(type, 'subSaturate', function(a, b) { return saturate(type, a - b); });
  });
});

simdTypes.filter(isBoolType).forEach(function(type) {
  test(type.name + ' not', function() {
    testUnaryOp(type, 'not', function(a) { return !a; });
  });
  test(type.name + ' anyTrue', function() {
    testAnyTrue(type, 'anyTrue');
  });
  test(type.name + ' allTrue', function() {
    testAllTrue(type, 'allTrue');
  });
});

// From<type> functions.
simdTypes.forEach(function(toType) {
  if (!toType.from) return;
  for (var fromType of toType.from) {
    var fn = 'from' + fromType.name;
    test(toType.name + ' ' + fn, function() {
      testFrom(toType, fromType, fn);
    });
  }
});

// From<type>Bits functions.
simdTypes.forEach(function(toType) {
  if (!toType.fromBits) return;
  for (var fromType of toType.fromBits) {
    var fn = 'from' + fromType.name + 'Bits';
    test(toType.name + ' ' + fn, function() {
      testFromBits(toType, fromType, fn);
    });
  }
});

// Miscellaneous test methods.

test('Float32x4 Int32x4 bit conversion', function() {
  var m = SIMD.Int32x4(0x3F800000, 0x40000000, 0x40400000, 0x40800000);
  var n = SIMD.Float32x4.fromInt32x4Bits(m);
  equal(1.0, SIMD.Float32x4.extractLane(n, 0));
  equal(2.0, SIMD.Float32x4.extractLane(n, 1));
  equal(3.0, SIMD.Float32x4.extractLane(n, 2));
  equal(4.0, SIMD.Float32x4.extractLane(n, 3));
  n = SIMD.Float32x4(5.0, 6.0, 7.0, 8.0);
  m = SIMD.Int32x4.fromFloat32x4Bits(n);
  equal(0x40A00000, SIMD.Int32x4.extractLane(m, 0));
  equal(0x40C00000, SIMD.Int32x4.extractLane(m, 1));
  equal(0x40E00000, SIMD.Int32x4.extractLane(m, 2));
  equal(0x41000000, SIMD.Int32x4.extractLane(m, 3));
  // Flip sign using bit-wise operators.
  n = SIMD.Float32x4(9.0, 10.0, 11.0, 12.0);
  m = SIMD.Int32x4(0x80000000, 0x80000000, 0x80000000, 0x80000000);
  var nMask = SIMD.Int32x4.fromFloat32x4Bits(n);
  nMask = SIMD.Int32x4.xor(nMask, m); // flip sign.
  n = SIMD.Float32x4.fromInt32x4Bits(nMask);
  equal(-9.0, SIMD.Float32x4.extractLane(n, 0));
  equal(-10.0, SIMD.Float32x4.extractLane(n, 1));
  equal(-11.0, SIMD.Float32x4.extractLane(n, 2));
  equal(-12.0, SIMD.Float32x4.extractLane(n, 3));
  nMask = SIMD.Int32x4.fromFloat32x4Bits(n);
  nMask = SIMD.Int32x4.xor(nMask, m); // flip sign.
  n = SIMD.Float32x4.fromInt32x4Bits(nMask);
  equal(9.0, SIMD.Float32x4.extractLane(n, 0));
  equal(10.0, SIMD.Float32x4.extractLane(n, 1));
  equal(11.0, SIMD.Float32x4.extractLane(n, 2));
  equal(12.0, SIMD.Float32x4.extractLane(n, 3));
});

test('Float32x4 Int32x4 round trip', function() {
  // NaNs should stay unmodified across bit conversions
  m = SIMD.Int32x4(0xFFFFFFFF, 0xFFFF0000, 0x80000000, 0x0);
  var m2 = SIMD.Int32x4.fromFloat32x4Bits(SIMD.Float32x4.fromInt32x4Bits(m));
  // NaNs may be canonicalized, so these tests may fail in some implementations.
  equal(SIMD.Int32x4.extractLane(m, 0), SIMD.Int32x4.extractLane(m2, 0));
  equal(SIMD.Int32x4.extractLane(m, 1), SIMD.Int32x4.extractLane(m2, 1));
});
