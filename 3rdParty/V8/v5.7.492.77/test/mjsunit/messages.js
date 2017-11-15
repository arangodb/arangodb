// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100 --harmony
// Flags: --harmony-simd

function test(f, expected, type) {
  try {
    f();
  } catch (e) {
    assertInstanceof(e, type);
    assertEquals(expected, e.message);
    return;
  }
  assertUnreachable("Exception expected");
}

// === Error ===

// kCyclicProto
test(function() {
  var o = {};
  o.__proto__ = o;
}, "Cyclic __proto__ value", Error);


// === TypeError ===

// kApplyNonFunction
test(function() {
  Function.prototype.apply.call(1, []);
}, "Function.prototype.apply was called on 1, which is a number " +
   "and not a function", TypeError);

// kArrayFunctionsOnFrozen
test(function() {
  var a = [1, 2];
  Object.freeze(a);
  a.splice(1, 1, [1]);
}, "Cannot modify frozen array elements", TypeError);

// kArrayFunctionsOnSealed
test(function() {
  var a = [1];
  Object.seal(a);
  a.shift();
}, "Cannot add/remove sealed array elements", TypeError);

// kCalledNonCallable
test(function() {
  [].forEach(1);
}, "1 is not a function", TypeError);

// kCalledOnNonObject
test(function() {
  Object.defineProperty(1, "x", {});
}, "Object.defineProperty called on non-object", TypeError);

test(function() {
  (function() {}).apply({}, 1);
}, "CreateListFromArrayLike called on non-object", TypeError);

test(function() {
  Reflect.apply(function() {}, {}, 1);
}, "CreateListFromArrayLike called on non-object", TypeError);

test(function() {
  Reflect.construct(function() {}, 1);
}, "CreateListFromArrayLike called on non-object", TypeError);

// kCalledOnNullOrUndefined
test(function() {
  Array.prototype.shift.call(null);
}, "Array.prototype.shift called on null or undefined", TypeError);

// kCannotFreezeArrayBufferView
test(function() {
  Object.freeze(new Uint16Array(1));
}, "Cannot freeze array buffer views with elements", TypeError);

// kConstAssign
test(function() {
  "use strict";
  const a = 1;
  a = 2;
}, "Assignment to constant variable.", TypeError);

// kCannotConvertToPrimitive
test(function() {
  var o = { toString: function() { return this } };
  [].join(o);
}, "Cannot convert object to primitive value", TypeError);

// kCircularStructure
test(function() {
  var o = {};
  o.o = o;
  JSON.stringify(o);
}, "Converting circular structure to JSON", TypeError);

// kConstructorNotFunction
test(function() {
  Uint16Array(1);
}, "Constructor Uint16Array requires 'new'", TypeError);

// kDataViewNotArrayBuffer
test(function() {
  new DataView(1);
}, "First argument to DataView constructor must be an ArrayBuffer", TypeError);

// kDefineDisallowed
test(function() {
  "use strict";
  var o = {};
  Object.preventExtensions(o);
  Object.defineProperty(o, "x", { value: 1 });
}, "Cannot define property:x, object is not extensible.", TypeError);

// kFirstArgumentNotRegExp
test(function() {
  "a".startsWith(/a/);
}, "First argument to String.prototype.startsWith " +
   "must not be a regular expression", TypeError);

// kFlagsGetterNonObject
test(function() {
  Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get.call(1);
}, "RegExp.prototype.flags getter called on non-object 1", TypeError);

// kFunctionBind
test(function() {
  Function.prototype.bind.call(1);
}, "Bind must be called on a function", TypeError);

// kGeneratorRunning
test(function() {
  var iter;
  function* generator() { yield iter.next(); }
  var iter = generator();
  iter.next();
}, "Generator is already running", TypeError);

// kIncompatibleMethodReceiver
test(function() {
  Set.prototype.add.call([]);
}, "Method Set.prototype.add called on incompatible receiver [object Array]",
TypeError);

// kNonCallableInInstanceOfCheck
test(function() {
  1 instanceof {};
}, "Right-hand side of 'instanceof' is not callable", TypeError);

// kNonObjectInInstanceOfCheck
test(function() {
  1 instanceof 1;
}, "Right-hand side of 'instanceof' is not an object", TypeError);

// kInstanceofNonobjectProto
test(function() {
  function f() {}
  var o = new f();
  f.prototype = 1;
  o instanceof f;
}, "Function has non-object prototype '1' in instanceof check", TypeError);

// kInvalidInOperatorUse
test(function() {
  1 in 1;
}, "Cannot use 'in' operator to search for '1' in 1", TypeError);

// kIteratorResultNotAnObject
test(function() {
  var obj = {};
  obj[Symbol.iterator] = function() { return { next: function() { return 1 }}};
  Array.from(obj);
}, "Iterator result 1 is not an object", TypeError);

// kIteratorValueNotAnObject
test(function() {
  new Map([1]);
}, "Iterator value 1 is not an entry object", TypeError);

// kNotConstructor
test(function() {
  new Symbol();
}, "Symbol is not a constructor", TypeError);

// kNotDateObject
test(function() {
  Date.prototype.getHours.call(1);
}, "this is not a Date object.", TypeError);

// kNotGeneric
test(function() {
  String.prototype.toString.call(1);
}, "String.prototype.toString is not generic", TypeError);

test(function() {
  String.prototype.valueOf.call(1);
}, "String.prototype.valueOf is not generic", TypeError);

test(function() {
  Boolean.prototype.toString.call(1);
}, "Boolean.prototype.toString is not generic", TypeError);

test(function() {
  Boolean.prototype.valueOf.call(1);
}, "Boolean.prototype.valueOf is not generic", TypeError);

test(function() {
  Number.prototype.toString.call({});
}, "Number.prototype.toString is not generic", TypeError);

test(function() {
  Number.prototype.valueOf.call({});
}, "Number.prototype.valueOf is not generic", TypeError);

test(function() {
  Function.prototype.toString.call(1);
}, "Function.prototype.toString is not generic", TypeError);

// kNotTypedArray
test(function() {
  Uint16Array.prototype.forEach.call(1);
}, "this is not a typed array.", TypeError);

// kObjectGetterExpectingFunction
test(function() {
  ({}).__defineGetter__("x", 0);
}, "Object.prototype.__defineGetter__: Expecting function", TypeError);

// kObjectGetterCallable
test(function() {
  Object.defineProperty({}, "x", { get: 1 });
}, "Getter must be a function: 1", TypeError);

// kObjectNotExtensible
test(function() {
  "use strict";
  var o = {};
  Object.freeze(o);
  o.a = 1;
}, "Can't add property a, object is not extensible", TypeError);

// kObjectSetterExpectingFunction
test(function() {
  ({}).__defineSetter__("x", 0);
}, "Object.prototype.__defineSetter__: Expecting function", TypeError);

// kObjectSetterCallable
test(function() {
  Object.defineProperty({}, "x", { set: 1 });
}, "Setter must be a function: 1", TypeError);

// kPropertyDescObject
test(function() {
  Object.defineProperty({}, "x", 1);
}, "Property description must be an object: 1", TypeError);

// kPropertyNotFunction
test(function() {
  Set.prototype.add = 0;
  new Set(1);
}, "'0' returned for property 'add' of object '#<Set>' is not a function", TypeError);

// kProtoObjectOrNull
test(function() {
  Object.setPrototypeOf({}, 1);
}, "Object prototype may only be an Object or null: 1", TypeError);

// kRedefineDisallowed
test(function() {
  "use strict";
  var o = {};
  Object.defineProperty(o, "x", { value: 1, configurable: false });
  Object.defineProperty(o, "x", { value: 2 });
}, "Cannot redefine property: x", TypeError);

// kReduceNoInitial
test(function() {
  [].reduce(function() {});
}, "Reduce of empty array with no initial value", TypeError);

// kResolverNotAFunction
test(function() {
  new Promise(1);
}, "Promise resolver 1 is not a function", TypeError);

// kStrictDeleteProperty
test(function() {
  "use strict";
  var o = {};
  Object.defineProperty(o, "p", { value: 1, writable: false });
  delete o.p;
}, "Cannot delete property 'p' of #<Object>", TypeError);

// kStrictPoisonPill
test(function() {
  "use strict";
  arguments.callee;
}, "'caller', 'callee', and 'arguments' properties may not be accessed on " +
   "strict mode functions or the arguments objects for calls to them",
   TypeError);

// kStrictReadOnlyProperty
test(function() {
  "use strict";
  (1).a = 1;
}, "Cannot create property 'a' on number '1'", TypeError);

// kSymbolToString
test(function() {
  "" + Symbol();
}, "Cannot convert a Symbol value to a string", TypeError);

// kSymbolToNumber
test(function() {
  1 + Symbol();
}, "Cannot convert a Symbol value to a number", TypeError);

// kSimdToNumber
test(function() {
  1 + SIMD.Float32x4(1, 2, 3, 4);
}, "Cannot convert a SIMD value to a number", TypeError);

// kUndefinedOrNullToObject
test(function() {
  Array.prototype.toString.call(null);
}, "Cannot convert undefined or null to object", TypeError);

// kValueAndAccessor
test(function() {
  Object.defineProperty({}, "x", { get: function(){}, value: 1});
}, "Invalid property descriptor. Cannot both specify accessors " +
   "and a value or writable attribute, #<Object>", TypeError);


// === SyntaxError ===

// kInvalidRegExpFlags
test(function() {
  eval("/a/x.test(\"a\");");
}, "Invalid regular expression flags", SyntaxError);

// kInvalidOrUnexpectedToken
test(function() {
  eval("'\n'");
}, "Invalid or unexpected token", SyntaxError);

//kJsonParseUnexpectedEOS
test(function() {
  JSON.parse("{")
}, "Unexpected end of JSON input", SyntaxError);

// kJsonParseUnexpectedTokenAt
test(function() {
  JSON.parse("/")
}, "Unexpected token / in JSON at position 0", SyntaxError);

// kJsonParseUnexpectedTokenNumberAt
test(function() {
  JSON.parse("{ 1")
}, "Unexpected number in JSON at position 2", SyntaxError);

// kJsonParseUnexpectedTokenStringAt
test(function() {
  JSON.parse('"""')
}, "Unexpected string in JSON at position 2", SyntaxError);

// kMalformedRegExp
test(function() {
  /(/.test("a");
}, "Invalid regular expression: /(/: Unterminated group", SyntaxError);

// kParenthesisInArgString
test(function() {
  new Function(")", "");
}, "Function arg string contains parenthesis", SyntaxError);

// === ReferenceError ===

// kNotDefined
test(function() {
  "use strict";
  o;
}, "o is not defined", ReferenceError);

// === RangeError ===

// kArrayLengthOutOfRange
test(function() {
  "use strict";
  Object.defineProperty([], "length", { value: 1E100 });
}, "Invalid array length", RangeError);

// kInvalidArrayBufferLength
test(function() {
  new ArrayBuffer(-1);
}, "Invalid array buffer length", RangeError);

// kInvalidArrayLength
test(function() {
  [].length = -1;
}, "Invalid array length", RangeError);

// kInvalidCodePoint
test(function() {
  String.fromCodePoint(-1);
}, "Invalid code point -1", RangeError);

// kInvalidCountValue
test(function() {
  "a".repeat(-1);
}, "Invalid count value", RangeError);

// kInvalidArrayBufferLength
test(function() {
  new Uint16Array(-1);
}, "Invalid typed array length", RangeError);

// kNormalizationForm
test(function() {
  "".normalize("ABC");
}, "The normalization form should be one of NFC, NFD, NFKC, NFKD.", RangeError);

// kNumberFormatRange
test(function() {
  Number(1).toFixed(100);
}, "toFixed() digits argument must be between 0 and 20", RangeError);

test(function() {
  Number(1).toExponential(100);
}, "toExponential() argument must be between 0 and 20", RangeError);

// kStackOverflow
test(function() {
  function f() { f(Array(1000)); }
  f();
}, "Maximum call stack size exceeded", RangeError);

// kToPrecisionFormatRange
test(function() {
  Number(1).toPrecision(100);
}, "toPrecision() argument must be between 1 and 21", RangeError);

// kToPrecisionFormatRange
test(function() {
  Number(1).toString(100);
}, "toString() radix argument must be between 2 and 36", RangeError);


// === URIError ===

// kURIMalformed
test(function() {
  decodeURI("%%");
}, "URI malformed", URIError);
