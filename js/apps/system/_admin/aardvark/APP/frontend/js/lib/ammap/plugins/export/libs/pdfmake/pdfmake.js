!function(e){if("object"==typeof exports)module.exports=e();else if("function"==typeof define&&define.amd)define(e);else{var f;"undefined"!=typeof window?f=window:"undefined"!=typeof global?f=global:"undefined"!=typeof self&&(f=self),f.pdfMake=e()}}(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);throw new Error("Cannot find module '"+o+"'")}var f=n[o]={exports:{}};t[o][0].call(f.exports,function(e){var n=t[o][1][e];return s(n?n:e)},f,f.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(_dereq_,module,exports){
/* FileSaver.js
 * A saveAs() FileSaver implementation.
 * 2014-08-29
 *
 * By Eli Grey, http://eligrey.com
 * License: X11/MIT
 *   See https://github.com/eligrey/FileSaver.js/blob/master/LICENSE.md
 */

/*global self */
/*jslint bitwise: true, indent: 4, laxbreak: true, laxcomma: true, smarttabs: true, plusplus: true */

/*! @source http://purl.eligrey.com/github/FileSaver.js/blob/master/FileSaver.js */

var saveAs = saveAs
  // IE 10+ (native saveAs)
  || (typeof navigator !== "undefined" &&
      navigator.msSaveOrOpenBlob && navigator.msSaveOrOpenBlob.bind(navigator))
  // Everyone else
  || (function(view) {
	"use strict";
	// IE <10 is explicitly unsupported
	if (typeof navigator !== "undefined" &&
	    /MSIE [1-9]\./.test(navigator.userAgent)) {
		return;
	}
	var
		  doc = view.document
		  // only get URL when necessary in case Blob.js hasn't overridden it yet
		, get_URL = function() {
			return view.URL || view.webkitURL || view;
		}
		, save_link = doc.createElementNS("http://www.w3.org/1999/xhtml", "a")
		, can_use_save_link = "download" in save_link
		, click = function(node) {
			var event = doc.createEvent("MouseEvents");
			event.initMouseEvent(
				"click", true, false, view, 0, 0, 0, 0, 0
				, false, false, false, false, 0, null
			);
			node.dispatchEvent(event);
		}
		, webkit_req_fs = view.webkitRequestFileSystem
		, req_fs = view.requestFileSystem || webkit_req_fs || view.mozRequestFileSystem
		, throw_outside = function(ex) {
			(view.setImmediate || view.setTimeout)(function() {
				throw ex;
			}, 0);
		}
		, force_saveable_type = "application/octet-stream"
		, fs_min_size = 0
		// See https://code.google.com/p/chromium/issues/detail?id=375297#c7 for
		// the reasoning behind the timeout and revocation flow
		, arbitrary_revoke_timeout = 10
		, revoke = function(file) {
			var revoker = function() {
				if (typeof file === "string") { // file is an object URL
					get_URL().revokeObjectURL(file);
				} else { // file is a File
					file.remove();
				}
			};
			if (view.chrome) {
				revoker();
			} else {
				setTimeout(revoker, arbitrary_revoke_timeout);
			}
		}
		, dispatch = function(filesaver, event_types, event) {
			event_types = [].concat(event_types);
			var i = event_types.length;
			while (i--) {
				var listener = filesaver["on" + event_types[i]];
				if (typeof listener === "function") {
					try {
						listener.call(filesaver, event || filesaver);
					} catch (ex) {
						throw_outside(ex);
					}
				}
			}
		}
		, FileSaver = function(blob, name) {
			// First try a.download, then web filesystem, then object URLs
			var
				  filesaver = this
				, type = blob.type
				, blob_changed = false
				, object_url
				, target_view
				, dispatch_all = function() {
					dispatch(filesaver, "writestart progress write writeend".split(" "));
				}
				// on any filesys errors revert to saving with object URLs
				, fs_error = function() {
					// don't create more object URLs than needed
					if (blob_changed || !object_url) {
						object_url = get_URL().createObjectURL(blob);
					}
					if (target_view) {
						target_view.location.href = object_url;
					} else {
						var new_tab = view.open(object_url, "_blank");
						if (new_tab == undefined && typeof safari !== "undefined") {
							//Apple do not allow window.open, see http://bit.ly/1kZffRI
							view.location.href = object_url
						}
					}
					filesaver.readyState = filesaver.DONE;
					dispatch_all();
					revoke(object_url);
				}
				, abortable = function(func) {
					return function() {
						if (filesaver.readyState !== filesaver.DONE) {
							return func.apply(this, arguments);
						}
					};
				}
				, create_if_not_found = {create: true, exclusive: false}
				, slice
			;
			filesaver.readyState = filesaver.INIT;
			if (!name) {
				name = "download";
			}
			if (can_use_save_link) {
				object_url = get_URL().createObjectURL(blob);
				save_link.href = object_url;
				save_link.download = name;
				click(save_link);
				filesaver.readyState = filesaver.DONE;
				dispatch_all();
				revoke(object_url);
				return;
			}
			// Object and web filesystem URLs have a problem saving in Google Chrome when
			// viewed in a tab, so I force save with application/octet-stream
			// http://code.google.com/p/chromium/issues/detail?id=91158
			// Update: Google errantly closed 91158, I submitted it again:
			// https://code.google.com/p/chromium/issues/detail?id=389642
			if (view.chrome && type && type !== force_saveable_type) {
				slice = blob.slice || blob.webkitSlice;
				blob = slice.call(blob, 0, blob.size, force_saveable_type);
				blob_changed = true;
			}
			// Since I can't be sure that the guessed media type will trigger a download
			// in WebKit, I append .download to the filename.
			// https://bugs.webkit.org/show_bug.cgi?id=65440
			if (webkit_req_fs && name !== "download") {
				name += ".download";
			}
			if (type === force_saveable_type || webkit_req_fs) {
				target_view = view;
			}
			if (!req_fs) {
				fs_error();
				return;
			}
			fs_min_size += blob.size;
			req_fs(view.TEMPORARY, fs_min_size, abortable(function(fs) {
				fs.root.getDirectory("saved", create_if_not_found, abortable(function(dir) {
					var save = function() {
						dir.getFile(name, create_if_not_found, abortable(function(file) {
							file.createWriter(abortable(function(writer) {
								writer.onwriteend = function(event) {
									target_view.location.href = file.toURL();
									filesaver.readyState = filesaver.DONE;
									dispatch(filesaver, "writeend", event);
									revoke(file);
								};
								writer.onerror = function() {
									var error = writer.error;
									if (error.code !== error.ABORT_ERR) {
										fs_error();
									}
								};
								"writestart progress write abort".split(" ").forEach(function(event) {
									writer["on" + event] = filesaver["on" + event];
								});
								writer.write(blob);
								filesaver.abort = function() {
									writer.abort();
									filesaver.readyState = filesaver.DONE;
								};
								filesaver.readyState = filesaver.WRITING;
							}), fs_error);
						}), fs_error);
					};
					dir.getFile(name, {create: false}, abortable(function(file) {
						// delete file if it already exists
						file.remove();
						save();
					}), abortable(function(ex) {
						if (ex.code === ex.NOT_FOUND_ERR) {
							save();
						} else {
							fs_error();
						}
					}));
				}), fs_error);
			}), fs_error);
		}
		, FS_proto = FileSaver.prototype
		, saveAs = function(blob, name) {
			return new FileSaver(blob, name);
		}
	;
	FS_proto.abort = function() {
		var filesaver = this;
		filesaver.readyState = filesaver.DONE;
		dispatch(filesaver, "abort");
	};
	FS_proto.readyState = FS_proto.INIT = 0;
	FS_proto.WRITING = 1;
	FS_proto.DONE = 2;

	FS_proto.error =
	FS_proto.onwritestart =
	FS_proto.onprogress =
	FS_proto.onwrite =
	FS_proto.onabort =
	FS_proto.onerror =
	FS_proto.onwriteend =
		null;

	return saveAs;
}(
	   typeof self !== "undefined" && self
	|| typeof window !== "undefined" && window
	|| this.content
));
// `self` is undefined in Firefox for Android content script context
// while `this` is nsIContentFrameMessageManager
// with an attribute `content` that corresponds to the window

if (typeof module !== "undefined" && module !== null) {
  module.exports = saveAs;
} else if ((typeof define !== "undefined" && define !== null) && (define.amd != null)) {
  define([], function() {
    return saveAs;
  });
}

},{}],2:[function(_dereq_,module,exports){
// http://wiki.commonjs.org/wiki/Unit_Testing/1.0
//
// THIS IS NOT TESTED NOR LIKELY TO WORK OUTSIDE V8!
//
// Originally from narwhal.js (http://narwhaljs.org)
// Copyright (c) 2009 Thomas Robinson <280north.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the 'Software'), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// when used in node, this will actually load the util module we depend on
// versus loading the builtin util module as happens otherwise
// this is a bug in node module loading as far as I am concerned
var util = _dereq_('util/');

var pSlice = Array.prototype.slice;
var hasOwn = Object.prototype.hasOwnProperty;

// 1. The assert module provides functions that throw
// AssertionError's when particular conditions are not met. The
// assert module must conform to the following interface.

var assert = module.exports = ok;

// 2. The AssertionError is defined in assert.
// new assert.AssertionError({ message: message,
//                             actual: actual,
//                             expected: expected })

assert.AssertionError = function AssertionError(options) {
  this.name = 'AssertionError';
  this.actual = options.actual;
  this.expected = options.expected;
  this.operator = options.operator;
  if (options.message) {
    this.message = options.message;
    this.generatedMessage = false;
  } else {
    this.message = getMessage(this);
    this.generatedMessage = true;
  }
  var stackStartFunction = options.stackStartFunction || fail;

  if (Error.captureStackTrace) {
    Error.captureStackTrace(this, stackStartFunction);
  }
  else {
    // non v8 browsers so we can have a stacktrace
    var err = new Error();
    if (err.stack) {
      var out = err.stack;

      // try to strip useless frames
      var fn_name = stackStartFunction.name;
      var idx = out.indexOf('\n' + fn_name);
      if (idx >= 0) {
        // once we have located the function frame
        // we need to strip out everything before it (and its line)
        var next_line = out.indexOf('\n', idx + 1);
        out = out.substring(next_line + 1);
      }

      this.stack = out;
    }
  }
};

// assert.AssertionError instanceof Error
util.inherits(assert.AssertionError, Error);

function replacer(key, value) {
  if (util.isUndefined(value)) {
    return '' + value;
  }
  if (util.isNumber(value) && (isNaN(value) || !isFinite(value))) {
    return value.toString();
  }
  if (util.isFunction(value) || util.isRegExp(value)) {
    return value.toString();
  }
  return value;
}

function truncate(s, n) {
  if (util.isString(s)) {
    return s.length < n ? s : s.slice(0, n);
  } else {
    return s;
  }
}

function getMessage(self) {
  return truncate(JSON.stringify(self.actual, replacer), 128) + ' ' +
         self.operator + ' ' +
         truncate(JSON.stringify(self.expected, replacer), 128);
}

// At present only the three keys mentioned above are used and
// understood by the spec. Implementations or sub modules can pass
// other keys to the AssertionError's constructor - they will be
// ignored.

// 3. All of the following functions must throw an AssertionError
// when a corresponding condition is not met, with a message that
// may be undefined if not provided.  All assertion methods provide
// both the actual and expected values to the assertion error for
// display purposes.

function fail(actual, expected, message, operator, stackStartFunction) {
  throw new assert.AssertionError({
    message: message,
    actual: actual,
    expected: expected,
    operator: operator,
    stackStartFunction: stackStartFunction
  });
}

// EXTENSION! allows for well behaved errors defined elsewhere.
assert.fail = fail;

// 4. Pure assertion tests whether a value is truthy, as determined
// by !!guard.
// assert.ok(guard, message_opt);
// This statement is equivalent to assert.equal(true, !!guard,
// message_opt);. To test strictly for the value true, use
// assert.strictEqual(true, guard, message_opt);.

function ok(value, message) {
  if (!value) fail(value, true, message, '==', assert.ok);
}
assert.ok = ok;

// 5. The equality assertion tests shallow, coercive equality with
// ==.
// assert.equal(actual, expected, message_opt);

assert.equal = function equal(actual, expected, message) {
  if (actual != expected) fail(actual, expected, message, '==', assert.equal);
};

// 6. The non-equality assertion tests for whether two objects are not equal
// with != assert.notEqual(actual, expected, message_opt);

assert.notEqual = function notEqual(actual, expected, message) {
  if (actual == expected) {
    fail(actual, expected, message, '!=', assert.notEqual);
  }
};

// 7. The equivalence assertion tests a deep equality relation.
// assert.deepEqual(actual, expected, message_opt);

assert.deepEqual = function deepEqual(actual, expected, message) {
  if (!_deepEqual(actual, expected)) {
    fail(actual, expected, message, 'deepEqual', assert.deepEqual);
  }
};

function _deepEqual(actual, expected) {
  // 7.1. All identical values are equivalent, as determined by ===.
  if (actual === expected) {
    return true;

  } else if (util.isBuffer(actual) && util.isBuffer(expected)) {
    if (actual.length != expected.length) return false;

    for (var i = 0; i < actual.length; i++) {
      if (actual[i] !== expected[i]) return false;
    }

    return true;

  // 7.2. If the expected value is a Date object, the actual value is
  // equivalent if it is also a Date object that refers to the same time.
  } else if (util.isDate(actual) && util.isDate(expected)) {
    return actual.getTime() === expected.getTime();

  // 7.3 If the expected value is a RegExp object, the actual value is
  // equivalent if it is also a RegExp object with the same source and
  // properties (`global`, `multiline`, `lastIndex`, `ignoreCase`).
  } else if (util.isRegExp(actual) && util.isRegExp(expected)) {
    return actual.source === expected.source &&
           actual.global === expected.global &&
           actual.multiline === expected.multiline &&
           actual.lastIndex === expected.lastIndex &&
           actual.ignoreCase === expected.ignoreCase;

  // 7.4. Other pairs that do not both pass typeof value == 'object',
  // equivalence is determined by ==.
  } else if (!util.isObject(actual) && !util.isObject(expected)) {
    return actual == expected;

  // 7.5 For all other Object pairs, including Array objects, equivalence is
  // determined by having the same number of owned properties (as verified
  // with Object.prototype.hasOwnProperty.call), the same set of keys
  // (although not necessarily the same order), equivalent values for every
  // corresponding key, and an identical 'prototype' property. Note: this
  // accounts for both named and indexed properties on Arrays.
  } else {
    return objEquiv(actual, expected);
  }
}

function isArguments(object) {
  return Object.prototype.toString.call(object) == '[object Arguments]';
}

function objEquiv(a, b) {
  if (util.isNullOrUndefined(a) || util.isNullOrUndefined(b))
    return false;
  // an identical 'prototype' property.
  if (a.prototype !== b.prototype) return false;
  //~~~I've managed to break Object.keys through screwy arguments passing.
  //   Converting to array solves the problem.
  if (isArguments(a)) {
    if (!isArguments(b)) {
      return false;
    }
    a = pSlice.call(a);
    b = pSlice.call(b);
    return _deepEqual(a, b);
  }
  try {
    var ka = objectKeys(a),
        kb = objectKeys(b),
        key, i;
  } catch (e) {//happens when one is a string literal and the other isn't
    return false;
  }
  // having the same number of owned properties (keys incorporates
  // hasOwnProperty)
  if (ka.length != kb.length)
    return false;
  //the same set of keys (although not necessarily the same order),
  ka.sort();
  kb.sort();
  //~~~cheap key test
  for (i = ka.length - 1; i >= 0; i--) {
    if (ka[i] != kb[i])
      return false;
  }
  //equivalent values for every corresponding key, and
  //~~~possibly expensive deep test
  for (i = ka.length - 1; i >= 0; i--) {
    key = ka[i];
    if (!_deepEqual(a[key], b[key])) return false;
  }
  return true;
}

// 8. The non-equivalence assertion tests for any deep inequality.
// assert.notDeepEqual(actual, expected, message_opt);

assert.notDeepEqual = function notDeepEqual(actual, expected, message) {
  if (_deepEqual(actual, expected)) {
    fail(actual, expected, message, 'notDeepEqual', assert.notDeepEqual);
  }
};

// 9. The strict equality assertion tests strict equality, as determined by ===.
// assert.strictEqual(actual, expected, message_opt);

assert.strictEqual = function strictEqual(actual, expected, message) {
  if (actual !== expected) {
    fail(actual, expected, message, '===', assert.strictEqual);
  }
};

// 10. The strict non-equality assertion tests for strict inequality, as
// determined by !==.  assert.notStrictEqual(actual, expected, message_opt);

assert.notStrictEqual = function notStrictEqual(actual, expected, message) {
  if (actual === expected) {
    fail(actual, expected, message, '!==', assert.notStrictEqual);
  }
};

function expectedException(actual, expected) {
  if (!actual || !expected) {
    return false;
  }

  if (Object.prototype.toString.call(expected) == '[object RegExp]') {
    return expected.test(actual);
  } else if (actual instanceof expected) {
    return true;
  } else if (expected.call({}, actual) === true) {
    return true;
  }

  return false;
}

function _throws(shouldThrow, block, expected, message) {
  var actual;

  if (util.isString(expected)) {
    message = expected;
    expected = null;
  }

  try {
    block();
  } catch (e) {
    actual = e;
  }

  message = (expected && expected.name ? ' (' + expected.name + ').' : '.') +
            (message ? ' ' + message : '.');

  if (shouldThrow && !actual) {
    fail(actual, expected, 'Missing expected exception' + message);
  }

  if (!shouldThrow && expectedException(actual, expected)) {
    fail(actual, expected, 'Got unwanted exception' + message);
  }

  if ((shouldThrow && actual && expected &&
      !expectedException(actual, expected)) || (!shouldThrow && actual)) {
    throw actual;
  }
}

// 11. Expected to throw an error:
// assert.throws(block, Error_opt, message_opt);

assert.throws = function(block, /*optional*/error, /*optional*/message) {
  _throws.apply(this, [true].concat(pSlice.call(arguments)));
};

// EXTENSION! This is annoying to write outside this module.
assert.doesNotThrow = function(block, /*optional*/message) {
  _throws.apply(this, [false].concat(pSlice.call(arguments)));
};

assert.ifError = function(err) { if (err) {throw err;}};

var objectKeys = Object.keys || function (obj) {
  var keys = [];
  for (var key in obj) {
    if (hasOwn.call(obj, key)) keys.push(key);
  }
  return keys;
};

},{"util/":31}],3:[function(_dereq_,module,exports){
'use strict';


var TYPED_OK =  (typeof Uint8Array !== 'undefined') &&
                (typeof Uint16Array !== 'undefined') &&
                (typeof Int32Array !== 'undefined');


exports.assign = function (obj /*from1, from2, from3, ...*/) {
  var sources = Array.prototype.slice.call(arguments, 1);
  while (sources.length) {
    var source = sources.shift();
    if (!source) { continue; }

    if (typeof(source) !== 'object') {
      throw new TypeError(source + 'must be non-object');
    }

    for (var p in source) {
      if (source.hasOwnProperty(p)) {
        obj[p] = source[p];
      }
    }
  }

  return obj;
};


// reduce buffer size, avoiding mem copy
exports.shrinkBuf = function (buf, size) {
  if (buf.length === size) { return buf; }
  if (buf.subarray) { return buf.subarray(0, size); }
  buf.length = size;
  return buf;
};


var fnTyped = {
  arraySet: function (dest, src, src_offs, len, dest_offs) {
    if (src.subarray && dest.subarray) {
      dest.set(src.subarray(src_offs, src_offs+len), dest_offs);
      return;
    }
    // Fallback to ordinary array
    for(var i=0; i<len; i++) {
      dest[dest_offs + i] = src[src_offs + i];
    }
  },
  // Join array of chunks to single array.
  flattenChunks: function(chunks) {
    var i, l, len, pos, chunk, result;

    // calculate data length
    len = 0;
    for (i=0, l=chunks.length; i<l; i++) {
      len += chunks[i].length;
    }

    // join chunks
    result = new Uint8Array(len);
    pos = 0;
    for (i=0, l=chunks.length; i<l; i++) {
      chunk = chunks[i];
      result.set(chunk, pos);
      pos += chunk.length;
    }

    return result;
  }
};

var fnUntyped = {
  arraySet: function (dest, src, src_offs, len, dest_offs) {
    for(var i=0; i<len; i++) {
      dest[dest_offs + i] = src[src_offs + i];
    }
  },
  // Join array of chunks to single array.
  flattenChunks: function(chunks) {
    return [].concat.apply([], chunks);
  }
};


// Enable/Disable typed arrays use, for testing
//
exports.setTyped = function (on) {
  if (on) {
    exports.Buf8  = Uint8Array;
    exports.Buf16 = Uint16Array;
    exports.Buf32 = Int32Array;
    exports.assign(exports, fnTyped);
  } else {
    exports.Buf8  = Array;
    exports.Buf16 = Array;
    exports.Buf32 = Array;
    exports.assign(exports, fnUntyped);
  }
};

exports.setTyped(TYPED_OK);
},{}],4:[function(_dereq_,module,exports){
'use strict';

// Note: adler32 takes 12% for level 0 and 2% for level 6.
// It doesn't worth to make additional optimizationa as in original.
// Small size is preferable.

function adler32(adler, buf, len, pos) {
  var s1 = (adler & 0xffff) |0
    , s2 = ((adler >>> 16) & 0xffff) |0
    , n = 0;

  while (len !== 0) {
    // Set limit ~ twice less than 5552, to keep
    // s2 in 31-bits, because we force signed ints.
    // in other case %= will fail.
    n = len > 2000 ? 2000 : len;
    len -= n;

    do {
      s1 = (s1 + buf[pos++]) |0;
      s2 = (s2 + s1) |0;
    } while (--n);

    s1 %= 65521;
    s2 %= 65521;
  }

  return (s1 | (s2 << 16)) |0;
}


module.exports = adler32;
},{}],5:[function(_dereq_,module,exports){
module.exports = {

  /* Allowed flush values; see deflate() and inflate() below for details */
  Z_NO_FLUSH:         0,
  Z_PARTIAL_FLUSH:    1,
  Z_SYNC_FLUSH:       2,
  Z_FULL_FLUSH:       3,
  Z_FINISH:           4,
  Z_BLOCK:            5,
  Z_TREES:            6,

  /* Return codes for the compression/decompression functions. Negative values
  * are errors, positive values are used for special but normal events.
  */
  Z_OK:               0,
  Z_STREAM_END:       1,
  Z_NEED_DICT:        2,
  Z_ERRNO:           -1,
  Z_STREAM_ERROR:    -2,
  Z_DATA_ERROR:      -3,
  //Z_MEM_ERROR:     -4,
  Z_BUF_ERROR:       -5,
  //Z_VERSION_ERROR: -6,

  /* compression levels */
  Z_NO_COMPRESSION:         0,
  Z_BEST_SPEED:             1,
  Z_BEST_COMPRESSION:       9,
  Z_DEFAULT_COMPRESSION:   -1,


  Z_FILTERED:               1,
  Z_HUFFMAN_ONLY:           2,
  Z_RLE:                    3,
  Z_FIXED:                  4,
  Z_DEFAULT_STRATEGY:       0,

  /* Possible values of the data_type field (though see inflate()) */
  Z_BINARY:                 0,
  Z_TEXT:                   1,
  //Z_ASCII:                1, // = Z_TEXT (deprecated)
  Z_UNKNOWN:                2,

  /* The deflate compression method */
  Z_DEFLATED:               8
  //Z_NULL:                 null // Use -1 or null inline, depending on var type
};
},{}],6:[function(_dereq_,module,exports){
'use strict';

// Note: we can't get significant speed boost here.
// So write code to minimize size - no pregenerated tables
// and array tools dependencies.


// Use ordinary array, since untyped makes no boost here
function makeTable() {
  var c, table = [];

  for(var n =0; n < 256; n++){
    c = n;
    for(var k =0; k < 8; k++){
      c = ((c&1) ? (0xEDB88320 ^ (c >>> 1)) : (c >>> 1));
    }
    table[n] = c;
  }

  return table;
}

// Create table on load. Just 255 signed longs. Not a problem.
var crcTable = makeTable();


function crc32(crc, buf, len, pos) {
  var t = crcTable
    , end = pos + len;

  crc = crc ^ (-1);

  for (var i = pos; i < end; i++ ) {
    crc = (crc >>> 8) ^ t[(crc ^ buf[i]) & 0xFF];
  }

  return (crc ^ (-1)); // >>> 0;
}


module.exports = crc32;
},{}],7:[function(_dereq_,module,exports){
'use strict';

var utils   = _dereq_('../utils/common');
var trees   = _dereq_('./trees');
var adler32 = _dereq_('./adler32');
var crc32   = _dereq_('./crc32');
var msg   = _dereq_('./messages');

/* Public constants ==========================================================*/
/* ===========================================================================*/


/* Allowed flush values; see deflate() and inflate() below for details */
var Z_NO_FLUSH      = 0;
var Z_PARTIAL_FLUSH = 1;
//var Z_SYNC_FLUSH    = 2;
var Z_FULL_FLUSH    = 3;
var Z_FINISH        = 4;
var Z_BLOCK         = 5;
//var Z_TREES         = 6;


/* Return codes for the compression/decompression functions. Negative values
 * are errors, positive values are used for special but normal events.
 */
var Z_OK            = 0;
var Z_STREAM_END    = 1;
//var Z_NEED_DICT     = 2;
//var Z_ERRNO         = -1;
var Z_STREAM_ERROR  = -2;
var Z_DATA_ERROR    = -3;
//var Z_MEM_ERROR     = -4;
var Z_BUF_ERROR     = -5;
//var Z_VERSION_ERROR = -6;


/* compression levels */
//var Z_NO_COMPRESSION      = 0;
//var Z_BEST_SPEED          = 1;
//var Z_BEST_COMPRESSION    = 9;
var Z_DEFAULT_COMPRESSION = -1;


var Z_FILTERED            = 1;
var Z_HUFFMAN_ONLY        = 2;
var Z_RLE                 = 3;
var Z_FIXED               = 4;
var Z_DEFAULT_STRATEGY    = 0;

/* Possible values of the data_type field (though see inflate()) */
//var Z_BINARY              = 0;
//var Z_TEXT                = 1;
//var Z_ASCII               = 1; // = Z_TEXT
var Z_UNKNOWN             = 2;


/* The deflate compression method */
var Z_DEFLATED  = 8;

/*============================================================================*/


var MAX_MEM_LEVEL = 9;
/* Maximum value for memLevel in deflateInit2 */
var MAX_WBITS = 15;
/* 32K LZ77 window */
var DEF_MEM_LEVEL = 8;


var LENGTH_CODES  = 29;
/* number of length codes, not counting the special END_BLOCK code */
var LITERALS      = 256;
/* number of literal bytes 0..255 */
var L_CODES       = LITERALS + 1 + LENGTH_CODES;
/* number of Literal or Length codes, including the END_BLOCK code */
var D_CODES       = 30;
/* number of distance codes */
var BL_CODES      = 19;
/* number of codes used to transfer the bit lengths */
var HEAP_SIZE     = 2*L_CODES + 1;
/* maximum heap size */
var MAX_BITS  = 15;
/* All codes must not exceed MAX_BITS bits */

var MIN_MATCH = 3;
var MAX_MATCH = 258;
var MIN_LOOKAHEAD = (MAX_MATCH + MIN_MATCH + 1);

var PRESET_DICT = 0x20;

var INIT_STATE = 42;
var EXTRA_STATE = 69;
var NAME_STATE = 73;
var COMMENT_STATE = 91;
var HCRC_STATE = 103;
var BUSY_STATE = 113;
var FINISH_STATE = 666;

var BS_NEED_MORE      = 1; /* block not completed, need more input or more output */
var BS_BLOCK_DONE     = 2; /* block flush performed */
var BS_FINISH_STARTED = 3; /* finish started, need only more output at next deflate */
var BS_FINISH_DONE    = 4; /* finish done, accept no more input or output */

var OS_CODE = 0x03; // Unix :) . Don't detect, use this default.

function err(strm, errorCode) {
  strm.msg = msg[errorCode];
  return errorCode;
}

function rank(f) {
  return ((f) << 1) - ((f) > 4 ? 9 : 0);
}

function zero(buf) { var len = buf.length; while (--len >= 0) { buf[len] = 0; } }


/* =========================================================================
 * Flush as much pending output as possible. All deflate() output goes
 * through this function so some applications may wish to modify it
 * to avoid allocating a large strm->output buffer and copying into it.
 * (See also read_buf()).
 */
function flush_pending(strm) {
  var s = strm.state;

  //_tr_flush_bits(s);
  var len = s.pending;
  if (len > strm.avail_out) {
    len = strm.avail_out;
  }
  if (len === 0) { return; }

  utils.arraySet(strm.output, s.pending_buf, s.pending_out, len, strm.next_out);
  strm.next_out += len;
  s.pending_out += len;
  strm.total_out += len;
  strm.avail_out -= len;
  s.pending -= len;
  if (s.pending === 0) {
    s.pending_out = 0;
  }
}


function flush_block_only (s, last) {
  trees._tr_flush_block(s, (s.block_start >= 0 ? s.block_start : -1), s.strstart - s.block_start, last);
  s.block_start = s.strstart;
  flush_pending(s.strm);
}


function put_byte(s, b) {
  s.pending_buf[s.pending++] = b;
}


/* =========================================================================
 * Put a short in the pending buffer. The 16-bit value is put in MSB order.
 * IN assertion: the stream state is correct and there is enough room in
 * pending_buf.
 */
function putShortMSB(s, b) {
//  put_byte(s, (Byte)(b >> 8));
//  put_byte(s, (Byte)(b & 0xff));
  s.pending_buf[s.pending++] = (b >>> 8) & 0xff;
  s.pending_buf[s.pending++] = b & 0xff;
}


/* ===========================================================================
 * Read a new buffer from the current input stream, update the adler32
 * and total number of bytes read.  All deflate() input goes through
 * this function so some applications may wish to modify it to avoid
 * allocating a large strm->input buffer and copying from it.
 * (See also flush_pending()).
 */
function read_buf(strm, buf, start, size) {
  var len = strm.avail_in;

  if (len > size) { len = size; }
  if (len === 0) { return 0; }

  strm.avail_in -= len;

  utils.arraySet(buf, strm.input, strm.next_in, len, start);
  if (strm.state.wrap === 1) {
    strm.adler = adler32(strm.adler, buf, len, start);
  }

  else if (strm.state.wrap === 2) {
    strm.adler = crc32(strm.adler, buf, len, start);
  }

  strm.next_in += len;
  strm.total_in += len;

  return len;
}


/* ===========================================================================
 * Set match_start to the longest match starting at the given string and
 * return its length. Matches shorter or equal to prev_length are discarded,
 * in which case the result is equal to prev_length and match_start is
 * garbage.
 * IN assertions: cur_match is the head of the hash chain for the current
 *   string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
 * OUT assertion: the match length is not greater than s->lookahead.
 */
function longest_match(s, cur_match) {
  var chain_length = s.max_chain_length;      /* max hash chain length */
  var scan = s.strstart; /* current string */
  var match;                       /* matched string */
  var len;                           /* length of current match */
  var best_len = s.prev_length;              /* best match length so far */
  var nice_match = s.nice_match;             /* stop if match long enough */
  var limit = (s.strstart > (s.w_size - MIN_LOOKAHEAD)) ?
      s.strstart - (s.w_size - MIN_LOOKAHEAD) : 0/*NIL*/;

  var _win = s.window; // shortcut

  var wmask = s.w_mask;
  var prev  = s.prev;

  /* Stop when cur_match becomes <= limit. To simplify the code,
   * we prevent matches with the string of window index 0.
   */

  var strend = s.strstart + MAX_MATCH;
  var scan_end1  = _win[scan + best_len - 1];
  var scan_end   = _win[scan + best_len];

  /* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
   * It is easy to get rid of this optimization if necessary.
   */
  // Assert(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

  /* Do not waste too much time if we already have a good match: */
  if (s.prev_length >= s.good_match) {
    chain_length >>= 2;
  }
  /* Do not look for matches beyond the end of the input. This is necessary
   * to make deflate deterministic.
   */
  if (nice_match > s.lookahead) { nice_match = s.lookahead; }

  // Assert((ulg)s->strstart <= s->window_size-MIN_LOOKAHEAD, "need lookahead");

  do {
    // Assert(cur_match < s->strstart, "no future");
    match = cur_match;

    /* Skip to next match if the match length cannot increase
     * or if the match length is less than 2.  Note that the checks below
     * for insufficient lookahead only occur occasionally for performance
     * reasons.  Therefore uninitialized memory will be accessed, and
     * conditional jumps will be made that depend on those values.
     * However the length of the match is limited to the lookahead, so
     * the output of deflate is not affected by the uninitialized values.
     */

    if (_win[match + best_len]     !== scan_end  ||
        _win[match + best_len - 1] !== scan_end1 ||
        _win[match]                !== _win[scan] ||
        _win[++match]              !== _win[scan + 1]) {
      continue;
    }

    /* The check at best_len-1 can be removed because it will be made
     * again later. (This heuristic is not always a win.)
     * It is not necessary to compare scan[2] and match[2] since they
     * are always equal when the other bytes match, given that
     * the hash keys are equal and that HASH_BITS >= 8.
     */
    scan += 2;
    match++;
    // Assert(*scan == *match, "match[2]?");

    /* We check for insufficient lookahead only every 8th comparison;
     * the 256th check will be made at strstart+258.
     */
    do {
      /*jshint noempty:false*/
    } while (_win[++scan] === _win[++match] && _win[++scan] === _win[++match] &&
             _win[++scan] === _win[++match] && _win[++scan] === _win[++match] &&
             _win[++scan] === _win[++match] && _win[++scan] === _win[++match] &&
             _win[++scan] === _win[++match] && _win[++scan] === _win[++match] &&
             scan < strend);

    // Assert(scan <= s->window+(unsigned)(s->window_size-1), "wild scan");

    len = MAX_MATCH - (strend - scan);
    scan = strend - MAX_MATCH;

    if (len > best_len) {
      s.match_start = cur_match;
      best_len = len;
      if (len >= nice_match) {
        break;
      }
      scan_end1  = _win[scan + best_len - 1];
      scan_end   = _win[scan + best_len];
    }
  } while ((cur_match = prev[cur_match & wmask]) > limit && --chain_length !== 0);

  if (best_len <= s.lookahead) {
    return best_len;
  }
  return s.lookahead;
}


/* ===========================================================================
 * Fill the window when the lookahead becomes insufficient.
 * Updates strstart and lookahead.
 *
 * IN assertion: lookahead < MIN_LOOKAHEAD
 * OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
 *    At least one byte has been read, or avail_in == 0; reads are
 *    performed for at least two bytes (required for the zip translate_eol
 *    option -- not supported here).
 */
function fill_window(s) {
  var _w_size = s.w_size;
  var p, n, m, more, str;

  //Assert(s->lookahead < MIN_LOOKAHEAD, "already enough lookahead");

  do {
    more = s.window_size - s.lookahead - s.strstart;

    // JS ints have 32 bit, block below not needed
    /* Deal with !@#$% 64K limit: */
    //if (sizeof(int) <= 2) {
    //    if (more == 0 && s->strstart == 0 && s->lookahead == 0) {
    //        more = wsize;
    //
    //  } else if (more == (unsigned)(-1)) {
    //        /* Very unlikely, but possible on 16 bit machine if
    //         * strstart == 0 && lookahead == 1 (input done a byte at time)
    //         */
    //        more--;
    //    }
    //}


    /* If the window is almost full and there is insufficient lookahead,
     * move the upper half to the lower one to make room in the upper half.
     */
    if (s.strstart >= _w_size + (_w_size - MIN_LOOKAHEAD)) {

      utils.arraySet(s.window, s.window, _w_size, _w_size, 0);
      s.match_start -= _w_size;
      s.strstart -= _w_size;
      /* we now have strstart >= MAX_DIST */
      s.block_start -= _w_size;

      /* Slide the hash table (could be avoided with 32 bit values
       at the expense of memory usage). We slide even when level == 0
       to keep the hash table consistent if we switch back to level > 0
       later. (Using level 0 permanently is not an optimal usage of
       zlib, so we don't care about this pathological case.)
       */

      n = s.hash_size;
      p = n;
      do {
        m = s.head[--p];
        s.head[p] = (m >= _w_size ? m - _w_size : 0);
      } while (--n);

      n = _w_size;
      p = n;
      do {
        m = s.prev[--p];
        s.prev[p] = (m >= _w_size ? m - _w_size : 0);
        /* If n is not on any hash chain, prev[n] is garbage but
         * its value will never be used.
         */
      } while (--n);

      more += _w_size;
    }
    if (s.strm.avail_in === 0) {
      break;
    }

    /* If there was no sliding:
     *    strstart <= WSIZE+MAX_DIST-1 && lookahead <= MIN_LOOKAHEAD - 1 &&
     *    more == window_size - lookahead - strstart
     * => more >= window_size - (MIN_LOOKAHEAD-1 + WSIZE + MAX_DIST-1)
     * => more >= window_size - 2*WSIZE + 2
     * In the BIG_MEM or MMAP case (not yet supported),
     *   window_size == input_size + MIN_LOOKAHEAD  &&
     *   strstart + s->lookahead <= input_size => more >= MIN_LOOKAHEAD.
     * Otherwise, window_size == 2*WSIZE so more >= 2.
     * If there was sliding, more >= WSIZE. So in all cases, more >= 2.
     */
    //Assert(more >= 2, "more < 2");
    n = read_buf(s.strm, s.window, s.strstart + s.lookahead, more);
    s.lookahead += n;

    /* Initialize the hash value now that we have some input: */
    if (s.lookahead + s.insert >= MIN_MATCH) {
      str = s.strstart - s.insert;
      s.ins_h = s.window[str];

      /* UPDATE_HASH(s, s->ins_h, s->window[str + 1]); */
      s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[str + 1]) & s.hash_mask;
//#if MIN_MATCH != 3
//        Call update_hash() MIN_MATCH-3 more times
//#endif
      while (s.insert) {
        /* UPDATE_HASH(s, s->ins_h, s->window[str + MIN_MATCH-1]); */
        s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[str + MIN_MATCH-1]) & s.hash_mask;

        s.prev[str & s.w_mask] = s.head[s.ins_h];
        s.head[s.ins_h] = str;
        str++;
        s.insert--;
        if (s.lookahead + s.insert < MIN_MATCH) {
          break;
        }
      }
    }
    /* If the whole input has less than MIN_MATCH bytes, ins_h is garbage,
     * but this is not important since only literal bytes will be emitted.
     */

  } while (s.lookahead < MIN_LOOKAHEAD && s.strm.avail_in !== 0);

  /* If the WIN_INIT bytes after the end of the current data have never been
   * written, then zero those bytes in order to avoid memory check reports of
   * the use of uninitialized (or uninitialised as Julian writes) bytes by
   * the longest match routines.  Update the high water mark for the next
   * time through here.  WIN_INIT is set to MAX_MATCH since the longest match
   * routines allow scanning to strstart + MAX_MATCH, ignoring lookahead.
   */
//  if (s.high_water < s.window_size) {
//    var curr = s.strstart + s.lookahead;
//    var init = 0;
//
//    if (s.high_water < curr) {
//      /* Previous high water mark below current data -- zero WIN_INIT
//       * bytes or up to end of window, whichever is less.
//       */
//      init = s.window_size - curr;
//      if (init > WIN_INIT)
//        init = WIN_INIT;
//      zmemzero(s->window + curr, (unsigned)init);
//      s->high_water = curr + init;
//    }
//    else if (s->high_water < (ulg)curr + WIN_INIT) {
//      /* High water mark at or above current data, but below current data
//       * plus WIN_INIT -- zero out to current data plus WIN_INIT, or up
//       * to end of window, whichever is less.
//       */
//      init = (ulg)curr + WIN_INIT - s->high_water;
//      if (init > s->window_size - s->high_water)
//        init = s->window_size - s->high_water;
//      zmemzero(s->window + s->high_water, (unsigned)init);
//      s->high_water += init;
//    }
//  }
//
//  Assert((ulg)s->strstart <= s->window_size - MIN_LOOKAHEAD,
//    "not enough room for search");
}

/* ===========================================================================
 * Copy without compression as much as possible from the input stream, return
 * the current block state.
 * This function does not insert new strings in the dictionary since
 * uncompressible data is probably not useful. This function is used
 * only for the level=0 compression option.
 * NOTE: this function should be optimized to avoid extra copying from
 * window to pending_buf.
 */
function deflate_stored(s, flush) {
  /* Stored blocks are limited to 0xffff bytes, pending_buf is limited
   * to pending_buf_size, and each stored block has a 5 byte header:
   */
  var max_block_size = 0xffff;

  if (max_block_size > s.pending_buf_size - 5) {
    max_block_size = s.pending_buf_size - 5;
  }

  /* Copy as much as possible from input to output: */
  for (;;) {
    /* Fill the window as much as possible: */
    if (s.lookahead <= 1) {

      //Assert(s->strstart < s->w_size+MAX_DIST(s) ||
      //  s->block_start >= (long)s->w_size, "slide too late");
//      if (!(s.strstart < s.w_size + (s.w_size - MIN_LOOKAHEAD) ||
//        s.block_start >= s.w_size)) {
//        throw  new Error("slide too late");
//      }

      fill_window(s);
      if (s.lookahead === 0 && flush === Z_NO_FLUSH) {
        return BS_NEED_MORE;
      }

      if (s.lookahead === 0) {
        break;
      }
      /* flush the current block */
    }
    //Assert(s->block_start >= 0L, "block gone");
//    if (s.block_start < 0) throw new Error("block gone");

    s.strstart += s.lookahead;
    s.lookahead = 0;

    /* Emit a stored block if pending_buf will be full: */
    var max_start = s.block_start + max_block_size;

    if (s.strstart === 0 || s.strstart >= max_start) {
      /* strstart == 0 is possible when wraparound on 16-bit machine */
      s.lookahead = s.strstart - max_start;
      s.strstart = max_start;
      /*** FLUSH_BLOCK(s, 0); ***/
      flush_block_only(s, false);
      if (s.strm.avail_out === 0) {
        return BS_NEED_MORE;
      }
      /***/


    }
    /* Flush if we may have to slide, otherwise block_start may become
     * negative and the data will be gone:
     */
    if (s.strstart - s.block_start >= (s.w_size - MIN_LOOKAHEAD)) {
      /*** FLUSH_BLOCK(s, 0); ***/
      flush_block_only(s, false);
      if (s.strm.avail_out === 0) {
        return BS_NEED_MORE;
      }
      /***/
    }
  }

  s.insert = 0;

  if (flush === Z_FINISH) {
    /*** FLUSH_BLOCK(s, 1); ***/
    flush_block_only(s, true);
    if (s.strm.avail_out === 0) {
      return BS_FINISH_STARTED;
    }
    /***/
    return BS_FINISH_DONE;
  }

  if (s.strstart > s.block_start) {
    /*** FLUSH_BLOCK(s, 0); ***/
    flush_block_only(s, false);
    if (s.strm.avail_out === 0) {
      return BS_NEED_MORE;
    }
    /***/
  }

  return BS_NEED_MORE;
}

/* ===========================================================================
 * Compress as much as possible from the input stream, return the current
 * block state.
 * This function does not perform lazy evaluation of matches and inserts
 * new strings in the dictionary only for unmatched strings or for short
 * matches. It is used only for the fast compression options.
 */
function deflate_fast(s, flush) {
  var hash_head;        /* head of the hash chain */
  var bflush;           /* set if current block must be flushed */

  for (;;) {
    /* Make sure that we always have enough lookahead, except
     * at the end of the input file. We need MAX_MATCH bytes
     * for the next match, plus MIN_MATCH bytes to insert the
     * string following the next match.
     */
    if (s.lookahead < MIN_LOOKAHEAD) {
      fill_window(s);
      if (s.lookahead < MIN_LOOKAHEAD && flush === Z_NO_FLUSH) {
        return BS_NEED_MORE;
      }
      if (s.lookahead === 0) {
        break; /* flush the current block */
      }
    }

    /* Insert the string window[strstart .. strstart+2] in the
     * dictionary, and set hash_head to the head of the hash chain:
     */
    hash_head = 0/*NIL*/;
    if (s.lookahead >= MIN_MATCH) {
      /*** INSERT_STRING(s, s.strstart, hash_head); ***/
      s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[s.strstart + MIN_MATCH - 1]) & s.hash_mask;
      hash_head = s.prev[s.strstart & s.w_mask] = s.head[s.ins_h];
      s.head[s.ins_h] = s.strstart;
      /***/
    }

    /* Find the longest match, discarding those <= prev_length.
     * At this point we have always match_length < MIN_MATCH
     */
    if (hash_head !== 0/*NIL*/ && ((s.strstart - hash_head) <= (s.w_size - MIN_LOOKAHEAD))) {
      /* To simplify the code, we prevent matches with the string
       * of window index 0 (in particular we have to avoid a match
       * of the string with itself at the start of the input file).
       */
      s.match_length = longest_match(s, hash_head);
      /* longest_match() sets match_start */
    }
    if (s.match_length >= MIN_MATCH) {
      // check_match(s, s.strstart, s.match_start, s.match_length); // for debug only

      /*** _tr_tally_dist(s, s.strstart - s.match_start,
                     s.match_length - MIN_MATCH, bflush); ***/
      bflush = trees._tr_tally(s, s.strstart - s.match_start, s.match_length - MIN_MATCH);

      s.lookahead -= s.match_length;

      /* Insert new strings in the hash table only if the match length
       * is not too large. This saves time but degrades compression.
       */
      if (s.match_length <= s.max_lazy_match/*max_insert_length*/ && s.lookahead >= MIN_MATCH) {
        s.match_length--; /* string at strstart already in table */
        do {
          s.strstart++;
          /*** INSERT_STRING(s, s.strstart, hash_head); ***/
          s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[s.strstart + MIN_MATCH - 1]) & s.hash_mask;
          hash_head = s.prev[s.strstart & s.w_mask] = s.head[s.ins_h];
          s.head[s.ins_h] = s.strstart;
          /***/
          /* strstart never exceeds WSIZE-MAX_MATCH, so there are
           * always MIN_MATCH bytes ahead.
           */
        } while (--s.match_length !== 0);
        s.strstart++;
      } else
      {
        s.strstart += s.match_length;
        s.match_length = 0;
        s.ins_h = s.window[s.strstart];
        /* UPDATE_HASH(s, s.ins_h, s.window[s.strstart+1]); */
        s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[s.strstart + 1]) & s.hash_mask;

//#if MIN_MATCH != 3
//                Call UPDATE_HASH() MIN_MATCH-3 more times
//#endif
        /* If lookahead < MIN_MATCH, ins_h is garbage, but it does not
         * matter since it will be recomputed at next deflate call.
         */
      }
    } else {
      /* No match, output a literal byte */
      //Tracevv((stderr,"%c", s.window[s.strstart]));
      /*** _tr_tally_lit(s, s.window[s.strstart], bflush); ***/
      bflush = trees._tr_tally(s, 0, s.window[s.strstart]);

      s.lookahead--;
      s.strstart++;
    }
    if (bflush) {
      /*** FLUSH_BLOCK(s, 0); ***/
      flush_block_only(s, false);
      if (s.strm.avail_out === 0) {
        return BS_NEED_MORE;
      }
      /***/
    }
  }
  s.insert = ((s.strstart < (MIN_MATCH-1)) ? s.strstart : MIN_MATCH-1);
  if (flush === Z_FINISH) {
    /*** FLUSH_BLOCK(s, 1); ***/
    flush_block_only(s, true);
    if (s.strm.avail_out === 0) {
      return BS_FINISH_STARTED;
    }
    /***/
    return BS_FINISH_DONE;
  }
  if (s.last_lit) {
    /*** FLUSH_BLOCK(s, 0); ***/
    flush_block_only(s, false);
    if (s.strm.avail_out === 0) {
      return BS_NEED_MORE;
    }
    /***/
  }
  return BS_BLOCK_DONE;
}

/* ===========================================================================
 * Same as above, but achieves better compression. We use a lazy
 * evaluation for matches: a match is finally adopted only if there is
 * no better match at the next window position.
 */
function deflate_slow(s, flush) {
  var hash_head;          /* head of hash chain */
  var bflush;              /* set if current block must be flushed */

  var max_insert;

  /* Process the input block. */
  for (;;) {
    /* Make sure that we always have enough lookahead, except
     * at the end of the input file. We need MAX_MATCH bytes
     * for the next match, plus MIN_MATCH bytes to insert the
     * string following the next match.
     */
    if (s.lookahead < MIN_LOOKAHEAD) {
      fill_window(s);
      if (s.lookahead < MIN_LOOKAHEAD && flush === Z_NO_FLUSH) {
        return BS_NEED_MORE;
      }
      if (s.lookahead === 0) { break; } /* flush the current block */
    }

    /* Insert the string window[strstart .. strstart+2] in the
     * dictionary, and set hash_head to the head of the hash chain:
     */
    hash_head = 0/*NIL*/;
    if (s.lookahead >= MIN_MATCH) {
      /*** INSERT_STRING(s, s.strstart, hash_head); ***/
      s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[s.strstart + MIN_MATCH - 1]) & s.hash_mask;
      hash_head = s.prev[s.strstart & s.w_mask] = s.head[s.ins_h];
      s.head[s.ins_h] = s.strstart;
      /***/
    }

    /* Find the longest match, discarding those <= prev_length.
     */
    s.prev_length = s.match_length;
    s.prev_match = s.match_start;
    s.match_length = MIN_MATCH-1;

    if (hash_head !== 0/*NIL*/ && s.prev_length < s.max_lazy_match &&
        s.strstart - hash_head <= (s.w_size-MIN_LOOKAHEAD)/*MAX_DIST(s)*/) {
      /* To simplify the code, we prevent matches with the string
       * of window index 0 (in particular we have to avoid a match
       * of the string with itself at the start of the input file).
       */
      s.match_length = longest_match(s, hash_head);
      /* longest_match() sets match_start */

      if (s.match_length <= 5 &&
         (s.strategy === Z_FILTERED || (s.match_length === MIN_MATCH && s.strstart - s.match_start > 4096/*TOO_FAR*/))) {

        /* If prev_match is also MIN_MATCH, match_start is garbage
         * but we will ignore the current match anyway.
         */
        s.match_length = MIN_MATCH-1;
      }
    }
    /* If there was a match at the previous step and the current
     * match is not better, output the previous match:
     */
    if (s.prev_length >= MIN_MATCH && s.match_length <= s.prev_length) {
      max_insert = s.strstart + s.lookahead - MIN_MATCH;
      /* Do not insert strings in hash table beyond this. */

      //check_match(s, s.strstart-1, s.prev_match, s.prev_length);

      /***_tr_tally_dist(s, s.strstart - 1 - s.prev_match,
                     s.prev_length - MIN_MATCH, bflush);***/
      bflush = trees._tr_tally(s, s.strstart - 1- s.prev_match, s.prev_length - MIN_MATCH);
      /* Insert in hash table all strings up to the end of the match.
       * strstart-1 and strstart are already inserted. If there is not
       * enough lookahead, the last two strings are not inserted in
       * the hash table.
       */
      s.lookahead -= s.prev_length-1;
      s.prev_length -= 2;
      do {
        if (++s.strstart <= max_insert) {
          /*** INSERT_STRING(s, s.strstart, hash_head); ***/
          s.ins_h = ((s.ins_h << s.hash_shift) ^ s.window[s.strstart + MIN_MATCH - 1]) & s.hash_mask;
          hash_head = s.prev[s.strstart & s.w_mask] = s.head[s.ins_h];
          s.head[s.ins_h] = s.strstart;
          /***/
        }
      } while (--s.prev_length !== 0);
      s.match_available = 0;
      s.match_length = MIN_MATCH-1;
      s.strstart++;

      if (bflush) {
        /*** FLUSH_BLOCK(s, 0); ***/
        flush_block_only(s, false);
        if (s.strm.avail_out === 0) {
          return BS_NEED_MORE;
        }
        /***/
      }

    } else if (s.match_available) {
      /* If there was no match at the previous position, output a
       * single literal. If there was a match but the current match
       * is longer, truncate the previous match to a single literal.
       */
      //Tracevv((stderr,"%c", s->window[s->strstart-1]));
      /*** _tr_tally_lit(s, s.window[s.strstart-1], bflush); ***/
      bflush = trees._tr_tally(s, 0, s.window[s.strstart-1]);

      if (bflush) {
        /*** FLUSH_BLOCK_ONLY(s, 0) ***/
        flush_block_only(s, false);
        /***/
      }
      s.strstart++;
      s.lookahead--;
      if (s.strm.avail_out === 0) {
        return BS_NEED_MORE;
      }
    } else {
      /* There is no previous match to compare with, wait for
       * the next step to decide.
       */
      s.match_available = 1;
      s.strstart++;
      s.lookahead--;
    }
  }
  //Assert (flush != Z_NO_FLUSH, "no flush?");
  if (s.match_available) {
    //Tracevv((stderr,"%c", s->window[s->strstart-1]));
    /*** _tr_tally_lit(s, s.window[s.strstart-1], bflush); ***/
    bflush = trees._tr_tally(s, 0, s.window[s.strstart-1]);

    s.match_available = 0;
  }
  s.insert = s.strstart < MIN_MATCH-1 ? s.strstart : MIN_MATCH-1;
  if (flush === Z_FINISH) {
    /*** FLUSH_BLOCK(s, 1); ***/
    flush_block_only(s, true);
    if (s.strm.avail_out === 0) {
      return BS_FINISH_STARTED;
    }
    /***/
    return BS_FINISH_DONE;
  }
  if (s.last_lit) {
    /*** FLUSH_BLOCK(s, 0); ***/
    flush_block_only(s, false);
    if (s.strm.avail_out === 0) {
      return BS_NEED_MORE;
    }
    /***/
  }

  return BS_BLOCK_DONE;
}


/* ===========================================================================
 * For Z_RLE, simply look for runs of bytes, generate matches only of distance
 * one.  Do not maintain a hash table.  (It will be regenerated if this run of
 * deflate switches away from Z_RLE.)
 */
function deflate_rle(s, flush) {
  var bflush;            /* set if current block must be flushed */
  var prev;              /* byte at distance one to match */
  var scan, strend;      /* scan goes up to strend for length of run */

  var _win = s.window;

  for (;;) {
    /* Make sure that we always have enough lookahead, except
     * at the end of the input file. We need MAX_MATCH bytes
     * for the longest run, plus one for the unrolled loop.
     */
    if (s.lookahead <= MAX_MATCH) {
      fill_window(s);
      if (s.lookahead <= MAX_MATCH && flush === Z_NO_FLUSH) {
        return BS_NEED_MORE;
      }
      if (s.lookahead === 0) { break; } /* flush the current block */
    }

    /* See how many times the previous byte repeats */
    s.match_length = 0;
    if (s.lookahead >= MIN_MATCH && s.strstart > 0) {
      scan = s.strstart - 1;
      prev = _win[scan];
      if (prev === _win[++scan] && prev === _win[++scan] && prev === _win[++scan]) {
        strend = s.strstart + MAX_MATCH;
        do {
          /*jshint noempty:false*/
        } while (prev === _win[++scan] && prev === _win[++scan] &&
                 prev === _win[++scan] && prev === _win[++scan] &&
                 prev === _win[++scan] && prev === _win[++scan] &&
                 prev === _win[++scan] && prev === _win[++scan] &&
                 scan < strend);
        s.match_length = MAX_MATCH - (strend - scan);
        if (s.match_length > s.lookahead) {
          s.match_length = s.lookahead;
        }
      }
      //Assert(scan <= s->window+(uInt)(s->window_size-1), "wild scan");
    }

    /* Emit match if have run of MIN_MATCH or longer, else emit literal */
    if (s.match_length >= MIN_MATCH) {
      //check_match(s, s.strstart, s.strstart - 1, s.match_length);

      /*** _tr_tally_dist(s, 1, s.match_length - MIN_MATCH, bflush); ***/
      bflush = trees._tr_tally(s, 1, s.match_length - MIN_MATCH);

      s.lookahead -= s.match_length;
      s.strstart += s.match_length;
      s.match_length = 0;
    } else {
      /* No match, output a literal byte */
      //Tracevv((stderr,"%c", s->window[s->strstart]));
      /*** _tr_tally_lit(s, s.window[s.strstart], bflush); ***/
      bflush = trees._tr_tally(s, 0, s.window[s.strstart]);

      s.lookahead--;
      s.strstart++;
    }
    if (bflush) {
      /*** FLUSH_BLOCK(s, 0); ***/
      flush_block_only(s, false);
      if (s.strm.avail_out === 0) {
        return BS_NEED_MORE;
      }
      /***/
    }
  }
  s.insert = 0;
  if (flush === Z_FINISH) {
    /*** FLUSH_BLOCK(s, 1); ***/
    flush_block_only(s, true);
    if (s.strm.avail_out === 0) {
      return BS_FINISH_STARTED;
    }
    /***/
    return BS_FINISH_DONE;
  }
  if (s.last_lit) {
    /*** FLUSH_BLOCK(s, 0); ***/
    flush_block_only(s, false);
    if (s.strm.avail_out === 0) {
      return BS_NEED_MORE;
    }
    /***/
  }
  return BS_BLOCK_DONE;
}

/* ===========================================================================
 * For Z_HUFFMAN_ONLY, do not look for matches.  Do not maintain a hash table.
 * (It will be regenerated if this run of deflate switches away from Huffman.)
 */
function deflate_huff(s, flush) {
  var bflush;             /* set if current block must be flushed */

  for (;;) {
    /* Make sure that we have a literal to write. */
    if (s.lookahead === 0) {
      fill_window(s);
      if (s.lookahead === 0) {
        if (flush === Z_NO_FLUSH) {
          return BS_NEED_MORE;
        }
        break;      /* flush the current block */
      }
    }

    /* Output a literal byte */
    s.match_length = 0;
    //Tracevv((stderr,"%c", s->window[s->strstart]));
    /*** _tr_tally_lit(s, s.window[s.strstart], bflush); ***/
    bflush = trees._tr_tally(s, 0, s.window[s.strstart]);
    s.lookahead--;
    s.strstart++;
    if (bflush) {
      /*** FLUSH_BLOCK(s, 0); ***/
      flush_block_only(s, false);
      if (s.strm.avail_out === 0) {
        return BS_NEED_MORE;
      }
      /***/
    }
  }
  s.insert = 0;
  if (flush === Z_FINISH) {
    /*** FLUSH_BLOCK(s, 1); ***/
    flush_block_only(s, true);
    if (s.strm.avail_out === 0) {
      return BS_FINISH_STARTED;
    }
    /***/
    return BS_FINISH_DONE;
  }
  if (s.last_lit) {
    /*** FLUSH_BLOCK(s, 0); ***/
    flush_block_only(s, false);
    if (s.strm.avail_out === 0) {
      return BS_NEED_MORE;
    }
    /***/
  }
  return BS_BLOCK_DONE;
}

/* Values for max_lazy_match, good_match and max_chain_length, depending on
 * the desired pack level (0..9). The values given below have been tuned to
 * exclude worst case performance for pathological files. Better values may be
 * found for specific files.
 */
var Config = function (good_length, max_lazy, nice_length, max_chain, func) {
  this.good_length = good_length;
  this.max_lazy = max_lazy;
  this.nice_length = nice_length;
  this.max_chain = max_chain;
  this.func = func;
};

var configuration_table;

configuration_table = [
  /*      good lazy nice chain */
  new Config(0, 0, 0, 0, deflate_stored),          /* 0 store only */
  new Config(4, 4, 8, 4, deflate_fast),            /* 1 max speed, no lazy matches */
  new Config(4, 5, 16, 8, deflate_fast),           /* 2 */
  new Config(4, 6, 32, 32, deflate_fast),          /* 3 */

  new Config(4, 4, 16, 16, deflate_slow),          /* 4 lazy matches */
  new Config(8, 16, 32, 32, deflate_slow),         /* 5 */
  new Config(8, 16, 128, 128, deflate_slow),       /* 6 */
  new Config(8, 32, 128, 256, deflate_slow),       /* 7 */
  new Config(32, 128, 258, 1024, deflate_slow),    /* 8 */
  new Config(32, 258, 258, 4096, deflate_slow)     /* 9 max compression */
];


/* ===========================================================================
 * Initialize the "longest match" routines for a new zlib stream
 */
function lm_init(s) {
  s.window_size = 2 * s.w_size;

  /*** CLEAR_HASH(s); ***/
  zero(s.head); // Fill with NIL (= 0);

  /* Set the default configuration parameters:
   */
  s.max_lazy_match = configuration_table[s.level].max_lazy;
  s.good_match = configuration_table[s.level].good_length;
  s.nice_match = configuration_table[s.level].nice_length;
  s.max_chain_length = configuration_table[s.level].max_chain;

  s.strstart = 0;
  s.block_start = 0;
  s.lookahead = 0;
  s.insert = 0;
  s.match_length = s.prev_length = MIN_MATCH - 1;
  s.match_available = 0;
  s.ins_h = 0;
}


function DeflateState() {
  this.strm = null;            /* pointer back to this zlib stream */
  this.status = 0;            /* as the name implies */
  this.pending_buf = null;      /* output still pending */
  this.pending_buf_size = 0;  /* size of pending_buf */
  this.pending_out = 0;       /* next pending byte to output to the stream */
  this.pending = 0;           /* nb of bytes in the pending buffer */
  this.wrap = 0;              /* bit 0 true for zlib, bit 1 true for gzip */
  this.gzhead = null;         /* gzip header information to write */
  this.gzindex = 0;           /* where in extra, name, or comment */
  this.method = Z_DEFLATED; /* can only be DEFLATED */
  this.last_flush = -1;   /* value of flush param for previous deflate call */

  this.w_size = 0;  /* LZ77 window size (32K by default) */
  this.w_bits = 0;  /* log2(w_size)  (8..16) */
  this.w_mask = 0;  /* w_size - 1 */

  this.window = null;
  /* Sliding window. Input bytes are read into the second half of the window,
   * and move to the first half later to keep a dictionary of at least wSize
   * bytes. With this organization, matches are limited to a distance of
   * wSize-MAX_MATCH bytes, but this ensures that IO is always
   * performed with a length multiple of the block size.
   */

  this.window_size = 0;
  /* Actual size of window: 2*wSize, except when the user input buffer
   * is directly used as sliding window.
   */

  this.prev = null;
  /* Link to older string with same hash index. To limit the size of this
   * array to 64K, this link is maintained only for the last 32K strings.
   * An index in this array is thus a window index modulo 32K.
   */

  this.head = null;   /* Heads of the hash chains or NIL. */

  this.ins_h = 0;       /* hash index of string to be inserted */
  this.hash_size = 0;   /* number of elements in hash table */
  this.hash_bits = 0;   /* log2(hash_size) */
  this.hash_mask = 0;   /* hash_size-1 */

  this.hash_shift = 0;
  /* Number of bits by which ins_h must be shifted at each input
   * step. It must be such that after MIN_MATCH steps, the oldest
   * byte no longer takes part in the hash key, that is:
   *   hash_shift * MIN_MATCH >= hash_bits
   */

  this.block_start = 0;
  /* Window position at the beginning of the current output block. Gets
   * negative when the window is moved backwards.
   */

  this.match_length = 0;      /* length of best match */
  this.prev_match = 0;        /* previous match */
  this.match_available = 0;   /* set if previous match exists */
  this.strstart = 0;          /* start of string to insert */
  this.match_start = 0;       /* start of matching string */
  this.lookahead = 0;         /* number of valid bytes ahead in window */

  this.prev_length = 0;
  /* Length of the best match at previous step. Matches not greater than this
   * are discarded. This is used in the lazy match evaluation.
   */

  this.max_chain_length = 0;
  /* To speed up deflation, hash chains are never searched beyond this
   * length.  A higher limit improves compression ratio but degrades the
   * speed.
   */

  this.max_lazy_match = 0;
  /* Attempt to find a better match only when the current match is strictly
   * smaller than this value. This mechanism is used only for compression
   * levels >= 4.
   */
  // That's alias to max_lazy_match, don't use directly
  //this.max_insert_length = 0;
  /* Insert new strings in the hash table only if the match length is not
   * greater than this length. This saves time but degrades compression.
   * max_insert_length is used only for compression levels <= 3.
   */

  this.level = 0;     /* compression level (1..9) */
  this.strategy = 0;  /* favor or force Huffman coding*/

  this.good_match = 0;
  /* Use a faster search when the previous match is longer than this */

  this.nice_match = 0; /* Stop searching when current match exceeds this */

              /* used by trees.c: */

  /* Didn't use ct_data typedef below to suppress compiler warning */

  // struct ct_data_s dyn_ltree[HEAP_SIZE];   /* literal and length tree */
  // struct ct_data_s dyn_dtree[2*D_CODES+1]; /* distance tree */
  // struct ct_data_s bl_tree[2*BL_CODES+1];  /* Huffman tree for bit lengths */

  // Use flat array of DOUBLE size, with interleaved fata,
  // because JS does not support effective
  this.dyn_ltree  = new utils.Buf16(HEAP_SIZE * 2);
  this.dyn_dtree  = new utils.Buf16((2*D_CODES+1) * 2);
  this.bl_tree    = new utils.Buf16((2*BL_CODES+1) * 2);
  zero(this.dyn_ltree);
  zero(this.dyn_dtree);
  zero(this.bl_tree);

  this.l_desc   = null;         /* desc. for literal tree */
  this.d_desc   = null;         /* desc. for distance tree */
  this.bl_desc  = null;         /* desc. for bit length tree */

  //ush bl_count[MAX_BITS+1];
  this.bl_count = new utils.Buf16(MAX_BITS+1);
  /* number of codes at each bit length for an optimal tree */

  //int heap[2*L_CODES+1];      /* heap used to build the Huffman trees */
  this.heap = new utils.Buf16(2*L_CODES+1);  /* heap used to build the Huffman trees */
  zero(this.heap);

  this.heap_len = 0;               /* number of elements in the heap */
  this.heap_max = 0;               /* element of largest frequency */
  /* The sons of heap[n] are heap[2*n] and heap[2*n+1]. heap[0] is not used.
   * The same heap array is used to build all trees.
   */

  this.depth = new utils.Buf16(2*L_CODES+1); //uch depth[2*L_CODES+1];
  zero(this.depth);
  /* Depth of each subtree used as tie breaker for trees of equal frequency
   */

  this.l_buf = 0;          /* buffer index for literals or lengths */

  this.lit_bufsize = 0;
  /* Size of match buffer for literals/lengths.  There are 4 reasons for
   * limiting lit_bufsize to 64K:
   *   - frequencies can be kept in 16 bit counters
   *   - if compression is not successful for the first block, all input
   *     data is still in the window so we can still emit a stored block even
   *     when input comes from standard input.  (This can also be done for
   *     all blocks if lit_bufsize is not greater than 32K.)
   *   - if compression is not successful for a file smaller than 64K, we can
   *     even emit a stored file instead of a stored block (saving 5 bytes).
   *     This is applicable only for zip (not gzip or zlib).
   *   - creating new Huffman trees less frequently may not provide fast
   *     adaptation to changes in the input data statistics. (Take for
   *     example a binary file with poorly compressible code followed by
   *     a highly compressible string table.) Smaller buffer sizes give
   *     fast adaptation but have of course the overhead of transmitting
   *     trees more frequently.
   *   - I can't count above 4
   */

  this.last_lit = 0;      /* running index in l_buf */

  this.d_buf = 0;
  /* Buffer index for distances. To simplify the code, d_buf and l_buf have
   * the same number of elements. To use different lengths, an extra flag
   * array would be necessary.
   */

  this.opt_len = 0;       /* bit length of current block with optimal trees */
  this.static_len = 0;    /* bit length of current block with static trees */
  this.matches = 0;       /* number of string matches in current block */
  this.insert = 0;        /* bytes at end of window left to insert */


  this.bi_buf = 0;
  /* Output buffer. bits are inserted starting at the bottom (least
   * significant bits).
   */
  this.bi_valid = 0;
  /* Number of valid bits in bi_buf.  All bits above the last valid bit
   * are always zero.
   */

  // Used for window memory init. We safely ignore it for JS. That makes
  // sense only for pointers and memory check tools.
  //this.high_water = 0;
  /* High water mark offset in window for initialized bytes -- bytes above
   * this are set to zero in order to avoid memory check warnings when
   * longest match routines access bytes past the input.  This is then
   * updated to the new high water mark.
   */
}


function deflateResetKeep(strm) {
  var s;

  if (!strm || !strm.state) {
    return err(strm, Z_STREAM_ERROR);
  }

  strm.total_in = strm.total_out = 0;
  strm.data_type = Z_UNKNOWN;

  s = strm.state;
  s.pending = 0;
  s.pending_out = 0;

  if (s.wrap < 0) {
    s.wrap = -s.wrap;
    /* was made negative by deflate(..., Z_FINISH); */
  }
  s.status = (s.wrap ? INIT_STATE : BUSY_STATE);
  strm.adler = (s.wrap === 2) ?
    0  // crc32(0, Z_NULL, 0)
  :
    1; // adler32(0, Z_NULL, 0)
  s.last_flush = Z_NO_FLUSH;
  trees._tr_init(s);
  return Z_OK;
}


function deflateReset(strm) {
  var ret = deflateResetKeep(strm);
  if (ret === Z_OK) {
    lm_init(strm.state);
  }
  return ret;
}


function deflateSetHeader(strm, head) {
  if (!strm || !strm.state) { return Z_STREAM_ERROR; }
  if (strm.state.wrap !== 2) { return Z_STREAM_ERROR; }
  strm.state.gzhead = head;
  return Z_OK;
}


function deflateInit2(strm, level, method, windowBits, memLevel, strategy) {
  if (!strm) { // === Z_NULL
    return Z_STREAM_ERROR;
  }
  var wrap = 1;

  if (level === Z_DEFAULT_COMPRESSION) {
    level = 6;
  }

  if (windowBits < 0) { /* suppress zlib wrapper */
    wrap = 0;
    windowBits = -windowBits;
  }

  else if (windowBits > 15) {
    wrap = 2;           /* write gzip wrapper instead */
    windowBits -= 16;
  }


  if (memLevel < 1 || memLevel > MAX_MEM_LEVEL || method !== Z_DEFLATED ||
    windowBits < 8 || windowBits > 15 || level < 0 || level > 9 ||
    strategy < 0 || strategy > Z_FIXED) {
    return err(strm, Z_STREAM_ERROR);
  }


  if (windowBits === 8) {
    windowBits = 9;
  }
  /* until 256-byte window bug fixed */

  var s = new DeflateState();

  strm.state = s;
  s.strm = strm;

  s.wrap = wrap;
  s.gzhead = null;
  s.w_bits = windowBits;
  s.w_size = 1 << s.w_bits;
  s.w_mask = s.w_size - 1;

  s.hash_bits = memLevel + 7;
  s.hash_size = 1 << s.hash_bits;
  s.hash_mask = s.hash_size - 1;
  s.hash_shift = ~~((s.hash_bits + MIN_MATCH - 1) / MIN_MATCH);

  s.window = new utils.Buf8(s.w_size * 2);
  s.head = new utils.Buf16(s.hash_size);
  s.prev = new utils.Buf16(s.w_size);

  // Don't need mem init magic for JS.
  //s.high_water = 0;  /* nothing written to s->window yet */

  s.lit_bufsize = 1 << (memLevel + 6); /* 16K elements by default */

  s.pending_buf_size = s.lit_bufsize * 4;
  s.pending_buf = new utils.Buf8(s.pending_buf_size);

  s.d_buf = s.lit_bufsize >> 1;
  s.l_buf = (1 + 2) * s.lit_bufsize;

  s.level = level;
  s.strategy = strategy;
  s.method = method;

  return deflateReset(strm);
}

function deflateInit(strm, level) {
  return deflateInit2(strm, level, Z_DEFLATED, MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
}


function deflate(strm, flush) {
  var old_flush, s;
  var beg, val; // for gzip header write only

  if (!strm || !strm.state ||
    flush > Z_BLOCK || flush < 0) {
    return strm ? err(strm, Z_STREAM_ERROR) : Z_STREAM_ERROR;
  }

  s = strm.state;

  if (!strm.output ||
      (!strm.input && strm.avail_in !== 0) ||
      (s.status === FINISH_STATE && flush !== Z_FINISH)) {
    return err(strm, (strm.avail_out === 0) ? Z_BUF_ERROR : Z_STREAM_ERROR);
  }

  s.strm = strm; /* just in case */
  old_flush = s.last_flush;
  s.last_flush = flush;

  /* Write the header */
  if (s.status === INIT_STATE) {

    if (s.wrap === 2) { // GZIP header
      strm.adler = 0;  //crc32(0L, Z_NULL, 0);
      put_byte(s, 31);
      put_byte(s, 139);
      put_byte(s, 8);
      if (!s.gzhead) { // s->gzhead == Z_NULL
        put_byte(s, 0);
        put_byte(s, 0);
        put_byte(s, 0);
        put_byte(s, 0);
        put_byte(s, 0);
        put_byte(s, s.level === 9 ? 2 :
                    (s.strategy >= Z_HUFFMAN_ONLY || s.level < 2 ?
                     4 : 0));
        put_byte(s, OS_CODE);
        s.status = BUSY_STATE;
      }
      else {
        put_byte(s, (s.gzhead.text ? 1 : 0) +
                    (s.gzhead.hcrc ? 2 : 0) +
                    (!s.gzhead.extra ? 0 : 4) +
                    (!s.gzhead.name ? 0 : 8) +
                    (!s.gzhead.comment ? 0 : 16)
                );
        put_byte(s, s.gzhead.time & 0xff);
        put_byte(s, (s.gzhead.time >> 8) & 0xff);
        put_byte(s, (s.gzhead.time >> 16) & 0xff);
        put_byte(s, (s.gzhead.time >> 24) & 0xff);
        put_byte(s, s.level === 9 ? 2 :
                    (s.strategy >= Z_HUFFMAN_ONLY || s.level < 2 ?
                     4 : 0));
        put_byte(s, s.gzhead.os & 0xff);
        if (s.gzhead.extra && s.gzhead.extra.length) {
          put_byte(s, s.gzhead.extra.length & 0xff);
          put_byte(s, (s.gzhead.extra.length >> 8) & 0xff);
        }
        if (s.gzhead.hcrc) {
          strm.adler = crc32(strm.adler, s.pending_buf, s.pending, 0);
        }
        s.gzindex = 0;
        s.status = EXTRA_STATE;
      }
    }
    else // DEFLATE header
    {
      var header = (Z_DEFLATED + ((s.w_bits - 8) << 4)) << 8;
      var level_flags = -1;

      if (s.strategy >= Z_HUFFMAN_ONLY || s.level < 2) {
        level_flags = 0;
      } else if (s.level < 6) {
        level_flags = 1;
      } else if (s.level === 6) {
        level_flags = 2;
      } else {
        level_flags = 3;
      }
      header |= (level_flags << 6);
      if (s.strstart !== 0) { header |= PRESET_DICT; }
      header += 31 - (header % 31);

      s.status = BUSY_STATE;
      putShortMSB(s, header);

      /* Save the adler32 of the preset dictionary: */
      if (s.strstart !== 0) {
        putShortMSB(s, strm.adler >>> 16);
        putShortMSB(s, strm.adler & 0xffff);
      }
      strm.adler = 1; // adler32(0L, Z_NULL, 0);
    }
  }

//#ifdef GZIP
  if (s.status === EXTRA_STATE) {
    if (s.gzhead.extra/* != Z_NULL*/) {
      beg = s.pending;  /* start of bytes to update crc */

      while (s.gzindex < (s.gzhead.extra.length & 0xffff)) {
        if (s.pending === s.pending_buf_size) {
          if (s.gzhead.hcrc && s.pending > beg) {
            strm.adler = crc32(strm.adler, s.pending_buf, s.pending - beg, beg);
          }
          flush_pending(strm);
          beg = s.pending;
          if (s.pending === s.pending_buf_size) {
            break;
          }
        }
        put_byte(s, s.gzhead.extra[s.gzindex] & 0xff);
        s.gzindex++;
      }
      if (s.gzhead.hcrc && s.pending > beg) {
        strm.adler = crc32(strm.adler, s.pending_buf, s.pending - beg, beg);
      }
      if (s.gzindex === s.gzhead.extra.length) {
        s.gzindex = 0;
        s.status = NAME_STATE;
      }
    }
    else {
      s.status = NAME_STATE;
    }
  }
  if (s.status === NAME_STATE) {
    if (s.gzhead.name/* != Z_NULL*/) {
      beg = s.pending;  /* start of bytes to update crc */
      //int val;

      do {
        if (s.pending === s.pending_buf_size) {
          if (s.gzhead.hcrc && s.pending > beg) {
            strm.adler = crc32(strm.adler, s.pending_buf, s.pending - beg, beg);
          }
          flush_pending(strm);
          beg = s.pending;
          if (s.pending === s.pending_buf_size) {
            val = 1;
            break;
          }
        }
        // JS specific: little magic to add zero terminator to end of string
        if (s.gzindex < s.gzhead.name.length) {
          val = s.gzhead.name.charCodeAt(s.gzindex++) & 0xff;
        } else {
          val = 0;
        }
        put_byte(s, val);
      } while (val !== 0);

      if (s.gzhead.hcrc && s.pending > beg){
        strm.adler = crc32(strm.adler, s.pending_buf, s.pending - beg, beg);
      }
      if (val === 0) {
        s.gzindex = 0;
        s.status = COMMENT_STATE;
      }
    }
    else {
      s.status = COMMENT_STATE;
    }
  }
  if (s.status === COMMENT_STATE) {
    if (s.gzhead.comment/* != Z_NULL*/) {
      beg = s.pending;  /* start of bytes to update crc */
      //int val;

      do {
        if (s.pending === s.pending_buf_size) {
          if (s.gzhead.hcrc && s.pending > beg) {
            strm.adler = crc32(strm.adler, s.pending_buf, s.pending - beg, beg);
          }
          flush_pending(strm);
          beg = s.pending;
          if (s.pending === s.pending_buf_size) {
            val = 1;
            break;
          }
        }
        // JS specific: little magic to add zero terminator to end of string
        if (s.gzindex < s.gzhead.comment.length) {
          val = s.gzhead.comment.charCodeAt(s.gzindex++) & 0xff;
        } else {
          val = 0;
        }
        put_byte(s, val);
      } while (val !== 0);

      if (s.gzhead.hcrc && s.pending > beg) {
        strm.adler = crc32(strm.adler, s.pending_buf, s.pending - beg, beg);
      }
      if (val === 0) {
        s.status = HCRC_STATE;
      }
    }
    else {
      s.status = HCRC_STATE;
    }
  }
  if (s.status === HCRC_STATE) {
    if (s.gzhead.hcrc) {
      if (s.pending + 2 > s.pending_buf_size) {
        flush_pending(strm);
      }
      if (s.pending + 2 <= s.pending_buf_size) {
        put_byte(s, strm.adler & 0xff);
        put_byte(s, (strm.adler >> 8) & 0xff);
        strm.adler = 0; //crc32(0L, Z_NULL, 0);
        s.status = BUSY_STATE;
      }
    }
    else {
      s.status = BUSY_STATE;
    }
  }
//#endif

  /* Flush as much pending output as possible */
  if (s.pending !== 0) {
    flush_pending(strm);
    if (strm.avail_out === 0) {
      /* Since avail_out is 0, deflate will be called again with
       * more output space, but possibly with both pending and
       * avail_in equal to zero. There won't be anything to do,
       * but this is not an error situation so make sure we
       * return OK instead of BUF_ERROR at next call of deflate:
       */
      s.last_flush = -1;
      return Z_OK;
    }

    /* Make sure there is something to do and avoid duplicate consecutive
     * flushes. For repeated and useless calls with Z_FINISH, we keep
     * returning Z_STREAM_END instead of Z_BUF_ERROR.
     */
  } else if (strm.avail_in === 0 && rank(flush) <= rank(old_flush) &&
    flush !== Z_FINISH) {
    return err(strm, Z_BUF_ERROR);
  }

  /* User must not provide more input after the first FINISH: */
  if (s.status === FINISH_STATE && strm.avail_in !== 0) {
    return err(strm, Z_BUF_ERROR);
  }

  /* Start a new block or continue the current one.
   */
  if (strm.avail_in !== 0 || s.lookahead !== 0 ||
    (flush !== Z_NO_FLUSH && s.status !== FINISH_STATE)) {
    var bstate = (s.strategy === Z_HUFFMAN_ONLY) ? deflate_huff(s, flush) :
      (s.strategy === Z_RLE ? deflate_rle(s, flush) :
        configuration_table[s.level].func(s, flush));

    if (bstate === BS_FINISH_STARTED || bstate === BS_FINISH_DONE) {
      s.status = FINISH_STATE;
    }
    if (bstate === BS_NEED_MORE || bstate === BS_FINISH_STARTED) {
      if (strm.avail_out === 0) {
        s.last_flush = -1;
        /* avoid BUF_ERROR next call, see above */
      }
      return Z_OK;
      /* If flush != Z_NO_FLUSH && avail_out == 0, the next call
       * of deflate should use the same flush parameter to make sure
       * that the flush is complete. So we don't have to output an
       * empty block here, this will be done at next call. This also
       * ensures that for a very small output buffer, we emit at most
       * one empty block.
       */
    }
    if (bstate === BS_BLOCK_DONE) {
      if (flush === Z_PARTIAL_FLUSH) {
        trees._tr_align(s);
      }
      else if (flush !== Z_BLOCK) { /* FULL_FLUSH or SYNC_FLUSH */

        trees._tr_stored_block(s, 0, 0, false);
        /* For a full flush, this empty block will be recognized
         * as a special marker by inflate_sync().
         */
        if (flush === Z_FULL_FLUSH) {
          /*** CLEAR_HASH(s); ***/             /* forget history */
          zero(s.head); // Fill with NIL (= 0);

          if (s.lookahead === 0) {
            s.strstart = 0;
            s.block_start = 0;
            s.insert = 0;
          }
        }
      }
      flush_pending(strm);
      if (strm.avail_out === 0) {
        s.last_flush = -1; /* avoid BUF_ERROR at next call, see above */
        return Z_OK;
      }
    }
  }
  //Assert(strm->avail_out > 0, "bug2");
  //if (strm.avail_out <= 0) { throw new Error("bug2");}

  if (flush !== Z_FINISH) { return Z_OK; }
  if (s.wrap <= 0) { return Z_STREAM_END; }

  /* Write the trailer */
  if (s.wrap === 2) {
    put_byte(s, strm.adler & 0xff);
    put_byte(s, (strm.adler >> 8) & 0xff);
    put_byte(s, (strm.adler >> 16) & 0xff);
    put_byte(s, (strm.adler >> 24) & 0xff);
    put_byte(s, strm.total_in & 0xff);
    put_byte(s, (strm.total_in >> 8) & 0xff);
    put_byte(s, (strm.total_in >> 16) & 0xff);
    put_byte(s, (strm.total_in >> 24) & 0xff);
  }
  else
  {
    putShortMSB(s, strm.adler >>> 16);
    putShortMSB(s, strm.adler & 0xffff);
  }

  flush_pending(strm);
  /* If avail_out is zero, the application will call deflate again
   * to flush the rest.
   */
  if (s.wrap > 0) { s.wrap = -s.wrap; }
  /* write the trailer only once! */
  return s.pending !== 0 ? Z_OK : Z_STREAM_END;
}

function deflateEnd(strm) {
  var status;

  if (!strm/*== Z_NULL*/ || !strm.state/*== Z_NULL*/) {
    return Z_STREAM_ERROR;
  }

  status = strm.state.status;
  if (status !== INIT_STATE &&
    status !== EXTRA_STATE &&
    status !== NAME_STATE &&
    status !== COMMENT_STATE &&
    status !== HCRC_STATE &&
    status !== BUSY_STATE &&
    status !== FINISH_STATE
  ) {
    return err(strm, Z_STREAM_ERROR);
  }

  strm.state = null;

  return status === BUSY_STATE ? err(strm, Z_DATA_ERROR) : Z_OK;
}

/* =========================================================================
 * Copy the source state to the destination state
 */
//function deflateCopy(dest, source) {
//
//}

exports.deflateInit = deflateInit;
exports.deflateInit2 = deflateInit2;
exports.deflateReset = deflateReset;
exports.deflateResetKeep = deflateResetKeep;
exports.deflateSetHeader = deflateSetHeader;
exports.deflate = deflate;
exports.deflateEnd = deflateEnd;
exports.deflateInfo = 'pako deflate (from Nodeca project)';

/* Not implemented
exports.deflateBound = deflateBound;
exports.deflateCopy = deflateCopy;
exports.deflateSetDictionary = deflateSetDictionary;
exports.deflateParams = deflateParams;
exports.deflatePending = deflatePending;
exports.deflatePrime = deflatePrime;
exports.deflateTune = deflateTune;
*/
},{"../utils/common":3,"./adler32":4,"./crc32":6,"./messages":11,"./trees":12}],8:[function(_dereq_,module,exports){
'use strict';

// See state defs from inflate.js
var BAD = 30;       /* got a data error -- remain here until reset */
var TYPE = 12;      /* i: waiting for type bits, including last-flag bit */

/*
   Decode literal, length, and distance codes and write out the resulting
   literal and match bytes until either not enough input or output is
   available, an end-of-block is encountered, or a data error is encountered.
   When large enough input and output buffers are supplied to inflate(), for
   example, a 16K input buffer and a 64K output buffer, more than 95% of the
   inflate execution time is spent in this routine.

   Entry assumptions:

        state.mode === LEN
        strm.avail_in >= 6
        strm.avail_out >= 258
        start >= strm.avail_out
        state.bits < 8

   On return, state.mode is one of:

        LEN -- ran out of enough output space or enough available input
        TYPE -- reached end of block code, inflate() to interpret next block
        BAD -- error in block data

   Notes:

    - The maximum input bits used by a length/distance pair is 15 bits for the
      length code, 5 bits for the length extra, 15 bits for the distance code,
      and 13 bits for the distance extra.  This totals 48 bits, or six bytes.
      Therefore if strm.avail_in >= 6, then there is enough input to avoid
      checking for available input while decoding.

    - The maximum bytes that a single length/distance pair can output is 258
      bytes, which is the maximum length that can be coded.  inflate_fast()
      requires strm.avail_out >= 258 for each loop to avoid checking for
      output space.
 */
module.exports = function inflate_fast(strm, start) {
  var state;
  var _in;                    /* local strm.input */
  var last;                   /* have enough input while in < last */
  var _out;                   /* local strm.output */
  var beg;                    /* inflate()'s initial strm.output */
  var end;                    /* while out < end, enough space available */
//#ifdef INFLATE_STRICT
  var dmax;                   /* maximum distance from zlib header */
//#endif
  var wsize;                  /* window size or zero if not using window */
  var whave;                  /* valid bytes in the window */
  var wnext;                  /* window write index */
  var window;                 /* allocated sliding window, if wsize != 0 */
  var hold;                   /* local strm.hold */
  var bits;                   /* local strm.bits */
  var lcode;                  /* local strm.lencode */
  var dcode;                  /* local strm.distcode */
  var lmask;                  /* mask for first level of length codes */
  var dmask;                  /* mask for first level of distance codes */
  var here;                   /* retrieved table entry */
  var op;                     /* code bits, operation, extra bits, or */
                              /*  window position, window bytes to copy */
  var len;                    /* match length, unused bytes */
  var dist;                   /* match distance */
  var from;                   /* where to copy match from */
  var from_source;


  var input, output; // JS specific, because we have no pointers

  /* copy state to local variables */
  state = strm.state;
  //here = state.here;
  _in = strm.next_in;
  input = strm.input;
  last = _in + (strm.avail_in - 5);
  _out = strm.next_out;
  output = strm.output;
  beg = _out - (start - strm.avail_out);
  end = _out + (strm.avail_out - 257);
//#ifdef INFLATE_STRICT
  dmax = state.dmax;
//#endif
  wsize = state.wsize;
  whave = state.whave;
  wnext = state.wnext;
  window = state.window;
  hold = state.hold;
  bits = state.bits;
  lcode = state.lencode;
  dcode = state.distcode;
  lmask = (1 << state.lenbits) - 1;
  dmask = (1 << state.distbits) - 1;


  /* decode literals and length/distances until end-of-block or not enough
     input data or output space */

  top:
  do {
    if (bits < 15) {
      hold += input[_in++] << bits;
      bits += 8;
      hold += input[_in++] << bits;
      bits += 8;
    }

    here = lcode[hold & lmask];

    dolen:
    for (;;) { // Goto emulation
      op = here >>> 24/*here.bits*/;
      hold >>>= op;
      bits -= op;
      op = (here >>> 16) & 0xff/*here.op*/;
      if (op === 0) {                          /* literal */
        //Tracevv((stderr, here.val >= 0x20 && here.val < 0x7f ?
        //        "inflate:         literal '%c'\n" :
        //        "inflate:         literal 0x%02x\n", here.val));
        output[_out++] = here & 0xffff/*here.val*/;
      }
      else if (op & 16) {                     /* length base */
        len = here & 0xffff/*here.val*/;
        op &= 15;                           /* number of extra bits */
        if (op) {
          if (bits < op) {
            hold += input[_in++] << bits;
            bits += 8;
          }
          len += hold & ((1 << op) - 1);
          hold >>>= op;
          bits -= op;
        }
        //Tracevv((stderr, "inflate:         length %u\n", len));
        if (bits < 15) {
          hold += input[_in++] << bits;
          bits += 8;
          hold += input[_in++] << bits;
          bits += 8;
        }
        here = dcode[hold & dmask];

        dodist:
        for (;;) { // goto emulation
          op = here >>> 24/*here.bits*/;
          hold >>>= op;
          bits -= op;
          op = (here >>> 16) & 0xff/*here.op*/;

          if (op & 16) {                      /* distance base */
            dist = here & 0xffff/*here.val*/;
            op &= 15;                       /* number of extra bits */
            if (bits < op) {
              hold += input[_in++] << bits;
              bits += 8;
              if (bits < op) {
                hold += input[_in++] << bits;
                bits += 8;
              }
            }
            dist += hold & ((1 << op) - 1);
//#ifdef INFLATE_STRICT
            if (dist > dmax) {
              strm.msg = 'invalid distance too far back';
              state.mode = BAD;
              break top;
            }
//#endif
            hold >>>= op;
            bits -= op;
            //Tracevv((stderr, "inflate:         distance %u\n", dist));
            op = _out - beg;                /* max distance in output */
            if (dist > op) {                /* see if copy from window */
              op = dist - op;               /* distance back in window */
              if (op > whave) {
                if (state.sane) {
                  strm.msg = 'invalid distance too far back';
                  state.mode = BAD;
                  break top;
                }

// (!) This block is disabled in zlib defailts,
// don't enable it for binary compatibility
//#ifdef INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR
//                if (len <= op - whave) {
//                  do {
//                    output[_out++] = 0;
//                  } while (--len);
//                  continue top;
//                }
//                len -= op - whave;
//                do {
//                  output[_out++] = 0;
//                } while (--op > whave);
//                if (op === 0) {
//                  from = _out - dist;
//                  do {
//                    output[_out++] = output[from++];
//                  } while (--len);
//                  continue top;
//                }
//#endif
              }
              from = 0; // window index
              from_source = window;
              if (wnext === 0) {           /* very common case */
                from += wsize - op;
                if (op < len) {         /* some from window */
                  len -= op;
                  do {
                    output[_out++] = window[from++];
                  } while (--op);
                  from = _out - dist;  /* rest from output */
                  from_source = output;
                }
              }
              else if (wnext < op) {      /* wrap around window */
                from += wsize + wnext - op;
                op -= wnext;
                if (op < len) {         /* some from end of window */
                  len -= op;
                  do {
                    output[_out++] = window[from++];
                  } while (--op);
                  from = 0;
                  if (wnext < len) {  /* some from start of window */
                    op = wnext;
                    len -= op;
                    do {
                      output[_out++] = window[from++];
                    } while (--op);
                    from = _out - dist;      /* rest from output */
                    from_source = output;
                  }
                }
              }
              else {                      /* contiguous in window */
                from += wnext - op;
                if (op < len) {         /* some from window */
                  len -= op;
                  do {
                    output[_out++] = window[from++];
                  } while (--op);
                  from = _out - dist;  /* rest from output */
                  from_source = output;
                }
              }
              while (len > 2) {
                output[_out++] = from_source[from++];
                output[_out++] = from_source[from++];
                output[_out++] = from_source[from++];
                len -= 3;
              }
              if (len) {
                output[_out++] = from_source[from++];
                if (len > 1) {
                  output[_out++] = from_source[from++];
                }
              }
            }
            else {
              from = _out - dist;          /* copy direct from output */
              do {                        /* minimum length is three */
                output[_out++] = output[from++];
                output[_out++] = output[from++];
                output[_out++] = output[from++];
                len -= 3;
              } while (len > 2);
              if (len) {
                output[_out++] = output[from++];
                if (len > 1) {
                  output[_out++] = output[from++];
                }
              }
            }
          }
          else if ((op & 64) === 0) {          /* 2nd level distance code */
            here = dcode[(here & 0xffff)/*here.val*/ + (hold & ((1 << op) - 1))];
            continue dodist;
          }
          else {
            strm.msg = 'invalid distance code';
            state.mode = BAD;
            break top;
          }

          break; // need to emulate goto via "continue"
        }
      }
      else if ((op & 64) === 0) {              /* 2nd level length code */
        here = lcode[(here & 0xffff)/*here.val*/ + (hold & ((1 << op) - 1))];
        continue dolen;
      }
      else if (op & 32) {                     /* end-of-block */
        //Tracevv((stderr, "inflate:         end of block\n"));
        state.mode = TYPE;
        break top;
      }
      else {
        strm.msg = 'invalid literal/length code';
        state.mode = BAD;
        break top;
      }

      break; // need to emulate goto via "continue"
    }
  } while (_in < last && _out < end);

  /* return unused bytes (on entry, bits < 8, so in won't go too far back) */
  len = bits >> 3;
  _in -= len;
  bits -= len << 3;
  hold &= (1 << bits) - 1;

  /* update state and return */
  strm.next_in = _in;
  strm.next_out = _out;
  strm.avail_in = (_in < last ? 5 + (last - _in) : 5 - (_in - last));
  strm.avail_out = (_out < end ? 257 + (end - _out) : 257 - (_out - end));
  state.hold = hold;
  state.bits = bits;
  return;
};

},{}],9:[function(_dereq_,module,exports){
'use strict';


var utils = _dereq_('../utils/common');
var adler32 = _dereq_('./adler32');
var crc32   = _dereq_('./crc32');
var inflate_fast = _dereq_('./inffast');
var inflate_table = _dereq_('./inftrees');

var CODES = 0;
var LENS = 1;
var DISTS = 2;

/* Public constants ==========================================================*/
/* ===========================================================================*/


/* Allowed flush values; see deflate() and inflate() below for details */
//var Z_NO_FLUSH      = 0;
//var Z_PARTIAL_FLUSH = 1;
//var Z_SYNC_FLUSH    = 2;
//var Z_FULL_FLUSH    = 3;
var Z_FINISH        = 4;
var Z_BLOCK         = 5;
var Z_TREES         = 6;


/* Return codes for the compression/decompression functions. Negative values
 * are errors, positive values are used for special but normal events.
 */
var Z_OK            = 0;
var Z_STREAM_END    = 1;
var Z_NEED_DICT     = 2;
//var Z_ERRNO         = -1;
var Z_STREAM_ERROR  = -2;
var Z_DATA_ERROR    = -3;
var Z_MEM_ERROR     = -4;
var Z_BUF_ERROR     = -5;
//var Z_VERSION_ERROR = -6;

/* The deflate compression method */
var Z_DEFLATED  = 8;


/* STATES ====================================================================*/
/* ===========================================================================*/


var    HEAD = 1;       /* i: waiting for magic header */
var    FLAGS = 2;      /* i: waiting for method and flags (gzip) */
var    TIME = 3;       /* i: waiting for modification time (gzip) */
var    OS = 4;         /* i: waiting for extra flags and operating system (gzip) */
var    EXLEN = 5;      /* i: waiting for extra length (gzip) */
var    EXTRA = 6;      /* i: waiting for extra bytes (gzip) */
var    NAME = 7;       /* i: waiting for end of file name (gzip) */
var    COMMENT = 8;    /* i: waiting for end of comment (gzip) */
var    HCRC = 9;       /* i: waiting for header crc (gzip) */
var    DICTID = 10;    /* i: waiting for dictionary check value */
var    DICT = 11;      /* waiting for inflateSetDictionary() call */
var        TYPE = 12;      /* i: waiting for type bits, including last-flag bit */
var        TYPEDO = 13;    /* i: same, but skip check to exit inflate on new block */
var        STORED = 14;    /* i: waiting for stored size (length and complement) */
var        COPY_ = 15;     /* i/o: same as COPY below, but only first time in */
var        COPY = 16;      /* i/o: waiting for input or output to copy stored block */
var        TABLE = 17;     /* i: waiting for dynamic block table lengths */
var        LENLENS = 18;   /* i: waiting for code length code lengths */
var        CODELENS = 19;  /* i: waiting for length/lit and distance code lengths */
var            LEN_ = 20;      /* i: same as LEN below, but only first time in */
var            LEN = 21;       /* i: waiting for length/lit/eob code */
var            LENEXT = 22;    /* i: waiting for length extra bits */
var            DIST = 23;      /* i: waiting for distance code */
var            DISTEXT = 24;   /* i: waiting for distance extra bits */
var            MATCH = 25;     /* o: waiting for output space to copy string */
var            LIT = 26;       /* o: waiting for output space to write literal */
var    CHECK = 27;     /* i: waiting for 32-bit check value */
var    LENGTH = 28;    /* i: waiting for 32-bit length (gzip) */
var    DONE = 29;      /* finished check, done -- remain here until reset */
var    BAD = 30;       /* got a data error -- remain here until reset */
var    MEM = 31;       /* got an inflate() memory error -- remain here until reset */
var    SYNC = 32;      /* looking for synchronization bytes to restart inflate() */

/* ===========================================================================*/



var ENOUGH_LENS = 852;
var ENOUGH_DISTS = 592;
//var ENOUGH =  (ENOUGH_LENS+ENOUGH_DISTS);

var MAX_WBITS = 15;
/* 32K LZ77 window */
var DEF_WBITS = MAX_WBITS;


function ZSWAP32(q) {
  return  (((q >>> 24) & 0xff) +
          ((q >>> 8) & 0xff00) +
          ((q & 0xff00) << 8) +
          ((q & 0xff) << 24));
}


function InflateState() {
  this.mode = 0;             /* current inflate mode */
  this.last = false;          /* true if processing last block */
  this.wrap = 0;              /* bit 0 true for zlib, bit 1 true for gzip */
  this.havedict = false;      /* true if dictionary provided */
  this.flags = 0;             /* gzip header method and flags (0 if zlib) */
  this.dmax = 0;              /* zlib header max distance (INFLATE_STRICT) */
  this.check = 0;             /* protected copy of check value */
  this.total = 0;             /* protected copy of output count */
  // TODO: may be {}
  this.head = null;           /* where to save gzip header information */

  /* sliding window */
  this.wbits = 0;             /* log base 2 of requested window size */
  this.wsize = 0;             /* window size or zero if not using window */
  this.whave = 0;             /* valid bytes in the window */
  this.wnext = 0;             /* window write index */
  this.window = null;         /* allocated sliding window, if needed */

  /* bit accumulator */
  this.hold = 0;              /* input bit accumulator */
  this.bits = 0;              /* number of bits in "in" */

  /* for string and stored block copying */
  this.length = 0;            /* literal or length of data to copy */
  this.offset = 0;            /* distance back to copy string from */

  /* for table and code decoding */
  this.extra = 0;             /* extra bits needed */

  /* fixed and dynamic code tables */
  this.lencode = null;          /* starting table for length/literal codes */
  this.distcode = null;         /* starting table for distance codes */
  this.lenbits = 0;           /* index bits for lencode */
  this.distbits = 0;          /* index bits for distcode */

  /* dynamic table building */
  this.ncode = 0;             /* number of code length code lengths */
  this.nlen = 0;              /* number of length code lengths */
  this.ndist = 0;             /* number of distance code lengths */
  this.have = 0;              /* number of code lengths in lens[] */
  this.next = null;              /* next available space in codes[] */

  this.lens = new utils.Buf16(320); /* temporary storage for code lengths */
  this.work = new utils.Buf16(288); /* work area for code table building */

  /*
   because we don't have pointers in js, we use lencode and distcode directly
   as buffers so we don't need codes
  */
  //this.codes = new utils.Buf32(ENOUGH);       /* space for code tables */
  this.lendyn = null;              /* dynamic table for length/literal codes (JS specific) */
  this.distdyn = null;             /* dynamic table for distance codes (JS specific) */
  this.sane = 0;                   /* if false, allow invalid distance too far */
  this.back = 0;                   /* bits back of last unprocessed length/lit */
  this.was = 0;                    /* initial length of match */
}

function inflateResetKeep(strm) {
  var state;

  if (!strm || !strm.state) { return Z_STREAM_ERROR; }
  state = strm.state;
  strm.total_in = strm.total_out = state.total = 0;
  strm.msg = ''; /*Z_NULL*/
  if (state.wrap) {       /* to support ill-conceived Java test suite */
    strm.adler = state.wrap & 1;
  }
  state.mode = HEAD;
  state.last = 0;
  state.havedict = 0;
  state.dmax = 32768;
  state.head = null/*Z_NULL*/;
  state.hold = 0;
  state.bits = 0;
  //state.lencode = state.distcode = state.next = state.codes;
  state.lencode = state.lendyn = new utils.Buf32(ENOUGH_LENS);
  state.distcode = state.distdyn = new utils.Buf32(ENOUGH_DISTS);

  state.sane = 1;
  state.back = -1;
  //Tracev((stderr, "inflate: reset\n"));
  return Z_OK;
}

function inflateReset(strm) {
  var state;

  if (!strm || !strm.state) { return Z_STREAM_ERROR; }
  state = strm.state;
  state.wsize = 0;
  state.whave = 0;
  state.wnext = 0;
  return inflateResetKeep(strm);

}

function inflateReset2(strm, windowBits) {
  var wrap;
  var state;

  /* get the state */
  if (!strm || !strm.state) { return Z_STREAM_ERROR; }
  state = strm.state;

  /* extract wrap request from windowBits parameter */
  if (windowBits < 0) {
    wrap = 0;
    windowBits = -windowBits;
  }
  else {
    wrap = (windowBits >> 4) + 1;
    if (windowBits < 48) {
      windowBits &= 15;
    }
  }

  /* set number of window bits, free window if different */
  if (windowBits && (windowBits < 8 || windowBits > 15)) {
    return Z_STREAM_ERROR;
  }
  if (state.window !== null && state.wbits !== windowBits) {
    state.window = null;
  }

  /* update state and reset the rest of it */
  state.wrap = wrap;
  state.wbits = windowBits;
  return inflateReset(strm);
}

function inflateInit2(strm, windowBits) {
  var ret;
  var state;

  if (!strm) { return Z_STREAM_ERROR; }
  //strm.msg = Z_NULL;                 /* in case we return an error */

  state = new InflateState();

  //if (state === Z_NULL) return Z_MEM_ERROR;
  //Tracev((stderr, "inflate: allocated\n"));
  strm.state = state;
  state.window = null/*Z_NULL*/;
  ret = inflateReset2(strm, windowBits);
  if (ret !== Z_OK) {
    strm.state = null/*Z_NULL*/;
  }
  return ret;
}

function inflateInit(strm) {
  return inflateInit2(strm, DEF_WBITS);
}


/*
 Return state with length and distance decoding tables and index sizes set to
 fixed code decoding.  Normally this returns fixed tables from inffixed.h.
 If BUILDFIXED is defined, then instead this routine builds the tables the
 first time it's called, and returns those tables the first time and
 thereafter.  This reduces the size of the code by about 2K bytes, in
 exchange for a little execution time.  However, BUILDFIXED should not be
 used for threaded applications, since the rewriting of the tables and virgin
 may not be thread-safe.
 */
var virgin = true;

var lenfix, distfix; // We have no pointers in JS, so keep tables separate

function fixedtables(state) {
  /* build fixed huffman tables if first call (may not be thread safe) */
  if (virgin) {
    var sym;

    lenfix = new utils.Buf32(512);
    distfix = new utils.Buf32(32);

    /* literal/length table */
    sym = 0;
    while (sym < 144) { state.lens[sym++] = 8; }
    while (sym < 256) { state.lens[sym++] = 9; }
    while (sym < 280) { state.lens[sym++] = 7; }
    while (sym < 288) { state.lens[sym++] = 8; }

    inflate_table(LENS,  state.lens, 0, 288, lenfix,   0, state.work, {bits: 9});

    /* distance table */
    sym = 0;
    while (sym < 32) { state.lens[sym++] = 5; }

    inflate_table(DISTS, state.lens, 0, 32,   distfix, 0, state.work, {bits: 5});

    /* do this just once */
    virgin = false;
  }

  state.lencode = lenfix;
  state.lenbits = 9;
  state.distcode = distfix;
  state.distbits = 5;
}


/*
 Update the window with the last wsize (normally 32K) bytes written before
 returning.  If window does not exist yet, create it.  This is only called
 when a window is already in use, or when output has been written during this
 inflate call, but the end of the deflate stream has not been reached yet.
 It is also called to create a window for dictionary data when a dictionary
 is loaded.

 Providing output buffers larger than 32K to inflate() should provide a speed
 advantage, since only the last 32K of output is copied to the sliding window
 upon return from inflate(), and since all distances after the first 32K of
 output will fall in the output data, making match copies simpler and faster.
 The advantage may be dependent on the size of the processor's data caches.
 */
function updatewindow(strm, src, end, copy) {
  var dist;
  var state = strm.state;

  /* if it hasn't been done already, allocate space for the window */
  if (state.window === null) {
    state.wsize = 1 << state.wbits;
    state.wnext = 0;
    state.whave = 0;

    state.window = new utils.Buf8(state.wsize);
  }

  /* copy state->wsize or less output bytes into the circular window */
  if (copy >= state.wsize) {
    utils.arraySet(state.window,src, end - state.wsize, state.wsize, 0);
    state.wnext = 0;
    state.whave = state.wsize;
  }
  else {
    dist = state.wsize - state.wnext;
    if (dist > copy) {
      dist = copy;
    }
    //zmemcpy(state->window + state->wnext, end - copy, dist);
    utils.arraySet(state.window,src, end - copy, dist, state.wnext);
    copy -= dist;
    if (copy) {
      //zmemcpy(state->window, end - copy, copy);
      utils.arraySet(state.window,src, end - copy, copy, 0);
      state.wnext = copy;
      state.whave = state.wsize;
    }
    else {
      state.wnext += dist;
      if (state.wnext === state.wsize) { state.wnext = 0; }
      if (state.whave < state.wsize) { state.whave += dist; }
    }
  }
  return 0;
}

function inflate(strm, flush) {
  var state;
  var input, output;          // input/output buffers
  var next;                   /* next input INDEX */
  var put;                    /* next output INDEX */
  var have, left;             /* available input and output */
  var hold;                   /* bit buffer */
  var bits;                   /* bits in bit buffer */
  var _in, _out;              /* save starting available input and output */
  var copy;                   /* number of stored or match bytes to copy */
  var from;                   /* where to copy match bytes from */
  var from_source;
  var here = 0;               /* current decoding table entry */
  var here_bits, here_op, here_val; // paked "here" denormalized (JS specific)
  //var last;                   /* parent table entry */
  var last_bits, last_op, last_val; // paked "last" denormalized (JS specific)
  var len;                    /* length to copy for repeats, bits to drop */
  var ret;                    /* return code */
  var hbuf = new utils.Buf8(4);    /* buffer for gzip header crc calculation */
  var opts;

  var n; // temporary var for NEED_BITS

  var order = /* permutation of code lengths */
    [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];


  if (!strm || !strm.state || !strm.output ||
      (!strm.input && strm.avail_in !== 0)) {
    return Z_STREAM_ERROR;
  }

  state = strm.state;
  if (state.mode === TYPE) { state.mode = TYPEDO; }    /* skip check */


  //--- LOAD() ---
  put = strm.next_out;
  output = strm.output;
  left = strm.avail_out;
  next = strm.next_in;
  input = strm.input;
  have = strm.avail_in;
  hold = state.hold;
  bits = state.bits;
  //---

  _in = have;
  _out = left;
  ret = Z_OK;

  inf_leave: // goto emulation
  for (;;) {
    switch (state.mode) {
    case HEAD:
      if (state.wrap === 0) {
        state.mode = TYPEDO;
        break;
      }
      //=== NEEDBITS(16);
      while (bits < 16) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      if ((state.wrap & 2) && hold === 0x8b1f) {  /* gzip header */
        state.check = 0/*crc32(0L, Z_NULL, 0)*/;
        //=== CRC2(state.check, hold);
        hbuf[0] = hold & 0xff;
        hbuf[1] = (hold >>> 8) & 0xff;
        state.check = crc32(state.check, hbuf, 2, 0);
        //===//

        //=== INITBITS();
        hold = 0;
        bits = 0;
        //===//
        state.mode = FLAGS;
        break;
      }
      state.flags = 0;           /* expect zlib header */
      if (state.head) {
        state.head.done = false;
      }
      if (!(state.wrap & 1) ||   /* check if zlib header allowed */
        (((hold & 0xff)/*BITS(8)*/ << 8) + (hold >> 8)) % 31) {
        strm.msg = 'incorrect header check';
        state.mode = BAD;
        break;
      }
      if ((hold & 0x0f)/*BITS(4)*/ !== Z_DEFLATED) {
        strm.msg = 'unknown compression method';
        state.mode = BAD;
        break;
      }
      //--- DROPBITS(4) ---//
      hold >>>= 4;
      bits -= 4;
      //---//
      len = (hold & 0x0f)/*BITS(4)*/ + 8;
      if (state.wbits === 0) {
        state.wbits = len;
      }
      else if (len > state.wbits) {
        strm.msg = 'invalid window size';
        state.mode = BAD;
        break;
      }
      state.dmax = 1 << len;
      //Tracev((stderr, "inflate:   zlib header ok\n"));
      strm.adler = state.check = 1/*adler32(0L, Z_NULL, 0)*/;
      state.mode = hold & 0x200 ? DICTID : TYPE;
      //=== INITBITS();
      hold = 0;
      bits = 0;
      //===//
      break;
    case FLAGS:
      //=== NEEDBITS(16); */
      while (bits < 16) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      state.flags = hold;
      if ((state.flags & 0xff) !== Z_DEFLATED) {
        strm.msg = 'unknown compression method';
        state.mode = BAD;
        break;
      }
      if (state.flags & 0xe000) {
        strm.msg = 'unknown header flags set';
        state.mode = BAD;
        break;
      }
      if (state.head) {
        state.head.text = ((hold >> 8) & 1);
      }
      if (state.flags & 0x0200) {
        //=== CRC2(state.check, hold);
        hbuf[0] = hold & 0xff;
        hbuf[1] = (hold >>> 8) & 0xff;
        state.check = crc32(state.check, hbuf, 2, 0);
        //===//
      }
      //=== INITBITS();
      hold = 0;
      bits = 0;
      //===//
      state.mode = TIME;
      /* falls through */
    case TIME:
      //=== NEEDBITS(32); */
      while (bits < 32) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      if (state.head) {
        state.head.time = hold;
      }
      if (state.flags & 0x0200) {
        //=== CRC4(state.check, hold)
        hbuf[0] = hold & 0xff;
        hbuf[1] = (hold >>> 8) & 0xff;
        hbuf[2] = (hold >>> 16) & 0xff;
        hbuf[3] = (hold >>> 24) & 0xff;
        state.check = crc32(state.check, hbuf, 4, 0);
        //===
      }
      //=== INITBITS();
      hold = 0;
      bits = 0;
      //===//
      state.mode = OS;
      /* falls through */
    case OS:
      //=== NEEDBITS(16); */
      while (bits < 16) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      if (state.head) {
        state.head.xflags = (hold & 0xff);
        state.head.os = (hold >> 8);
      }
      if (state.flags & 0x0200) {
        //=== CRC2(state.check, hold);
        hbuf[0] = hold & 0xff;
        hbuf[1] = (hold >>> 8) & 0xff;
        state.check = crc32(state.check, hbuf, 2, 0);
        //===//
      }
      //=== INITBITS();
      hold = 0;
      bits = 0;
      //===//
      state.mode = EXLEN;
      /* falls through */
    case EXLEN:
      if (state.flags & 0x0400) {
        //=== NEEDBITS(16); */
        while (bits < 16) {
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
        }
        //===//
        state.length = hold;
        if (state.head) {
          state.head.extra_len = hold;
        }
        if (state.flags & 0x0200) {
          //=== CRC2(state.check, hold);
          hbuf[0] = hold & 0xff;
          hbuf[1] = (hold >>> 8) & 0xff;
          state.check = crc32(state.check, hbuf, 2, 0);
          //===//
        }
        //=== INITBITS();
        hold = 0;
        bits = 0;
        //===//
      }
      else if (state.head) {
        state.head.extra = null/*Z_NULL*/;
      }
      state.mode = EXTRA;
      /* falls through */
    case EXTRA:
      if (state.flags & 0x0400) {
        copy = state.length;
        if (copy > have) { copy = have; }
        if (copy) {
          if (state.head) {
            len = state.head.extra_len - state.length;
            if (!state.head.extra) {
              // Use untyped array for more conveniend processing later
              state.head.extra = new Array(state.head.extra_len);
            }
            utils.arraySet(
              state.head.extra,
              input,
              next,
              // extra field is limited to 65536 bytes
              // - no need for additional size check
              copy,
              /*len + copy > state.head.extra_max - len ? state.head.extra_max : copy,*/
              len
            );
            //zmemcpy(state.head.extra + len, next,
            //        len + copy > state.head.extra_max ?
            //        state.head.extra_max - len : copy);
          }
          if (state.flags & 0x0200) {
            state.check = crc32(state.check, input, copy, next);
          }
          have -= copy;
          next += copy;
          state.length -= copy;
        }
        if (state.length) { break inf_leave; }
      }
      state.length = 0;
      state.mode = NAME;
      /* falls through */
    case NAME:
      if (state.flags & 0x0800) {
        if (have === 0) { break inf_leave; }
        copy = 0;
        do {
          // TODO: 2 or 1 bytes?
          len = input[next + copy++];
          /* use constant limit because in js we should not preallocate memory */
          if (state.head && len &&
              (state.length < 65536 /*state.head.name_max*/)) {
            state.head.name += String.fromCharCode(len);
          }
        } while (len && copy < have);

        if (state.flags & 0x0200) {
          state.check = crc32(state.check, input, copy, next);
        }
        have -= copy;
        next += copy;
        if (len) { break inf_leave; }
      }
      else if (state.head) {
        state.head.name = null;
      }
      state.length = 0;
      state.mode = COMMENT;
      /* falls through */
    case COMMENT:
      if (state.flags & 0x1000) {
        if (have === 0) { break inf_leave; }
        copy = 0;
        do {
          len = input[next + copy++];
          /* use constant limit because in js we should not preallocate memory */
          if (state.head && len &&
              (state.length < 65536 /*state.head.comm_max*/)) {
            state.head.comment += String.fromCharCode(len);
          }
        } while (len && copy < have);
        if (state.flags & 0x0200) {
          state.check = crc32(state.check, input, copy, next);
        }
        have -= copy;
        next += copy;
        if (len) { break inf_leave; }
      }
      else if (state.head) {
        state.head.comment = null;
      }
      state.mode = HCRC;
      /* falls through */
    case HCRC:
      if (state.flags & 0x0200) {
        //=== NEEDBITS(16); */
        while (bits < 16) {
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
        }
        //===//
        if (hold !== (state.check & 0xffff)) {
          strm.msg = 'header crc mismatch';
          state.mode = BAD;
          break;
        }
        //=== INITBITS();
        hold = 0;
        bits = 0;
        //===//
      }
      if (state.head) {
        state.head.hcrc = ((state.flags >> 9) & 1);
        state.head.done = true;
      }
      strm.adler = state.check = 0 /*crc32(0L, Z_NULL, 0)*/;
      state.mode = TYPE;
      break;
    case DICTID:
      //=== NEEDBITS(32); */
      while (bits < 32) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      strm.adler = state.check = ZSWAP32(hold);
      //=== INITBITS();
      hold = 0;
      bits = 0;
      //===//
      state.mode = DICT;
      /* falls through */
    case DICT:
      if (state.havedict === 0) {
        //--- RESTORE() ---
        strm.next_out = put;
        strm.avail_out = left;
        strm.next_in = next;
        strm.avail_in = have;
        state.hold = hold;
        state.bits = bits;
        //---
        return Z_NEED_DICT;
      }
      strm.adler = state.check = 1/*adler32(0L, Z_NULL, 0)*/;
      state.mode = TYPE;
      /* falls through */
    case TYPE:
      if (flush === Z_BLOCK || flush === Z_TREES) { break inf_leave; }
      /* falls through */
    case TYPEDO:
      if (state.last) {
        //--- BYTEBITS() ---//
        hold >>>= bits & 7;
        bits -= bits & 7;
        //---//
        state.mode = CHECK;
        break;
      }
      //=== NEEDBITS(3); */
      while (bits < 3) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      state.last = (hold & 0x01)/*BITS(1)*/;
      //--- DROPBITS(1) ---//
      hold >>>= 1;
      bits -= 1;
      //---//

      switch ((hold & 0x03)/*BITS(2)*/) {
      case 0:                             /* stored block */
        //Tracev((stderr, "inflate:     stored block%s\n",
        //        state.last ? " (last)" : ""));
        state.mode = STORED;
        break;
      case 1:                             /* fixed block */
        fixedtables(state);
        //Tracev((stderr, "inflate:     fixed codes block%s\n",
        //        state.last ? " (last)" : ""));
        state.mode = LEN_;             /* decode codes */
        if (flush === Z_TREES) {
          //--- DROPBITS(2) ---//
          hold >>>= 2;
          bits -= 2;
          //---//
          break inf_leave;
        }
        break;
      case 2:                             /* dynamic block */
        //Tracev((stderr, "inflate:     dynamic codes block%s\n",
        //        state.last ? " (last)" : ""));
        state.mode = TABLE;
        break;
      case 3:
        strm.msg = 'invalid block type';
        state.mode = BAD;
      }
      //--- DROPBITS(2) ---//
      hold >>>= 2;
      bits -= 2;
      //---//
      break;
    case STORED:
      //--- BYTEBITS() ---// /* go to byte boundary */
      hold >>>= bits & 7;
      bits -= bits & 7;
      //---//
      //=== NEEDBITS(32); */
      while (bits < 32) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      if ((hold & 0xffff) !== ((hold >>> 16) ^ 0xffff)) {
        strm.msg = 'invalid stored block lengths';
        state.mode = BAD;
        break;
      }
      state.length = hold & 0xffff;
      //Tracev((stderr, "inflate:       stored length %u\n",
      //        state.length));
      //=== INITBITS();
      hold = 0;
      bits = 0;
      //===//
      state.mode = COPY_;
      if (flush === Z_TREES) { break inf_leave; }
      /* falls through */
    case COPY_:
      state.mode = COPY;
      /* falls through */
    case COPY:
      copy = state.length;
      if (copy) {
        if (copy > have) { copy = have; }
        if (copy > left) { copy = left; }
        if (copy === 0) { break inf_leave; }
        //--- zmemcpy(put, next, copy); ---
        utils.arraySet(output, input, next, copy, put);
        //---//
        have -= copy;
        next += copy;
        left -= copy;
        put += copy;
        state.length -= copy;
        break;
      }
      //Tracev((stderr, "inflate:       stored end\n"));
      state.mode = TYPE;
      break;
    case TABLE:
      //=== NEEDBITS(14); */
      while (bits < 14) {
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
      }
      //===//
      state.nlen = (hold & 0x1f)/*BITS(5)*/ + 257;
      //--- DROPBITS(5) ---//
      hold >>>= 5;
      bits -= 5;
      //---//
      state.ndist = (hold & 0x1f)/*BITS(5)*/ + 1;
      //--- DROPBITS(5) ---//
      hold >>>= 5;
      bits -= 5;
      //---//
      state.ncode = (hold & 0x0f)/*BITS(4)*/ + 4;
      //--- DROPBITS(4) ---//
      hold >>>= 4;
      bits -= 4;
      //---//
//#ifndef PKZIP_BUG_WORKAROUND
      if (state.nlen > 286 || state.ndist > 30) {
        strm.msg = 'too many length or distance symbols';
        state.mode = BAD;
        break;
      }
//#endif
      //Tracev((stderr, "inflate:       table sizes ok\n"));
      state.have = 0;
      state.mode = LENLENS;
      /* falls through */
    case LENLENS:
      while (state.have < state.ncode) {
        //=== NEEDBITS(3);
        while (bits < 3) {
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
        }
        //===//
        state.lens[order[state.have++]] = (hold & 0x07);//BITS(3);
        //--- DROPBITS(3) ---//
        hold >>>= 3;
        bits -= 3;
        //---//
      }
      while (state.have < 19) {
        state.lens[order[state.have++]] = 0;
      }
      // We have separate tables & no pointers. 2 commented lines below not needed.
      //state.next = state.codes;
      //state.lencode = state.next;
      // Switch to use dynamic table
      state.lencode = state.lendyn;
      state.lenbits = 7;

      opts = {bits: state.lenbits};
      ret = inflate_table(CODES, state.lens, 0, 19, state.lencode, 0, state.work, opts);
      state.lenbits = opts.bits;

      if (ret) {
        strm.msg = 'invalid code lengths set';
        state.mode = BAD;
        break;
      }
      //Tracev((stderr, "inflate:       code lengths ok\n"));
      state.have = 0;
      state.mode = CODELENS;
      /* falls through */
    case CODELENS:
      while (state.have < state.nlen + state.ndist) {
        for (;;) {
          here = state.lencode[hold & ((1 << state.lenbits) - 1)];/*BITS(state.lenbits)*/
          here_bits = here >>> 24;
          here_op = (here >>> 16) & 0xff;
          here_val = here & 0xffff;

          if ((here_bits) <= bits) { break; }
          //--- PULLBYTE() ---//
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
          //---//
        }
        if (here_val < 16) {
          //--- DROPBITS(here.bits) ---//
          hold >>>= here_bits;
          bits -= here_bits;
          //---//
          state.lens[state.have++] = here_val;
        }
        else {
          if (here_val === 16) {
            //=== NEEDBITS(here.bits + 2);
            n = here_bits + 2;
            while (bits < n) {
              if (have === 0) { break inf_leave; }
              have--;
              hold += input[next++] << bits;
              bits += 8;
            }
            //===//
            //--- DROPBITS(here.bits) ---//
            hold >>>= here_bits;
            bits -= here_bits;
            //---//
            if (state.have === 0) {
              strm.msg = 'invalid bit length repeat';
              state.mode = BAD;
              break;
            }
            len = state.lens[state.have - 1];
            copy = 3 + (hold & 0x03);//BITS(2);
            //--- DROPBITS(2) ---//
            hold >>>= 2;
            bits -= 2;
            //---//
          }
          else if (here_val === 17) {
            //=== NEEDBITS(here.bits + 3);
            n = here_bits + 3;
            while (bits < n) {
              if (have === 0) { break inf_leave; }
              have--;
              hold += input[next++] << bits;
              bits += 8;
            }
            //===//
            //--- DROPBITS(here.bits) ---//
            hold >>>= here_bits;
            bits -= here_bits;
            //---//
            len = 0;
            copy = 3 + (hold & 0x07);//BITS(3);
            //--- DROPBITS(3) ---//
            hold >>>= 3;
            bits -= 3;
            //---//
          }
          else {
            //=== NEEDBITS(here.bits + 7);
            n = here_bits + 7;
            while (bits < n) {
              if (have === 0) { break inf_leave; }
              have--;
              hold += input[next++] << bits;
              bits += 8;
            }
            //===//
            //--- DROPBITS(here.bits) ---//
            hold >>>= here_bits;
            bits -= here_bits;
            //---//
            len = 0;
            copy = 11 + (hold & 0x7f);//BITS(7);
            //--- DROPBITS(7) ---//
            hold >>>= 7;
            bits -= 7;
            //---//
          }
          if (state.have + copy > state.nlen + state.ndist) {
            strm.msg = 'invalid bit length repeat';
            state.mode = BAD;
            break;
          }
          while (copy--) {
            state.lens[state.have++] = len;
          }
        }
      }

      /* handle error breaks in while */
      if (state.mode === BAD) { break; }

      /* check for end-of-block code (better have one) */
      if (state.lens[256] === 0) {
        strm.msg = 'invalid code -- missing end-of-block';
        state.mode = BAD;
        break;
      }

      /* build code tables -- note: do not change the lenbits or distbits
         values here (9 and 6) without reading the comments in inftrees.h
         concerning the ENOUGH constants, which depend on those values */
      state.lenbits = 9;

      opts = {bits: state.lenbits};
      ret = inflate_table(LENS, state.lens, 0, state.nlen, state.lencode, 0, state.work, opts);
      // We have separate tables & no pointers. 2 commented lines below not needed.
      // state.next_index = opts.table_index;
      state.lenbits = opts.bits;
      // state.lencode = state.next;

      if (ret) {
        strm.msg = 'invalid literal/lengths set';
        state.mode = BAD;
        break;
      }

      state.distbits = 6;
      //state.distcode.copy(state.codes);
      // Switch to use dynamic table
      state.distcode = state.distdyn;
      opts = {bits: state.distbits};
      ret = inflate_table(DISTS, state.lens, state.nlen, state.ndist, state.distcode, 0, state.work, opts);
      // We have separate tables & no pointers. 2 commented lines below not needed.
      // state.next_index = opts.table_index;
      state.distbits = opts.bits;
      // state.distcode = state.next;

      if (ret) {
        strm.msg = 'invalid distances set';
        state.mode = BAD;
        break;
      }
      //Tracev((stderr, 'inflate:       codes ok\n'));
      state.mode = LEN_;
      if (flush === Z_TREES) { break inf_leave; }
      /* falls through */
    case LEN_:
      state.mode = LEN;
      /* falls through */
    case LEN:
      if (have >= 6 && left >= 258) {
        //--- RESTORE() ---
        strm.next_out = put;
        strm.avail_out = left;
        strm.next_in = next;
        strm.avail_in = have;
        state.hold = hold;
        state.bits = bits;
        //---
        inflate_fast(strm, _out);
        //--- LOAD() ---
        put = strm.next_out;
        output = strm.output;
        left = strm.avail_out;
        next = strm.next_in;
        input = strm.input;
        have = strm.avail_in;
        hold = state.hold;
        bits = state.bits;
        //---

        if (state.mode === TYPE) {
          state.back = -1;
        }
        break;
      }
      state.back = 0;
      for (;;) {
        here = state.lencode[hold & ((1 << state.lenbits) -1)];  /*BITS(state.lenbits)*/
        here_bits = here >>> 24;
        here_op = (here >>> 16) & 0xff;
        here_val = here & 0xffff;

        if (here_bits <= bits) { break; }
        //--- PULLBYTE() ---//
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
        //---//
      }
      if (here_op && (here_op & 0xf0) === 0) {
        last_bits = here_bits;
        last_op = here_op;
        last_val = here_val;
        for (;;) {
          here = state.lencode[last_val +
                  ((hold & ((1 << (last_bits + last_op)) -1))/*BITS(last.bits + last.op)*/ >> last_bits)];
          here_bits = here >>> 24;
          here_op = (here >>> 16) & 0xff;
          here_val = here & 0xffff;

          if ((last_bits + here_bits) <= bits) { break; }
          //--- PULLBYTE() ---//
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
          //---//
        }
        //--- DROPBITS(last.bits) ---//
        hold >>>= last_bits;
        bits -= last_bits;
        //---//
        state.back += last_bits;
      }
      //--- DROPBITS(here.bits) ---//
      hold >>>= here_bits;
      bits -= here_bits;
      //---//
      state.back += here_bits;
      state.length = here_val;
      if (here_op === 0) {
        //Tracevv((stderr, here.val >= 0x20 && here.val < 0x7f ?
        //        "inflate:         literal '%c'\n" :
        //        "inflate:         literal 0x%02x\n", here.val));
        state.mode = LIT;
        break;
      }
      if (here_op & 32) {
        //Tracevv((stderr, "inflate:         end of block\n"));
        state.back = -1;
        state.mode = TYPE;
        break;
      }
      if (here_op & 64) {
        strm.msg = 'invalid literal/length code';
        state.mode = BAD;
        break;
      }
      state.extra = here_op & 15;
      state.mode = LENEXT;
      /* falls through */
    case LENEXT:
      if (state.extra) {
        //=== NEEDBITS(state.extra);
        n = state.extra;
        while (bits < n) {
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
        }
        //===//
        state.length += hold & ((1 << state.extra) -1)/*BITS(state.extra)*/;
        //--- DROPBITS(state.extra) ---//
        hold >>>= state.extra;
        bits -= state.extra;
        //---//
        state.back += state.extra;
      }
      //Tracevv((stderr, "inflate:         length %u\n", state.length));
      state.was = state.length;
      state.mode = DIST;
      /* falls through */
    case DIST:
      for (;;) {
        here = state.distcode[hold & ((1 << state.distbits) -1)];/*BITS(state.distbits)*/
        here_bits = here >>> 24;
        here_op = (here >>> 16) & 0xff;
        here_val = here & 0xffff;

        if ((here_bits) <= bits) { break; }
        //--- PULLBYTE() ---//
        if (have === 0) { break inf_leave; }
        have--;
        hold += input[next++] << bits;
        bits += 8;
        //---//
      }
      if ((here_op & 0xf0) === 0) {
        last_bits = here_bits;
        last_op = here_op;
        last_val = here_val;
        for (;;) {
          here = state.distcode[last_val +
                  ((hold & ((1 << (last_bits + last_op)) -1))/*BITS(last.bits + last.op)*/ >> last_bits)];
          here_bits = here >>> 24;
          here_op = (here >>> 16) & 0xff;
          here_val = here & 0xffff;

          if ((last_bits + here_bits) <= bits) { break; }
          //--- PULLBYTE() ---//
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
          //---//
        }
        //--- DROPBITS(last.bits) ---//
        hold >>>= last_bits;
        bits -= last_bits;
        //---//
        state.back += last_bits;
      }
      //--- DROPBITS(here.bits) ---//
      hold >>>= here_bits;
      bits -= here_bits;
      //---//
      state.back += here_bits;
      if (here_op & 64) {
        strm.msg = 'invalid distance code';
        state.mode = BAD;
        break;
      }
      state.offset = here_val;
      state.extra = (here_op) & 15;
      state.mode = DISTEXT;
      /* falls through */
    case DISTEXT:
      if (state.extra) {
        //=== NEEDBITS(state.extra);
        n = state.extra;
        while (bits < n) {
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
        }
        //===//
        state.offset += hold & ((1 << state.extra) -1)/*BITS(state.extra)*/;
        //--- DROPBITS(state.extra) ---//
        hold >>>= state.extra;
        bits -= state.extra;
        //---//
        state.back += state.extra;
      }
//#ifdef INFLATE_STRICT
      if (state.offset > state.dmax) {
        strm.msg = 'invalid distance too far back';
        state.mode = BAD;
        break;
      }
//#endif
      //Tracevv((stderr, "inflate:         distance %u\n", state.offset));
      state.mode = MATCH;
      /* falls through */
    case MATCH:
      if (left === 0) { break inf_leave; }
      copy = _out - left;
      if (state.offset > copy) {         /* copy from window */
        copy = state.offset - copy;
        if (copy > state.whave) {
          if (state.sane) {
            strm.msg = 'invalid distance too far back';
            state.mode = BAD;
            break;
          }
// (!) This block is disabled in zlib defailts,
// don't enable it for binary compatibility
//#ifdef INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR
//          Trace((stderr, "inflate.c too far\n"));
//          copy -= state.whave;
//          if (copy > state.length) { copy = state.length; }
//          if (copy > left) { copy = left; }
//          left -= copy;
//          state.length -= copy;
//          do {
//            output[put++] = 0;
//          } while (--copy);
//          if (state.length === 0) { state.mode = LEN; }
//          break;
//#endif
        }
        if (copy > state.wnext) {
          copy -= state.wnext;
          from = state.wsize - copy;
        }
        else {
          from = state.wnext - copy;
        }
        if (copy > state.length) { copy = state.length; }
        from_source = state.window;
      }
      else {                              /* copy from output */
        from_source = output;
        from = put - state.offset;
        copy = state.length;
      }
      if (copy > left) { copy = left; }
      left -= copy;
      state.length -= copy;
      do {
        output[put++] = from_source[from++];
      } while (--copy);
      if (state.length === 0) { state.mode = LEN; }
      break;
    case LIT:
      if (left === 0) { break inf_leave; }
      output[put++] = state.length;
      left--;
      state.mode = LEN;
      break;
    case CHECK:
      if (state.wrap) {
        //=== NEEDBITS(32);
        while (bits < 32) {
          if (have === 0) { break inf_leave; }
          have--;
          // Use '|' insdead of '+' to make sure that result is signed
          hold |= input[next++] << bits;
          bits += 8;
        }
        //===//
        _out -= left;
        strm.total_out += _out;
        state.total += _out;
        if (_out) {
          strm.adler = state.check =
              /*UPDATE(state.check, put - _out, _out);*/
              (state.flags ? crc32(state.check, output, _out, put - _out) : adler32(state.check, output, _out, put - _out));

        }
        _out = left;
        // NB: crc32 stored as signed 32-bit int, ZSWAP32 returns signed too
        if ((state.flags ? hold : ZSWAP32(hold)) !== state.check) {
          strm.msg = 'incorrect data check';
          state.mode = BAD;
          break;
        }
        //=== INITBITS();
        hold = 0;
        bits = 0;
        //===//
        //Tracev((stderr, "inflate:   check matches trailer\n"));
      }
      state.mode = LENGTH;
      /* falls through */
    case LENGTH:
      if (state.wrap && state.flags) {
        //=== NEEDBITS(32);
        while (bits < 32) {
          if (have === 0) { break inf_leave; }
          have--;
          hold += input[next++] << bits;
          bits += 8;
        }
        //===//
        if (hold !== (state.total & 0xffffffff)) {
          strm.msg = 'incorrect length check';
          state.mode = BAD;
          break;
        }
        //=== INITBITS();
        hold = 0;
        bits = 0;
        //===//
        //Tracev((stderr, "inflate:   length matches trailer\n"));
      }
      state.mode = DONE;
      /* falls through */
    case DONE:
      ret = Z_STREAM_END;
      break inf_leave;
    case BAD:
      ret = Z_DATA_ERROR;
      break inf_leave;
    case MEM:
      return Z_MEM_ERROR;
    case SYNC:
      /* falls through */
    default:
      return Z_STREAM_ERROR;
    }
  }

  // inf_leave <- here is real place for "goto inf_leave", emulated via "break inf_leave"

  /*
     Return from inflate(), updating the total counts and the check value.
     If there was no progress during the inflate() call, return a buffer
     error.  Call updatewindow() to create and/or update the window state.
     Note: a memory error from inflate() is non-recoverable.
   */

  //--- RESTORE() ---
  strm.next_out = put;
  strm.avail_out = left;
  strm.next_in = next;
  strm.avail_in = have;
  state.hold = hold;
  state.bits = bits;
  //---

  if (state.wsize || (_out !== strm.avail_out && state.mode < BAD &&
                      (state.mode < CHECK || flush !== Z_FINISH))) {
    if (updatewindow(strm, strm.output, strm.next_out, _out - strm.avail_out)) {
      state.mode = MEM;
      return Z_MEM_ERROR;
    }
  }
  _in -= strm.avail_in;
  _out -= strm.avail_out;
  strm.total_in += _in;
  strm.total_out += _out;
  state.total += _out;
  if (state.wrap && _out) {
    strm.adler = state.check = /*UPDATE(state.check, strm.next_out - _out, _out);*/
      (state.flags ? crc32(state.check, output, _out, strm.next_out - _out) : adler32(state.check, output, _out, strm.next_out - _out));
  }
  strm.data_type = state.bits + (state.last ? 64 : 0) +
                    (state.mode === TYPE ? 128 : 0) +
                    (state.mode === LEN_ || state.mode === COPY_ ? 256 : 0);
  if (((_in === 0 && _out === 0) || flush === Z_FINISH) && ret === Z_OK) {
    ret = Z_BUF_ERROR;
  }
  return ret;
}

function inflateEnd(strm) {

  if (!strm || !strm.state /*|| strm->zfree == (free_func)0*/) {
    return Z_STREAM_ERROR;
  }

  var state = strm.state;
  if (state.window) {
    state.window = null;
  }
  strm.state = null;
  return Z_OK;
}

function inflateGetHeader(strm, head) {
  var state;

  /* check state */
  if (!strm || !strm.state) { return Z_STREAM_ERROR; }
  state = strm.state;
  if ((state.wrap & 2) === 0) { return Z_STREAM_ERROR; }

  /* save header structure */
  state.head = head;
  head.done = false;
  return Z_OK;
}


exports.inflateReset = inflateReset;
exports.inflateReset2 = inflateReset2;
exports.inflateResetKeep = inflateResetKeep;
exports.inflateInit = inflateInit;
exports.inflateInit2 = inflateInit2;
exports.inflate = inflate;
exports.inflateEnd = inflateEnd;
exports.inflateGetHeader = inflateGetHeader;
exports.inflateInfo = 'pako inflate (from Nodeca project)';

/* Not implemented
exports.inflateCopy = inflateCopy;
exports.inflateGetDictionary = inflateGetDictionary;
exports.inflateMark = inflateMark;
exports.inflatePrime = inflatePrime;
exports.inflateSetDictionary = inflateSetDictionary;
exports.inflateSync = inflateSync;
exports.inflateSyncPoint = inflateSyncPoint;
exports.inflateUndermine = inflateUndermine;
*/
},{"../utils/common":3,"./adler32":4,"./crc32":6,"./inffast":8,"./inftrees":10}],10:[function(_dereq_,module,exports){
'use strict';


var utils = _dereq_('../utils/common');

var MAXBITS = 15;
var ENOUGH_LENS = 852;
var ENOUGH_DISTS = 592;
//var ENOUGH = (ENOUGH_LENS+ENOUGH_DISTS);

var CODES = 0;
var LENS = 1;
var DISTS = 2;

var lbase = [ /* Length codes 257..285 base */
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0
];

var lext = [ /* Length codes 257..285 extra */
  16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
  19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 16, 72, 78
];

var dbase = [ /* Distance codes 0..29 base */
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
  257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
  8193, 12289, 16385, 24577, 0, 0
];

var dext = [ /* Distance codes 0..29 extra */
  16, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
  23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
  28, 28, 29, 29, 64, 64
];

module.exports = function inflate_table(type, lens, lens_index, codes, table, table_index, work, opts)
{
  var bits = opts.bits;
      //here = opts.here; /* table entry for duplication */

  var len = 0;               /* a code's length in bits */
  var sym = 0;               /* index of code symbols */
  var min = 0, max = 0;          /* minimum and maximum code lengths */
  var root = 0;              /* number of index bits for root table */
  var curr = 0;              /* number of index bits for current table */
  var drop = 0;              /* code bits to drop for sub-table */
  var left = 0;                   /* number of prefix codes available */
  var used = 0;              /* code entries in table used */
  var huff = 0;              /* Huffman code */
  var incr;              /* for incrementing code, index */
  var fill;              /* index for replicating entries */
  var low;               /* low bits for current root entry */
  var mask;              /* mask for low root bits */
  var next;             /* next available space in table */
  var base = null;     /* base value table to use */
  var base_index = 0;
//  var shoextra;    /* extra bits table to use */
  var end;                    /* use base and extra for symbol > end */
  var count = new utils.Buf16(MAXBITS+1); //[MAXBITS+1];    /* number of codes of each length */
  var offs = new utils.Buf16(MAXBITS+1); //[MAXBITS+1];     /* offsets in table for each length */
  var extra = null;
  var extra_index = 0;

  var here_bits, here_op, here_val;

  /*
   Process a set of code lengths to create a canonical Huffman code.  The
   code lengths are lens[0..codes-1].  Each length corresponds to the
   symbols 0..codes-1.  The Huffman code is generated by first sorting the
   symbols by length from short to long, and retaining the symbol order
   for codes with equal lengths.  Then the code starts with all zero bits
   for the first code of the shortest length, and the codes are integer
   increments for the same length, and zeros are appended as the length
   increases.  For the deflate format, these bits are stored backwards
   from their more natural integer increment ordering, and so when the
   decoding tables are built in the large loop below, the integer codes
   are incremented backwards.

   This routine assumes, but does not check, that all of the entries in
   lens[] are in the range 0..MAXBITS.  The caller must assure this.
   1..MAXBITS is interpreted as that code length.  zero means that that
   symbol does not occur in this code.

   The codes are sorted by computing a count of codes for each length,
   creating from that a table of starting indices for each length in the
   sorted table, and then entering the symbols in order in the sorted
   table.  The sorted table is work[], with that space being provided by
   the caller.

   The length counts are used for other purposes as well, i.e. finding
   the minimum and maximum length codes, determining if there are any
   codes at all, checking for a valid set of lengths, and looking ahead
   at length counts to determine sub-table sizes when building the
   decoding tables.
   */

  /* accumulate lengths for codes (assumes lens[] all in 0..MAXBITS) */
  for (len = 0; len <= MAXBITS; len++) {
    count[len] = 0;
  }
  for (sym = 0; sym < codes; sym++) {
    count[lens[lens_index + sym]]++;
  }

  /* bound code lengths, force root to be within code lengths */
  root = bits;
  for (max = MAXBITS; max >= 1; max--) {
    if (count[max] !== 0) { break; }
  }
  if (root > max) {
    root = max;
  }
  if (max === 0) {                     /* no symbols to code at all */
    //table.op[opts.table_index] = 64;  //here.op = (var char)64;    /* invalid code marker */
    //table.bits[opts.table_index] = 1;   //here.bits = (var char)1;
    //table.val[opts.table_index++] = 0;   //here.val = (var short)0;
    table[table_index++] = (1 << 24) | (64 << 16) | 0;


    //table.op[opts.table_index] = 64;
    //table.bits[opts.table_index] = 1;
    //table.val[opts.table_index++] = 0;
    table[table_index++] = (1 << 24) | (64 << 16) | 0;

    opts.bits = 1;
    return 0;     /* no symbols, but wait for decoding to report error */
  }
  for (min = 1; min < max; min++) {
    if (count[min] !== 0) { break; }
  }
  if (root < min) {
    root = min;
  }

  /* check for an over-subscribed or incomplete set of lengths */
  left = 1;
  for (len = 1; len <= MAXBITS; len++) {
    left <<= 1;
    left -= count[len];
    if (left < 0) {
      return -1;
    }        /* over-subscribed */
  }
  if (left > 0 && (type === CODES || max !== 1)) {
    return -1;                      /* incomplete set */
  }

  /* generate offsets into symbol table for each length for sorting */
  offs[1] = 0;
  for (len = 1; len < MAXBITS; len++) {
    offs[len + 1] = offs[len] + count[len];
  }

  /* sort symbols by length, by symbol order within each length */
  for (sym = 0; sym < codes; sym++) {
    if (lens[lens_index + sym] !== 0) {
      work[offs[lens[lens_index + sym]]++] = sym;
    }
  }

  /*
   Create and fill in decoding tables.  In this loop, the table being
   filled is at next and has curr index bits.  The code being used is huff
   with length len.  That code is converted to an index by dropping drop
   bits off of the bottom.  For codes where len is less than drop + curr,
   those top drop + curr - len bits are incremented through all values to
   fill the table with replicated entries.

   root is the number of index bits for the root table.  When len exceeds
   root, sub-tables are created pointed to by the root entry with an index
   of the low root bits of huff.  This is saved in low to check for when a
   new sub-table should be started.  drop is zero when the root table is
   being filled, and drop is root when sub-tables are being filled.

   When a new sub-table is needed, it is necessary to look ahead in the
   code lengths to determine what size sub-table is needed.  The length
   counts are used for this, and so count[] is decremented as codes are
   entered in the tables.

   used keeps track of how many table entries have been allocated from the
   provided *table space.  It is checked for LENS and DIST tables against
   the constants ENOUGH_LENS and ENOUGH_DISTS to guard against changes in
   the initial root table size constants.  See the comments in inftrees.h
   for more information.

   sym increments through all symbols, and the loop terminates when
   all codes of length max, i.e. all codes, have been processed.  This
   routine permits incomplete codes, so another loop after this one fills
   in the rest of the decoding tables with invalid code markers.
   */

  /* set up for code type */
  // poor man optimization - use if-else instead of switch,
  // to avoid deopts in old v8
  if (type === CODES) {
      base = extra = work;    /* dummy value--not used */
      end = 19;
  } else if (type === LENS) {
      base = lbase;
      base_index -= 257;
      extra = lext;
      extra_index -= 257;
      end = 256;
  } else {                    /* DISTS */
      base = dbase;
      extra = dext;
      end = -1;
  }

  /* initialize opts for loop */
  huff = 0;                   /* starting code */
  sym = 0;                    /* starting code symbol */
  len = min;                  /* starting code length */
  next = table_index;              /* current table to fill in */
  curr = root;                /* current table index bits */
  drop = 0;                   /* current bits to drop from code for index */
  low = -1;                   /* trigger new sub-table when len > root */
  used = 1 << root;          /* use root table entries */
  mask = used - 1;            /* mask for comparing low */

  /* check available table space */
  if ((type === LENS && used > ENOUGH_LENS) ||
    (type === DISTS && used > ENOUGH_DISTS)) {
    return 1;
  }

  var i=0;
  /* process all codes and make table entries */
  for (;;) {
    i++;
    /* create table entry */
    here_bits = len - drop;
    if (work[sym] < end) {
      here_op = 0;
      here_val = work[sym];
    }
    else if (work[sym] > end) {
      here_op = extra[extra_index + work[sym]];
      here_val = base[base_index + work[sym]];
    }
    else {
      here_op = 32 + 64;         /* end of block */
      here_val = 0;
    }

    /* replicate for those indices with low len bits equal to huff */
    incr = 1 << (len - drop);
    fill = 1 << curr;
    min = fill;                 /* save offset to next table */
    do {
      fill -= incr;
      table[next + (huff >> drop) + fill] = (here_bits << 24) | (here_op << 16) | here_val |0;
    } while (fill !== 0);

    /* backwards increment the len-bit code huff */
    incr = 1 << (len - 1);
    while (huff & incr) {
      incr >>= 1;
    }
    if (incr !== 0) {
      huff &= incr - 1;
      huff += incr;
    } else {
      huff = 0;
    }

    /* go to next symbol, update count, len */
    sym++;
    if (--count[len] === 0) {
      if (len === max) { break; }
      len = lens[lens_index + work[sym]];
    }

    /* create new sub-table if needed */
    if (len > root && (huff & mask) !== low) {
      /* if first time, transition to sub-tables */
      if (drop === 0) {
        drop = root;
      }

      /* increment past last table */
      next += min;            /* here min is 1 << curr */

      /* determine length of next table */
      curr = len - drop;
      left = 1 << curr;
      while (curr + drop < max) {
        left -= count[curr + drop];
        if (left <= 0) { break; }
        curr++;
        left <<= 1;
      }

      /* check for enough space */
      used += 1 << curr;
      if ((type === LENS && used > ENOUGH_LENS) ||
        (type === DISTS && used > ENOUGH_DISTS)) {
        return 1;
      }

      /* point entry in root table to sub-table */
      low = huff & mask;
      /*table.op[low] = curr;
      table.bits[low] = root;
      table.val[low] = next - opts.table_index;*/
      table[low] = (root << 24) | (curr << 16) | (next - table_index) |0;
    }
  }

  /* fill in remaining table entry if code is incomplete (guaranteed to have
   at most one remaining entry, since if the code is incomplete, the
   maximum code length that was allowed to get this far is one bit) */
  if (huff !== 0) {
    //table.op[next + huff] = 64;            /* invalid code marker */
    //table.bits[next + huff] = len - drop;
    //table.val[next + huff] = 0;
    table[next + huff] = ((len - drop) << 24) | (64 << 16) |0;
  }

  /* set return parameters */
  //opts.table_index += used;
  opts.bits = root;
  return 0;
};

},{"../utils/common":3}],11:[function(_dereq_,module,exports){
'use strict';

module.exports = {
  '2':    'need dictionary',     /* Z_NEED_DICT       2  */
  '1':    'stream end',          /* Z_STREAM_END      1  */
  '0':    '',                    /* Z_OK              0  */
  '-1':   'file error',          /* Z_ERRNO         (-1) */
  '-2':   'stream error',        /* Z_STREAM_ERROR  (-2) */
  '-3':   'data error',          /* Z_DATA_ERROR    (-3) */
  '-4':   'insufficient memory', /* Z_MEM_ERROR     (-4) */
  '-5':   'buffer error',        /* Z_BUF_ERROR     (-5) */
  '-6':   'incompatible version' /* Z_VERSION_ERROR (-6) */
};
},{}],12:[function(_dereq_,module,exports){
'use strict';


var utils = _dereq_('../utils/common');

/* Public constants ==========================================================*/
/* ===========================================================================*/


//var Z_FILTERED          = 1;
//var Z_HUFFMAN_ONLY      = 2;
//var Z_RLE               = 3;
var Z_FIXED               = 4;
//var Z_DEFAULT_STRATEGY  = 0;

/* Possible values of the data_type field (though see inflate()) */
var Z_BINARY              = 0;
var Z_TEXT                = 1;
//var Z_ASCII             = 1; // = Z_TEXT
var Z_UNKNOWN             = 2;

/*============================================================================*/


function zero(buf) { var len = buf.length; while (--len >= 0) { buf[len] = 0; } }

// From zutil.h

var STORED_BLOCK = 0;
var STATIC_TREES = 1;
var DYN_TREES    = 2;
/* The three kinds of block type */

var MIN_MATCH    = 3;
var MAX_MATCH    = 258;
/* The minimum and maximum match lengths */

// From deflate.h
/* ===========================================================================
 * Internal compression state.
 */

var LENGTH_CODES  = 29;
/* number of length codes, not counting the special END_BLOCK code */

var LITERALS      = 256;
/* number of literal bytes 0..255 */

var L_CODES       = LITERALS + 1 + LENGTH_CODES;
/* number of Literal or Length codes, including the END_BLOCK code */

var D_CODES       = 30;
/* number of distance codes */

var BL_CODES      = 19;
/* number of codes used to transfer the bit lengths */

var HEAP_SIZE     = 2*L_CODES + 1;
/* maximum heap size */

var MAX_BITS      = 15;
/* All codes must not exceed MAX_BITS bits */

var Buf_size      = 16;
/* size of bit buffer in bi_buf */


/* ===========================================================================
 * Constants
 */

var MAX_BL_BITS = 7;
/* Bit length codes must not exceed MAX_BL_BITS bits */

var END_BLOCK   = 256;
/* end of block literal code */

var REP_3_6     = 16;
/* repeat previous bit length 3-6 times (2 bits of repeat count) */

var REPZ_3_10   = 17;
/* repeat a zero length 3-10 times  (3 bits of repeat count) */

var REPZ_11_138 = 18;
/* repeat a zero length 11-138 times  (7 bits of repeat count) */

var extra_lbits =   /* extra bits for each length code */
  [0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0];

var extra_dbits =   /* extra bits for each distance code */
  [0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13];

var extra_blbits =  /* extra bits for each bit length code */
  [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7];

var bl_order =
  [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15];
/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */

/* ===========================================================================
 * Local data. These are initialized only once.
 */

// We pre-fill arrays with 0 to avoid uninitialized gaps

var DIST_CODE_LEN = 512; /* see definition of array dist_code below */

// !!!! Use flat array insdead of structure, Freq = i*2, Len = i*2+1
var static_ltree  = new Array((L_CODES+2) * 2);
zero(static_ltree);
/* The static literal tree. Since the bit lengths are imposed, there is no
 * need for the L_CODES extra codes used during heap construction. However
 * The codes 286 and 287 are needed to build a canonical tree (see _tr_init
 * below).
 */

var static_dtree  = new Array(D_CODES * 2);
zero(static_dtree);
/* The static distance tree. (Actually a trivial tree since all codes use
 * 5 bits.)
 */

var _dist_code    = new Array(DIST_CODE_LEN);
zero(_dist_code);
/* Distance codes. The first 256 values correspond to the distances
 * 3 .. 258, the last 256 values correspond to the top 8 bits of
 * the 15 bit distances.
 */

var _length_code  = new Array(MAX_MATCH-MIN_MATCH+1);
zero(_length_code);
/* length code for each normalized match length (0 == MIN_MATCH) */

var base_length   = new Array(LENGTH_CODES);
zero(base_length);
/* First normalized length for each code (0 = MIN_MATCH) */

var base_dist     = new Array(D_CODES);
zero(base_dist);
/* First normalized distance for each code (0 = distance of 1) */


var StaticTreeDesc = function (static_tree, extra_bits, extra_base, elems, max_length) {

  this.static_tree  = static_tree;  /* static tree or NULL */
  this.extra_bits   = extra_bits;   /* extra bits for each code or NULL */
  this.extra_base   = extra_base;   /* base index for extra_bits */
  this.elems        = elems;        /* max number of elements in the tree */
  this.max_length   = max_length;   /* max bit length for the codes */

  // show if `static_tree` has data or dummy - needed for monomorphic objects
  this.has_stree    = static_tree && static_tree.length;
};


var static_l_desc;
var static_d_desc;
var static_bl_desc;


var TreeDesc = function(dyn_tree, stat_desc) {
  this.dyn_tree = dyn_tree;     /* the dynamic tree */
  this.max_code = 0;            /* largest code with non zero frequency */
  this.stat_desc = stat_desc;   /* the corresponding static tree */
};



function d_code(dist) {
  return dist < 256 ? _dist_code[dist] : _dist_code[256 + (dist >>> 7)];
}


/* ===========================================================================
 * Output a short LSB first on the stream.
 * IN assertion: there is enough room in pendingBuf.
 */
function put_short (s, w) {
//    put_byte(s, (uch)((w) & 0xff));
//    put_byte(s, (uch)((ush)(w) >> 8));
  s.pending_buf[s.pending++] = (w) & 0xff;
  s.pending_buf[s.pending++] = (w >>> 8) & 0xff;
}


/* ===========================================================================
 * Send a value on a given number of bits.
 * IN assertion: length <= 16 and value fits in length bits.
 */
function send_bits(s, value, length) {
  if (s.bi_valid > (Buf_size - length)) {
    s.bi_buf |= (value << s.bi_valid) & 0xffff;
    put_short(s, s.bi_buf);
    s.bi_buf = value >> (Buf_size - s.bi_valid);
    s.bi_valid += length - Buf_size;
  } else {
    s.bi_buf |= (value << s.bi_valid) & 0xffff;
    s.bi_valid += length;
  }
}


function send_code(s, c, tree) {
  send_bits(s, tree[c*2]/*.Code*/, tree[c*2 + 1]/*.Len*/);
}


/* ===========================================================================
 * Reverse the first len bits of a code, using straightforward code (a faster
 * method would use a table)
 * IN assertion: 1 <= len <= 15
 */
function bi_reverse(code, len) {
  var res = 0;
  do {
    res |= code & 1;
    code >>>= 1;
    res <<= 1;
  } while (--len > 0);
  return res >>> 1;
}


/* ===========================================================================
 * Flush the bit buffer, keeping at most 7 bits in it.
 */
function bi_flush(s) {
  if (s.bi_valid === 16) {
    put_short(s, s.bi_buf);
    s.bi_buf = 0;
    s.bi_valid = 0;

  } else if (s.bi_valid >= 8) {
    s.pending_buf[s.pending++] = s.bi_buf & 0xff;
    s.bi_buf >>= 8;
    s.bi_valid -= 8;
  }
}


/* ===========================================================================
 * Compute the optimal bit lengths for a tree and update the total bit length
 * for the current block.
 * IN assertion: the fields freq and dad are set, heap[heap_max] and
 *    above are the tree nodes sorted by increasing frequency.
 * OUT assertions: the field len is set to the optimal bit length, the
 *     array bl_count contains the frequencies for each bit length.
 *     The length opt_len is updated; static_len is also updated if stree is
 *     not null.
 */
function gen_bitlen(s, desc)
//    deflate_state *s;
//    tree_desc *desc;    /* the tree descriptor */
{
  var tree            = desc.dyn_tree;
  var max_code        = desc.max_code;
  var stree           = desc.stat_desc.static_tree;
  var has_stree       = desc.stat_desc.has_stree;
  var extra           = desc.stat_desc.extra_bits;
  var base            = desc.stat_desc.extra_base;
  var max_length      = desc.stat_desc.max_length;
  var h;              /* heap index */
  var n, m;           /* iterate over the tree elements */
  var bits;           /* bit length */
  var xbits;          /* extra bits */
  var f;              /* frequency */
  var overflow = 0;   /* number of elements with bit length too large */

  for (bits = 0; bits <= MAX_BITS; bits++) {
    s.bl_count[bits] = 0;
  }

  /* In a first pass, compute the optimal bit lengths (which may
   * overflow in the case of the bit length tree).
   */
  tree[s.heap[s.heap_max]*2 + 1]/*.Len*/ = 0; /* root of the heap */

  for (h = s.heap_max+1; h < HEAP_SIZE; h++) {
    n = s.heap[h];
    bits = tree[tree[n*2 +1]/*.Dad*/ * 2 + 1]/*.Len*/ + 1;
    if (bits > max_length) {
      bits = max_length;
      overflow++;
    }
    tree[n*2 + 1]/*.Len*/ = bits;
    /* We overwrite tree[n].Dad which is no longer needed */

    if (n > max_code) { continue; } /* not a leaf node */

    s.bl_count[bits]++;
    xbits = 0;
    if (n >= base) {
      xbits = extra[n-base];
    }
    f = tree[n * 2]/*.Freq*/;
    s.opt_len += f * (bits + xbits);
    if (has_stree) {
      s.static_len += f * (stree[n*2 + 1]/*.Len*/ + xbits);
    }
  }
  if (overflow === 0) { return; }

  // Trace((stderr,"\nbit length overflow\n"));
  /* This happens for example on obj2 and pic of the Calgary corpus */

  /* Find the first bit length which could increase: */
  do {
    bits = max_length-1;
    while (s.bl_count[bits] === 0) { bits--; }
    s.bl_count[bits]--;      /* move one leaf down the tree */
    s.bl_count[bits+1] += 2; /* move one overflow item as its brother */
    s.bl_count[max_length]--;
    /* The brother of the overflow item also moves one step up,
     * but this does not affect bl_count[max_length]
     */
    overflow -= 2;
  } while (overflow > 0);

  /* Now recompute all bit lengths, scanning in increasing frequency.
   * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
   * lengths instead of fixing only the wrong ones. This idea is taken
   * from 'ar' written by Haruhiko Okumura.)
   */
  for (bits = max_length; bits !== 0; bits--) {
    n = s.bl_count[bits];
    while (n !== 0) {
      m = s.heap[--h];
      if (m > max_code) { continue; }
      if (tree[m*2 + 1]/*.Len*/ !== bits) {
        // Trace((stderr,"code %d bits %d->%d\n", m, tree[m].Len, bits));
        s.opt_len += (bits - tree[m*2 + 1]/*.Len*/)*tree[m*2]/*.Freq*/;
        tree[m*2 + 1]/*.Len*/ = bits;
      }
      n--;
    }
  }
}


/* ===========================================================================
 * Generate the codes for a given tree and bit counts (which need not be
 * optimal).
 * IN assertion: the array bl_count contains the bit length statistics for
 * the given tree and the field len is set for all tree elements.
 * OUT assertion: the field code is set for all tree elements of non
 *     zero code length.
 */
function gen_codes(tree, max_code, bl_count)
//    ct_data *tree;             /* the tree to decorate */
//    int max_code;              /* largest code with non zero frequency */
//    ushf *bl_count;            /* number of codes at each bit length */
{
  var next_code = new Array(MAX_BITS+1); /* next code value for each bit length */
  var code = 0;              /* running code value */
  var bits;                  /* bit index */
  var n;                     /* code index */

  /* The distribution counts are first used to generate the code values
   * without bit reversal.
   */
  for (bits = 1; bits <= MAX_BITS; bits++) {
    next_code[bits] = code = (code + bl_count[bits-1]) << 1;
  }
  /* Check that the bit counts in bl_count are consistent. The last code
   * must be all ones.
   */
  //Assert (code + bl_count[MAX_BITS]-1 == (1<<MAX_BITS)-1,
  //        "inconsistent bit counts");
  //Tracev((stderr,"\ngen_codes: max_code %d ", max_code));

  for (n = 0;  n <= max_code; n++) {
    var len = tree[n*2 + 1]/*.Len*/;
    if (len === 0) { continue; }
    /* Now reverse the bits */
    tree[n*2]/*.Code*/ = bi_reverse(next_code[len]++, len);

    //Tracecv(tree != static_ltree, (stderr,"\nn %3d %c l %2d c %4x (%x) ",
    //     n, (isgraph(n) ? n : ' '), len, tree[n].Code, next_code[len]-1));
  }
}


/* ===========================================================================
 * Initialize the various 'constant' tables.
 */
function tr_static_init() {
  var n;        /* iterates over tree elements */
  var bits;     /* bit counter */
  var length;   /* length value */
  var code;     /* code value */
  var dist;     /* distance index */
  var bl_count = new Array(MAX_BITS+1);
  /* number of codes at each bit length for an optimal tree */

  // do check in _tr_init()
  //if (static_init_done) return;

  /* For some embedded targets, global variables are not initialized: */
/*#ifdef NO_INIT_GLOBAL_POINTERS
  static_l_desc.static_tree = static_ltree;
  static_l_desc.extra_bits = extra_lbits;
  static_d_desc.static_tree = static_dtree;
  static_d_desc.extra_bits = extra_dbits;
  static_bl_desc.extra_bits = extra_blbits;
#endif*/

  /* Initialize the mapping length (0..255) -> length code (0..28) */
  length = 0;
  for (code = 0; code < LENGTH_CODES-1; code++) {
    base_length[code] = length;
    for (n = 0; n < (1<<extra_lbits[code]); n++) {
      _length_code[length++] = code;
    }
  }
  //Assert (length == 256, "tr_static_init: length != 256");
  /* Note that the length 255 (match length 258) can be represented
   * in two different ways: code 284 + 5 bits or code 285, so we
   * overwrite length_code[255] to use the best encoding:
   */
  _length_code[length-1] = code;

  /* Initialize the mapping dist (0..32K) -> dist code (0..29) */
  dist = 0;
  for (code = 0 ; code < 16; code++) {
    base_dist[code] = dist;
    for (n = 0; n < (1<<extra_dbits[code]); n++) {
      _dist_code[dist++] = code;
    }
  }
  //Assert (dist == 256, "tr_static_init: dist != 256");
  dist >>= 7; /* from now on, all distances are divided by 128 */
  for ( ; code < D_CODES; code++) {
    base_dist[code] = dist << 7;
    for (n = 0; n < (1<<(extra_dbits[code]-7)); n++) {
      _dist_code[256 + dist++] = code;
    }
  }
  //Assert (dist == 256, "tr_static_init: 256+dist != 512");

  /* Construct the codes of the static literal tree */
  for (bits = 0; bits <= MAX_BITS; bits++) {
    bl_count[bits] = 0;
  }

  n = 0;
  while (n <= 143) {
    static_ltree[n*2 + 1]/*.Len*/ = 8;
    n++;
    bl_count[8]++;
  }
  while (n <= 255) {
    static_ltree[n*2 + 1]/*.Len*/ = 9;
    n++;
    bl_count[9]++;
  }
  while (n <= 279) {
    static_ltree[n*2 + 1]/*.Len*/ = 7;
    n++;
    bl_count[7]++;
  }
  while (n <= 287) {
    static_ltree[n*2 + 1]/*.Len*/ = 8;
    n++;
    bl_count[8]++;
  }
  /* Codes 286 and 287 do not exist, but we must include them in the
   * tree construction to get a canonical Huffman tree (longest code
   * all ones)
   */
  gen_codes(static_ltree, L_CODES+1, bl_count);

  /* The static distance tree is trivial: */
  for (n = 0; n < D_CODES; n++) {
    static_dtree[n*2 + 1]/*.Len*/ = 5;
    static_dtree[n*2]/*.Code*/ = bi_reverse(n, 5);
  }

  // Now data ready and we can init static trees
  static_l_desc = new StaticTreeDesc(static_ltree, extra_lbits, LITERALS+1, L_CODES, MAX_BITS);
  static_d_desc = new StaticTreeDesc(static_dtree, extra_dbits, 0,          D_CODES, MAX_BITS);
  static_bl_desc =new StaticTreeDesc(new Array(0), extra_blbits, 0,         BL_CODES, MAX_BL_BITS);

  //static_init_done = true;
}


/* ===========================================================================
 * Initialize a new block.
 */
function init_block(s) {
  var n; /* iterates over tree elements */

  /* Initialize the trees. */
  for (n = 0; n < L_CODES;  n++) { s.dyn_ltree[n*2]/*.Freq*/ = 0; }
  for (n = 0; n < D_CODES;  n++) { s.dyn_dtree[n*2]/*.Freq*/ = 0; }
  for (n = 0; n < BL_CODES; n++) { s.bl_tree[n*2]/*.Freq*/ = 0; }

  s.dyn_ltree[END_BLOCK*2]/*.Freq*/ = 1;
  s.opt_len = s.static_len = 0;
  s.last_lit = s.matches = 0;
}


/* ===========================================================================
 * Flush the bit buffer and align the output on a byte boundary
 */
function bi_windup(s)
{
  if (s.bi_valid > 8) {
    put_short(s, s.bi_buf);
  } else if (s.bi_valid > 0) {
    //put_byte(s, (Byte)s->bi_buf);
    s.pending_buf[s.pending++] = s.bi_buf;
  }
  s.bi_buf = 0;
  s.bi_valid = 0;
}

/* ===========================================================================
 * Copy a stored block, storing first the length and its
 * one's complement if requested.
 */
function copy_block(s, buf, len, header)
//DeflateState *s;
//charf    *buf;    /* the input data */
//unsigned len;     /* its length */
//int      header;  /* true if block header must be written */
{
  bi_windup(s);        /* align on byte boundary */

  if (header) {
    put_short(s, len);
    put_short(s, ~len);
  }
//  while (len--) {
//    put_byte(s, *buf++);
//  }
  utils.arraySet(s.pending_buf, s.window, buf, len, s.pending);
  s.pending += len;
}

/* ===========================================================================
 * Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length.
 */
function smaller(tree, n, m, depth) {
  var _n2 = n*2;
  var _m2 = m*2;
  return (tree[_n2]/*.Freq*/ < tree[_m2]/*.Freq*/ ||
         (tree[_n2]/*.Freq*/ === tree[_m2]/*.Freq*/ && depth[n] <= depth[m]));
}

/* ===========================================================================
 * Restore the heap property by moving down the tree starting at node k,
 * exchanging a node with the smallest of its two sons if necessary, stopping
 * when the heap property is re-established (each father smaller than its
 * two sons).
 */
function pqdownheap(s, tree, k)
//    deflate_state *s;
//    ct_data *tree;  /* the tree to restore */
//    int k;               /* node to move down */
{
  var v = s.heap[k];
  var j = k << 1;  /* left son of k */
  while (j <= s.heap_len) {
    /* Set j to the smallest of the two sons: */
    if (j < s.heap_len &&
      smaller(tree, s.heap[j+1], s.heap[j], s.depth)) {
      j++;
    }
    /* Exit if v is smaller than both sons */
    if (smaller(tree, v, s.heap[j], s.depth)) { break; }

    /* Exchange v with the smallest son */
    s.heap[k] = s.heap[j];
    k = j;

    /* And continue down the tree, setting j to the left son of k */
    j <<= 1;
  }
  s.heap[k] = v;
}


// inlined manually
// var SMALLEST = 1;

/* ===========================================================================
 * Send the block data compressed using the given Huffman trees
 */
function compress_block(s, ltree, dtree)
//    deflate_state *s;
//    const ct_data *ltree; /* literal tree */
//    const ct_data *dtree; /* distance tree */
{
  var dist;           /* distance of matched string */
  var lc;             /* match length or unmatched char (if dist == 0) */
  var lx = 0;         /* running index in l_buf */
  var code;           /* the code to send */
  var extra;          /* number of extra bits to send */

  if (s.last_lit !== 0) {
    do {
      dist = (s.pending_buf[s.d_buf + lx*2] << 8) | (s.pending_buf[s.d_buf + lx*2 + 1]);
      lc = s.pending_buf[s.l_buf + lx];
      lx++;

      if (dist === 0) {
        send_code(s, lc, ltree); /* send a literal byte */
        //Tracecv(isgraph(lc), (stderr," '%c' ", lc));
      } else {
        /* Here, lc is the match length - MIN_MATCH */
        code = _length_code[lc];
        send_code(s, code+LITERALS+1, ltree); /* send the length code */
        extra = extra_lbits[code];
        if (extra !== 0) {
          lc -= base_length[code];
          send_bits(s, lc, extra);       /* send the extra length bits */
        }
        dist--; /* dist is now the match distance - 1 */
        code = d_code(dist);
        //Assert (code < D_CODES, "bad d_code");

        send_code(s, code, dtree);       /* send the distance code */
        extra = extra_dbits[code];
        if (extra !== 0) {
          dist -= base_dist[code];
          send_bits(s, dist, extra);   /* send the extra distance bits */
        }
      } /* literal or match pair ? */

      /* Check that the overlay between pending_buf and d_buf+l_buf is ok: */
      //Assert((uInt)(s->pending) < s->lit_bufsize + 2*lx,
      //       "pendingBuf overflow");

    } while (lx < s.last_lit);
  }

  send_code(s, END_BLOCK, ltree);
}


/* ===========================================================================
 * Construct one Huffman tree and assigns the code bit strings and lengths.
 * Update the total bit length for the current block.
 * IN assertion: the field freq is set for all tree elements.
 * OUT assertions: the fields len and code are set to the optimal bit length
 *     and corresponding code. The length opt_len is updated; static_len is
 *     also updated if stree is not null. The field max_code is set.
 */
function build_tree(s, desc)
//    deflate_state *s;
//    tree_desc *desc; /* the tree descriptor */
{
  var tree     = desc.dyn_tree;
  var stree    = desc.stat_desc.static_tree;
  var has_stree = desc.stat_desc.has_stree;
  var elems    = desc.stat_desc.elems;
  var n, m;          /* iterate over heap elements */
  var max_code = -1; /* largest code with non zero frequency */
  var node;          /* new node being created */

  /* Construct the initial heap, with least frequent element in
   * heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
   * heap[0] is not used.
   */
  s.heap_len = 0;
  s.heap_max = HEAP_SIZE;

  for (n = 0; n < elems; n++) {
    if (tree[n * 2]/*.Freq*/ !== 0) {
      s.heap[++s.heap_len] = max_code = n;
      s.depth[n] = 0;

    } else {
      tree[n*2 + 1]/*.Len*/ = 0;
    }
  }

  /* The pkzip format requires that at least one distance code exists,
   * and that at least one bit should be sent even if there is only one
   * possible code. So to avoid special checks later on we force at least
   * two codes of non zero frequency.
   */
  while (s.heap_len < 2) {
    node = s.heap[++s.heap_len] = (max_code < 2 ? ++max_code : 0);
    tree[node * 2]/*.Freq*/ = 1;
    s.depth[node] = 0;
    s.opt_len--;

    if (has_stree) {
      s.static_len -= stree[node*2 + 1]/*.Len*/;
    }
    /* node is 0 or 1 so it does not have extra bits */
  }
  desc.max_code = max_code;

  /* The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
   * establish sub-heaps of increasing lengths:
   */
  for (n = (s.heap_len >> 1/*int /2*/); n >= 1; n--) { pqdownheap(s, tree, n); }

  /* Construct the Huffman tree by repeatedly combining the least two
   * frequent nodes.
   */
  node = elems;              /* next internal node of the tree */
  do {
    //pqremove(s, tree, n);  /* n = node of least frequency */
    /*** pqremove ***/
    n = s.heap[1/*SMALLEST*/];
    s.heap[1/*SMALLEST*/] = s.heap[s.heap_len--];
    pqdownheap(s, tree, 1/*SMALLEST*/);
    /***/

    m = s.heap[1/*SMALLEST*/]; /* m = node of next least frequency */

    s.heap[--s.heap_max] = n; /* keep the nodes sorted by frequency */
    s.heap[--s.heap_max] = m;

    /* Create a new node father of n and m */
    tree[node * 2]/*.Freq*/ = tree[n * 2]/*.Freq*/ + tree[m * 2]/*.Freq*/;
    s.depth[node] = (s.depth[n] >= s.depth[m] ? s.depth[n] : s.depth[m]) + 1;
    tree[n*2 + 1]/*.Dad*/ = tree[m*2 + 1]/*.Dad*/ = node;

    /* and insert the new node in the heap */
    s.heap[1/*SMALLEST*/] = node++;
    pqdownheap(s, tree, 1/*SMALLEST*/);

  } while (s.heap_len >= 2);

  s.heap[--s.heap_max] = s.heap[1/*SMALLEST*/];

  /* At this point, the fields freq and dad are set. We can now
   * generate the bit lengths.
   */
  gen_bitlen(s, desc);

  /* The field len is now set, we can generate the bit codes */
  gen_codes(tree, max_code, s.bl_count);
}


/* ===========================================================================
 * Scan a literal or distance tree to determine the frequencies of the codes
 * in the bit length tree.
 */
function scan_tree(s, tree, max_code)
//    deflate_state *s;
//    ct_data *tree;   /* the tree to be scanned */
//    int max_code;    /* and its largest code of non zero frequency */
{
  var n;                     /* iterates over all tree elements */
  var prevlen = -1;          /* last emitted length */
  var curlen;                /* length of current code */

  var nextlen = tree[0*2 + 1]/*.Len*/; /* length of next code */

  var count = 0;             /* repeat count of the current code */
  var max_count = 7;         /* max repeat count */
  var min_count = 4;         /* min repeat count */

  if (nextlen === 0) {
    max_count = 138;
    min_count = 3;
  }
  tree[(max_code+1)*2 + 1]/*.Len*/ = 0xffff; /* guard */

  for (n = 0; n <= max_code; n++) {
    curlen = nextlen;
    nextlen = tree[(n+1)*2 + 1]/*.Len*/;

    if (++count < max_count && curlen === nextlen) {
      continue;

    } else if (count < min_count) {
      s.bl_tree[curlen * 2]/*.Freq*/ += count;

    } else if (curlen !== 0) {

      if (curlen !== prevlen) { s.bl_tree[curlen * 2]/*.Freq*/++; }
      s.bl_tree[REP_3_6*2]/*.Freq*/++;

    } else if (count <= 10) {
      s.bl_tree[REPZ_3_10*2]/*.Freq*/++;

    } else {
      s.bl_tree[REPZ_11_138*2]/*.Freq*/++;
    }

    count = 0;
    prevlen = curlen;

    if (nextlen === 0) {
      max_count = 138;
      min_count = 3;

    } else if (curlen === nextlen) {
      max_count = 6;
      min_count = 3;

    } else {
      max_count = 7;
      min_count = 4;
    }
  }
}


/* ===========================================================================
 * Send a literal or distance tree in compressed form, using the codes in
 * bl_tree.
 */
function send_tree(s, tree, max_code)
//    deflate_state *s;
//    ct_data *tree; /* the tree to be scanned */
//    int max_code;       /* and its largest code of non zero frequency */
{
  var n;                     /* iterates over all tree elements */
  var prevlen = -1;          /* last emitted length */
  var curlen;                /* length of current code */

  var nextlen = tree[0*2 + 1]/*.Len*/; /* length of next code */

  var count = 0;             /* repeat count of the current code */
  var max_count = 7;         /* max repeat count */
  var min_count = 4;         /* min repeat count */

  /* tree[max_code+1].Len = -1; */  /* guard already set */
  if (nextlen === 0) {
    max_count = 138;
    min_count = 3;
  }

  for (n = 0; n <= max_code; n++) {
    curlen = nextlen;
    nextlen = tree[(n+1)*2 + 1]/*.Len*/;

    if (++count < max_count && curlen === nextlen) {
      continue;

    } else if (count < min_count) {
      do { send_code(s, curlen, s.bl_tree); } while (--count !== 0);

    } else if (curlen !== 0) {
      if (curlen !== prevlen) {
        send_code(s, curlen, s.bl_tree);
        count--;
      }
      //Assert(count >= 3 && count <= 6, " 3_6?");
      send_code(s, REP_3_6, s.bl_tree);
      send_bits(s, count-3, 2);

    } else if (count <= 10) {
      send_code(s, REPZ_3_10, s.bl_tree);
      send_bits(s, count-3, 3);

    } else {
      send_code(s, REPZ_11_138, s.bl_tree);
      send_bits(s, count-11, 7);
    }

    count = 0;
    prevlen = curlen;
    if (nextlen === 0) {
      max_count = 138;
      min_count = 3;

    } else if (curlen === nextlen) {
      max_count = 6;
      min_count = 3;

    } else {
      max_count = 7;
      min_count = 4;
    }
  }
}


/* ===========================================================================
 * Construct the Huffman tree for the bit lengths and return the index in
 * bl_order of the last bit length code to send.
 */
function build_bl_tree(s) {
  var max_blindex;  /* index of last bit length code of non zero freq */

  /* Determine the bit length frequencies for literal and distance trees */
  scan_tree(s, s.dyn_ltree, s.l_desc.max_code);
  scan_tree(s, s.dyn_dtree, s.d_desc.max_code);

  /* Build the bit length tree: */
  build_tree(s, s.bl_desc);
  /* opt_len now includes the length of the tree representations, except
   * the lengths of the bit lengths codes and the 5+5+4 bits for the counts.
   */

  /* Determine the number of bit length codes to send. The pkzip format
   * requires that at least 4 bit length codes be sent. (appnote.txt says
   * 3 but the actual value used is 4.)
   */
  for (max_blindex = BL_CODES-1; max_blindex >= 3; max_blindex--) {
    if (s.bl_tree[bl_order[max_blindex]*2 + 1]/*.Len*/ !== 0) {
      break;
    }
  }
  /* Update opt_len to include the bit length tree and counts */
  s.opt_len += 3*(max_blindex+1) + 5+5+4;
  //Tracev((stderr, "\ndyn trees: dyn %ld, stat %ld",
  //        s->opt_len, s->static_len));

  return max_blindex;
}


/* ===========================================================================
 * Send the header for a block using dynamic Huffman trees: the counts, the
 * lengths of the bit length codes, the literal tree and the distance tree.
 * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
 */
function send_all_trees(s, lcodes, dcodes, blcodes)
//    deflate_state *s;
//    int lcodes, dcodes, blcodes; /* number of codes for each tree */
{
  var rank;                    /* index in bl_order */

  //Assert (lcodes >= 257 && dcodes >= 1 && blcodes >= 4, "not enough codes");
  //Assert (lcodes <= L_CODES && dcodes <= D_CODES && blcodes <= BL_CODES,
  //        "too many codes");
  //Tracev((stderr, "\nbl counts: "));
  send_bits(s, lcodes-257, 5); /* not +255 as stated in appnote.txt */
  send_bits(s, dcodes-1,   5);
  send_bits(s, blcodes-4,  4); /* not -3 as stated in appnote.txt */
  for (rank = 0; rank < blcodes; rank++) {
    //Tracev((stderr, "\nbl code %2d ", bl_order[rank]));
    send_bits(s, s.bl_tree[bl_order[rank]*2 + 1]/*.Len*/, 3);
  }
  //Tracev((stderr, "\nbl tree: sent %ld", s->bits_sent));

  send_tree(s, s.dyn_ltree, lcodes-1); /* literal tree */
  //Tracev((stderr, "\nlit tree: sent %ld", s->bits_sent));

  send_tree(s, s.dyn_dtree, dcodes-1); /* distance tree */
  //Tracev((stderr, "\ndist tree: sent %ld", s->bits_sent));
}


/* ===========================================================================
 * Check if the data type is TEXT or BINARY, using the following algorithm:
 * - TEXT if the two conditions below are satisfied:
 *    a) There are no non-portable control characters belonging to the
 *       "black list" (0..6, 14..25, 28..31).
 *    b) There is at least one printable character belonging to the
 *       "white list" (9 {TAB}, 10 {LF}, 13 {CR}, 32..255).
 * - BINARY otherwise.
 * - The following partially-portable control characters form a
 *   "gray list" that is ignored in this detection algorithm:
 *   (7 {BEL}, 8 {BS}, 11 {VT}, 12 {FF}, 26 {SUB}, 27 {ESC}).
 * IN assertion: the fields Freq of dyn_ltree are set.
 */
function detect_data_type(s) {
  /* black_mask is the bit mask of black-listed bytes
   * set bits 0..6, 14..25, and 28..31
   * 0xf3ffc07f = binary 11110011111111111100000001111111
   */
  var black_mask = 0xf3ffc07f;
  var n;

  /* Check for non-textual ("black-listed") bytes. */
  for (n = 0; n <= 31; n++, black_mask >>>= 1) {
    if ((black_mask & 1) && (s.dyn_ltree[n*2]/*.Freq*/ !== 0)) {
      return Z_BINARY;
    }
  }

  /* Check for textual ("white-listed") bytes. */
  if (s.dyn_ltree[9 * 2]/*.Freq*/ !== 0 || s.dyn_ltree[10 * 2]/*.Freq*/ !== 0 ||
      s.dyn_ltree[13 * 2]/*.Freq*/ !== 0) {
    return Z_TEXT;
  }
  for (n = 32; n < LITERALS; n++) {
    if (s.dyn_ltree[n * 2]/*.Freq*/ !== 0) {
      return Z_TEXT;
    }
  }

  /* There are no "black-listed" or "white-listed" bytes:
   * this stream either is empty or has tolerated ("gray-listed") bytes only.
   */
  return Z_BINARY;
}


var static_init_done = false;

/* ===========================================================================
 * Initialize the tree data structures for a new zlib stream.
 */
function _tr_init(s)
{

  if (!static_init_done) {
    tr_static_init();
    static_init_done = true;
  }

  s.l_desc  = new TreeDesc(s.dyn_ltree, static_l_desc);
  s.d_desc  = new TreeDesc(s.dyn_dtree, static_d_desc);
  s.bl_desc = new TreeDesc(s.bl_tree, static_bl_desc);

  s.bi_buf = 0;
  s.bi_valid = 0;

  /* Initialize the first block of the first file: */
  init_block(s);
}


/* ===========================================================================
 * Send a stored block
 */
function _tr_stored_block(s, buf, stored_len, last)
//DeflateState *s;
//charf *buf;       /* input block */
//ulg stored_len;   /* length of input block */
//int last;         /* one if this is the last block for a file */
{
  send_bits(s, (STORED_BLOCK<<1)+(last ? 1 : 0), 3);    /* send block type */
  copy_block(s, buf, stored_len, true); /* with header */
}


/* ===========================================================================
 * Send one empty static block to give enough lookahead for inflate.
 * This takes 10 bits, of which 7 may remain in the bit buffer.
 */
function _tr_align(s) {
  send_bits(s, STATIC_TREES<<1, 3);
  send_code(s, END_BLOCK, static_ltree);
  bi_flush(s);
}


/* ===========================================================================
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and output the encoded block to the zip file.
 */
function _tr_flush_block(s, buf, stored_len, last)
//DeflateState *s;
//charf *buf;       /* input block, or NULL if too old */
//ulg stored_len;   /* length of input block */
//int last;         /* one if this is the last block for a file */
{
  var opt_lenb, static_lenb;  /* opt_len and static_len in bytes */
  var max_blindex = 0;        /* index of last bit length code of non zero freq */

  /* Build the Huffman trees unless a stored block is forced */
  if (s.level > 0) {

    /* Check if the file is binary or text */
    if (s.strm.data_type === Z_UNKNOWN) {
      s.strm.data_type = detect_data_type(s);
    }

    /* Construct the literal and distance trees */
    build_tree(s, s.l_desc);
    // Tracev((stderr, "\nlit data: dyn %ld, stat %ld", s->opt_len,
    //        s->static_len));

    build_tree(s, s.d_desc);
    // Tracev((stderr, "\ndist data: dyn %ld, stat %ld", s->opt_len,
    //        s->static_len));
    /* At this point, opt_len and static_len are the total bit lengths of
     * the compressed block data, excluding the tree representations.
     */

    /* Build the bit length tree for the above two trees, and get the index
     * in bl_order of the last bit length code to send.
     */
    max_blindex = build_bl_tree(s);

    /* Determine the best encoding. Compute the block lengths in bytes. */
    opt_lenb = (s.opt_len+3+7) >>> 3;
    static_lenb = (s.static_len+3+7) >>> 3;

    // Tracev((stderr, "\nopt %lu(%lu) stat %lu(%lu) stored %lu lit %u ",
    //        opt_lenb, s->opt_len, static_lenb, s->static_len, stored_len,
    //        s->last_lit));

    if (static_lenb <= opt_lenb) { opt_lenb = static_lenb; }

  } else {
    // Assert(buf != (char*)0, "lost buf");
    opt_lenb = static_lenb = stored_len + 5; /* force a stored block */
  }

  if ((stored_len+4 <= opt_lenb) && (buf !== -1)) {
    /* 4: two words for the lengths */

    /* The test buf != NULL is only necessary if LIT_BUFSIZE > WSIZE.
     * Otherwise we can't have processed more than WSIZE input bytes since
     * the last block flush, because compression would have been
     * successful. If LIT_BUFSIZE <= WSIZE, it is never too late to
     * transform a block into a stored block.
     */
    _tr_stored_block(s, buf, stored_len, last);

  } else if (s.strategy === Z_FIXED || static_lenb === opt_lenb) {

    send_bits(s, (STATIC_TREES<<1) + (last ? 1 : 0), 3);
    compress_block(s, static_ltree, static_dtree);

  } else {
    send_bits(s, (DYN_TREES<<1) + (last ? 1 : 0), 3);
    send_all_trees(s, s.l_desc.max_code+1, s.d_desc.max_code+1, max_blindex+1);
    compress_block(s, s.dyn_ltree, s.dyn_dtree);
  }
  // Assert (s->compressed_len == s->bits_sent, "bad compressed size");
  /* The above check is made mod 2^32, for files larger than 512 MB
   * and uLong implemented on 32 bits.
   */
  init_block(s);

  if (last) {
    bi_windup(s);
  }
  // Tracev((stderr,"\ncomprlen %lu(%lu) ", s->compressed_len>>3,
  //       s->compressed_len-7*last));
}

/* ===========================================================================
 * Save the match info and tally the frequency counts. Return true if
 * the current block must be flushed.
 */
function _tr_tally(s, dist, lc)
//    deflate_state *s;
//    unsigned dist;  /* distance of matched string */
//    unsigned lc;    /* match length-MIN_MATCH or unmatched char (if dist==0) */
{
  //var out_length, in_length, dcode;

  s.pending_buf[s.d_buf + s.last_lit * 2]     = (dist >>> 8) & 0xff;
  s.pending_buf[s.d_buf + s.last_lit * 2 + 1] = dist & 0xff;

  s.pending_buf[s.l_buf + s.last_lit] = lc & 0xff;
  s.last_lit++;

  if (dist === 0) {
    /* lc is the unmatched char */
    s.dyn_ltree[lc*2]/*.Freq*/++;
  } else {
    s.matches++;
    /* Here, lc is the match length - MIN_MATCH */
    dist--;             /* dist = match distance - 1 */
    //Assert((ush)dist < (ush)MAX_DIST(s) &&
    //       (ush)lc <= (ush)(MAX_MATCH-MIN_MATCH) &&
    //       (ush)d_code(dist) < (ush)D_CODES,  "_tr_tally: bad match");

    s.dyn_ltree[(_length_code[lc]+LITERALS+1) * 2]/*.Freq*/++;
    s.dyn_dtree[d_code(dist) * 2]/*.Freq*/++;
  }

// (!) This block is disabled in zlib defailts,
// don't enable it for binary compatibility

//#ifdef TRUNCATE_BLOCK
//  /* Try to guess if it is profitable to stop the current block here */
//  if ((s.last_lit & 0x1fff) === 0 && s.level > 2) {
//    /* Compute an upper bound for the compressed length */
//    out_length = s.last_lit*8;
//    in_length = s.strstart - s.block_start;
//
//    for (dcode = 0; dcode < D_CODES; dcode++) {
//      out_length += s.dyn_dtree[dcode*2]/*.Freq*/ * (5 + extra_dbits[dcode]);
//    }
//    out_length >>>= 3;
//    //Tracev((stderr,"\nlast_lit %u, in %ld, out ~%ld(%ld%%) ",
//    //       s->last_lit, in_length, out_length,
//    //       100L - out_length*100L/in_length));
//    if (s.matches < (s.last_lit>>1)/*int /2*/ && out_length < (in_length>>1)/*int /2*/) {
//      return true;
//    }
//  }
//#endif

  return (s.last_lit === s.lit_bufsize-1);
  /* We avoid equality with lit_bufsize because of wraparound at 64K
   * on 16 bit machines and because stored blocks are restricted to
   * 64K-1 bytes.
   */
}

exports._tr_init  = _tr_init;
exports._tr_stored_block = _tr_stored_block;
exports._tr_flush_block  = _tr_flush_block;
exports._tr_tally = _tr_tally;
exports._tr_align = _tr_align;
},{"../utils/common":3}],13:[function(_dereq_,module,exports){
'use strict';


function ZStream() {
  /* next input byte */
  this.input = null; // JS specific, because we have no pointers
  this.next_in = 0;
  /* number of bytes available at input */
  this.avail_in = 0;
  /* total number of input bytes read so far */
  this.total_in = 0;
  /* next output byte should be put there */
  this.output = null; // JS specific, because we have no pointers
  this.next_out = 0;
  /* remaining free space at output */
  this.avail_out = 0;
  /* total number of bytes output so far */
  this.total_out = 0;
  /* last error message, NULL if no error */
  this.msg = ''/*Z_NULL*/;
  /* not visible by applications */
  this.state = null;
  /* best guess about the data type: binary or text */
  this.data_type = 2/*Z_UNKNOWN*/;
  /* adler32 value of the uncompressed data */
  this.adler = 0;
}

module.exports = ZStream;
},{}],14:[function(_dereq_,module,exports){
(function (process,Buffer){
var msg = _dereq_('pako/lib/zlib/messages');
var zstream = _dereq_('pako/lib/zlib/zstream');
var zlib_deflate = _dereq_('pako/lib/zlib/deflate.js');
var zlib_inflate = _dereq_('pako/lib/zlib/inflate.js');
var constants = _dereq_('pako/lib/zlib/constants');

for (var key in constants) {
  exports[key] = constants[key];
}

// zlib modes
exports.NONE = 0;
exports.DEFLATE = 1;
exports.INFLATE = 2;
exports.GZIP = 3;
exports.GUNZIP = 4;
exports.DEFLATERAW = 5;
exports.INFLATERAW = 6;
exports.UNZIP = 7;

/**
 * Emulate Node's zlib C++ layer for use by the JS layer in index.js
 */
function Zlib(mode) {
  if (mode < exports.DEFLATE || mode > exports.UNZIP)
    throw new TypeError("Bad argument");
    
  this.mode = mode;
  this.init_done = false;
  this.write_in_progress = false;
  this.pending_close = false;
  this.windowBits = 0;
  this.level = 0;
  this.memLevel = 0;
  this.strategy = 0;
  this.dictionary = null;
}

Zlib.prototype.init = function(windowBits, level, memLevel, strategy, dictionary) {
  this.windowBits = windowBits;
  this.level = level;
  this.memLevel = memLevel;
  this.strategy = strategy;
  // dictionary not supported.
  
  if (this.mode === exports.GZIP || this.mode === exports.GUNZIP)
    this.windowBits += 16;
    
  if (this.mode === exports.UNZIP)
    this.windowBits += 32;
    
  if (this.mode === exports.DEFLATERAW || this.mode === exports.INFLATERAW)
    this.windowBits = -this.windowBits;
    
  this.strm = new zstream();
  
  switch (this.mode) {
    case exports.DEFLATE:
    case exports.GZIP:
    case exports.DEFLATERAW:
      var status = zlib_deflate.deflateInit2(
        this.strm,
        this.level,
        exports.Z_DEFLATED,
        this.windowBits,
        this.memLevel,
        this.strategy
      );
      break;
    case exports.INFLATE:
    case exports.GUNZIP:
    case exports.INFLATERAW:
    case exports.UNZIP:
      var status  = zlib_inflate.inflateInit2(
        this.strm,
        this.windowBits
      );
      break;
    default:
      throw new Error("Unknown mode " + this.mode);
  }
  
  if (status !== exports.Z_OK) {
    this._error(status);
    return;
  }
  
  this.write_in_progress = false;
  this.init_done = true;
};

Zlib.prototype.params = function() {
  throw new Error("deflateParams Not supported");
};

Zlib.prototype._writeCheck = function() {
  if (!this.init_done)
    throw new Error("write before init");
    
  if (this.mode === exports.NONE)
    throw new Error("already finalized");
    
  if (this.write_in_progress)
    throw new Error("write already in progress");
    
  if (this.pending_close)
    throw new Error("close is pending");
};

Zlib.prototype.write = function(flush, input, in_off, in_len, out, out_off, out_len) {    
  this._writeCheck();
  this.write_in_progress = true;
  
  var self = this;
  process.nextTick(function() {
    self.write_in_progress = false;
    var res = self._write(flush, input, in_off, in_len, out, out_off, out_len);
    self.callback(res[0], res[1]);
    
    if (self.pending_close)
      self.close();
  });
  
  return this;
};

// set method for Node buffers, used by pako
function bufferSet(data, offset) {
  for (var i = 0; i < data.length; i++) {
    this[offset + i] = data[i];
  }
}

Zlib.prototype.writeSync = function(flush, input, in_off, in_len, out, out_off, out_len) {
  this._writeCheck();
  return this._write(flush, input, in_off, in_len, out, out_off, out_len);
};

Zlib.prototype._write = function(flush, input, in_off, in_len, out, out_off, out_len) {
  this.write_in_progress = true;
  
  if (flush !== exports.Z_NO_FLUSH &&
      flush !== exports.Z_PARTIAL_FLUSH &&
      flush !== exports.Z_SYNC_FLUSH &&
      flush !== exports.Z_FULL_FLUSH &&
      flush !== exports.Z_FINISH &&
      flush !== exports.Z_BLOCK) {
    throw new Error("Invalid flush value");
  }
  
  if (input == null) {
    input = new Buffer(0);
    in_len = 0;
    in_off = 0;
  }
  
  if (out._set)
    out.set = out._set;
  else
    out.set = bufferSet;
  
  var strm = this.strm;
  strm.avail_in = in_len;
  strm.input = input;
  strm.next_in = in_off;
  strm.avail_out = out_len;
  strm.output = out;
  strm.next_out = out_off;
  
  switch (this.mode) {
    case exports.DEFLATE:
    case exports.GZIP:
    case exports.DEFLATERAW:
      var status = zlib_deflate.deflate(strm, flush);
      break;
    case exports.UNZIP:
    case exports.INFLATE:
    case exports.GUNZIP:
    case exports.INFLATERAW:
      var status = zlib_inflate.inflate(strm, flush);
      break;
    default:
      throw new Error("Unknown mode " + this.mode);
  }
  
  if (status !== exports.Z_STREAM_END && status !== exports.Z_OK) {
    this._error(status);
  }
  
  this.write_in_progress = false;
  return [strm.avail_in, strm.avail_out];
};

Zlib.prototype.close = function() {
  if (this.write_in_progress) {
    this.pending_close = true;
    return;
  }
  
  this.pending_close = false;
  
  if (this.mode === exports.DEFLATE || this.mode === exports.GZIP || this.mode === exports.DEFLATERAW) {
    zlib_deflate.deflateEnd(this.strm);
  } else {
    zlib_inflate.inflateEnd(this.strm);
  }
  
  this.mode = exports.NONE;
};

Zlib.prototype.reset = function() {
  switch (this.mode) {
    case exports.DEFLATE:
    case exports.DEFLATERAW:
      var status = zlib_deflate.deflateReset(this.strm);
      break;
    case exports.INFLATE:
    case exports.INFLATERAW:
      var status = zlib_inflate.inflateReset(this.strm);
      break;
  }
  
  if (status !== exports.Z_OK) {
    this._error(status);
  }
};

Zlib.prototype._error = function(status) {
  this.onerror(msg[status] + ': ' + this.strm.msg, status);
  
  this.write_in_progress = false;
  if (this.pending_close)
    this.close();
};

exports.Zlib = Zlib;

}).call(this,_dereq_("JkpR2F"),_dereq_("buffer").Buffer)
},{"JkpR2F":21,"buffer":16,"pako/lib/zlib/constants":5,"pako/lib/zlib/deflate.js":7,"pako/lib/zlib/inflate.js":9,"pako/lib/zlib/messages":11,"pako/lib/zlib/zstream":13}],15:[function(_dereq_,module,exports){
(function (process,Buffer){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var Transform = _dereq_('_stream_transform');

var binding = _dereq_('./binding');
var util = _dereq_('util');
var assert = _dereq_('assert').ok;

// zlib doesn't provide these, so kludge them in following the same
// const naming scheme zlib uses.
binding.Z_MIN_WINDOWBITS = 8;
binding.Z_MAX_WINDOWBITS = 15;
binding.Z_DEFAULT_WINDOWBITS = 15;

// fewer than 64 bytes per chunk is stupid.
// technically it could work with as few as 8, but even 64 bytes
// is absurdly low.  Usually a MB or more is best.
binding.Z_MIN_CHUNK = 64;
binding.Z_MAX_CHUNK = Infinity;
binding.Z_DEFAULT_CHUNK = (16 * 1024);

binding.Z_MIN_MEMLEVEL = 1;
binding.Z_MAX_MEMLEVEL = 9;
binding.Z_DEFAULT_MEMLEVEL = 8;

binding.Z_MIN_LEVEL = -1;
binding.Z_MAX_LEVEL = 9;
binding.Z_DEFAULT_LEVEL = binding.Z_DEFAULT_COMPRESSION;

// expose all the zlib constants
Object.keys(binding).forEach(function(k) {
  if (k.match(/^Z/)) exports[k] = binding[k];
});

// translation table for return codes.
exports.codes = {
  Z_OK: binding.Z_OK,
  Z_STREAM_END: binding.Z_STREAM_END,
  Z_NEED_DICT: binding.Z_NEED_DICT,
  Z_ERRNO: binding.Z_ERRNO,
  Z_STREAM_ERROR: binding.Z_STREAM_ERROR,
  Z_DATA_ERROR: binding.Z_DATA_ERROR,
  Z_MEM_ERROR: binding.Z_MEM_ERROR,
  Z_BUF_ERROR: binding.Z_BUF_ERROR,
  Z_VERSION_ERROR: binding.Z_VERSION_ERROR
};

Object.keys(exports.codes).forEach(function(k) {
  exports.codes[exports.codes[k]] = k;
});

exports.Deflate = Deflate;
exports.Inflate = Inflate;
exports.Gzip = Gzip;
exports.Gunzip = Gunzip;
exports.DeflateRaw = DeflateRaw;
exports.InflateRaw = InflateRaw;
exports.Unzip = Unzip;

exports.createDeflate = function(o) {
  return new Deflate(o);
};

exports.createInflate = function(o) {
  return new Inflate(o);
};

exports.createDeflateRaw = function(o) {
  return new DeflateRaw(o);
};

exports.createInflateRaw = function(o) {
  return new InflateRaw(o);
};

exports.createGzip = function(o) {
  return new Gzip(o);
};

exports.createGunzip = function(o) {
  return new Gunzip(o);
};

exports.createUnzip = function(o) {
  return new Unzip(o);
};


// Convenience methods.
// compress/decompress a string or buffer in one step.
exports.deflate = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new Deflate(opts), buffer, callback);
};

exports.deflateSync = function(buffer, opts) {
  return zlibBufferSync(new Deflate(opts), buffer);
};

exports.gzip = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new Gzip(opts), buffer, callback);
};

exports.gzipSync = function(buffer, opts) {
  return zlibBufferSync(new Gzip(opts), buffer);
};

exports.deflateRaw = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new DeflateRaw(opts), buffer, callback);
};

exports.deflateRawSync = function(buffer, opts) {
  return zlibBufferSync(new DeflateRaw(opts), buffer);
};

exports.unzip = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new Unzip(opts), buffer, callback);
};

exports.unzipSync = function(buffer, opts) {
  return zlibBufferSync(new Unzip(opts), buffer);
};

exports.inflate = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new Inflate(opts), buffer, callback);
};

exports.inflateSync = function(buffer, opts) {
  return zlibBufferSync(new Inflate(opts), buffer);
};

exports.gunzip = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new Gunzip(opts), buffer, callback);
};

exports.gunzipSync = function(buffer, opts) {
  return zlibBufferSync(new Gunzip(opts), buffer);
};

exports.inflateRaw = function(buffer, opts, callback) {
  if (typeof opts === 'function') {
    callback = opts;
    opts = {};
  }
  return zlibBuffer(new InflateRaw(opts), buffer, callback);
};

exports.inflateRawSync = function(buffer, opts) {
  return zlibBufferSync(new InflateRaw(opts), buffer);
};

function zlibBuffer(engine, buffer, callback) {
  var buffers = [];
  var nread = 0;

  engine.on('error', onError);
  engine.on('end', onEnd);

  engine.end(buffer);
  flow();

  function flow() {
    var chunk;
    while (null !== (chunk = engine.read())) {
      buffers.push(chunk);
      nread += chunk.length;
    }
    engine.once('readable', flow);
  }

  function onError(err) {
    engine.removeListener('end', onEnd);
    engine.removeListener('readable', flow);
    callback(err);
  }

  function onEnd() {
    var buf = Buffer.concat(buffers, nread);
    buffers = [];
    callback(null, buf);
    engine.close();
  }
}

function zlibBufferSync(engine, buffer) {
  if (typeof buffer === 'string')
    buffer = new Buffer(buffer);
  if (!Buffer.isBuffer(buffer))
    throw new TypeError('Not a string or buffer');

  var flushFlag = binding.Z_FINISH;

  return engine._processChunk(buffer, flushFlag);
}

// generic zlib
// minimal 2-byte header
function Deflate(opts) {
  if (!(this instanceof Deflate)) return new Deflate(opts);
  Zlib.call(this, opts, binding.DEFLATE);
}

function Inflate(opts) {
  if (!(this instanceof Inflate)) return new Inflate(opts);
  Zlib.call(this, opts, binding.INFLATE);
}



// gzip - bigger header, same deflate compression
function Gzip(opts) {
  if (!(this instanceof Gzip)) return new Gzip(opts);
  Zlib.call(this, opts, binding.GZIP);
}

function Gunzip(opts) {
  if (!(this instanceof Gunzip)) return new Gunzip(opts);
  Zlib.call(this, opts, binding.GUNZIP);
}



// raw - no header
function DeflateRaw(opts) {
  if (!(this instanceof DeflateRaw)) return new DeflateRaw(opts);
  Zlib.call(this, opts, binding.DEFLATERAW);
}

function InflateRaw(opts) {
  if (!(this instanceof InflateRaw)) return new InflateRaw(opts);
  Zlib.call(this, opts, binding.INFLATERAW);
}


// auto-detect header.
function Unzip(opts) {
  if (!(this instanceof Unzip)) return new Unzip(opts);
  Zlib.call(this, opts, binding.UNZIP);
}


// the Zlib class they all inherit from
// This thing manages the queue of requests, and returns
// true or false if there is anything in the queue when
// you call the .write() method.

function Zlib(opts, mode) {
  this._opts = opts = opts || {};
  this._chunkSize = opts.chunkSize || exports.Z_DEFAULT_CHUNK;

  Transform.call(this, opts);

  if (opts.flush) {
    if (opts.flush !== binding.Z_NO_FLUSH &&
        opts.flush !== binding.Z_PARTIAL_FLUSH &&
        opts.flush !== binding.Z_SYNC_FLUSH &&
        opts.flush !== binding.Z_FULL_FLUSH &&
        opts.flush !== binding.Z_FINISH &&
        opts.flush !== binding.Z_BLOCK) {
      throw new Error('Invalid flush flag: ' + opts.flush);
    }
  }
  this._flushFlag = opts.flush || binding.Z_NO_FLUSH;

  if (opts.chunkSize) {
    if (opts.chunkSize < exports.Z_MIN_CHUNK ||
        opts.chunkSize > exports.Z_MAX_CHUNK) {
      throw new Error('Invalid chunk size: ' + opts.chunkSize);
    }
  }

  if (opts.windowBits) {
    if (opts.windowBits < exports.Z_MIN_WINDOWBITS ||
        opts.windowBits > exports.Z_MAX_WINDOWBITS) {
      throw new Error('Invalid windowBits: ' + opts.windowBits);
    }
  }

  if (opts.level) {
    if (opts.level < exports.Z_MIN_LEVEL ||
        opts.level > exports.Z_MAX_LEVEL) {
      throw new Error('Invalid compression level: ' + opts.level);
    }
  }

  if (opts.memLevel) {
    if (opts.memLevel < exports.Z_MIN_MEMLEVEL ||
        opts.memLevel > exports.Z_MAX_MEMLEVEL) {
      throw new Error('Invalid memLevel: ' + opts.memLevel);
    }
  }

  if (opts.strategy) {
    if (opts.strategy != exports.Z_FILTERED &&
        opts.strategy != exports.Z_HUFFMAN_ONLY &&
        opts.strategy != exports.Z_RLE &&
        opts.strategy != exports.Z_FIXED &&
        opts.strategy != exports.Z_DEFAULT_STRATEGY) {
      throw new Error('Invalid strategy: ' + opts.strategy);
    }
  }

  if (opts.dictionary) {
    if (!Buffer.isBuffer(opts.dictionary)) {
      throw new Error('Invalid dictionary: it should be a Buffer instance');
    }
  }

  this._binding = new binding.Zlib(mode);

  var self = this;
  this._hadError = false;
  this._binding.onerror = function(message, errno) {
    // there is no way to cleanly recover.
    // continuing only obscures problems.
    self._binding = null;
    self._hadError = true;

    var error = new Error(message);
    error.errno = errno;
    error.code = exports.codes[errno];
    self.emit('error', error);
  };

  var level = exports.Z_DEFAULT_COMPRESSION;
  if (typeof opts.level === 'number') level = opts.level;

  var strategy = exports.Z_DEFAULT_STRATEGY;
  if (typeof opts.strategy === 'number') strategy = opts.strategy;

  this._binding.init(opts.windowBits || exports.Z_DEFAULT_WINDOWBITS,
                     level,
                     opts.memLevel || exports.Z_DEFAULT_MEMLEVEL,
                     strategy,
                     opts.dictionary);

  this._buffer = new Buffer(this._chunkSize);
  this._offset = 0;
  this._closed = false;
  this._level = level;
  this._strategy = strategy;

  this.once('end', this.close);
}

util.inherits(Zlib, Transform);

Zlib.prototype.params = function(level, strategy, callback) {
  if (level < exports.Z_MIN_LEVEL ||
      level > exports.Z_MAX_LEVEL) {
    throw new RangeError('Invalid compression level: ' + level);
  }
  if (strategy != exports.Z_FILTERED &&
      strategy != exports.Z_HUFFMAN_ONLY &&
      strategy != exports.Z_RLE &&
      strategy != exports.Z_FIXED &&
      strategy != exports.Z_DEFAULT_STRATEGY) {
    throw new TypeError('Invalid strategy: ' + strategy);
  }

  if (this._level !== level || this._strategy !== strategy) {
    var self = this;
    this.flush(binding.Z_SYNC_FLUSH, function() {
      self._binding.params(level, strategy);
      if (!self._hadError) {
        self._level = level;
        self._strategy = strategy;
        if (callback) callback();
      }
    });
  } else {
    process.nextTick(callback);
  }
};

Zlib.prototype.reset = function() {
  return this._binding.reset();
};

// This is the _flush function called by the transform class,
// internally, when the last chunk has been written.
Zlib.prototype._flush = function(callback) {
  this._transform(new Buffer(0), '', callback);
};

Zlib.prototype.flush = function(kind, callback) {
  var ws = this._writableState;

  if (typeof kind === 'function' || (kind === void 0 && !callback)) {
    callback = kind;
    kind = binding.Z_FULL_FLUSH;
  }

  if (ws.ended) {
    if (callback)
      process.nextTick(callback);
  } else if (ws.ending) {
    if (callback)
      this.once('end', callback);
  } else if (ws.needDrain) {
    var self = this;
    this.once('drain', function() {
      self.flush(callback);
    });
  } else {
    this._flushFlag = kind;
    this.write(new Buffer(0), '', callback);
  }
};

Zlib.prototype.close = function(callback) {
  if (callback)
    process.nextTick(callback);

  if (this._closed)
    return;

  this._closed = true;

  this._binding.close();

  var self = this;
  process.nextTick(function() {
    self.emit('close');
  });
};

Zlib.prototype._transform = function(chunk, encoding, cb) {
  var flushFlag;
  var ws = this._writableState;
  var ending = ws.ending || ws.ended;
  var last = ending && (!chunk || ws.length === chunk.length);

  if (!chunk === null && !Buffer.isBuffer(chunk))
    return cb(new Error('invalid input'));

  // If it's the last chunk, or a final flush, we use the Z_FINISH flush flag.
  // If it's explicitly flushing at some other time, then we use
  // Z_FULL_FLUSH. Otherwise, use Z_NO_FLUSH for maximum compression
  // goodness.
  if (last)
    flushFlag = binding.Z_FINISH;
  else {
    flushFlag = this._flushFlag;
    // once we've flushed the last of the queue, stop flushing and
    // go back to the normal behavior.
    if (chunk.length >= ws.length) {
      this._flushFlag = this._opts.flush || binding.Z_NO_FLUSH;
    }
  }

  var self = this;
  this._processChunk(chunk, flushFlag, cb);
};

Zlib.prototype._processChunk = function(chunk, flushFlag, cb) {
  var availInBefore = chunk && chunk.length;
  var availOutBefore = this._chunkSize - this._offset;
  var inOff = 0;

  var self = this;

  var async = typeof cb === 'function';

  if (!async) {
    var buffers = [];
    var nread = 0;

    var error;
    this.on('error', function(er) {
      error = er;
    });

    do {
      var res = this._binding.writeSync(flushFlag,
                                        chunk, // in
                                        inOff, // in_off
                                        availInBefore, // in_len
                                        this._buffer, // out
                                        this._offset, //out_off
                                        availOutBefore); // out_len
    } while (!this._hadError && callback(res[0], res[1]));

    if (this._hadError) {
      throw error;
    }

    var buf = Buffer.concat(buffers, nread);
    this.close();

    return buf;
  }

  var req = this._binding.write(flushFlag,
                                chunk, // in
                                inOff, // in_off
                                availInBefore, // in_len
                                this._buffer, // out
                                this._offset, //out_off
                                availOutBefore); // out_len

  req.buffer = chunk;
  req.callback = callback;

  function callback(availInAfter, availOutAfter) {
    if (self._hadError)
      return;

    var have = availOutBefore - availOutAfter;
    assert(have >= 0, 'have should not go down');

    if (have > 0) {
      var out = self._buffer.slice(self._offset, self._offset + have);
      self._offset += have;
      // serve some output to the consumer.
      if (async) {
        self.push(out);
      } else {
        buffers.push(out);
        nread += out.length;
      }
    }

    // exhausted the output buffer, or used all the input create a new one.
    if (availOutAfter === 0 || self._offset >= self._chunkSize) {
      availOutBefore = self._chunkSize;
      self._offset = 0;
      self._buffer = new Buffer(self._chunkSize);
    }

    if (availOutAfter === 0) {
      // Not actually done.  Need to reprocess.
      // Also, update the availInBefore to the availInAfter value,
      // so that if we have to hit it a third (fourth, etc.) time,
      // it'll have the correct byte counts.
      inOff += (availInBefore - availInAfter);
      availInBefore = availInAfter;

      if (!async)
        return true;

      var newReq = self._binding.write(flushFlag,
                                       chunk,
                                       inOff,
                                       availInBefore,
                                       self._buffer,
                                       self._offset,
                                       self._chunkSize);
      newReq.callback = callback; // this same function
      newReq.buffer = chunk;
      return;
    }

    if (!async)
      return false;

    // finished with the chunk.
    cb();
  }
};

util.inherits(Deflate, Zlib);
util.inherits(Inflate, Zlib);
util.inherits(Gzip, Zlib);
util.inherits(Gunzip, Zlib);
util.inherits(DeflateRaw, Zlib);
util.inherits(InflateRaw, Zlib);
util.inherits(Unzip, Zlib);

}).call(this,_dereq_("JkpR2F"),_dereq_("buffer").Buffer)
},{"./binding":14,"JkpR2F":21,"_stream_transform":27,"assert":2,"buffer":16,"util":31}],16:[function(_dereq_,module,exports){
/*!
 * The buffer module from node.js, for the browser.
 *
 * @author   Feross Aboukhadijeh <feross@feross.org> <http://feross.org>
 * @license  MIT
 */

var base64 = _dereq_('base64-js')
var ieee754 = _dereq_('ieee754')

exports.Buffer = Buffer
exports.SlowBuffer = Buffer
exports.INSPECT_MAX_BYTES = 50
Buffer.poolSize = 8192

/**
 * If `Buffer._useTypedArrays`:
 *   === true    Use Uint8Array implementation (fastest)
 *   === false   Use Object implementation (compatible down to IE6)
 */
Buffer._useTypedArrays = (function () {
  // Detect if browser supports Typed Arrays. Supported browsers are IE 10+, Firefox 4+,
  // Chrome 7+, Safari 5.1+, Opera 11.6+, iOS 4.2+. If the browser does not support adding
  // properties to `Uint8Array` instances, then that's the same as no `Uint8Array` support
  // because we need to be able to add all the node Buffer API methods. This is an issue
  // in Firefox 4-29. Now fixed: https://bugzilla.mozilla.org/show_bug.cgi?id=695438
  try {
    var buf = new ArrayBuffer(0)
    var arr = new Uint8Array(buf)
    arr.foo = function () { return 42 }
    return 42 === arr.foo() &&
        typeof arr.subarray === 'function' // Chrome 9-10 lack `subarray`
  } catch (e) {
    return false
  }
})()

/**
 * Class: Buffer
 * =============
 *
 * The Buffer constructor returns instances of `Uint8Array` that are augmented
 * with function properties for all the node `Buffer` API functions. We use
 * `Uint8Array` so that square bracket notation works as expected -- it returns
 * a single octet.
 *
 * By augmenting the instances, we can avoid modifying the `Uint8Array`
 * prototype.
 */
function Buffer (subject, encoding, noZero) {
  if (!(this instanceof Buffer))
    return new Buffer(subject, encoding, noZero)

  var type = typeof subject

  // Workaround: node's base64 implementation allows for non-padded strings
  // while base64-js does not.
  if (encoding === 'base64' && type === 'string') {
    subject = stringtrim(subject)
    while (subject.length % 4 !== 0) {
      subject = subject + '='
    }
  }

  // Find the length
  var length
  if (type === 'number')
    length = coerce(subject)
  else if (type === 'string')
    length = Buffer.byteLength(subject, encoding)
  else if (type === 'object')
    length = coerce(subject.length) // assume that object is array-like
  else
    throw new Error('First argument needs to be a number, array or string.')

  var buf
  if (Buffer._useTypedArrays) {
    // Preferred: Return an augmented `Uint8Array` instance for best performance
    buf = Buffer._augment(new Uint8Array(length))
  } else {
    // Fallback: Return THIS instance of Buffer (created by `new`)
    buf = this
    buf.length = length
    buf._isBuffer = true
  }

  var i
  if (Buffer._useTypedArrays && typeof subject.byteLength === 'number') {
    // Speed optimization -- use set if we're copying from a typed array
    buf._set(subject)
  } else if (isArrayish(subject)) {
    // Treat array-ish objects as a byte array
    for (i = 0; i < length; i++) {
      if (Buffer.isBuffer(subject))
        buf[i] = subject.readUInt8(i)
      else
        buf[i] = subject[i]
    }
  } else if (type === 'string') {
    buf.write(subject, 0, encoding)
  } else if (type === 'number' && !Buffer._useTypedArrays && !noZero) {
    for (i = 0; i < length; i++) {
      buf[i] = 0
    }
  }

  return buf
}

// STATIC METHODS
// ==============

Buffer.isEncoding = function (encoding) {
  switch (String(encoding).toLowerCase()) {
    case 'hex':
    case 'utf8':
    case 'utf-8':
    case 'ascii':
    case 'binary':
    case 'base64':
    case 'raw':
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      return true
    default:
      return false
  }
}

Buffer.isBuffer = function (b) {
  return !!(b !== null && b !== undefined && b._isBuffer)
}

Buffer.byteLength = function (str, encoding) {
  var ret
  str = str + ''
  switch (encoding || 'utf8') {
    case 'hex':
      ret = str.length / 2
      break
    case 'utf8':
    case 'utf-8':
      ret = utf8ToBytes(str).length
      break
    case 'ascii':
    case 'binary':
    case 'raw':
      ret = str.length
      break
    case 'base64':
      ret = base64ToBytes(str).length
      break
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = str.length * 2
      break
    default:
      throw new Error('Unknown encoding')
  }
  return ret
}

Buffer.concat = function (list, totalLength) {
  assert(isArray(list), 'Usage: Buffer.concat(list, [totalLength])\n' +
      'list should be an Array.')

  if (list.length === 0) {
    return new Buffer(0)
  } else if (list.length === 1) {
    return list[0]
  }

  var i
  if (typeof totalLength !== 'number') {
    totalLength = 0
    for (i = 0; i < list.length; i++) {
      totalLength += list[i].length
    }
  }

  var buf = new Buffer(totalLength)
  var pos = 0
  for (i = 0; i < list.length; i++) {
    var item = list[i]
    item.copy(buf, pos)
    pos += item.length
  }
  return buf
}

// BUFFER INSTANCE METHODS
// =======================

function _hexWrite (buf, string, offset, length) {
  offset = Number(offset) || 0
  var remaining = buf.length - offset
  if (!length) {
    length = remaining
  } else {
    length = Number(length)
    if (length > remaining) {
      length = remaining
    }
  }

  // must be an even number of digits
  var strLen = string.length
  assert(strLen % 2 === 0, 'Invalid hex string')

  if (length > strLen / 2) {
    length = strLen / 2
  }
  for (var i = 0; i < length; i++) {
    var byte = parseInt(string.substr(i * 2, 2), 16)
    assert(!isNaN(byte), 'Invalid hex string')
    buf[offset + i] = byte
  }
  Buffer._charsWritten = i * 2
  return i
}

function _utf8Write (buf, string, offset, length) {
  var charsWritten = Buffer._charsWritten =
    blitBuffer(utf8ToBytes(string), buf, offset, length)
  return charsWritten
}

function _asciiWrite (buf, string, offset, length) {
  var charsWritten = Buffer._charsWritten =
    blitBuffer(asciiToBytes(string), buf, offset, length)
  return charsWritten
}

function _binaryWrite (buf, string, offset, length) {
  return _asciiWrite(buf, string, offset, length)
}

function _base64Write (buf, string, offset, length) {
  var charsWritten = Buffer._charsWritten =
    blitBuffer(base64ToBytes(string), buf, offset, length)
  return charsWritten
}

function _utf16leWrite (buf, string, offset, length) {
  var charsWritten = Buffer._charsWritten =
    blitBuffer(utf16leToBytes(string), buf, offset, length)
  return charsWritten
}

Buffer.prototype.write = function (string, offset, length, encoding) {
  // Support both (string, offset, length, encoding)
  // and the legacy (string, encoding, offset, length)
  if (isFinite(offset)) {
    if (!isFinite(length)) {
      encoding = length
      length = undefined
    }
  } else {  // legacy
    var swap = encoding
    encoding = offset
    offset = length
    length = swap
  }

  offset = Number(offset) || 0
  var remaining = this.length - offset
  if (!length) {
    length = remaining
  } else {
    length = Number(length)
    if (length > remaining) {
      length = remaining
    }
  }
  encoding = String(encoding || 'utf8').toLowerCase()

  var ret
  switch (encoding) {
    case 'hex':
      ret = _hexWrite(this, string, offset, length)
      break
    case 'utf8':
    case 'utf-8':
      ret = _utf8Write(this, string, offset, length)
      break
    case 'ascii':
      ret = _asciiWrite(this, string, offset, length)
      break
    case 'binary':
      ret = _binaryWrite(this, string, offset, length)
      break
    case 'base64':
      ret = _base64Write(this, string, offset, length)
      break
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = _utf16leWrite(this, string, offset, length)
      break
    default:
      throw new Error('Unknown encoding')
  }
  return ret
}

Buffer.prototype.toString = function (encoding, start, end) {
  var self = this

  encoding = String(encoding || 'utf8').toLowerCase()
  start = Number(start) || 0
  end = (end !== undefined)
    ? Number(end)
    : end = self.length

  // Fastpath empty strings
  if (end === start)
    return ''

  var ret
  switch (encoding) {
    case 'hex':
      ret = _hexSlice(self, start, end)
      break
    case 'utf8':
    case 'utf-8':
      ret = _utf8Slice(self, start, end)
      break
    case 'ascii':
      ret = _asciiSlice(self, start, end)
      break
    case 'binary':
      ret = _binarySlice(self, start, end)
      break
    case 'base64':
      ret = _base64Slice(self, start, end)
      break
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = _utf16leSlice(self, start, end)
      break
    default:
      throw new Error('Unknown encoding')
  }
  return ret
}

Buffer.prototype.toJSON = function () {
  return {
    type: 'Buffer',
    data: Array.prototype.slice.call(this._arr || this, 0)
  }
}

// copy(targetBuffer, targetStart=0, sourceStart=0, sourceEnd=buffer.length)
Buffer.prototype.copy = function (target, target_start, start, end) {
  var source = this

  if (!start) start = 0
  if (!end && end !== 0) end = this.length
  if (!target_start) target_start = 0

  // Copy 0 bytes; we're done
  if (end === start) return
  if (target.length === 0 || source.length === 0) return

  // Fatal error conditions
  assert(end >= start, 'sourceEnd < sourceStart')
  assert(target_start >= 0 && target_start < target.length,
      'targetStart out of bounds')
  assert(start >= 0 && start < source.length, 'sourceStart out of bounds')
  assert(end >= 0 && end <= source.length, 'sourceEnd out of bounds')

  // Are we oob?
  if (end > this.length)
    end = this.length
  if (target.length - target_start < end - start)
    end = target.length - target_start + start

  var len = end - start

  if (len < 100 || !Buffer._useTypedArrays) {
    for (var i = 0; i < len; i++)
      target[i + target_start] = this[i + start]
  } else {
    target._set(this.subarray(start, start + len), target_start)
  }
}

function _base64Slice (buf, start, end) {
  if (start === 0 && end === buf.length) {
    return base64.fromByteArray(buf)
  } else {
    return base64.fromByteArray(buf.slice(start, end))
  }
}

function _utf8Slice (buf, start, end) {
  var res = ''
  var tmp = ''
  end = Math.min(buf.length, end)

  for (var i = start; i < end; i++) {
    if (buf[i] <= 0x7F) {
      res += decodeUtf8Char(tmp) + String.fromCharCode(buf[i])
      tmp = ''
    } else {
      tmp += '%' + buf[i].toString(16)
    }
  }

  return res + decodeUtf8Char(tmp)
}

function _asciiSlice (buf, start, end) {
  var ret = ''
  end = Math.min(buf.length, end)

  for (var i = start; i < end; i++)
    ret += String.fromCharCode(buf[i])
  return ret
}

function _binarySlice (buf, start, end) {
  return _asciiSlice(buf, start, end)
}

function _hexSlice (buf, start, end) {
  var len = buf.length

  if (!start || start < 0) start = 0
  if (!end || end < 0 || end > len) end = len

  var out = ''
  for (var i = start; i < end; i++) {
    out += toHex(buf[i])
  }
  return out
}

function _utf16leSlice (buf, start, end) {
  var bytes = buf.slice(start, end)
  var res = ''
  for (var i = 0; i < bytes.length; i += 2) {
    res += String.fromCharCode(bytes[i] + bytes[i+1] * 256)
  }
  return res
}

Buffer.prototype.slice = function (start, end) {
  var len = this.length
  start = clamp(start, len, 0)
  end = clamp(end, len, len)

  if (Buffer._useTypedArrays) {
    return Buffer._augment(this.subarray(start, end))
  } else {
    var sliceLen = end - start
    var newBuf = new Buffer(sliceLen, undefined, true)
    for (var i = 0; i < sliceLen; i++) {
      newBuf[i] = this[i + start]
    }
    return newBuf
  }
}

// `get` will be removed in Node 0.13+
Buffer.prototype.get = function (offset) {
  console.log('.get() is deprecated. Access using array indexes instead.')
  return this.readUInt8(offset)
}

// `set` will be removed in Node 0.13+
Buffer.prototype.set = function (v, offset) {
  console.log('.set() is deprecated. Access using array indexes instead.')
  return this.writeUInt8(v, offset)
}

Buffer.prototype.readUInt8 = function (offset, noAssert) {
  if (!noAssert) {
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset < this.length, 'Trying to read beyond buffer length')
  }

  if (offset >= this.length)
    return

  return this[offset]
}

function _readUInt16 (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 1 < buf.length, 'Trying to read beyond buffer length')
  }

  var len = buf.length
  if (offset >= len)
    return

  var val
  if (littleEndian) {
    val = buf[offset]
    if (offset + 1 < len)
      val |= buf[offset + 1] << 8
  } else {
    val = buf[offset] << 8
    if (offset + 1 < len)
      val |= buf[offset + 1]
  }
  return val
}

Buffer.prototype.readUInt16LE = function (offset, noAssert) {
  return _readUInt16(this, offset, true, noAssert)
}

Buffer.prototype.readUInt16BE = function (offset, noAssert) {
  return _readUInt16(this, offset, false, noAssert)
}

function _readUInt32 (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 3 < buf.length, 'Trying to read beyond buffer length')
  }

  var len = buf.length
  if (offset >= len)
    return

  var val
  if (littleEndian) {
    if (offset + 2 < len)
      val = buf[offset + 2] << 16
    if (offset + 1 < len)
      val |= buf[offset + 1] << 8
    val |= buf[offset]
    if (offset + 3 < len)
      val = val + (buf[offset + 3] << 24 >>> 0)
  } else {
    if (offset + 1 < len)
      val = buf[offset + 1] << 16
    if (offset + 2 < len)
      val |= buf[offset + 2] << 8
    if (offset + 3 < len)
      val |= buf[offset + 3]
    val = val + (buf[offset] << 24 >>> 0)
  }
  return val
}

Buffer.prototype.readUInt32LE = function (offset, noAssert) {
  return _readUInt32(this, offset, true, noAssert)
}

Buffer.prototype.readUInt32BE = function (offset, noAssert) {
  return _readUInt32(this, offset, false, noAssert)
}

Buffer.prototype.readInt8 = function (offset, noAssert) {
  if (!noAssert) {
    assert(offset !== undefined && offset !== null,
        'missing offset')
    assert(offset < this.length, 'Trying to read beyond buffer length')
  }

  if (offset >= this.length)
    return

  var neg = this[offset] & 0x80
  if (neg)
    return (0xff - this[offset] + 1) * -1
  else
    return this[offset]
}

function _readInt16 (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 1 < buf.length, 'Trying to read beyond buffer length')
  }

  var len = buf.length
  if (offset >= len)
    return

  var val = _readUInt16(buf, offset, littleEndian, true)
  var neg = val & 0x8000
  if (neg)
    return (0xffff - val + 1) * -1
  else
    return val
}

Buffer.prototype.readInt16LE = function (offset, noAssert) {
  return _readInt16(this, offset, true, noAssert)
}

Buffer.prototype.readInt16BE = function (offset, noAssert) {
  return _readInt16(this, offset, false, noAssert)
}

function _readInt32 (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 3 < buf.length, 'Trying to read beyond buffer length')
  }

  var len = buf.length
  if (offset >= len)
    return

  var val = _readUInt32(buf, offset, littleEndian, true)
  var neg = val & 0x80000000
  if (neg)
    return (0xffffffff - val + 1) * -1
  else
    return val
}

Buffer.prototype.readInt32LE = function (offset, noAssert) {
  return _readInt32(this, offset, true, noAssert)
}

Buffer.prototype.readInt32BE = function (offset, noAssert) {
  return _readInt32(this, offset, false, noAssert)
}

function _readFloat (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset + 3 < buf.length, 'Trying to read beyond buffer length')
  }

  return ieee754.read(buf, offset, littleEndian, 23, 4)
}

Buffer.prototype.readFloatLE = function (offset, noAssert) {
  return _readFloat(this, offset, true, noAssert)
}

Buffer.prototype.readFloatBE = function (offset, noAssert) {
  return _readFloat(this, offset, false, noAssert)
}

function _readDouble (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset + 7 < buf.length, 'Trying to read beyond buffer length')
  }

  return ieee754.read(buf, offset, littleEndian, 52, 8)
}

Buffer.prototype.readDoubleLE = function (offset, noAssert) {
  return _readDouble(this, offset, true, noAssert)
}

Buffer.prototype.readDoubleBE = function (offset, noAssert) {
  return _readDouble(this, offset, false, noAssert)
}

Buffer.prototype.writeUInt8 = function (value, offset, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset < this.length, 'trying to write beyond buffer length')
    verifuint(value, 0xff)
  }

  if (offset >= this.length) return

  this[offset] = value
}

function _writeUInt16 (buf, value, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 1 < buf.length, 'trying to write beyond buffer length')
    verifuint(value, 0xffff)
  }

  var len = buf.length
  if (offset >= len)
    return

  for (var i = 0, j = Math.min(len - offset, 2); i < j; i++) {
    buf[offset + i] =
        (value & (0xff << (8 * (littleEndian ? i : 1 - i)))) >>>
            (littleEndian ? i : 1 - i) * 8
  }
}

Buffer.prototype.writeUInt16LE = function (value, offset, noAssert) {
  _writeUInt16(this, value, offset, true, noAssert)
}

Buffer.prototype.writeUInt16BE = function (value, offset, noAssert) {
  _writeUInt16(this, value, offset, false, noAssert)
}

function _writeUInt32 (buf, value, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 3 < buf.length, 'trying to write beyond buffer length')
    verifuint(value, 0xffffffff)
  }

  var len = buf.length
  if (offset >= len)
    return

  for (var i = 0, j = Math.min(len - offset, 4); i < j; i++) {
    buf[offset + i] =
        (value >>> (littleEndian ? i : 3 - i) * 8) & 0xff
  }
}

Buffer.prototype.writeUInt32LE = function (value, offset, noAssert) {
  _writeUInt32(this, value, offset, true, noAssert)
}

Buffer.prototype.writeUInt32BE = function (value, offset, noAssert) {
  _writeUInt32(this, value, offset, false, noAssert)
}

Buffer.prototype.writeInt8 = function (value, offset, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset < this.length, 'Trying to write beyond buffer length')
    verifsint(value, 0x7f, -0x80)
  }

  if (offset >= this.length)
    return

  if (value >= 0)
    this.writeUInt8(value, offset, noAssert)
  else
    this.writeUInt8(0xff + value + 1, offset, noAssert)
}

function _writeInt16 (buf, value, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 1 < buf.length, 'Trying to write beyond buffer length')
    verifsint(value, 0x7fff, -0x8000)
  }

  var len = buf.length
  if (offset >= len)
    return

  if (value >= 0)
    _writeUInt16(buf, value, offset, littleEndian, noAssert)
  else
    _writeUInt16(buf, 0xffff + value + 1, offset, littleEndian, noAssert)
}

Buffer.prototype.writeInt16LE = function (value, offset, noAssert) {
  _writeInt16(this, value, offset, true, noAssert)
}

Buffer.prototype.writeInt16BE = function (value, offset, noAssert) {
  _writeInt16(this, value, offset, false, noAssert)
}

function _writeInt32 (buf, value, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 3 < buf.length, 'Trying to write beyond buffer length')
    verifsint(value, 0x7fffffff, -0x80000000)
  }

  var len = buf.length
  if (offset >= len)
    return

  if (value >= 0)
    _writeUInt32(buf, value, offset, littleEndian, noAssert)
  else
    _writeUInt32(buf, 0xffffffff + value + 1, offset, littleEndian, noAssert)
}

Buffer.prototype.writeInt32LE = function (value, offset, noAssert) {
  _writeInt32(this, value, offset, true, noAssert)
}

Buffer.prototype.writeInt32BE = function (value, offset, noAssert) {
  _writeInt32(this, value, offset, false, noAssert)
}

function _writeFloat (buf, value, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 3 < buf.length, 'Trying to write beyond buffer length')
    verifIEEE754(value, 3.4028234663852886e+38, -3.4028234663852886e+38)
  }

  var len = buf.length
  if (offset >= len)
    return

  ieee754.write(buf, value, offset, littleEndian, 23, 4)
}

Buffer.prototype.writeFloatLE = function (value, offset, noAssert) {
  _writeFloat(this, value, offset, true, noAssert)
}

Buffer.prototype.writeFloatBE = function (value, offset, noAssert) {
  _writeFloat(this, value, offset, false, noAssert)
}

function _writeDouble (buf, value, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(value !== undefined && value !== null, 'missing value')
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 7 < buf.length,
        'Trying to write beyond buffer length')
    verifIEEE754(value, 1.7976931348623157E+308, -1.7976931348623157E+308)
  }

  var len = buf.length
  if (offset >= len)
    return

  ieee754.write(buf, value, offset, littleEndian, 52, 8)
}

Buffer.prototype.writeDoubleLE = function (value, offset, noAssert) {
  _writeDouble(this, value, offset, true, noAssert)
}

Buffer.prototype.writeDoubleBE = function (value, offset, noAssert) {
  _writeDouble(this, value, offset, false, noAssert)
}

// fill(value, start=0, end=buffer.length)
Buffer.prototype.fill = function (value, start, end) {
  if (!value) value = 0
  if (!start) start = 0
  if (!end) end = this.length

  if (typeof value === 'string') {
    value = value.charCodeAt(0)
  }

  assert(typeof value === 'number' && !isNaN(value), 'value is not a number')
  assert(end >= start, 'end < start')

  // Fill 0 bytes; we're done
  if (end === start) return
  if (this.length === 0) return

  assert(start >= 0 && start < this.length, 'start out of bounds')
  assert(end >= 0 && end <= this.length, 'end out of bounds')

  for (var i = start; i < end; i++) {
    this[i] = value
  }
}

Buffer.prototype.inspect = function () {
  var out = []
  var len = this.length
  for (var i = 0; i < len; i++) {
    out[i] = toHex(this[i])
    if (i === exports.INSPECT_MAX_BYTES) {
      out[i + 1] = '...'
      break
    }
  }
  return '<Buffer ' + out.join(' ') + '>'
}

/**
 * Creates a new `ArrayBuffer` with the *copied* memory of the buffer instance.
 * Added in Node 0.12. Only available in browsers that support ArrayBuffer.
 */
Buffer.prototype.toArrayBuffer = function () {
  if (typeof Uint8Array !== 'undefined') {
    if (Buffer._useTypedArrays) {
      return (new Buffer(this)).buffer
    } else {
      var buf = new Uint8Array(this.length)
      for (var i = 0, len = buf.length; i < len; i += 1)
        buf[i] = this[i]
      return buf.buffer
    }
  } else {
    throw new Error('Buffer.toArrayBuffer not supported in this browser')
  }
}

// HELPER FUNCTIONS
// ================

function stringtrim (str) {
  if (str.trim) return str.trim()
  return str.replace(/^\s+|\s+$/g, '')
}

var BP = Buffer.prototype

/**
 * Augment a Uint8Array *instance* (not the Uint8Array class!) with Buffer methods
 */
Buffer._augment = function (arr) {
  arr._isBuffer = true

  // save reference to original Uint8Array get/set methods before overwriting
  arr._get = arr.get
  arr._set = arr.set

  // deprecated, will be removed in node 0.13+
  arr.get = BP.get
  arr.set = BP.set

  arr.write = BP.write
  arr.toString = BP.toString
  arr.toLocaleString = BP.toString
  arr.toJSON = BP.toJSON
  arr.copy = BP.copy
  arr.slice = BP.slice
  arr.readUInt8 = BP.readUInt8
  arr.readUInt16LE = BP.readUInt16LE
  arr.readUInt16BE = BP.readUInt16BE
  arr.readUInt32LE = BP.readUInt32LE
  arr.readUInt32BE = BP.readUInt32BE
  arr.readInt8 = BP.readInt8
  arr.readInt16LE = BP.readInt16LE
  arr.readInt16BE = BP.readInt16BE
  arr.readInt32LE = BP.readInt32LE
  arr.readInt32BE = BP.readInt32BE
  arr.readFloatLE = BP.readFloatLE
  arr.readFloatBE = BP.readFloatBE
  arr.readDoubleLE = BP.readDoubleLE
  arr.readDoubleBE = BP.readDoubleBE
  arr.writeUInt8 = BP.writeUInt8
  arr.writeUInt16LE = BP.writeUInt16LE
  arr.writeUInt16BE = BP.writeUInt16BE
  arr.writeUInt32LE = BP.writeUInt32LE
  arr.writeUInt32BE = BP.writeUInt32BE
  arr.writeInt8 = BP.writeInt8
  arr.writeInt16LE = BP.writeInt16LE
  arr.writeInt16BE = BP.writeInt16BE
  arr.writeInt32LE = BP.writeInt32LE
  arr.writeInt32BE = BP.writeInt32BE
  arr.writeFloatLE = BP.writeFloatLE
  arr.writeFloatBE = BP.writeFloatBE
  arr.writeDoubleLE = BP.writeDoubleLE
  arr.writeDoubleBE = BP.writeDoubleBE
  arr.fill = BP.fill
  arr.inspect = BP.inspect
  arr.toArrayBuffer = BP.toArrayBuffer

  return arr
}

// slice(start, end)
function clamp (index, len, defaultValue) {
  if (typeof index !== 'number') return defaultValue
  index = ~~index;  // Coerce to integer.
  if (index >= len) return len
  if (index >= 0) return index
  index += len
  if (index >= 0) return index
  return 0
}

function coerce (length) {
  // Coerce length to a number (possibly NaN), round up
  // in case it's fractional (e.g. 123.456) then do a
  // double negate to coerce a NaN to 0. Easy, right?
  length = ~~Math.ceil(+length)
  return length < 0 ? 0 : length
}

function isArray (subject) {
  return (Array.isArray || function (subject) {
    return Object.prototype.toString.call(subject) === '[object Array]'
  })(subject)
}

function isArrayish (subject) {
  return isArray(subject) || Buffer.isBuffer(subject) ||
      subject && typeof subject === 'object' &&
      typeof subject.length === 'number'
}

function toHex (n) {
  if (n < 16) return '0' + n.toString(16)
  return n.toString(16)
}

function utf8ToBytes (str) {
  var byteArray = []
  for (var i = 0; i < str.length; i++) {
    var b = str.charCodeAt(i)
    if (b <= 0x7F)
      byteArray.push(str.charCodeAt(i))
    else {
      var start = i
      if (b >= 0xD800 && b <= 0xDFFF) i++
      var h = encodeURIComponent(str.slice(start, i+1)).substr(1).split('%')
      for (var j = 0; j < h.length; j++)
        byteArray.push(parseInt(h[j], 16))
    }
  }
  return byteArray
}

function asciiToBytes (str) {
  var byteArray = []
  for (var i = 0; i < str.length; i++) {
    // Node's code seems to be doing this and not & 0x7F..
    byteArray.push(str.charCodeAt(i) & 0xFF)
  }
  return byteArray
}

function utf16leToBytes (str) {
  var c, hi, lo
  var byteArray = []
  for (var i = 0; i < str.length; i++) {
    c = str.charCodeAt(i)
    hi = c >> 8
    lo = c % 256
    byteArray.push(lo)
    byteArray.push(hi)
  }

  return byteArray
}

function base64ToBytes (str) {
  return base64.toByteArray(str)
}

function blitBuffer (src, dst, offset, length) {
  var pos
  for (var i = 0; i < length; i++) {
    if ((i + offset >= dst.length) || (i >= src.length))
      break
    dst[i + offset] = src[i]
  }
  return i
}

function decodeUtf8Char (str) {
  try {
    return decodeURIComponent(str)
  } catch (err) {
    return String.fromCharCode(0xFFFD) // UTF 8 invalid char
  }
}

/*
 * We have to make sure that the value is a valid integer. This means that it
 * is non-negative. It has no fractional component and that it does not
 * exceed the maximum allowed value.
 */
function verifuint (value, max) {
  assert(typeof value === 'number', 'cannot write a non-number as a number')
  assert(value >= 0, 'specified a negative value for writing an unsigned value')
  assert(value <= max, 'value is larger than maximum value for type')
  assert(Math.floor(value) === value, 'value has a fractional component')
}

function verifsint (value, max, min) {
  assert(typeof value === 'number', 'cannot write a non-number as a number')
  assert(value <= max, 'value larger than maximum allowed value')
  assert(value >= min, 'value smaller than minimum allowed value')
  assert(Math.floor(value) === value, 'value has a fractional component')
}

function verifIEEE754 (value, max, min) {
  assert(typeof value === 'number', 'cannot write a non-number as a number')
  assert(value <= max, 'value larger than maximum allowed value')
  assert(value >= min, 'value smaller than minimum allowed value')
}

function assert (test, message) {
  if (!test) throw new Error(message || 'Failed assertion')
}

},{"base64-js":17,"ieee754":18}],17:[function(_dereq_,module,exports){
var lookup = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

;(function (exports) {
	'use strict';

  var Arr = (typeof Uint8Array !== 'undefined')
    ? Uint8Array
    : Array

	var PLUS   = '+'.charCodeAt(0)
	var SLASH  = '/'.charCodeAt(0)
	var NUMBER = '0'.charCodeAt(0)
	var LOWER  = 'a'.charCodeAt(0)
	var UPPER  = 'A'.charCodeAt(0)

	function decode (elt) {
		var code = elt.charCodeAt(0)
		if (code === PLUS)
			return 62 // '+'
		if (code === SLASH)
			return 63 // '/'
		if (code < NUMBER)
			return -1 //no match
		if (code < NUMBER + 10)
			return code - NUMBER + 26 + 26
		if (code < UPPER + 26)
			return code - UPPER
		if (code < LOWER + 26)
			return code - LOWER + 26
	}

	function b64ToByteArray (b64) {
		var i, j, l, tmp, placeHolders, arr

		if (b64.length % 4 > 0) {
			throw new Error('Invalid string. Length must be a multiple of 4')
		}

		// the number of equal signs (place holders)
		// if there are two placeholders, than the two characters before it
		// represent one byte
		// if there is only one, then the three characters before it represent 2 bytes
		// this is just a cheap hack to not do indexOf twice
		var len = b64.length
		placeHolders = '=' === b64.charAt(len - 2) ? 2 : '=' === b64.charAt(len - 1) ? 1 : 0

		// base64 is 4/3 + up to two characters of the original data
		arr = new Arr(b64.length * 3 / 4 - placeHolders)

		// if there are placeholders, only get up to the last complete 4 chars
		l = placeHolders > 0 ? b64.length - 4 : b64.length

		var L = 0

		function push (v) {
			arr[L++] = v
		}

		for (i = 0, j = 0; i < l; i += 4, j += 3) {
			tmp = (decode(b64.charAt(i)) << 18) | (decode(b64.charAt(i + 1)) << 12) | (decode(b64.charAt(i + 2)) << 6) | decode(b64.charAt(i + 3))
			push((tmp & 0xFF0000) >> 16)
			push((tmp & 0xFF00) >> 8)
			push(tmp & 0xFF)
		}

		if (placeHolders === 2) {
			tmp = (decode(b64.charAt(i)) << 2) | (decode(b64.charAt(i + 1)) >> 4)
			push(tmp & 0xFF)
		} else if (placeHolders === 1) {
			tmp = (decode(b64.charAt(i)) << 10) | (decode(b64.charAt(i + 1)) << 4) | (decode(b64.charAt(i + 2)) >> 2)
			push((tmp >> 8) & 0xFF)
			push(tmp & 0xFF)
		}

		return arr
	}

	function uint8ToBase64 (uint8) {
		var i,
			extraBytes = uint8.length % 3, // if we have 1 byte left, pad 2 bytes
			output = "",
			temp, length

		function encode (num) {
			return lookup.charAt(num)
		}

		function tripletToBase64 (num) {
			return encode(num >> 18 & 0x3F) + encode(num >> 12 & 0x3F) + encode(num >> 6 & 0x3F) + encode(num & 0x3F)
		}

		// go through the array every three bytes, we'll deal with trailing stuff later
		for (i = 0, length = uint8.length - extraBytes; i < length; i += 3) {
			temp = (uint8[i] << 16) + (uint8[i + 1] << 8) + (uint8[i + 2])
			output += tripletToBase64(temp)
		}

		// pad the end with zeros, but make sure to not forget the extra bytes
		switch (extraBytes) {
			case 1:
				temp = uint8[uint8.length - 1]
				output += encode(temp >> 2)
				output += encode((temp << 4) & 0x3F)
				output += '=='
				break
			case 2:
				temp = (uint8[uint8.length - 2] << 8) + (uint8[uint8.length - 1])
				output += encode(temp >> 10)
				output += encode((temp >> 4) & 0x3F)
				output += encode((temp << 2) & 0x3F)
				output += '='
				break
		}

		return output
	}

	exports.toByteArray = b64ToByteArray
	exports.fromByteArray = uint8ToBase64
}(typeof exports === 'undefined' ? (this.base64js = {}) : exports))

},{}],18:[function(_dereq_,module,exports){
exports.read = function(buffer, offset, isLE, mLen, nBytes) {
  var e, m,
      eLen = nBytes * 8 - mLen - 1,
      eMax = (1 << eLen) - 1,
      eBias = eMax >> 1,
      nBits = -7,
      i = isLE ? (nBytes - 1) : 0,
      d = isLE ? -1 : 1,
      s = buffer[offset + i];

  i += d;

  e = s & ((1 << (-nBits)) - 1);
  s >>= (-nBits);
  nBits += eLen;
  for (; nBits > 0; e = e * 256 + buffer[offset + i], i += d, nBits -= 8);

  m = e & ((1 << (-nBits)) - 1);
  e >>= (-nBits);
  nBits += mLen;
  for (; nBits > 0; m = m * 256 + buffer[offset + i], i += d, nBits -= 8);

  if (e === 0) {
    e = 1 - eBias;
  } else if (e === eMax) {
    return m ? NaN : ((s ? -1 : 1) * Infinity);
  } else {
    m = m + Math.pow(2, mLen);
    e = e - eBias;
  }
  return (s ? -1 : 1) * m * Math.pow(2, e - mLen);
};

exports.write = function(buffer, value, offset, isLE, mLen, nBytes) {
  var e, m, c,
      eLen = nBytes * 8 - mLen - 1,
      eMax = (1 << eLen) - 1,
      eBias = eMax >> 1,
      rt = (mLen === 23 ? Math.pow(2, -24) - Math.pow(2, -77) : 0),
      i = isLE ? 0 : (nBytes - 1),
      d = isLE ? 1 : -1,
      s = value < 0 || (value === 0 && 1 / value < 0) ? 1 : 0;

  value = Math.abs(value);

  if (isNaN(value) || value === Infinity) {
    m = isNaN(value) ? 1 : 0;
    e = eMax;
  } else {
    e = Math.floor(Math.log(value) / Math.LN2);
    if (value * (c = Math.pow(2, -e)) < 1) {
      e--;
      c *= 2;
    }
    if (e + eBias >= 1) {
      value += rt / c;
    } else {
      value += rt * Math.pow(2, 1 - eBias);
    }
    if (value * c >= 2) {
      e++;
      c /= 2;
    }

    if (e + eBias >= eMax) {
      m = 0;
      e = eMax;
    } else if (e + eBias >= 1) {
      m = (value * c - 1) * Math.pow(2, mLen);
      e = e + eBias;
    } else {
      m = value * Math.pow(2, eBias - 1) * Math.pow(2, mLen);
      e = 0;
    }
  }

  for (; mLen >= 8; buffer[offset + i] = m & 0xff, i += d, m /= 256, mLen -= 8);

  e = (e << mLen) | m;
  eLen += mLen;
  for (; eLen > 0; buffer[offset + i] = e & 0xff, i += d, e /= 256, eLen -= 8);

  buffer[offset + i - d] |= s * 128;
};

},{}],19:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

function EventEmitter() {
  this._events = this._events || {};
  this._maxListeners = this._maxListeners || undefined;
}
module.exports = EventEmitter;

// Backwards-compat with node 0.10.x
EventEmitter.EventEmitter = EventEmitter;

EventEmitter.prototype._events = undefined;
EventEmitter.prototype._maxListeners = undefined;

// By default EventEmitters will print a warning if more than 10 listeners are
// added to it. This is a useful default which helps finding memory leaks.
EventEmitter.defaultMaxListeners = 10;

// Obviously not all Emitters should be limited to 10. This function allows
// that to be increased. Set to zero for unlimited.
EventEmitter.prototype.setMaxListeners = function(n) {
  if (!isNumber(n) || n < 0 || isNaN(n))
    throw TypeError('n must be a positive number');
  this._maxListeners = n;
  return this;
};

EventEmitter.prototype.emit = function(type) {
  var er, handler, len, args, i, listeners;

  if (!this._events)
    this._events = {};

  // If there is no 'error' event listener then throw.
  if (type === 'error') {
    if (!this._events.error ||
        (isObject(this._events.error) && !this._events.error.length)) {
      er = arguments[1];
      if (er instanceof Error) {
        throw er; // Unhandled 'error' event
      }
      throw TypeError('Uncaught, unspecified "error" event.');
    }
  }

  handler = this._events[type];

  if (isUndefined(handler))
    return false;

  if (isFunction(handler)) {
    switch (arguments.length) {
      // fast cases
      case 1:
        handler.call(this);
        break;
      case 2:
        handler.call(this, arguments[1]);
        break;
      case 3:
        handler.call(this, arguments[1], arguments[2]);
        break;
      // slower
      default:
        len = arguments.length;
        args = new Array(len - 1);
        for (i = 1; i < len; i++)
          args[i - 1] = arguments[i];
        handler.apply(this, args);
    }
  } else if (isObject(handler)) {
    len = arguments.length;
    args = new Array(len - 1);
    for (i = 1; i < len; i++)
      args[i - 1] = arguments[i];

    listeners = handler.slice();
    len = listeners.length;
    for (i = 0; i < len; i++)
      listeners[i].apply(this, args);
  }

  return true;
};

EventEmitter.prototype.addListener = function(type, listener) {
  var m;

  if (!isFunction(listener))
    throw TypeError('listener must be a function');

  if (!this._events)
    this._events = {};

  // To avoid recursion in the case that type === "newListener"! Before
  // adding it to the listeners, first emit "newListener".
  if (this._events.newListener)
    this.emit('newListener', type,
              isFunction(listener.listener) ?
              listener.listener : listener);

  if (!this._events[type])
    // Optimize the case of one listener. Don't need the extra array object.
    this._events[type] = listener;
  else if (isObject(this._events[type]))
    // If we've already got an array, just append.
    this._events[type].push(listener);
  else
    // Adding the second element, need to change to array.
    this._events[type] = [this._events[type], listener];

  // Check for listener leak
  if (isObject(this._events[type]) && !this._events[type].warned) {
    var m;
    if (!isUndefined(this._maxListeners)) {
      m = this._maxListeners;
    } else {
      m = EventEmitter.defaultMaxListeners;
    }

    if (m && m > 0 && this._events[type].length > m) {
      this._events[type].warned = true;
      console.error('(node) warning: possible EventEmitter memory ' +
                    'leak detected. %d listeners added. ' +
                    'Use emitter.setMaxListeners() to increase limit.',
                    this._events[type].length);
      if (typeof console.trace === 'function') {
        // not supported in IE 10
        console.trace();
      }
    }
  }

  return this;
};

EventEmitter.prototype.on = EventEmitter.prototype.addListener;

EventEmitter.prototype.once = function(type, listener) {
  if (!isFunction(listener))
    throw TypeError('listener must be a function');

  var fired = false;

  function g() {
    this.removeListener(type, g);

    if (!fired) {
      fired = true;
      listener.apply(this, arguments);
    }
  }

  g.listener = listener;
  this.on(type, g);

  return this;
};

// emits a 'removeListener' event iff the listener was removed
EventEmitter.prototype.removeListener = function(type, listener) {
  var list, position, length, i;

  if (!isFunction(listener))
    throw TypeError('listener must be a function');

  if (!this._events || !this._events[type])
    return this;

  list = this._events[type];
  length = list.length;
  position = -1;

  if (list === listener ||
      (isFunction(list.listener) && list.listener === listener)) {
    delete this._events[type];
    if (this._events.removeListener)
      this.emit('removeListener', type, listener);

  } else if (isObject(list)) {
    for (i = length; i-- > 0;) {
      if (list[i] === listener ||
          (list[i].listener && list[i].listener === listener)) {
        position = i;
        break;
      }
    }

    if (position < 0)
      return this;

    if (list.length === 1) {
      list.length = 0;
      delete this._events[type];
    } else {
      list.splice(position, 1);
    }

    if (this._events.removeListener)
      this.emit('removeListener', type, listener);
  }

  return this;
};

EventEmitter.prototype.removeAllListeners = function(type) {
  var key, listeners;

  if (!this._events)
    return this;

  // not listening for removeListener, no need to emit
  if (!this._events.removeListener) {
    if (arguments.length === 0)
      this._events = {};
    else if (this._events[type])
      delete this._events[type];
    return this;
  }

  // emit removeListener for all listeners on all events
  if (arguments.length === 0) {
    for (key in this._events) {
      if (key === 'removeListener') continue;
      this.removeAllListeners(key);
    }
    this.removeAllListeners('removeListener');
    this._events = {};
    return this;
  }

  listeners = this._events[type];

  if (isFunction(listeners)) {
    this.removeListener(type, listeners);
  } else {
    // LIFO order
    while (listeners.length)
      this.removeListener(type, listeners[listeners.length - 1]);
  }
  delete this._events[type];

  return this;
};

EventEmitter.prototype.listeners = function(type) {
  var ret;
  if (!this._events || !this._events[type])
    ret = [];
  else if (isFunction(this._events[type]))
    ret = [this._events[type]];
  else
    ret = this._events[type].slice();
  return ret;
};

EventEmitter.listenerCount = function(emitter, type) {
  var ret;
  if (!emitter._events || !emitter._events[type])
    ret = 0;
  else if (isFunction(emitter._events[type]))
    ret = 1;
  else
    ret = emitter._events[type].length;
  return ret;
};

function isFunction(arg) {
  return typeof arg === 'function';
}

function isNumber(arg) {
  return typeof arg === 'number';
}

function isObject(arg) {
  return typeof arg === 'object' && arg !== null;
}

function isUndefined(arg) {
  return arg === void 0;
}

},{}],20:[function(_dereq_,module,exports){
if (typeof Object.create === 'function') {
  // implementation from standard node.js 'util' module
  module.exports = function inherits(ctor, superCtor) {
    ctor.super_ = superCtor
    ctor.prototype = Object.create(superCtor.prototype, {
      constructor: {
        value: ctor,
        enumerable: false,
        writable: true,
        configurable: true
      }
    });
  };
} else {
  // old school shim for old browsers
  module.exports = function inherits(ctor, superCtor) {
    ctor.super_ = superCtor
    var TempCtor = function () {}
    TempCtor.prototype = superCtor.prototype
    ctor.prototype = new TempCtor()
    ctor.prototype.constructor = ctor
  }
}

},{}],21:[function(_dereq_,module,exports){
// shim for using process in browser

var process = module.exports = {};

process.nextTick = (function () {
    var canSetImmediate = typeof window !== 'undefined'
    && window.setImmediate;
    var canPost = typeof window !== 'undefined'
    && window.postMessage && window.addEventListener
    ;

    if (canSetImmediate) {
        return function (f) { return window.setImmediate(f) };
    }

    if (canPost) {
        var queue = [];
        window.addEventListener('message', function (ev) {
            var source = ev.source;
            if ((source === window || source === null) && ev.data === 'process-tick') {
                ev.stopPropagation();
                if (queue.length > 0) {
                    var fn = queue.shift();
                    fn();
                }
            }
        }, true);

        return function nextTick(fn) {
            queue.push(fn);
            window.postMessage('process-tick', '*');
        };
    }

    return function nextTick(fn) {
        setTimeout(fn, 0);
    };
})();

process.title = 'browser';
process.browser = true;
process.env = {};
process.argv = [];

function noop() {}

process.on = noop;
process.addListener = noop;
process.once = noop;
process.off = noop;
process.removeListener = noop;
process.removeAllListeners = noop;
process.emit = noop;

process.binding = function (name) {
    throw new Error('process.binding is not supported');
}

// TODO(shtylman)
process.cwd = function () { return '/' };
process.chdir = function (dir) {
    throw new Error('process.chdir is not supported');
};

},{}],22:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// a duplex stream is just a stream that is both readable and writable.
// Since JS doesn't have multiple prototypal inheritance, this class
// prototypally inherits from Readable, and then parasitically from
// Writable.

module.exports = Duplex;
var inherits = _dereq_('inherits');
var setImmediate = _dereq_('process/browser.js').nextTick;
var Readable = _dereq_('./readable.js');
var Writable = _dereq_('./writable.js');

inherits(Duplex, Readable);

Duplex.prototype.write = Writable.prototype.write;
Duplex.prototype.end = Writable.prototype.end;
Duplex.prototype._write = Writable.prototype._write;

function Duplex(options) {
  if (!(this instanceof Duplex))
    return new Duplex(options);

  Readable.call(this, options);
  Writable.call(this, options);

  if (options && options.readable === false)
    this.readable = false;

  if (options && options.writable === false)
    this.writable = false;

  this.allowHalfOpen = true;
  if (options && options.allowHalfOpen === false)
    this.allowHalfOpen = false;

  this.once('end', onend);
}

// the no-half-open enforcer
function onend() {
  // if we allow half-open state, or if the writable side ended,
  // then we're ok.
  if (this.allowHalfOpen || this._writableState.ended)
    return;

  // no more data can be written.
  // But allow more writes to happen in this tick.
  var self = this;
  setImmediate(function () {
    self.end();
  });
}

},{"./readable.js":26,"./writable.js":28,"inherits":20,"process/browser.js":24}],23:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

module.exports = Stream;

var EE = _dereq_('events').EventEmitter;
var inherits = _dereq_('inherits');

inherits(Stream, EE);
Stream.Readable = _dereq_('./readable.js');
Stream.Writable = _dereq_('./writable.js');
Stream.Duplex = _dereq_('./duplex.js');
Stream.Transform = _dereq_('./transform.js');
Stream.PassThrough = _dereq_('./passthrough.js');

// Backwards-compat with node 0.4.x
Stream.Stream = Stream;



// old-style streams.  Note that the pipe method (the only relevant
// part of this class) is overridden in the Readable class.

function Stream() {
  EE.call(this);
}

Stream.prototype.pipe = function(dest, options) {
  var source = this;

  function ondata(chunk) {
    if (dest.writable) {
      if (false === dest.write(chunk) && source.pause) {
        source.pause();
      }
    }
  }

  source.on('data', ondata);

  function ondrain() {
    if (source.readable && source.resume) {
      source.resume();
    }
  }

  dest.on('drain', ondrain);

  // If the 'end' option is not supplied, dest.end() will be called when
  // source gets the 'end' or 'close' events.  Only dest.end() once.
  if (!dest._isStdio && (!options || options.end !== false)) {
    source.on('end', onend);
    source.on('close', onclose);
  }

  var didOnEnd = false;
  function onend() {
    if (didOnEnd) return;
    didOnEnd = true;

    dest.end();
  }


  function onclose() {
    if (didOnEnd) return;
    didOnEnd = true;

    if (typeof dest.destroy === 'function') dest.destroy();
  }

  // don't leave dangling pipes when there are errors.
  function onerror(er) {
    cleanup();
    if (EE.listenerCount(this, 'error') === 0) {
      throw er; // Unhandled stream error in pipe.
    }
  }

  source.on('error', onerror);
  dest.on('error', onerror);

  // remove all the event listeners that were added.
  function cleanup() {
    source.removeListener('data', ondata);
    dest.removeListener('drain', ondrain);

    source.removeListener('end', onend);
    source.removeListener('close', onclose);

    source.removeListener('error', onerror);
    dest.removeListener('error', onerror);

    source.removeListener('end', cleanup);
    source.removeListener('close', cleanup);

    dest.removeListener('close', cleanup);
  }

  source.on('end', cleanup);
  source.on('close', cleanup);

  dest.on('close', cleanup);

  dest.emit('pipe', source);

  // Allow for unix-like usage: A.pipe(B).pipe(C)
  return dest;
};

},{"./duplex.js":22,"./passthrough.js":25,"./readable.js":26,"./transform.js":27,"./writable.js":28,"events":19,"inherits":20}],24:[function(_dereq_,module,exports){
// shim for using process in browser

var process = module.exports = {};

process.nextTick = (function () {
    var canSetImmediate = typeof window !== 'undefined'
    && window.setImmediate;
    var canPost = typeof window !== 'undefined'
    && window.postMessage && window.addEventListener
    ;

    if (canSetImmediate) {
        return function (f) { return window.setImmediate(f) };
    }

    if (canPost) {
        var queue = [];
        window.addEventListener('message', function (ev) {
            var source = ev.source;
            if ((source === window || source === null) && ev.data === 'process-tick') {
                ev.stopPropagation();
                if (queue.length > 0) {
                    var fn = queue.shift();
                    fn();
                }
            }
        }, true);

        return function nextTick(fn) {
            queue.push(fn);
            window.postMessage('process-tick', '*');
        };
    }

    return function nextTick(fn) {
        setTimeout(fn, 0);
    };
})();

process.title = 'browser';
process.browser = true;
process.env = {};
process.argv = [];

process.binding = function (name) {
    throw new Error('process.binding is not supported');
}

// TODO(shtylman)
process.cwd = function () { return '/' };
process.chdir = function (dir) {
    throw new Error('process.chdir is not supported');
};

},{}],25:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// a passthrough stream.
// basically just the most minimal sort of Transform stream.
// Every written chunk gets output as-is.

module.exports = PassThrough;

var Transform = _dereq_('./transform.js');
var inherits = _dereq_('inherits');
inherits(PassThrough, Transform);

function PassThrough(options) {
  if (!(this instanceof PassThrough))
    return new PassThrough(options);

  Transform.call(this, options);
}

PassThrough.prototype._transform = function(chunk, encoding, cb) {
  cb(null, chunk);
};

},{"./transform.js":27,"inherits":20}],26:[function(_dereq_,module,exports){
(function (process){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

module.exports = Readable;
Readable.ReadableState = ReadableState;

var EE = _dereq_('events').EventEmitter;
var Stream = _dereq_('./index.js');
var Buffer = _dereq_('buffer').Buffer;
var setImmediate = _dereq_('process/browser.js').nextTick;
var StringDecoder;

var inherits = _dereq_('inherits');
inherits(Readable, Stream);

function ReadableState(options, stream) {
  options = options || {};

  // the point at which it stops calling _read() to fill the buffer
  // Note: 0 is a valid value, means "don't call _read preemptively ever"
  var hwm = options.highWaterMark;
  this.highWaterMark = (hwm || hwm === 0) ? hwm : 16 * 1024;

  // cast to ints.
  this.highWaterMark = ~~this.highWaterMark;

  this.buffer = [];
  this.length = 0;
  this.pipes = null;
  this.pipesCount = 0;
  this.flowing = false;
  this.ended = false;
  this.endEmitted = false;
  this.reading = false;

  // In streams that never have any data, and do push(null) right away,
  // the consumer can miss the 'end' event if they do some I/O before
  // consuming the stream.  So, we don't emit('end') until some reading
  // happens.
  this.calledRead = false;

  // a flag to be able to tell if the onwrite cb is called immediately,
  // or on a later tick.  We set this to true at first, becuase any
  // actions that shouldn't happen until "later" should generally also
  // not happen before the first write call.
  this.sync = true;

  // whenever we return null, then we set a flag to say
  // that we're awaiting a 'readable' event emission.
  this.needReadable = false;
  this.emittedReadable = false;
  this.readableListening = false;


  // object stream flag. Used to make read(n) ignore n and to
  // make all the buffer merging and length checks go away
  this.objectMode = !!options.objectMode;

  // Crypto is kind of old and crusty.  Historically, its default string
  // encoding is 'binary' so we have to make this configurable.
  // Everything else in the universe uses 'utf8', though.
  this.defaultEncoding = options.defaultEncoding || 'utf8';

  // when piping, we only care about 'readable' events that happen
  // after read()ing all the bytes and not getting any pushback.
  this.ranOut = false;

  // the number of writers that are awaiting a drain event in .pipe()s
  this.awaitDrain = 0;

  // if true, a maybeReadMore has been scheduled
  this.readingMore = false;

  this.decoder = null;
  this.encoding = null;
  if (options.encoding) {
    if (!StringDecoder)
      StringDecoder = _dereq_('string_decoder').StringDecoder;
    this.decoder = new StringDecoder(options.encoding);
    this.encoding = options.encoding;
  }
}

function Readable(options) {
  if (!(this instanceof Readable))
    return new Readable(options);

  this._readableState = new ReadableState(options, this);

  // legacy
  this.readable = true;

  Stream.call(this);
}

// Manually shove something into the read() buffer.
// This returns true if the highWaterMark has not been hit yet,
// similar to how Writable.write() returns true if you should
// write() some more.
Readable.prototype.push = function(chunk, encoding) {
  var state = this._readableState;

  if (typeof chunk === 'string' && !state.objectMode) {
    encoding = encoding || state.defaultEncoding;
    if (encoding !== state.encoding) {
      chunk = new Buffer(chunk, encoding);
      encoding = '';
    }
  }

  return readableAddChunk(this, state, chunk, encoding, false);
};

// Unshift should *always* be something directly out of read()
Readable.prototype.unshift = function(chunk) {
  var state = this._readableState;
  return readableAddChunk(this, state, chunk, '', true);
};

function readableAddChunk(stream, state, chunk, encoding, addToFront) {
  var er = chunkInvalid(state, chunk);
  if (er) {
    stream.emit('error', er);
  } else if (chunk === null || chunk === undefined) {
    state.reading = false;
    if (!state.ended)
      onEofChunk(stream, state);
  } else if (state.objectMode || chunk && chunk.length > 0) {
    if (state.ended && !addToFront) {
      var e = new Error('stream.push() after EOF');
      stream.emit('error', e);
    } else if (state.endEmitted && addToFront) {
      var e = new Error('stream.unshift() after end event');
      stream.emit('error', e);
    } else {
      if (state.decoder && !addToFront && !encoding)
        chunk = state.decoder.write(chunk);

      // update the buffer info.
      state.length += state.objectMode ? 1 : chunk.length;
      if (addToFront) {
        state.buffer.unshift(chunk);
      } else {
        state.reading = false;
        state.buffer.push(chunk);
      }

      if (state.needReadable)
        emitReadable(stream);

      maybeReadMore(stream, state);
    }
  } else if (!addToFront) {
    state.reading = false;
  }

  return needMoreData(state);
}



// if it's past the high water mark, we can push in some more.
// Also, if we have no data yet, we can stand some
// more bytes.  This is to work around cases where hwm=0,
// such as the repl.  Also, if the push() triggered a
// readable event, and the user called read(largeNumber) such that
// needReadable was set, then we ought to push more, so that another
// 'readable' event will be triggered.
function needMoreData(state) {
  return !state.ended &&
         (state.needReadable ||
          state.length < state.highWaterMark ||
          state.length === 0);
}

// backwards compatibility.
Readable.prototype.setEncoding = function(enc) {
  if (!StringDecoder)
    StringDecoder = _dereq_('string_decoder').StringDecoder;
  this._readableState.decoder = new StringDecoder(enc);
  this._readableState.encoding = enc;
};

// Don't raise the hwm > 128MB
var MAX_HWM = 0x800000;
function roundUpToNextPowerOf2(n) {
  if (n >= MAX_HWM) {
    n = MAX_HWM;
  } else {
    // Get the next highest power of 2
    n--;
    for (var p = 1; p < 32; p <<= 1) n |= n >> p;
    n++;
  }
  return n;
}

function howMuchToRead(n, state) {
  if (state.length === 0 && state.ended)
    return 0;

  if (state.objectMode)
    return n === 0 ? 0 : 1;

  if (isNaN(n) || n === null) {
    // only flow one buffer at a time
    if (state.flowing && state.buffer.length)
      return state.buffer[0].length;
    else
      return state.length;
  }

  if (n <= 0)
    return 0;

  // If we're asking for more than the target buffer level,
  // then raise the water mark.  Bump up to the next highest
  // power of 2, to prevent increasing it excessively in tiny
  // amounts.
  if (n > state.highWaterMark)
    state.highWaterMark = roundUpToNextPowerOf2(n);

  // don't have that much.  return null, unless we've ended.
  if (n > state.length) {
    if (!state.ended) {
      state.needReadable = true;
      return 0;
    } else
      return state.length;
  }

  return n;
}

// you can override either this method, or the async _read(n) below.
Readable.prototype.read = function(n) {
  var state = this._readableState;
  state.calledRead = true;
  var nOrig = n;

  if (typeof n !== 'number' || n > 0)
    state.emittedReadable = false;

  // if we're doing read(0) to trigger a readable event, but we
  // already have a bunch of data in the buffer, then just trigger
  // the 'readable' event and move on.
  if (n === 0 &&
      state.needReadable &&
      (state.length >= state.highWaterMark || state.ended)) {
    emitReadable(this);
    return null;
  }

  n = howMuchToRead(n, state);

  // if we've ended, and we're now clear, then finish it up.
  if (n === 0 && state.ended) {
    if (state.length === 0)
      endReadable(this);
    return null;
  }

  // All the actual chunk generation logic needs to be
  // *below* the call to _read.  The reason is that in certain
  // synthetic stream cases, such as passthrough streams, _read
  // may be a completely synchronous operation which may change
  // the state of the read buffer, providing enough data when
  // before there was *not* enough.
  //
  // So, the steps are:
  // 1. Figure out what the state of things will be after we do
  // a read from the buffer.
  //
  // 2. If that resulting state will trigger a _read, then call _read.
  // Note that this may be asynchronous, or synchronous.  Yes, it is
  // deeply ugly to write APIs this way, but that still doesn't mean
  // that the Readable class should behave improperly, as streams are
  // designed to be sync/async agnostic.
  // Take note if the _read call is sync or async (ie, if the read call
  // has returned yet), so that we know whether or not it's safe to emit
  // 'readable' etc.
  //
  // 3. Actually pull the requested chunks out of the buffer and return.

  // if we need a readable event, then we need to do some reading.
  var doRead = state.needReadable;

  // if we currently have less than the highWaterMark, then also read some
  if (state.length - n <= state.highWaterMark)
    doRead = true;

  // however, if we've ended, then there's no point, and if we're already
  // reading, then it's unnecessary.
  if (state.ended || state.reading)
    doRead = false;

  if (doRead) {
    state.reading = true;
    state.sync = true;
    // if the length is currently zero, then we *need* a readable event.
    if (state.length === 0)
      state.needReadable = true;
    // call internal read method
    this._read(state.highWaterMark);
    state.sync = false;
  }

  // If _read called its callback synchronously, then `reading`
  // will be false, and we need to re-evaluate how much data we
  // can return to the user.
  if (doRead && !state.reading)
    n = howMuchToRead(nOrig, state);

  var ret;
  if (n > 0)
    ret = fromList(n, state);
  else
    ret = null;

  if (ret === null) {
    state.needReadable = true;
    n = 0;
  }

  state.length -= n;

  // If we have nothing in the buffer, then we want to know
  // as soon as we *do* get something into the buffer.
  if (state.length === 0 && !state.ended)
    state.needReadable = true;

  // If we happened to read() exactly the remaining amount in the
  // buffer, and the EOF has been seen at this point, then make sure
  // that we emit 'end' on the very next tick.
  if (state.ended && !state.endEmitted && state.length === 0)
    endReadable(this);

  return ret;
};

function chunkInvalid(state, chunk) {
  var er = null;
  if (!Buffer.isBuffer(chunk) &&
      'string' !== typeof chunk &&
      chunk !== null &&
      chunk !== undefined &&
      !state.objectMode &&
      !er) {
    er = new TypeError('Invalid non-string/buffer chunk');
  }
  return er;
}


function onEofChunk(stream, state) {
  if (state.decoder && !state.ended) {
    var chunk = state.decoder.end();
    if (chunk && chunk.length) {
      state.buffer.push(chunk);
      state.length += state.objectMode ? 1 : chunk.length;
    }
  }
  state.ended = true;

  // if we've ended and we have some data left, then emit
  // 'readable' now to make sure it gets picked up.
  if (state.length > 0)
    emitReadable(stream);
  else
    endReadable(stream);
}

// Don't emit readable right away in sync mode, because this can trigger
// another read() call => stack overflow.  This way, it might trigger
// a nextTick recursion warning, but that's not so bad.
function emitReadable(stream) {
  var state = stream._readableState;
  state.needReadable = false;
  if (state.emittedReadable)
    return;

  state.emittedReadable = true;
  if (state.sync)
    setImmediate(function() {
      emitReadable_(stream);
    });
  else
    emitReadable_(stream);
}

function emitReadable_(stream) {
  stream.emit('readable');
}


// at this point, the user has presumably seen the 'readable' event,
// and called read() to consume some data.  that may have triggered
// in turn another _read(n) call, in which case reading = true if
// it's in progress.
// However, if we're not ended, or reading, and the length < hwm,
// then go ahead and try to read some more preemptively.
function maybeReadMore(stream, state) {
  if (!state.readingMore) {
    state.readingMore = true;
    setImmediate(function() {
      maybeReadMore_(stream, state);
    });
  }
}

function maybeReadMore_(stream, state) {
  var len = state.length;
  while (!state.reading && !state.flowing && !state.ended &&
         state.length < state.highWaterMark) {
    stream.read(0);
    if (len === state.length)
      // didn't get any data, stop spinning.
      break;
    else
      len = state.length;
  }
  state.readingMore = false;
}

// abstract method.  to be overridden in specific implementation classes.
// call cb(er, data) where data is <= n in length.
// for virtual (non-string, non-buffer) streams, "length" is somewhat
// arbitrary, and perhaps not very meaningful.
Readable.prototype._read = function(n) {
  this.emit('error', new Error('not implemented'));
};

Readable.prototype.pipe = function(dest, pipeOpts) {
  var src = this;
  var state = this._readableState;

  switch (state.pipesCount) {
    case 0:
      state.pipes = dest;
      break;
    case 1:
      state.pipes = [state.pipes, dest];
      break;
    default:
      state.pipes.push(dest);
      break;
  }
  state.pipesCount += 1;

  var doEnd = (!pipeOpts || pipeOpts.end !== false) &&
              dest !== process.stdout &&
              dest !== process.stderr;

  var endFn = doEnd ? onend : cleanup;
  if (state.endEmitted)
    setImmediate(endFn);
  else
    src.once('end', endFn);

  dest.on('unpipe', onunpipe);
  function onunpipe(readable) {
    if (readable !== src) return;
    cleanup();
  }

  function onend() {
    dest.end();
  }

  // when the dest drains, it reduces the awaitDrain counter
  // on the source.  This would be more elegant with a .once()
  // handler in flow(), but adding and removing repeatedly is
  // too slow.
  var ondrain = pipeOnDrain(src);
  dest.on('drain', ondrain);

  function cleanup() {
    // cleanup event handlers once the pipe is broken
    dest.removeListener('close', onclose);
    dest.removeListener('finish', onfinish);
    dest.removeListener('drain', ondrain);
    dest.removeListener('error', onerror);
    dest.removeListener('unpipe', onunpipe);
    src.removeListener('end', onend);
    src.removeListener('end', cleanup);

    // if the reader is waiting for a drain event from this
    // specific writer, then it would cause it to never start
    // flowing again.
    // So, if this is awaiting a drain, then we just call it now.
    // If we don't know, then assume that we are waiting for one.
    if (!dest._writableState || dest._writableState.needDrain)
      ondrain();
  }

  // if the dest has an error, then stop piping into it.
  // however, don't suppress the throwing behavior for this.
  // check for listeners before emit removes one-time listeners.
  var errListeners = EE.listenerCount(dest, 'error');
  function onerror(er) {
    unpipe();
    if (errListeners === 0 && EE.listenerCount(dest, 'error') === 0)
      dest.emit('error', er);
  }
  dest.once('error', onerror);

  // Both close and finish should trigger unpipe, but only once.
  function onclose() {
    dest.removeListener('finish', onfinish);
    unpipe();
  }
  dest.once('close', onclose);
  function onfinish() {
    dest.removeListener('close', onclose);
    unpipe();
  }
  dest.once('finish', onfinish);

  function unpipe() {
    src.unpipe(dest);
  }

  // tell the dest that it's being piped to
  dest.emit('pipe', src);

  // start the flow if it hasn't been started already.
  if (!state.flowing) {
    // the handler that waits for readable events after all
    // the data gets sucked out in flow.
    // This would be easier to follow with a .once() handler
    // in flow(), but that is too slow.
    this.on('readable', pipeOnReadable);

    state.flowing = true;
    setImmediate(function() {
      flow(src);
    });
  }

  return dest;
};

function pipeOnDrain(src) {
  return function() {
    var dest = this;
    var state = src._readableState;
    state.awaitDrain--;
    if (state.awaitDrain === 0)
      flow(src);
  };
}

function flow(src) {
  var state = src._readableState;
  var chunk;
  state.awaitDrain = 0;

  function write(dest, i, list) {
    var written = dest.write(chunk);
    if (false === written) {
      state.awaitDrain++;
    }
  }

  while (state.pipesCount && null !== (chunk = src.read())) {

    if (state.pipesCount === 1)
      write(state.pipes, 0, null);
    else
      forEach(state.pipes, write);

    src.emit('data', chunk);

    // if anyone needs a drain, then we have to wait for that.
    if (state.awaitDrain > 0)
      return;
  }

  // if every destination was unpiped, either before entering this
  // function, or in the while loop, then stop flowing.
  //
  // NB: This is a pretty rare edge case.
  if (state.pipesCount === 0) {
    state.flowing = false;

    // if there were data event listeners added, then switch to old mode.
    if (EE.listenerCount(src, 'data') > 0)
      emitDataEvents(src);
    return;
  }

  // at this point, no one needed a drain, so we just ran out of data
  // on the next readable event, start it over again.
  state.ranOut = true;
}

function pipeOnReadable() {
  if (this._readableState.ranOut) {
    this._readableState.ranOut = false;
    flow(this);
  }
}


Readable.prototype.unpipe = function(dest) {
  var state = this._readableState;

  // if we're not piping anywhere, then do nothing.
  if (state.pipesCount === 0)
    return this;

  // just one destination.  most common case.
  if (state.pipesCount === 1) {
    // passed in one, but it's not the right one.
    if (dest && dest !== state.pipes)
      return this;

    if (!dest)
      dest = state.pipes;

    // got a match.
    state.pipes = null;
    state.pipesCount = 0;
    this.removeListener('readable', pipeOnReadable);
    state.flowing = false;
    if (dest)
      dest.emit('unpipe', this);
    return this;
  }

  // slow case. multiple pipe destinations.

  if (!dest) {
    // remove all.
    var dests = state.pipes;
    var len = state.pipesCount;
    state.pipes = null;
    state.pipesCount = 0;
    this.removeListener('readable', pipeOnReadable);
    state.flowing = false;

    for (var i = 0; i < len; i++)
      dests[i].emit('unpipe', this);
    return this;
  }

  // try to find the right one.
  var i = indexOf(state.pipes, dest);
  if (i === -1)
    return this;

  state.pipes.splice(i, 1);
  state.pipesCount -= 1;
  if (state.pipesCount === 1)
    state.pipes = state.pipes[0];

  dest.emit('unpipe', this);

  return this;
};

// set up data events if they are asked for
// Ensure readable listeners eventually get something
Readable.prototype.on = function(ev, fn) {
  var res = Stream.prototype.on.call(this, ev, fn);

  if (ev === 'data' && !this._readableState.flowing)
    emitDataEvents(this);

  if (ev === 'readable' && this.readable) {
    var state = this._readableState;
    if (!state.readableListening) {
      state.readableListening = true;
      state.emittedReadable = false;
      state.needReadable = true;
      if (!state.reading) {
        this.read(0);
      } else if (state.length) {
        emitReadable(this, state);
      }
    }
  }

  return res;
};
Readable.prototype.addListener = Readable.prototype.on;

// pause() and resume() are remnants of the legacy readable stream API
// If the user uses them, then switch into old mode.
Readable.prototype.resume = function() {
  emitDataEvents(this);
  this.read(0);
  this.emit('resume');
};

Readable.prototype.pause = function() {
  emitDataEvents(this, true);
  this.emit('pause');
};

function emitDataEvents(stream, startPaused) {
  var state = stream._readableState;

  if (state.flowing) {
    // https://github.com/isaacs/readable-stream/issues/16
    throw new Error('Cannot switch to old mode now.');
  }

  var paused = startPaused || false;
  var readable = false;

  // convert to an old-style stream.
  stream.readable = true;
  stream.pipe = Stream.prototype.pipe;
  stream.on = stream.addListener = Stream.prototype.on;

  stream.on('readable', function() {
    readable = true;

    var c;
    while (!paused && (null !== (c = stream.read())))
      stream.emit('data', c);

    if (c === null) {
      readable = false;
      stream._readableState.needReadable = true;
    }
  });

  stream.pause = function() {
    paused = true;
    this.emit('pause');
  };

  stream.resume = function() {
    paused = false;
    if (readable)
      setImmediate(function() {
        stream.emit('readable');
      });
    else
      this.read(0);
    this.emit('resume');
  };

  // now make it start, just in case it hadn't already.
  stream.emit('readable');
}

// wrap an old-style stream as the async data source.
// This is *not* part of the readable stream interface.
// It is an ugly unfortunate mess of history.
Readable.prototype.wrap = function(stream) {
  var state = this._readableState;
  var paused = false;

  var self = this;
  stream.on('end', function() {
    if (state.decoder && !state.ended) {
      var chunk = state.decoder.end();
      if (chunk && chunk.length)
        self.push(chunk);
    }

    self.push(null);
  });

  stream.on('data', function(chunk) {
    if (state.decoder)
      chunk = state.decoder.write(chunk);
    if (!chunk || !state.objectMode && !chunk.length)
      return;

    var ret = self.push(chunk);
    if (!ret) {
      paused = true;
      stream.pause();
    }
  });

  // proxy all the other methods.
  // important when wrapping filters and duplexes.
  for (var i in stream) {
    if (typeof stream[i] === 'function' &&
        typeof this[i] === 'undefined') {
      this[i] = function(method) { return function() {
        return stream[method].apply(stream, arguments);
      }}(i);
    }
  }

  // proxy certain important events.
  var events = ['error', 'close', 'destroy', 'pause', 'resume'];
  forEach(events, function(ev) {
    stream.on(ev, function (x) {
      return self.emit.apply(self, ev, x);
    });
  });

  // when we try to consume some more bytes, simply unpause the
  // underlying stream.
  self._read = function(n) {
    if (paused) {
      paused = false;
      stream.resume();
    }
  };

  return self;
};



// exposed for testing purposes only.
Readable._fromList = fromList;

// Pluck off n bytes from an array of buffers.
// Length is the combined lengths of all the buffers in the list.
function fromList(n, state) {
  var list = state.buffer;
  var length = state.length;
  var stringMode = !!state.decoder;
  var objectMode = !!state.objectMode;
  var ret;

  // nothing in the list, definitely empty.
  if (list.length === 0)
    return null;

  if (length === 0)
    ret = null;
  else if (objectMode)
    ret = list.shift();
  else if (!n || n >= length) {
    // read it all, truncate the array.
    if (stringMode)
      ret = list.join('');
    else
      ret = Buffer.concat(list, length);
    list.length = 0;
  } else {
    // read just some of it.
    if (n < list[0].length) {
      // just take a part of the first list item.
      // slice is the same for buffers and strings.
      var buf = list[0];
      ret = buf.slice(0, n);
      list[0] = buf.slice(n);
    } else if (n === list[0].length) {
      // first list is a perfect match
      ret = list.shift();
    } else {
      // complex case.
      // we have enough to cover it, but it spans past the first buffer.
      if (stringMode)
        ret = '';
      else
        ret = new Buffer(n);

      var c = 0;
      for (var i = 0, l = list.length; i < l && c < n; i++) {
        var buf = list[0];
        var cpy = Math.min(n - c, buf.length);

        if (stringMode)
          ret += buf.slice(0, cpy);
        else
          buf.copy(ret, c, 0, cpy);

        if (cpy < buf.length)
          list[0] = buf.slice(cpy);
        else
          list.shift();

        c += cpy;
      }
    }
  }

  return ret;
}

function endReadable(stream) {
  var state = stream._readableState;

  // If we get here before consuming all the bytes, then that is a
  // bug in node.  Should never happen.
  if (state.length > 0)
    throw new Error('endReadable called on non-empty stream');

  if (!state.endEmitted && state.calledRead) {
    state.ended = true;
    setImmediate(function() {
      // Check that we didn't get one last unshift.
      if (!state.endEmitted && state.length === 0) {
        state.endEmitted = true;
        stream.readable = false;
        stream.emit('end');
      }
    });
  }
}

function forEach (xs, f) {
  for (var i = 0, l = xs.length; i < l; i++) {
    f(xs[i], i);
  }
}

function indexOf (xs, x) {
  for (var i = 0, l = xs.length; i < l; i++) {
    if (xs[i] === x) return i;
  }
  return -1;
}

}).call(this,_dereq_("JkpR2F"))
},{"./index.js":23,"JkpR2F":21,"buffer":16,"events":19,"inherits":20,"process/browser.js":24,"string_decoder":29}],27:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// a transform stream is a readable/writable stream where you do
// something with the data.  Sometimes it's called a "filter",
// but that's not a great name for it, since that implies a thing where
// some bits pass through, and others are simply ignored.  (That would
// be a valid example of a transform, of course.)
//
// While the output is causally related to the input, it's not a
// necessarily symmetric or synchronous transformation.  For example,
// a zlib stream might take multiple plain-text writes(), and then
// emit a single compressed chunk some time in the future.
//
// Here's how this works:
//
// The Transform stream has all the aspects of the readable and writable
// stream classes.  When you write(chunk), that calls _write(chunk,cb)
// internally, and returns false if there's a lot of pending writes
// buffered up.  When you call read(), that calls _read(n) until
// there's enough pending readable data buffered up.
//
// In a transform stream, the written data is placed in a buffer.  When
// _read(n) is called, it transforms the queued up data, calling the
// buffered _write cb's as it consumes chunks.  If consuming a single
// written chunk would result in multiple output chunks, then the first
// outputted bit calls the readcb, and subsequent chunks just go into
// the read buffer, and will cause it to emit 'readable' if necessary.
//
// This way, back-pressure is actually determined by the reading side,
// since _read has to be called to start processing a new chunk.  However,
// a pathological inflate type of transform can cause excessive buffering
// here.  For example, imagine a stream where every byte of input is
// interpreted as an integer from 0-255, and then results in that many
// bytes of output.  Writing the 4 bytes {ff,ff,ff,ff} would result in
// 1kb of data being output.  In this case, you could write a very small
// amount of input, and end up with a very large amount of output.  In
// such a pathological inflating mechanism, there'd be no way to tell
// the system to stop doing the transform.  A single 4MB write could
// cause the system to run out of memory.
//
// However, even in such a pathological case, only a single written chunk
// would be consumed, and then the rest would wait (un-transformed) until
// the results of the previous transformed chunk were consumed.

module.exports = Transform;

var Duplex = _dereq_('./duplex.js');
var inherits = _dereq_('inherits');
inherits(Transform, Duplex);


function TransformState(options, stream) {
  this.afterTransform = function(er, data) {
    return afterTransform(stream, er, data);
  };

  this.needTransform = false;
  this.transforming = false;
  this.writecb = null;
  this.writechunk = null;
}

function afterTransform(stream, er, data) {
  var ts = stream._transformState;
  ts.transforming = false;

  var cb = ts.writecb;

  if (!cb)
    return stream.emit('error', new Error('no writecb in Transform class'));

  ts.writechunk = null;
  ts.writecb = null;

  if (data !== null && data !== undefined)
    stream.push(data);

  if (cb)
    cb(er);

  var rs = stream._readableState;
  rs.reading = false;
  if (rs.needReadable || rs.length < rs.highWaterMark) {
    stream._read(rs.highWaterMark);
  }
}


function Transform(options) {
  if (!(this instanceof Transform))
    return new Transform(options);

  Duplex.call(this, options);

  var ts = this._transformState = new TransformState(options, this);

  // when the writable side finishes, then flush out anything remaining.
  var stream = this;

  // start out asking for a readable event once data is transformed.
  this._readableState.needReadable = true;

  // we have implemented the _read method, and done the other things
  // that Readable wants before the first _read call, so unset the
  // sync guard flag.
  this._readableState.sync = false;

  this.once('finish', function() {
    if ('function' === typeof this._flush)
      this._flush(function(er) {
        done(stream, er);
      });
    else
      done(stream);
  });
}

Transform.prototype.push = function(chunk, encoding) {
  this._transformState.needTransform = false;
  return Duplex.prototype.push.call(this, chunk, encoding);
};

// This is the part where you do stuff!
// override this function in implementation classes.
// 'chunk' is an input chunk.
//
// Call `push(newChunk)` to pass along transformed output
// to the readable side.  You may call 'push' zero or more times.
//
// Call `cb(err)` when you are done with this chunk.  If you pass
// an error, then that'll put the hurt on the whole operation.  If you
// never call cb(), then you'll never get another chunk.
Transform.prototype._transform = function(chunk, encoding, cb) {
  throw new Error('not implemented');
};

Transform.prototype._write = function(chunk, encoding, cb) {
  var ts = this._transformState;
  ts.writecb = cb;
  ts.writechunk = chunk;
  ts.writeencoding = encoding;
  if (!ts.transforming) {
    var rs = this._readableState;
    if (ts.needTransform ||
        rs.needReadable ||
        rs.length < rs.highWaterMark)
      this._read(rs.highWaterMark);
  }
};

// Doesn't matter what the args are here.
// _transform does all the work.
// That we got here means that the readable side wants more data.
Transform.prototype._read = function(n) {
  var ts = this._transformState;

  if (ts.writechunk && ts.writecb && !ts.transforming) {
    ts.transforming = true;
    this._transform(ts.writechunk, ts.writeencoding, ts.afterTransform);
  } else {
    // mark that we need a transform, so that any data that comes in
    // will get processed, now that we've asked for it.
    ts.needTransform = true;
  }
};


function done(stream, er) {
  if (er)
    return stream.emit('error', er);

  // if there's nothing in the write buffer, then that means
  // that nothing more will ever be provided
  var ws = stream._writableState;
  var rs = stream._readableState;
  var ts = stream._transformState;

  if (ws.length)
    throw new Error('calling transform done when ws.length != 0');

  if (ts.transforming)
    throw new Error('calling transform done when still transforming');

  return stream.push(null);
}

},{"./duplex.js":22,"inherits":20}],28:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// A bit simpler than readable streams.
// Implement an async ._write(chunk, cb), and it'll handle all
// the drain event emission and buffering.

module.exports = Writable;
Writable.WritableState = WritableState;

var isUint8Array = typeof Uint8Array !== 'undefined'
  ? function (x) { return x instanceof Uint8Array }
  : function (x) {
    return x && x.constructor && x.constructor.name === 'Uint8Array'
  }
;
var isArrayBuffer = typeof ArrayBuffer !== 'undefined'
  ? function (x) { return x instanceof ArrayBuffer }
  : function (x) {
    return x && x.constructor && x.constructor.name === 'ArrayBuffer'
  }
;

var inherits = _dereq_('inherits');
var Stream = _dereq_('./index.js');
var setImmediate = _dereq_('process/browser.js').nextTick;
var Buffer = _dereq_('buffer').Buffer;

inherits(Writable, Stream);

function WriteReq(chunk, encoding, cb) {
  this.chunk = chunk;
  this.encoding = encoding;
  this.callback = cb;
}

function WritableState(options, stream) {
  options = options || {};

  // the point at which write() starts returning false
  // Note: 0 is a valid value, means that we always return false if
  // the entire buffer is not flushed immediately on write()
  var hwm = options.highWaterMark;
  this.highWaterMark = (hwm || hwm === 0) ? hwm : 16 * 1024;

  // object stream flag to indicate whether or not this stream
  // contains buffers or objects.
  this.objectMode = !!options.objectMode;

  // cast to ints.
  this.highWaterMark = ~~this.highWaterMark;

  this.needDrain = false;
  // at the start of calling end()
  this.ending = false;
  // when end() has been called, and returned
  this.ended = false;
  // when 'finish' is emitted
  this.finished = false;

  // should we decode strings into buffers before passing to _write?
  // this is here so that some node-core streams can optimize string
  // handling at a lower level.
  var noDecode = options.decodeStrings === false;
  this.decodeStrings = !noDecode;

  // Crypto is kind of old and crusty.  Historically, its default string
  // encoding is 'binary' so we have to make this configurable.
  // Everything else in the universe uses 'utf8', though.
  this.defaultEncoding = options.defaultEncoding || 'utf8';

  // not an actual buffer we keep track of, but a measurement
  // of how much we're waiting to get pushed to some underlying
  // socket or file.
  this.length = 0;

  // a flag to see when we're in the middle of a write.
  this.writing = false;

  // a flag to be able to tell if the onwrite cb is called immediately,
  // or on a later tick.  We set this to true at first, becuase any
  // actions that shouldn't happen until "later" should generally also
  // not happen before the first write call.
  this.sync = true;

  // a flag to know if we're processing previously buffered items, which
  // may call the _write() callback in the same tick, so that we don't
  // end up in an overlapped onwrite situation.
  this.bufferProcessing = false;

  // the callback that's passed to _write(chunk,cb)
  this.onwrite = function(er) {
    onwrite(stream, er);
  };

  // the callback that the user supplies to write(chunk,encoding,cb)
  this.writecb = null;

  // the amount that is being written when _write is called.
  this.writelen = 0;

  this.buffer = [];
}

function Writable(options) {
  // Writable ctor is applied to Duplexes, though they're not
  // instanceof Writable, they're instanceof Readable.
  if (!(this instanceof Writable) && !(this instanceof Stream.Duplex))
    return new Writable(options);

  this._writableState = new WritableState(options, this);

  // legacy.
  this.writable = true;

  Stream.call(this);
}

// Otherwise people can pipe Writable streams, which is just wrong.
Writable.prototype.pipe = function() {
  this.emit('error', new Error('Cannot pipe. Not readable.'));
};


function writeAfterEnd(stream, state, cb) {
  var er = new Error('write after end');
  // TODO: defer error events consistently everywhere, not just the cb
  stream.emit('error', er);
  setImmediate(function() {
    cb(er);
  });
}

// If we get something that is not a buffer, string, null, or undefined,
// and we're not in objectMode, then that's an error.
// Otherwise stream chunks are all considered to be of length=1, and the
// watermarks determine how many objects to keep in the buffer, rather than
// how many bytes or characters.
function validChunk(stream, state, chunk, cb) {
  var valid = true;
  if (!Buffer.isBuffer(chunk) &&
      'string' !== typeof chunk &&
      chunk !== null &&
      chunk !== undefined &&
      !state.objectMode) {
    var er = new TypeError('Invalid non-string/buffer chunk');
    stream.emit('error', er);
    setImmediate(function() {
      cb(er);
    });
    valid = false;
  }
  return valid;
}

Writable.prototype.write = function(chunk, encoding, cb) {
  var state = this._writableState;
  var ret = false;

  if (typeof encoding === 'function') {
    cb = encoding;
    encoding = null;
  }

  if (!Buffer.isBuffer(chunk) && isUint8Array(chunk))
    chunk = new Buffer(chunk);
  if (isArrayBuffer(chunk) && typeof Uint8Array !== 'undefined')
    chunk = new Buffer(new Uint8Array(chunk));
  
  if (Buffer.isBuffer(chunk))
    encoding = 'buffer';
  else if (!encoding)
    encoding = state.defaultEncoding;

  if (typeof cb !== 'function')
    cb = function() {};

  if (state.ended)
    writeAfterEnd(this, state, cb);
  else if (validChunk(this, state, chunk, cb))
    ret = writeOrBuffer(this, state, chunk, encoding, cb);

  return ret;
};

function decodeChunk(state, chunk, encoding) {
  if (!state.objectMode &&
      state.decodeStrings !== false &&
      typeof chunk === 'string') {
    chunk = new Buffer(chunk, encoding);
  }
  return chunk;
}

// if we're already writing something, then just put this
// in the queue, and wait our turn.  Otherwise, call _write
// If we return false, then we need a drain event, so set that flag.
function writeOrBuffer(stream, state, chunk, encoding, cb) {
  chunk = decodeChunk(state, chunk, encoding);
  var len = state.objectMode ? 1 : chunk.length;

  state.length += len;

  var ret = state.length < state.highWaterMark;
  state.needDrain = !ret;

  if (state.writing)
    state.buffer.push(new WriteReq(chunk, encoding, cb));
  else
    doWrite(stream, state, len, chunk, encoding, cb);

  return ret;
}

function doWrite(stream, state, len, chunk, encoding, cb) {
  state.writelen = len;
  state.writecb = cb;
  state.writing = true;
  state.sync = true;
  stream._write(chunk, encoding, state.onwrite);
  state.sync = false;
}

function onwriteError(stream, state, sync, er, cb) {
  if (sync)
    setImmediate(function() {
      cb(er);
    });
  else
    cb(er);

  stream.emit('error', er);
}

function onwriteStateUpdate(state) {
  state.writing = false;
  state.writecb = null;
  state.length -= state.writelen;
  state.writelen = 0;
}

function onwrite(stream, er) {
  var state = stream._writableState;
  var sync = state.sync;
  var cb = state.writecb;

  onwriteStateUpdate(state);

  if (er)
    onwriteError(stream, state, sync, er, cb);
  else {
    // Check if we're actually ready to finish, but don't emit yet
    var finished = needFinish(stream, state);

    if (!finished && !state.bufferProcessing && state.buffer.length)
      clearBuffer(stream, state);

    if (sync) {
      setImmediate(function() {
        afterWrite(stream, state, finished, cb);
      });
    } else {
      afterWrite(stream, state, finished, cb);
    }
  }
}

function afterWrite(stream, state, finished, cb) {
  if (!finished)
    onwriteDrain(stream, state);
  cb();
  if (finished)
    finishMaybe(stream, state);
}

// Must force callback to be called on nextTick, so that we don't
// emit 'drain' before the write() consumer gets the 'false' return
// value, and has a chance to attach a 'drain' listener.
function onwriteDrain(stream, state) {
  if (state.length === 0 && state.needDrain) {
    state.needDrain = false;
    stream.emit('drain');
  }
}


// if there's something in the buffer waiting, then process it
function clearBuffer(stream, state) {
  state.bufferProcessing = true;

  for (var c = 0; c < state.buffer.length; c++) {
    var entry = state.buffer[c];
    var chunk = entry.chunk;
    var encoding = entry.encoding;
    var cb = entry.callback;
    var len = state.objectMode ? 1 : chunk.length;

    doWrite(stream, state, len, chunk, encoding, cb);

    // if we didn't call the onwrite immediately, then
    // it means that we need to wait until it does.
    // also, that means that the chunk and cb are currently
    // being processed, so move the buffer counter past them.
    if (state.writing) {
      c++;
      break;
    }
  }

  state.bufferProcessing = false;
  if (c < state.buffer.length)
    state.buffer = state.buffer.slice(c);
  else
    state.buffer.length = 0;
}

Writable.prototype._write = function(chunk, encoding, cb) {
  cb(new Error('not implemented'));
};

Writable.prototype.end = function(chunk, encoding, cb) {
  var state = this._writableState;

  if (typeof chunk === 'function') {
    cb = chunk;
    chunk = null;
    encoding = null;
  } else if (typeof encoding === 'function') {
    cb = encoding;
    encoding = null;
  }

  if (typeof chunk !== 'undefined' && chunk !== null)
    this.write(chunk, encoding);

  // ignore unnecessary end() calls.
  if (!state.ending && !state.finished)
    endWritable(this, state, cb);
};


function needFinish(stream, state) {
  return (state.ending &&
          state.length === 0 &&
          !state.finished &&
          !state.writing);
}

function finishMaybe(stream, state) {
  var need = needFinish(stream, state);
  if (need) {
    state.finished = true;
    stream.emit('finish');
  }
  return need;
}

function endWritable(stream, state, cb) {
  state.ending = true;
  finishMaybe(stream, state);
  if (cb) {
    if (state.finished)
      setImmediate(cb);
    else
      stream.once('finish', cb);
  }
  state.ended = true;
}

},{"./index.js":23,"buffer":16,"inherits":20,"process/browser.js":24}],29:[function(_dereq_,module,exports){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var Buffer = _dereq_('buffer').Buffer;

function assertEncoding(encoding) {
  if (encoding && !Buffer.isEncoding(encoding)) {
    throw new Error('Unknown encoding: ' + encoding);
  }
}

var StringDecoder = exports.StringDecoder = function(encoding) {
  this.encoding = (encoding || 'utf8').toLowerCase().replace(/[-_]/, '');
  assertEncoding(encoding);
  switch (this.encoding) {
    case 'utf8':
      // CESU-8 represents each of Surrogate Pair by 3-bytes
      this.surrogateSize = 3;
      break;
    case 'ucs2':
    case 'utf16le':
      // UTF-16 represents each of Surrogate Pair by 2-bytes
      this.surrogateSize = 2;
      this.detectIncompleteChar = utf16DetectIncompleteChar;
      break;
    case 'base64':
      // Base-64 stores 3 bytes in 4 chars, and pads the remainder.
      this.surrogateSize = 3;
      this.detectIncompleteChar = base64DetectIncompleteChar;
      break;
    default:
      this.write = passThroughWrite;
      return;
  }

  this.charBuffer = new Buffer(6);
  this.charReceived = 0;
  this.charLength = 0;
};


StringDecoder.prototype.write = function(buffer) {
  var charStr = '';
  var offset = 0;

  // if our last write ended with an incomplete multibyte character
  while (this.charLength) {
    // determine how many remaining bytes this buffer has to offer for this char
    var i = (buffer.length >= this.charLength - this.charReceived) ?
                this.charLength - this.charReceived :
                buffer.length;

    // add the new bytes to the char buffer
    buffer.copy(this.charBuffer, this.charReceived, offset, i);
    this.charReceived += (i - offset);
    offset = i;

    if (this.charReceived < this.charLength) {
      // still not enough chars in this buffer? wait for more ...
      return '';
    }

    // get the character that was split
    charStr = this.charBuffer.slice(0, this.charLength).toString(this.encoding);

    // lead surrogate (D800-DBFF) is also the incomplete character
    var charCode = charStr.charCodeAt(charStr.length - 1);
    if (charCode >= 0xD800 && charCode <= 0xDBFF) {
      this.charLength += this.surrogateSize;
      charStr = '';
      continue;
    }
    this.charReceived = this.charLength = 0;

    // if there are no more bytes in this buffer, just emit our char
    if (i == buffer.length) return charStr;

    // otherwise cut off the characters end from the beginning of this buffer
    buffer = buffer.slice(i, buffer.length);
    break;
  }

  var lenIncomplete = this.detectIncompleteChar(buffer);

  var end = buffer.length;
  if (this.charLength) {
    // buffer the incomplete character bytes we got
    buffer.copy(this.charBuffer, 0, buffer.length - lenIncomplete, end);
    this.charReceived = lenIncomplete;
    end -= lenIncomplete;
  }

  charStr += buffer.toString(this.encoding, 0, end);

  var end = charStr.length - 1;
  var charCode = charStr.charCodeAt(end);
  // lead surrogate (D800-DBFF) is also the incomplete character
  if (charCode >= 0xD800 && charCode <= 0xDBFF) {
    var size = this.surrogateSize;
    this.charLength += size;
    this.charReceived += size;
    this.charBuffer.copy(this.charBuffer, size, 0, size);
    this.charBuffer.write(charStr.charAt(charStr.length - 1), this.encoding);
    return charStr.substring(0, end);
  }

  // or just emit the charStr
  return charStr;
};

StringDecoder.prototype.detectIncompleteChar = function(buffer) {
  // determine how many bytes we have to check at the end of this buffer
  var i = (buffer.length >= 3) ? 3 : buffer.length;

  // Figure out if one of the last i bytes of our buffer announces an
  // incomplete char.
  for (; i > 0; i--) {
    var c = buffer[buffer.length - i];

    // See http://en.wikipedia.org/wiki/UTF-8#Description

    // 110XXXXX
    if (i == 1 && c >> 5 == 0x06) {
      this.charLength = 2;
      break;
    }

    // 1110XXXX
    if (i <= 2 && c >> 4 == 0x0E) {
      this.charLength = 3;
      break;
    }

    // 11110XXX
    if (i <= 3 && c >> 3 == 0x1E) {
      this.charLength = 4;
      break;
    }
  }

  return i;
};

StringDecoder.prototype.end = function(buffer) {
  var res = '';
  if (buffer && buffer.length)
    res = this.write(buffer);

  if (this.charReceived) {
    var cr = this.charReceived;
    var buf = this.charBuffer;
    var enc = this.encoding;
    res += buf.slice(0, cr).toString(enc);
  }

  return res;
};

function passThroughWrite(buffer) {
  return buffer.toString(this.encoding);
}

function utf16DetectIncompleteChar(buffer) {
  var incomplete = this.charReceived = buffer.length % 2;
  this.charLength = incomplete ? 2 : 0;
  return incomplete;
}

function base64DetectIncompleteChar(buffer) {
  var incomplete = this.charReceived = buffer.length % 3;
  this.charLength = incomplete ? 3 : 0;
  return incomplete;
}

},{"buffer":16}],30:[function(_dereq_,module,exports){
module.exports = function isBuffer(arg) {
  return arg && typeof arg === 'object'
    && typeof arg.copy === 'function'
    && typeof arg.fill === 'function'
    && typeof arg.readUInt8 === 'function';
}
},{}],31:[function(_dereq_,module,exports){
(function (process,global){
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var formatRegExp = /%[sdj%]/g;
exports.format = function(f) {
  if (!isString(f)) {
    var objects = [];
    for (var i = 0; i < arguments.length; i++) {
      objects.push(inspect(arguments[i]));
    }
    return objects.join(' ');
  }

  var i = 1;
  var args = arguments;
  var len = args.length;
  var str = String(f).replace(formatRegExp, function(x) {
    if (x === '%%') return '%';
    if (i >= len) return x;
    switch (x) {
      case '%s': return String(args[i++]);
      case '%d': return Number(args[i++]);
      case '%j':
        try {
          return JSON.stringify(args[i++]);
        } catch (_) {
          return '[Circular]';
        }
      default:
        return x;
    }
  });
  for (var x = args[i]; i < len; x = args[++i]) {
    if (isNull(x) || !isObject(x)) {
      str += ' ' + x;
    } else {
      str += ' ' + inspect(x);
    }
  }
  return str;
};


// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
exports.deprecate = function(fn, msg) {
  // Allow for deprecating things in the process of starting up.
  if (isUndefined(global.process)) {
    return function() {
      return exports.deprecate(fn, msg).apply(this, arguments);
    };
  }

  if (process.noDeprecation === true) {
    return fn;
  }

  var warned = false;
  function deprecated() {
    if (!warned) {
      if (process.throwDeprecation) {
        throw new Error(msg);
      } else if (process.traceDeprecation) {
        console.trace(msg);
      } else {
        console.error(msg);
      }
      warned = true;
    }
    return fn.apply(this, arguments);
  }

  return deprecated;
};


var debugs = {};
var debugEnviron;
exports.debuglog = function(set) {
  if (isUndefined(debugEnviron))
    debugEnviron = process.env.NODE_DEBUG || '';
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (new RegExp('\\b' + set + '\\b', 'i').test(debugEnviron)) {
      var pid = process.pid;
      debugs[set] = function() {
        var msg = exports.format.apply(exports, arguments);
        console.error('%s %d: %s', set, pid, msg);
      };
    } else {
      debugs[set] = function() {};
    }
  }
  return debugs[set];
};


/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 *
 * @param {Object} obj The object to print out.
 * @param {Object} opts Optional options object that alters the output.
 */
/* legacy: obj, showHidden, depth, colors*/
function inspect(obj, opts) {
  // default options
  var ctx = {
    seen: [],
    stylize: stylizeNoColor
  };
  // legacy...
  if (arguments.length >= 3) ctx.depth = arguments[2];
  if (arguments.length >= 4) ctx.colors = arguments[3];
  if (isBoolean(opts)) {
    // legacy...
    ctx.showHidden = opts;
  } else if (opts) {
    // got an "options" object
    exports._extend(ctx, opts);
  }
  // set default options
  if (isUndefined(ctx.showHidden)) ctx.showHidden = false;
  if (isUndefined(ctx.depth)) ctx.depth = 2;
  if (isUndefined(ctx.colors)) ctx.colors = false;
  if (isUndefined(ctx.customInspect)) ctx.customInspect = true;
  if (ctx.colors) ctx.stylize = stylizeWithColor;
  return formatValue(ctx, obj, ctx.depth);
}
exports.inspect = inspect;


// http://en.wikipedia.org/wiki/ANSI_escape_code#graphics
inspect.colors = {
  'bold' : [1, 22],
  'italic' : [3, 23],
  'underline' : [4, 24],
  'inverse' : [7, 27],
  'white' : [37, 39],
  'grey' : [90, 39],
  'black' : [30, 39],
  'blue' : [34, 39],
  'cyan' : [36, 39],
  'green' : [32, 39],
  'magenta' : [35, 39],
  'red' : [31, 39],
  'yellow' : [33, 39]
};

// Don't use 'blue' not visible on cmd.exe
inspect.styles = {
  'special': 'cyan',
  'number': 'yellow',
  'boolean': 'yellow',
  'undefined': 'grey',
  'null': 'bold',
  'string': 'green',
  'date': 'magenta',
  // "name": intentionally not styling
  'regexp': 'red'
};


function stylizeWithColor(str, styleType) {
  var style = inspect.styles[styleType];

  if (style) {
    return '\u001b[' + inspect.colors[style][0] + 'm' + str +
           '\u001b[' + inspect.colors[style][1] + 'm';
  } else {
    return str;
  }
}


function stylizeNoColor(str, styleType) {
  return str;
}


function arrayToHash(array) {
  var hash = {};

  array.forEach(function(val, idx) {
    hash[val] = true;
  });

  return hash;
}


function formatValue(ctx, value, recurseTimes) {
  // Provide a hook for user-specified inspect functions.
  // Check that value is an object with an inspect function on it
  if (ctx.customInspect &&
      value &&
      isFunction(value.inspect) &&
      // Filter out the util module, it's inspect function is special
      value.inspect !== exports.inspect &&
      // Also filter out any prototype objects using the circular check.
      !(value.constructor && value.constructor.prototype === value)) {
    var ret = value.inspect(recurseTimes, ctx);
    if (!isString(ret)) {
      ret = formatValue(ctx, ret, recurseTimes);
    }
    return ret;
  }

  // Primitive types cannot have properties
  var primitive = formatPrimitive(ctx, value);
  if (primitive) {
    return primitive;
  }

  // Look up the keys of the object.
  var keys = Object.keys(value);
  var visibleKeys = arrayToHash(keys);

  if (ctx.showHidden) {
    keys = Object.getOwnPropertyNames(value);
  }

  // IE doesn't make error fields non-enumerable
  // http://msdn.microsoft.com/en-us/library/ie/dww52sbt(v=vs.94).aspx
  if (isError(value)
      && (keys.indexOf('message') >= 0 || keys.indexOf('description') >= 0)) {
    return formatError(value);
  }

  // Some type of object without properties can be shortcutted.
  if (keys.length === 0) {
    if (isFunction(value)) {
      var name = value.name ? ': ' + value.name : '';
      return ctx.stylize('[Function' + name + ']', 'special');
    }
    if (isRegExp(value)) {
      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
    }
    if (isDate(value)) {
      return ctx.stylize(Date.prototype.toString.call(value), 'date');
    }
    if (isError(value)) {
      return formatError(value);
    }
  }

  var base = '', array = false, braces = ['{', '}'];

  // Make Array say that they are Array
  if (isArray(value)) {
    array = true;
    braces = ['[', ']'];
  }

  // Make functions say that they are functions
  if (isFunction(value)) {
    var n = value.name ? ': ' + value.name : '';
    base = ' [Function' + n + ']';
  }

  // Make RegExps say that they are RegExps
  if (isRegExp(value)) {
    base = ' ' + RegExp.prototype.toString.call(value);
  }

  // Make dates with properties first say the date
  if (isDate(value)) {
    base = ' ' + Date.prototype.toUTCString.call(value);
  }

  // Make error with message first say the error
  if (isError(value)) {
    base = ' ' + formatError(value);
  }

  if (keys.length === 0 && (!array || value.length == 0)) {
    return braces[0] + base + braces[1];
  }

  if (recurseTimes < 0) {
    if (isRegExp(value)) {
      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
    } else {
      return ctx.stylize('[Object]', 'special');
    }
  }

  ctx.seen.push(value);

  var output;
  if (array) {
    output = formatArray(ctx, value, recurseTimes, visibleKeys, keys);
  } else {
    output = keys.map(function(key) {
      return formatProperty(ctx, value, recurseTimes, visibleKeys, key, array);
    });
  }

  ctx.seen.pop();

  return reduceToSingleString(output, base, braces);
}


function formatPrimitive(ctx, value) {
  if (isUndefined(value))
    return ctx.stylize('undefined', 'undefined');
  if (isString(value)) {
    var simple = '\'' + JSON.stringify(value).replace(/^"|"$/g, '')
                                             .replace(/'/g, "\\'")
                                             .replace(/\\"/g, '"') + '\'';
    return ctx.stylize(simple, 'string');
  }
  if (isNumber(value))
    return ctx.stylize('' + value, 'number');
  if (isBoolean(value))
    return ctx.stylize('' + value, 'boolean');
  // For some reason typeof null is "object", so special case here.
  if (isNull(value))
    return ctx.stylize('null', 'null');
}


function formatError(value) {
  return '[' + Error.prototype.toString.call(value) + ']';
}


function formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
  var output = [];
  for (var i = 0, l = value.length; i < l; ++i) {
    if (hasOwnProperty(value, String(i))) {
      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
          String(i), true));
    } else {
      output.push('');
    }
  }
  keys.forEach(function(key) {
    if (!key.match(/^\d+$/)) {
      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
          key, true));
    }
  });
  return output;
}


function formatProperty(ctx, value, recurseTimes, visibleKeys, key, array) {
  var name, str, desc;
  desc = Object.getOwnPropertyDescriptor(value, key) || { value: value[key] };
  if (desc.get) {
    if (desc.set) {
      str = ctx.stylize('[Getter/Setter]', 'special');
    } else {
      str = ctx.stylize('[Getter]', 'special');
    }
  } else {
    if (desc.set) {
      str = ctx.stylize('[Setter]', 'special');
    }
  }
  if (!hasOwnProperty(visibleKeys, key)) {
    name = '[' + key + ']';
  }
  if (!str) {
    if (ctx.seen.indexOf(desc.value) < 0) {
      if (isNull(recurseTimes)) {
        str = formatValue(ctx, desc.value, null);
      } else {
        str = formatValue(ctx, desc.value, recurseTimes - 1);
      }
      if (str.indexOf('\n') > -1) {
        if (array) {
          str = str.split('\n').map(function(line) {
            return '  ' + line;
          }).join('\n').substr(2);
        } else {
          str = '\n' + str.split('\n').map(function(line) {
            return '   ' + line;
          }).join('\n');
        }
      }
    } else {
      str = ctx.stylize('[Circular]', 'special');
    }
  }
  if (isUndefined(name)) {
    if (array && key.match(/^\d+$/)) {
      return str;
    }
    name = JSON.stringify('' + key);
    if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
      name = name.substr(1, name.length - 2);
      name = ctx.stylize(name, 'name');
    } else {
      name = name.replace(/'/g, "\\'")
                 .replace(/\\"/g, '"')
                 .replace(/(^"|"$)/g, "'");
      name = ctx.stylize(name, 'string');
    }
  }

  return name + ': ' + str;
}


function reduceToSingleString(output, base, braces) {
  var numLinesEst = 0;
  var length = output.reduce(function(prev, cur) {
    numLinesEst++;
    if (cur.indexOf('\n') >= 0) numLinesEst++;
    return prev + cur.replace(/\u001b\[\d\d?m/g, '').length + 1;
  }, 0);

  if (length > 60) {
    return braces[0] +
           (base === '' ? '' : base + '\n ') +
           ' ' +
           output.join(',\n  ') +
           ' ' +
           braces[1];
  }

  return braces[0] + base + ' ' + output.join(', ') + ' ' + braces[1];
}


// NOTE: These type checking functions intentionally don't use `instanceof`
// because it is fragile and can be easily faked with `Object.create()`.
function isArray(ar) {
  return Array.isArray(ar);
}
exports.isArray = isArray;

function isBoolean(arg) {
  return typeof arg === 'boolean';
}
exports.isBoolean = isBoolean;

function isNull(arg) {
  return arg === null;
}
exports.isNull = isNull;

function isNullOrUndefined(arg) {
  return arg == null;
}
exports.isNullOrUndefined = isNullOrUndefined;

function isNumber(arg) {
  return typeof arg === 'number';
}
exports.isNumber = isNumber;

function isString(arg) {
  return typeof arg === 'string';
}
exports.isString = isString;

function isSymbol(arg) {
  return typeof arg === 'symbol';
}
exports.isSymbol = isSymbol;

function isUndefined(arg) {
  return arg === void 0;
}
exports.isUndefined = isUndefined;

function isRegExp(re) {
  return isObject(re) && objectToString(re) === '[object RegExp]';
}
exports.isRegExp = isRegExp;

function isObject(arg) {
  return typeof arg === 'object' && arg !== null;
}
exports.isObject = isObject;

function isDate(d) {
  return isObject(d) && objectToString(d) === '[object Date]';
}
exports.isDate = isDate;

function isError(e) {
  return isObject(e) &&
      (objectToString(e) === '[object Error]' || e instanceof Error);
}
exports.isError = isError;

function isFunction(arg) {
  return typeof arg === 'function';
}
exports.isFunction = isFunction;

function isPrimitive(arg) {
  return arg === null ||
         typeof arg === 'boolean' ||
         typeof arg === 'number' ||
         typeof arg === 'string' ||
         typeof arg === 'symbol' ||  // ES6 symbol
         typeof arg === 'undefined';
}
exports.isPrimitive = isPrimitive;

exports.isBuffer = _dereq_('./support/isBuffer');

function objectToString(o) {
  return Object.prototype.toString.call(o);
}


function pad(n) {
  return n < 10 ? '0' + n.toString(10) : n.toString(10);
}


var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
              'Oct', 'Nov', 'Dec'];

// 26 Feb 16:19:34
function timestamp() {
  var d = new Date();
  var time = [pad(d.getHours()),
              pad(d.getMinutes()),
              pad(d.getSeconds())].join(':');
  return [d.getDate(), months[d.getMonth()], time].join(' ');
}


// log is just a thin wrapper to console.log that prepends a timestamp
exports.log = function() {
  console.log('%s - %s', timestamp(), exports.format.apply(exports, arguments));
};


/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be rewritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype.
 * @param {function} superCtor Constructor function to inherit prototype from.
 */
exports.inherits = _dereq_('inherits');

exports._extend = function(origin, add) {
  // Don't do anything if add isn't an object
  if (!add || !isObject(add)) return origin;

  var keys = Object.keys(add);
  var i = keys.length;
  while (i--) {
    origin[keys[i]] = add[keys[i]];
  }
  return origin;
};

function hasOwnProperty(obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop);
}

}).call(this,_dereq_("JkpR2F"),typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
},{"./support/isBuffer":30,"JkpR2F":21,"inherits":20}],32:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data;

  Data = (function() {
    function Data(data) {
      this.data = data != null ? data : [];
      this.pos = 0;
      this.length = this.data.length;
    }

    Data.prototype.readByte = function() {
      return this.data[this.pos++];
    };

    Data.prototype.writeByte = function(byte) {
      return this.data[this.pos++] = byte;
    };

    Data.prototype.byteAt = function(index) {
      return this.data[index];
    };

    Data.prototype.readBool = function() {
      return !!this.readByte();
    };

    Data.prototype.writeBool = function(val) {
      return this.writeByte(val ? 1 : 0);
    };

    Data.prototype.readUInt32 = function() {
      var b1, b2, b3, b4;
      b1 = this.readByte() * 0x1000000;
      b2 = this.readByte() << 16;
      b3 = this.readByte() << 8;
      b4 = this.readByte();
      return b1 + b2 + b3 + b4;
    };

    Data.prototype.writeUInt32 = function(val) {
      this.writeByte((val >>> 24) & 0xff);
      this.writeByte((val >> 16) & 0xff);
      this.writeByte((val >> 8) & 0xff);
      return this.writeByte(val & 0xff);
    };

    Data.prototype.readInt32 = function() {
      var int;
      int = this.readUInt32();
      if (int >= 0x80000000) {
        return int - 0x100000000;
      } else {
        return int;
      }
    };

    Data.prototype.writeInt32 = function(val) {
      if (val < 0) {
        val += 0x100000000;
      }
      return this.writeUInt32(val);
    };

    Data.prototype.readUInt16 = function() {
      var b1, b2;
      b1 = this.readByte() << 8;
      b2 = this.readByte();
      return b1 | b2;
    };

    Data.prototype.writeUInt16 = function(val) {
      this.writeByte((val >> 8) & 0xff);
      return this.writeByte(val & 0xff);
    };

    Data.prototype.readInt16 = function() {
      var int;
      int = this.readUInt16();
      if (int >= 0x8000) {
        return int - 0x10000;
      } else {
        return int;
      }
    };

    Data.prototype.writeInt16 = function(val) {
      if (val < 0) {
        val += 0x10000;
      }
      return this.writeUInt16(val);
    };

    Data.prototype.readString = function(length) {
      var i, ret, _i;
      ret = [];
      for (i = _i = 0; 0 <= length ? _i < length : _i > length; i = 0 <= length ? ++_i : --_i) {
        ret[i] = String.fromCharCode(this.readByte());
      }
      return ret.join('');
    };

    Data.prototype.writeString = function(val) {
      var i, _i, _ref, _results;
      _results = [];
      for (i = _i = 0, _ref = val.length; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        _results.push(this.writeByte(val.charCodeAt(i)));
      }
      return _results;
    };

    Data.prototype.stringAt = function(pos, length) {
      this.pos = pos;
      return this.readString(length);
    };

    Data.prototype.readShort = function() {
      return this.readInt16();
    };

    Data.prototype.writeShort = function(val) {
      return this.writeInt16(val);
    };

    Data.prototype.readLongLong = function() {
      var b1, b2, b3, b4, b5, b6, b7, b8;
      b1 = this.readByte();
      b2 = this.readByte();
      b3 = this.readByte();
      b4 = this.readByte();
      b5 = this.readByte();
      b6 = this.readByte();
      b7 = this.readByte();
      b8 = this.readByte();
      if (b1 & 0x80) {
        return ((b1 ^ 0xff) * 0x100000000000000 + (b2 ^ 0xff) * 0x1000000000000 + (b3 ^ 0xff) * 0x10000000000 + (b4 ^ 0xff) * 0x100000000 + (b5 ^ 0xff) * 0x1000000 + (b6 ^ 0xff) * 0x10000 + (b7 ^ 0xff) * 0x100 + (b8 ^ 0xff) + 1) * -1;
      }
      return b1 * 0x100000000000000 + b2 * 0x1000000000000 + b3 * 0x10000000000 + b4 * 0x100000000 + b5 * 0x1000000 + b6 * 0x10000 + b7 * 0x100 + b8;
    };

    Data.prototype.writeLongLong = function(val) {
      var high, low;
      high = Math.floor(val / 0x100000000);
      low = val & 0xffffffff;
      this.writeByte((high >> 24) & 0xff);
      this.writeByte((high >> 16) & 0xff);
      this.writeByte((high >> 8) & 0xff);
      this.writeByte(high & 0xff);
      this.writeByte((low >> 24) & 0xff);
      this.writeByte((low >> 16) & 0xff);
      this.writeByte((low >> 8) & 0xff);
      return this.writeByte(low & 0xff);
    };

    Data.prototype.readInt = function() {
      return this.readInt32();
    };

    Data.prototype.writeInt = function(val) {
      return this.writeInt32(val);
    };

    Data.prototype.slice = function(start, end) {
      return this.data.slice(start, end);
    };

    Data.prototype.read = function(bytes) {
      var buf, i, _i;
      buf = [];
      for (i = _i = 0; 0 <= bytes ? _i < bytes : _i > bytes; i = 0 <= bytes ? ++_i : --_i) {
        buf.push(this.readByte());
      }
      return buf;
    };

    Data.prototype.write = function(bytes) {
      var byte, _i, _len, _results;
      _results = [];
      for (_i = 0, _len = bytes.length; _i < _len; _i++) {
        byte = bytes[_i];
        _results.push(this.writeByte(byte));
      }
      return _results;
    };

    return Data;

  })();

  module.exports = Data;

}).call(this);

},{}],33:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1

/*
PDFDocument - represents an entire PDF document
By Devon Govett
 */

(function() {
  var PDFDocument, PDFObject, PDFPage, PDFReference, fs, stream,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  stream = _dereq_('stream');

  fs = _dereq_('fs');

  PDFObject = _dereq_('./object');

  PDFReference = _dereq_('./reference');

  PDFPage = _dereq_('./page');

  PDFDocument = (function(_super) {
    var mixin;

    __extends(PDFDocument, _super);

    function PDFDocument(options) {
      var key, val, _ref, _ref1;
      this.options = options != null ? options : {};
      PDFDocument.__super__.constructor.apply(this, arguments);
      this.version = 1.3;
      this.compress = (_ref = this.options.compress) != null ? _ref : true;
      this._pageBuffer = [];
      this._pageBufferStart = 0;
      this._offsets = [];
      this._waiting = 0;
      this._ended = false;
      this._offset = 0;
      this._root = this.ref({
        Type: 'Catalog',
        Pages: this.ref({
          Type: 'Pages',
          Count: 0,
          Kids: []
        })
      });
      this.page = null;
      this.initColor();
      this.initVector();
      this.initFonts();
      this.initText();
      this.initImages();
      this.info = {
        Producer: 'PDFKit',
        Creator: 'PDFKit',
        CreationDate: new Date()
      };
      if (this.options.info) {
        _ref1 = this.options.info;
        for (key in _ref1) {
          val = _ref1[key];
          this.info[key] = val;
        }
      }
      this._write("%PDF-" + this.version);
      this._write("%\xFF\xFF\xFF\xFF");
      this.addPage();
    }

    mixin = function(methods) {
      var method, name, _results;
      _results = [];
      for (name in methods) {
        method = methods[name];
        _results.push(PDFDocument.prototype[name] = method);
      }
      return _results;
    };

    mixin(_dereq_('./mixins/color'));

    mixin(_dereq_('./mixins/vector'));

    mixin(_dereq_('./mixins/fonts'));

    mixin(_dereq_('./mixins/text'));

    mixin(_dereq_('./mixins/images'));

    mixin(_dereq_('./mixins/annotations'));

    PDFDocument.prototype.addPage = function(options) {
      var pages;
      if (options == null) {
        options = this.options;
      }
      if (!this.options.bufferPages) {
        this.flushPages();
      }
      this.page = new PDFPage(this, options);
      this._pageBuffer.push(this.page);
      pages = this._root.data.Pages.data;
      pages.Kids.push(this.page.dictionary);
      pages.Count++;
      this.x = this.page.margins.left;
      this.y = this.page.margins.top;
      this._ctm = [1, 0, 0, 1, 0, 0];
      this.transform(1, 0, 0, -1, 0, this.page.height);
      return this;
    };

    PDFDocument.prototype.bufferedPageRange = function() {
      return {
        start: this._pageBufferStart,
        count: this._pageBuffer.length
      };
    };

    PDFDocument.prototype.switchToPage = function(n) {
      var page;
      if (!(page = this._pageBuffer[n - this._pageBufferStart])) {
        throw new Error("switchToPage(" + n + ") out of bounds, current buffer covers pages " + this._pageBufferStart + " to " + (this._pageBufferStart + this._pageBuffer.length - 1));
      }
      return this.page = page;
    };

    PDFDocument.prototype.flushPages = function() {
      var page, pages, _i, _len;
      pages = this._pageBuffer;
      this._pageBuffer = [];
      this._pageBufferStart += pages.length;
      for (_i = 0, _len = pages.length; _i < _len; _i++) {
        page = pages[_i];
        page.end();
      }
    };

    PDFDocument.prototype.ref = function(data) {
      var ref;
      ref = new PDFReference(this, this._offsets.length + 1, data);
      this._offsets.push(null);
      this._waiting++;
      return ref;
    };

    PDFDocument.prototype._read = function() {};

    PDFDocument.prototype._write = function(data) {
      if (!Buffer.isBuffer(data)) {
        data = new Buffer(data + '\n', 'binary');
      }
      this.push(data);
      return this._offset += data.length;
    };

    PDFDocument.prototype.addContent = function(data) {
      this.page.write(data);
      return this;
    };

    PDFDocument.prototype._refEnd = function(ref) {
      this._offsets[ref.id - 1] = ref.offset;
      if (--this._waiting === 0 && this._ended) {
        this._finalize();
        return this._ended = false;
      }
    };

    PDFDocument.prototype.write = function(filename, fn) {
      var err;
      err = new Error('PDFDocument#write is deprecated, and will be removed in a future version of PDFKit. Please pipe the document into a Node stream.');
      console.warn(err.stack);
      this.pipe(fs.createWriteStream(filename));
      this.end();
      return this.once('end', fn);
    };

    PDFDocument.prototype.output = function(fn) {
      throw new Error('PDFDocument#output is deprecated, and has been removed from PDFKit. Please pipe the document into a Node stream.');
    };

    PDFDocument.prototype.end = function() {
      var font, key, name, val, _ref, _ref1;
      this.flushPages();
      this._info = this.ref();
      _ref = this.info;
      for (key in _ref) {
        val = _ref[key];
        if (typeof val === 'string') {
          val = PDFObject.s(val, true);
        }
        this._info.data[key] = val;
      }
      this._info.end();
      _ref1 = this._fontFamilies;
      for (name in _ref1) {
        font = _ref1[name];
        font.embed();
      }
      this._root.end();
      this._root.data.Pages.end();
      if (this._waiting === 0) {
        return this._finalize();
      } else {
        return this._ended = true;
      }
    };

    PDFDocument.prototype._finalize = function(fn) {
      var offset, xRefOffset, _i, _len, _ref;
      xRefOffset = this._offset;
      this._write("xref");
      this._write("0 " + (this._offsets.length + 1));
      this._write("0000000000 65535 f ");
      _ref = this._offsets;
      for (_i = 0, _len = _ref.length; _i < _len; _i++) {
        offset = _ref[_i];
        offset = ('0000000000' + offset).slice(-10);
        this._write(offset + ' 00000 n ');
      }
      this._write('trailer');
      this._write(PDFObject.convert({
        Size: this._offsets.length,
        Root: this._root,
        Info: this._info
      }));
      this._write('startxref');
      this._write("" + xRefOffset);
      this._write('%%EOF');
      return this.push(null);
    };

    PDFDocument.prototype.toString = function() {
      return "[object PDFDocument]";
    };

    return PDFDocument;

  })(stream.Readable);

  module.exports = PDFDocument;

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"./mixins/annotations":57,"./mixins/color":58,"./mixins/fonts":59,"./mixins/images":60,"./mixins/text":61,"./mixins/vector":62,"./object":63,"./page":64,"./reference":66,"buffer":16,"fs":"x/K9gc","stream":23}],34:[function(_dereq_,module,exports){
(function (Buffer,__dirname){
// Generated by CoffeeScript 1.7.1

/*
PDFFont - embeds fonts in PDF documents
By Devon Govett
 */

(function() {
  var AFMFont, PDFFont, Subset, TTFFont, fs;

  TTFFont = _dereq_('./font/ttf');

  AFMFont = _dereq_('./font/afm');

  Subset = _dereq_('./font/subset');

  fs = _dereq_('fs');

  PDFFont = (function() {
    var STANDARD_FONTS, toUnicodeCmap;

    function PDFFont(document, src, family, id) {
      this.document = document;
      this.id = id;
      if (typeof src === 'string') {
        if (src in STANDARD_FONTS) {
          this.isAFM = true;
          this.font = new AFMFont(STANDARD_FONTS[src]());
          this.registerAFM(src);
          return;
        } else if (/\.(ttf|ttc)$/i.test(src)) {
          this.font = TTFFont.open(src, family);
        } else if (/\.dfont$/i.test(src)) {
          this.font = TTFFont.fromDFont(src, family);
        } else {
          throw new Error('Not a supported font format or standard PDF font.');
        }
      } else if (Buffer.isBuffer(src)) {
        this.font = TTFFont.fromBuffer(src, family);
      } else if (src instanceof Uint8Array) {
        this.font = TTFFont.fromBuffer(new Buffer(src), family);
      } else if (src instanceof ArrayBuffer) {
        this.font = TTFFont.fromBuffer(new Buffer(new Uint8Array(src)), family);
      } else {
        throw new Error('Not a supported font format or standard PDF font.');
      }
      this.subset = new Subset(this.font);
      this.registerTTF();
    }

    STANDARD_FONTS = {
      "Courier": function() {
        return fs.readFileSync(__dirname + "/font/data/Courier.afm", 'utf8');
      },
      "Courier-Bold": function() {
        return fs.readFileSync(__dirname + "/font/data/Courier-Bold.afm", 'utf8');
      },
      "Courier-Oblique": function() {
        return fs.readFileSync(__dirname + "/font/data/Courier-Oblique.afm", 'utf8');
      },
      "Courier-BoldOblique": function() {
        return fs.readFileSync(__dirname + "/font/data/Courier-BoldOblique.afm", 'utf8');
      },
      "Helvetica": function() {
        return fs.readFileSync(__dirname + "/font/data/Helvetica.afm", 'utf8');
      },
      "Helvetica-Bold": function() {
        return fs.readFileSync(__dirname + "/font/data/Helvetica-Bold.afm", 'utf8');
      },
      "Helvetica-Oblique": function() {
        return fs.readFileSync(__dirname + "/font/data/Helvetica-Oblique.afm", 'utf8');
      },
      "Helvetica-BoldOblique": function() {
        return fs.readFileSync(__dirname + "/font/data/Helvetica-BoldOblique.afm", 'utf8');
      },
      "Times-Roman": function() {
        return fs.readFileSync(__dirname + "/font/data/Times-Roman.afm", 'utf8');
      },
      "Times-Bold": function() {
        return fs.readFileSync(__dirname + "/font/data/Times-Bold.afm", 'utf8');
      },
      "Times-Italic": function() {
        return fs.readFileSync(__dirname + "/font/data/Times-Italic.afm", 'utf8');
      },
      "Times-BoldItalic": function() {
        return fs.readFileSync(__dirname + "/font/data/Times-BoldItalic.afm", 'utf8');
      },
      "Symbol": function() {
        return fs.readFileSync(__dirname + "/font/data/Symbol.afm", 'utf8');
      },
      "ZapfDingbats": function() {
        return fs.readFileSync(__dirname + "/font/data/ZapfDingbats.afm", 'utf8');
      }
    };

    PDFFont.prototype.use = function(characters) {
      var _ref;
      return (_ref = this.subset) != null ? _ref.use(characters) : void 0;
    };

    PDFFont.prototype.embed = function() {
      if (this.embedded || (this.dictionary == null)) {
        return;
      }
      if (this.isAFM) {
        this.embedAFM();
      } else {
        this.embedTTF();
      }
      return this.embedded = true;
    };

    PDFFont.prototype.encode = function(text) {
      var _ref;
      if (this.isAFM) {
        return this.font.encodeText(text);
      } else {
        return ((_ref = this.subset) != null ? _ref.encodeText(text) : void 0) || text;
      }
    };

    PDFFont.prototype.ref = function() {
      return this.dictionary != null ? this.dictionary : this.dictionary = this.document.ref();
    };

    PDFFont.prototype.registerTTF = function() {
      var e, hi, low, raw, _ref;
      this.name = this.font.name.postscriptName;
      this.scaleFactor = 1000.0 / this.font.head.unitsPerEm;
      this.bbox = (function() {
        var _i, _len, _ref, _results;
        _ref = this.font.bbox;
        _results = [];
        for (_i = 0, _len = _ref.length; _i < _len; _i++) {
          e = _ref[_i];
          _results.push(Math.round(e * this.scaleFactor));
        }
        return _results;
      }).call(this);
      this.stemV = 0;
      if (this.font.post.exists) {
        raw = this.font.post.italic_angle;
        hi = raw >> 16;
        low = raw & 0xFF;
        if (hi & 0x8000 !== 0) {
          hi = -((hi ^ 0xFFFF) + 1);
        }
        this.italicAngle = +("" + hi + "." + low);
      } else {
        this.italicAngle = 0;
      }
      this.ascender = Math.round(this.font.ascender * this.scaleFactor);
      this.decender = Math.round(this.font.decender * this.scaleFactor);
      this.lineGap = Math.round(this.font.lineGap * this.scaleFactor);
      this.capHeight = (this.font.os2.exists && this.font.os2.capHeight) || this.ascender;
      this.xHeight = (this.font.os2.exists && this.font.os2.xHeight) || 0;
      this.familyClass = (this.font.os2.exists && this.font.os2.familyClass || 0) >> 8;
      this.isSerif = (_ref = this.familyClass) === 1 || _ref === 2 || _ref === 3 || _ref === 4 || _ref === 5 || _ref === 7;
      this.isScript = this.familyClass === 10;
      this.flags = 0;
      if (this.font.post.isFixedPitch) {
        this.flags |= 1 << 0;
      }
      if (this.isSerif) {
        this.flags |= 1 << 1;
      }
      if (this.isScript) {
        this.flags |= 1 << 3;
      }
      if (this.italicAngle !== 0) {
        this.flags |= 1 << 6;
      }
      this.flags |= 1 << 5;
      if (!this.font.cmap.unicode) {
        throw new Error('No unicode cmap for font');
      }
    };

    PDFFont.prototype.embedTTF = function() {
      var charWidths, cmap, code, data, descriptor, firstChar, fontfile, glyph;
      data = this.subset.encode();
      fontfile = this.document.ref();
      fontfile.write(data);
      fontfile.data.Length1 = fontfile.uncompressedLength;
      fontfile.end();
      descriptor = this.document.ref({
        Type: 'FontDescriptor',
        FontName: this.subset.postscriptName,
        FontFile2: fontfile,
        FontBBox: this.bbox,
        Flags: this.flags,
        StemV: this.stemV,
        ItalicAngle: this.italicAngle,
        Ascent: this.ascender,
        Descent: this.decender,
        CapHeight: this.capHeight,
        XHeight: this.xHeight
      });
      descriptor.end();
      firstChar = +Object.keys(this.subset.cmap)[0];
      charWidths = (function() {
        var _ref, _results;
        _ref = this.subset.cmap;
        _results = [];
        for (code in _ref) {
          glyph = _ref[code];
          _results.push(Math.round(this.font.widthOfGlyph(glyph)));
        }
        return _results;
      }).call(this);
      cmap = this.document.ref();
      cmap.end(toUnicodeCmap(this.subset.subset));
      this.dictionary.data = {
        Type: 'Font',
        BaseFont: this.subset.postscriptName,
        Subtype: 'TrueType',
        FontDescriptor: descriptor,
        FirstChar: firstChar,
        LastChar: firstChar + charWidths.length - 1,
        Widths: charWidths,
        Encoding: 'MacRomanEncoding',
        ToUnicode: cmap
      };
      return this.dictionary.end();
    };

    toUnicodeCmap = function(map) {
      var code, codes, range, unicode, unicodeMap, _i, _len;
      unicodeMap = '/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n/CIDSystemInfo <<\n  /Registry (Adobe)\n  /Ordering (UCS)\n  /Supplement 0\n>> def\n/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n1 begincodespacerange\n<00><ff>\nendcodespacerange';
      codes = Object.keys(map).sort(function(a, b) {
        return a - b;
      });
      range = [];
      for (_i = 0, _len = codes.length; _i < _len; _i++) {
        code = codes[_i];
        if (range.length >= 100) {
          unicodeMap += "\n" + range.length + " beginbfchar\n" + (range.join('\n')) + "\nendbfchar";
          range = [];
        }
        unicode = ('0000' + map[code].toString(16)).slice(-4);
        code = (+code).toString(16);
        range.push("<" + code + "><" + unicode + ">");
      }
      if (range.length) {
        unicodeMap += "\n" + range.length + " beginbfchar\n" + (range.join('\n')) + "\nendbfchar\n";
      }
      return unicodeMap += 'endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend';
    };

    PDFFont.prototype.registerAFM = function(name) {
      var _ref;
      this.name = name;
      return _ref = this.font, this.ascender = _ref.ascender, this.decender = _ref.decender, this.bbox = _ref.bbox, this.lineGap = _ref.lineGap, _ref;
    };

    PDFFont.prototype.embedAFM = function() {
      this.dictionary.data = {
        Type: 'Font',
        BaseFont: this.name,
        Subtype: 'Type1',
        Encoding: 'WinAnsiEncoding'
      };
      return this.dictionary.end();
    };

    PDFFont.prototype.widthOfString = function(string, size) {
      var charCode, i, scale, width, _i, _ref;
      string = '' + string;
      width = 0;
      for (i = _i = 0, _ref = string.length; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        charCode = string.charCodeAt(i);
        width += this.font.widthOfGlyph(this.font.characterToGlyph(charCode)) || 0;
      }
      scale = size / 1000;
      return width * scale;
    };

    PDFFont.prototype.lineHeight = function(size, includeGap) {
      var gap;
      if (includeGap == null) {
        includeGap = false;
      }
      gap = includeGap ? this.lineGap : 0;
      return (this.ascender + gap - this.decender) / 1000 * size;
    };

    return PDFFont;

  })();

  module.exports = PDFFont;

}).call(this);

}).call(this,_dereq_("buffer").Buffer,"/../../node_modules/pdfkit/js")
},{"./font/afm":35,"./font/subset":38,"./font/ttf":50,"buffer":16,"fs":"x/K9gc"}],35:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var AFMFont, fs;

  fs = _dereq_('fs');

  AFMFont = (function() {
    var WIN_ANSI_MAP, characters;

    AFMFont.open = function(filename) {
      return new AFMFont(fs.readFileSync(filename, 'utf8'));
    };

    function AFMFont(contents) {
      var e, i;
      this.contents = contents;
      this.attributes = {};
      this.glyphWidths = {};
      this.boundingBoxes = {};
      this.parse();
      this.charWidths = (function() {
        var _i, _results;
        _results = [];
        for (i = _i = 0; _i <= 255; i = ++_i) {
          _results.push(this.glyphWidths[characters[i]]);
        }
        return _results;
      }).call(this);
      this.bbox = (function() {
        var _i, _len, _ref, _results;
        _ref = this.attributes['FontBBox'].split(/\s+/);
        _results = [];
        for (_i = 0, _len = _ref.length; _i < _len; _i++) {
          e = _ref[_i];
          _results.push(+e);
        }
        return _results;
      }).call(this);
      this.ascender = +(this.attributes['Ascender'] || 0);
      this.decender = +(this.attributes['Descender'] || 0);
      this.lineGap = (this.bbox[3] - this.bbox[1]) - (this.ascender - this.decender);
    }

    AFMFont.prototype.parse = function() {
      var a, key, line, match, name, section, value, _i, _len, _ref;
      section = '';
      _ref = this.contents.split('\n');
      for (_i = 0, _len = _ref.length; _i < _len; _i++) {
        line = _ref[_i];
        if (match = line.match(/^Start(\w+)/)) {
          section = match[1];
          continue;
        } else if (match = line.match(/^End(\w+)/)) {
          section = '';
          continue;
        }
        switch (section) {
          case 'FontMetrics':
            match = line.match(/(^\w+)\s+(.*)/);
            key = match[1];
            value = match[2];
            if (a = this.attributes[key]) {
              if (!Array.isArray(a)) {
                a = this.attributes[key] = [a];
              }
              a.push(value);
            } else {
              this.attributes[key] = value;
            }
            break;
          case 'CharMetrics':
            if (!/^CH?\s/.test(line)) {
              continue;
            }
            name = line.match(/\bN\s+(\.?\w+)\s*;/)[1];
            this.glyphWidths[name] = +line.match(/\bWX\s+(\d+)\s*;/)[1];
        }
      }
    };

    WIN_ANSI_MAP = {
      402: 131,
      8211: 150,
      8212: 151,
      8216: 145,
      8217: 146,
      8218: 130,
      8220: 147,
      8221: 148,
      8222: 132,
      8224: 134,
      8225: 135,
      8226: 149,
      8230: 133,
      8364: 128,
      8240: 137,
      8249: 139,
      8250: 155,
      710: 136,
      8482: 153,
      338: 140,
      339: 156,
      732: 152,
      352: 138,
      353: 154,
      376: 159,
      381: 142,
      382: 158
    };

    AFMFont.prototype.encodeText = function(text) {
      var char, i, string, _i, _ref;
      string = '';
      for (i = _i = 0, _ref = text.length; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        char = text.charCodeAt(i);
        char = WIN_ANSI_MAP[char] || char;
        string += String.fromCharCode(char);
      }
      return string;
    };

    AFMFont.prototype.characterToGlyph = function(character) {
      return characters[WIN_ANSI_MAP[character] || character];
    };

    AFMFont.prototype.widthOfGlyph = function(glyph) {
      return this.glyphWidths[glyph];
    };

    characters = '.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n.notdef       .notdef        .notdef        .notdef\n\nspace         exclam         quotedbl       numbersign\ndollar        percent        ampersand      quotesingle\nparenleft     parenright     asterisk       plus\ncomma         hyphen         period         slash\nzero          one            two            three\nfour          five           six            seven\neight         nine           colon          semicolon\nless          equal          greater        question\n\nat            A              B              C\nD             E              F              G\nH             I              J              K\nL             M              N              O\nP             Q              R              S\nT             U              V              W\nX             Y              Z              bracketleft\nbackslash     bracketright   asciicircum    underscore\n\ngrave         a              b              c\nd             e              f              g\nh             i              j              k\nl             m              n              o\np             q              r              s\nt             u              v              w\nx             y              z              braceleft\nbar           braceright     asciitilde     .notdef\n\nEuro          .notdef        quotesinglbase florin\nquotedblbase  ellipsis       dagger         daggerdbl\ncircumflex    perthousand    Scaron         guilsinglleft\nOE            .notdef        Zcaron         .notdef\n.notdef       quoteleft      quoteright     quotedblleft\nquotedblright bullet         endash         emdash\ntilde         trademark      scaron         guilsinglright\noe            .notdef        zcaron         ydieresis\n\nspace         exclamdown     cent           sterling\ncurrency      yen            brokenbar      section\ndieresis      copyright      ordfeminine    guillemotleft\nlogicalnot    hyphen         registered     macron\ndegree        plusminus      twosuperior    threesuperior\nacute         mu             paragraph      periodcentered\ncedilla       onesuperior    ordmasculine   guillemotright\nonequarter    onehalf        threequarters  questiondown\n\nAgrave        Aacute         Acircumflex    Atilde\nAdieresis     Aring          AE             Ccedilla\nEgrave        Eacute         Ecircumflex    Edieresis\nIgrave        Iacute         Icircumflex    Idieresis\nEth           Ntilde         Ograve         Oacute\nOcircumflex   Otilde         Odieresis      multiply\nOslash        Ugrave         Uacute         Ucircumflex\nUdieresis     Yacute         Thorn          germandbls\n\nagrave        aacute         acircumflex    atilde\nadieresis     aring          ae             ccedilla\negrave        eacute         ecircumflex    edieresis\nigrave        iacute         icircumflex    idieresis\neth           ntilde         ograve         oacute\nocircumflex   otilde         odieresis      divide\noslash        ugrave         uacute         ucircumflex\nudieresis     yacute         thorn          ydieresis'.split(/\s+/);

    return AFMFont;

  })();

  module.exports = AFMFont;

}).call(this);

},{"fs":"x/K9gc"}],36:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var DFont, Data, Directory, NameTable, fs;

  fs = _dereq_('fs');

  Data = _dereq_('../data');

  Directory = _dereq_('./directory');

  NameTable = _dereq_('./tables/name');

  DFont = (function() {
    DFont.open = function(filename) {
      var contents;
      contents = fs.readFileSync(filename);
      return new DFont(contents);
    };

    function DFont(contents) {
      this.contents = new Data(contents);
      this.parse(this.contents);
    }

    DFont.prototype.parse = function(data) {
      var attr, b2, b3, b4, dataLength, dataOffset, dataOfs, entry, font, handle, i, id, j, len, length, mapLength, mapOffset, maxIndex, maxTypeIndex, name, nameListOffset, nameOfs, p, pos, refListOffset, type, typeListOffset, _i, _j;
      dataOffset = data.readInt();
      mapOffset = data.readInt();
      dataLength = data.readInt();
      mapLength = data.readInt();
      this.map = {};
      data.pos = mapOffset + 24;
      typeListOffset = data.readShort() + mapOffset;
      nameListOffset = data.readShort() + mapOffset;
      data.pos = typeListOffset;
      maxIndex = data.readShort();
      for (i = _i = 0; _i <= maxIndex; i = _i += 1) {
        type = data.readString(4);
        maxTypeIndex = data.readShort();
        refListOffset = data.readShort();
        this.map[type] = {
          list: [],
          named: {}
        };
        pos = data.pos;
        data.pos = typeListOffset + refListOffset;
        for (j = _j = 0; _j <= maxTypeIndex; j = _j += 1) {
          id = data.readShort();
          nameOfs = data.readShort();
          attr = data.readByte();
          b2 = data.readByte() << 16;
          b3 = data.readByte() << 8;
          b4 = data.readByte();
          dataOfs = dataOffset + (0 | b2 | b3 | b4);
          handle = data.readUInt32();
          entry = {
            id: id,
            attributes: attr,
            offset: dataOfs,
            handle: handle
          };
          p = data.pos;
          if (nameOfs !== -1 && (nameListOffset + nameOfs < mapOffset + mapLength)) {
            data.pos = nameListOffset + nameOfs;
            len = data.readByte();
            entry.name = data.readString(len);
          } else if (type === 'sfnt') {
            data.pos = entry.offset;
            length = data.readUInt32();
            font = {};
            font.contents = new Data(data.slice(data.pos, data.pos + length));
            font.directory = new Directory(font.contents);
            name = new NameTable(font);
            entry.name = name.fontName[0].raw;
          }
          data.pos = p;
          this.map[type].list.push(entry);
          if (entry.name) {
            this.map[type].named[entry.name] = entry;
          }
        }
        data.pos = pos;
      }
    };

    DFont.prototype.getNamedFont = function(name) {
      var data, entry, length, pos, ret, _ref;
      data = this.contents;
      pos = data.pos;
      entry = (_ref = this.map.sfnt) != null ? _ref.named[name] : void 0;
      if (!entry) {
        throw new Error("Font " + name + " not found in DFont file.");
      }
      data.pos = entry.offset;
      length = data.readUInt32();
      ret = data.slice(data.pos, data.pos + length);
      data.pos = pos;
      return ret;
    };

    return DFont;

  })();

  module.exports = DFont;

}).call(this);

},{"../data":32,"./directory":37,"./tables/name":47,"fs":"x/K9gc"}],37:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, Directory,
    __slice = [].slice;

  Data = _dereq_('../data');

  Directory = (function() {
    var checksum;

    function Directory(data) {
      var entry, i, _i, _ref;
      this.scalarType = data.readInt();
      this.tableCount = data.readShort();
      this.searchRange = data.readShort();
      this.entrySelector = data.readShort();
      this.rangeShift = data.readShort();
      this.tables = {};
      for (i = _i = 0, _ref = this.tableCount; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        entry = {
          tag: data.readString(4),
          checksum: data.readInt(),
          offset: data.readInt(),
          length: data.readInt()
        };
        this.tables[entry.tag] = entry;
      }
    }

    Directory.prototype.encode = function(tables) {
      var adjustment, directory, directoryLength, entrySelector, headOffset, log2, offset, rangeShift, searchRange, sum, table, tableCount, tableData, tag;
      tableCount = Object.keys(tables).length;
      log2 = Math.log(2);
      searchRange = Math.floor(Math.log(tableCount) / log2) * 16;
      entrySelector = Math.floor(searchRange / log2);
      rangeShift = tableCount * 16 - searchRange;
      directory = new Data;
      directory.writeInt(this.scalarType);
      directory.writeShort(tableCount);
      directory.writeShort(searchRange);
      directory.writeShort(entrySelector);
      directory.writeShort(rangeShift);
      directoryLength = tableCount * 16;
      offset = directory.pos + directoryLength;
      headOffset = null;
      tableData = [];
      for (tag in tables) {
        table = tables[tag];
        directory.writeString(tag);
        directory.writeInt(checksum(table));
        directory.writeInt(offset);
        directory.writeInt(table.length);
        tableData = tableData.concat(table);
        if (tag === 'head') {
          headOffset = offset;
        }
        offset += table.length;
        while (offset % 4) {
          tableData.push(0);
          offset++;
        }
      }
      directory.write(tableData);
      sum = checksum(directory.data);
      adjustment = 0xB1B0AFBA - sum;
      directory.pos = headOffset + 8;
      directory.writeUInt32(adjustment);
      return new Buffer(directory.data);
    };

    checksum = function(data) {
      var i, sum, tmp, _i, _ref;
      data = __slice.call(data);
      while (data.length % 4) {
        data.push(0);
      }
      tmp = new Data(data);
      sum = 0;
      for (i = _i = 0, _ref = data.length; _i < _ref; i = _i += 4) {
        sum += tmp.readUInt32();
      }
      return sum & 0xFFFFFFFF;
    };

    return Directory;

  })();

  module.exports = Directory;

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"../data":32,"buffer":16}],38:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var CmapTable, Subset, utils,
    __indexOf = [].indexOf || function(item) { for (var i = 0, l = this.length; i < l; i++) { if (i in this && this[i] === item) return i; } return -1; };

  CmapTable = _dereq_('./tables/cmap');

  utils = _dereq_('./utils');

  Subset = (function() {
    function Subset(font) {
      this.font = font;
      this.subset = {};
      this.unicodes = {};
      this.next = 33;
    }

    Subset.prototype.use = function(character) {
      var i, _i, _ref;
      if (typeof character === 'string') {
        for (i = _i = 0, _ref = character.length; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
          this.use(character.charCodeAt(i));
        }
        return;
      }
      if (!this.unicodes[character]) {
        this.subset[this.next] = character;
        return this.unicodes[character] = this.next++;
      }
    };

    Subset.prototype.encodeText = function(text) {
      var char, i, string, _i, _ref;
      string = '';
      for (i = _i = 0, _ref = text.length; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        char = this.unicodes[text.charCodeAt(i)];
        string += String.fromCharCode(char);
      }
      return string;
    };

    Subset.prototype.generateCmap = function() {
      var mapping, roman, unicode, unicodeCmap, _ref;
      unicodeCmap = this.font.cmap.tables[0].codeMap;
      mapping = {};
      _ref = this.subset;
      for (roman in _ref) {
        unicode = _ref[roman];
        mapping[roman] = unicodeCmap[unicode];
      }
      return mapping;
    };

    Subset.prototype.glyphIDs = function() {
      var ret, roman, unicode, unicodeCmap, val, _ref;
      unicodeCmap = this.font.cmap.tables[0].codeMap;
      ret = [0];
      _ref = this.subset;
      for (roman in _ref) {
        unicode = _ref[roman];
        val = unicodeCmap[unicode];
        if ((val != null) && __indexOf.call(ret, val) < 0) {
          ret.push(val);
        }
      }
      return ret.sort();
    };

    Subset.prototype.glyphsFor = function(glyphIDs) {
      var additionalIDs, glyph, glyphs, id, _i, _len, _ref;
      glyphs = {};
      for (_i = 0, _len = glyphIDs.length; _i < _len; _i++) {
        id = glyphIDs[_i];
        glyphs[id] = this.font.glyf.glyphFor(id);
      }
      additionalIDs = [];
      for (id in glyphs) {
        glyph = glyphs[id];
        if (glyph != null ? glyph.compound : void 0) {
          additionalIDs.push.apply(additionalIDs, glyph.glyphIDs);
        }
      }
      if (additionalIDs.length > 0) {
        _ref = this.glyphsFor(additionalIDs);
        for (id in _ref) {
          glyph = _ref[id];
          glyphs[id] = glyph;
        }
      }
      return glyphs;
    };

    Subset.prototype.encode = function() {
      var cmap, code, glyf, glyphs, id, ids, loca, name, new2old, newIDs, nextGlyphID, old2new, oldID, oldIDs, tables, _ref, _ref1;
      cmap = CmapTable.encode(this.generateCmap(), 'unicode');
      glyphs = this.glyphsFor(this.glyphIDs());
      old2new = {
        0: 0
      };
      _ref = cmap.charMap;
      for (code in _ref) {
        ids = _ref[code];
        old2new[ids.old] = ids["new"];
      }
      nextGlyphID = cmap.maxGlyphID;
      for (oldID in glyphs) {
        if (!(oldID in old2new)) {
          old2new[oldID] = nextGlyphID++;
        }
      }
      new2old = utils.invert(old2new);
      newIDs = Object.keys(new2old).sort(function(a, b) {
        return a - b;
      });
      oldIDs = (function() {
        var _i, _len, _results;
        _results = [];
        for (_i = 0, _len = newIDs.length; _i < _len; _i++) {
          id = newIDs[_i];
          _results.push(new2old[id]);
        }
        return _results;
      })();
      glyf = this.font.glyf.encode(glyphs, oldIDs, old2new);
      loca = this.font.loca.encode(glyf.offsets);
      name = this.font.name.encode();
      this.postscriptName = name.postscriptName;
      this.cmap = {};
      _ref1 = cmap.charMap;
      for (code in _ref1) {
        ids = _ref1[code];
        this.cmap[code] = ids.old;
      }
      tables = {
        cmap: cmap.table,
        glyf: glyf.table,
        loca: loca.table,
        hmtx: this.font.hmtx.encode(oldIDs),
        hhea: this.font.hhea.encode(oldIDs),
        maxp: this.font.maxp.encode(oldIDs),
        post: this.font.post.encode(oldIDs),
        name: name.table,
        head: this.font.head.encode(loca)
      };
      if (this.font.os2.exists) {
        tables['OS/2'] = this.font.os2.raw();
      }
      return this.font.directory.encode(tables);
    };

    return Subset;

  })();

  module.exports = Subset;

}).call(this);

},{"./tables/cmap":40,"./utils":51}],39:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Table;

  Table = (function() {
    function Table(file) {
      var info;
      this.file = file;
      info = this.file.directory.tables[this.tag];
      this.exists = !!info;
      if (info) {
        this.offset = info.offset, this.length = info.length;
        this.parse(this.file.contents);
      }
    }

    Table.prototype.parse = function() {};

    Table.prototype.encode = function() {};

    Table.prototype.raw = function() {
      if (!this.exists) {
        return null;
      }
      this.file.contents.pos = this.offset;
      return this.file.contents.read(this.length);
    };

    return Table;

  })();

  module.exports = Table;

}).call(this);

},{}],40:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var CmapEntry, CmapTable, Data, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  CmapTable = (function(_super) {
    __extends(CmapTable, _super);

    function CmapTable() {
      return CmapTable.__super__.constructor.apply(this, arguments);
    }

    CmapTable.prototype.tag = 'cmap';

    CmapTable.prototype.parse = function(data) {
      var entry, i, tableCount, _i;
      data.pos = this.offset;
      this.version = data.readUInt16();
      tableCount = data.readUInt16();
      this.tables = [];
      this.unicode = null;
      for (i = _i = 0; 0 <= tableCount ? _i < tableCount : _i > tableCount; i = 0 <= tableCount ? ++_i : --_i) {
        entry = new CmapEntry(data, this.offset);
        this.tables.push(entry);
        if (entry.isUnicode) {
          if (this.unicode == null) {
            this.unicode = entry;
          }
        }
      }
      return true;
    };

    CmapTable.encode = function(charmap, encoding) {
      var result, table;
      if (encoding == null) {
        encoding = 'macroman';
      }
      result = CmapEntry.encode(charmap, encoding);
      table = new Data;
      table.writeUInt16(0);
      table.writeUInt16(1);
      result.table = table.data.concat(result.subtable);
      return result;
    };

    return CmapTable;

  })(Table);

  CmapEntry = (function() {
    function CmapEntry(data, offset) {
      var code, count, endCode, glyphId, glyphIds, i, idDelta, idRangeOffset, index, saveOffset, segCount, segCountX2, start, startCode, tail, _i, _j, _k, _len;
      this.platformID = data.readUInt16();
      this.encodingID = data.readShort();
      this.offset = offset + data.readInt();
      saveOffset = data.pos;
      data.pos = this.offset;
      this.format = data.readUInt16();
      this.length = data.readUInt16();
      this.language = data.readUInt16();
      this.isUnicode = (this.platformID === 3 && this.encodingID === 1 && this.format === 4) || this.platformID === 0 && this.format === 4;
      this.codeMap = {};
      switch (this.format) {
        case 0:
          for (i = _i = 0; _i < 256; i = ++_i) {
            this.codeMap[i] = data.readByte();
          }
          break;
        case 4:
          segCountX2 = data.readUInt16();
          segCount = segCountX2 / 2;
          data.pos += 6;
          endCode = (function() {
            var _j, _results;
            _results = [];
            for (i = _j = 0; 0 <= segCount ? _j < segCount : _j > segCount; i = 0 <= segCount ? ++_j : --_j) {
              _results.push(data.readUInt16());
            }
            return _results;
          })();
          data.pos += 2;
          startCode = (function() {
            var _j, _results;
            _results = [];
            for (i = _j = 0; 0 <= segCount ? _j < segCount : _j > segCount; i = 0 <= segCount ? ++_j : --_j) {
              _results.push(data.readUInt16());
            }
            return _results;
          })();
          idDelta = (function() {
            var _j, _results;
            _results = [];
            for (i = _j = 0; 0 <= segCount ? _j < segCount : _j > segCount; i = 0 <= segCount ? ++_j : --_j) {
              _results.push(data.readUInt16());
            }
            return _results;
          })();
          idRangeOffset = (function() {
            var _j, _results;
            _results = [];
            for (i = _j = 0; 0 <= segCount ? _j < segCount : _j > segCount; i = 0 <= segCount ? ++_j : --_j) {
              _results.push(data.readUInt16());
            }
            return _results;
          })();
          count = (this.length - data.pos + this.offset) / 2;
          glyphIds = (function() {
            var _j, _results;
            _results = [];
            for (i = _j = 0; 0 <= count ? _j < count : _j > count; i = 0 <= count ? ++_j : --_j) {
              _results.push(data.readUInt16());
            }
            return _results;
          })();
          for (i = _j = 0, _len = endCode.length; _j < _len; i = ++_j) {
            tail = endCode[i];
            start = startCode[i];
            for (code = _k = start; start <= tail ? _k <= tail : _k >= tail; code = start <= tail ? ++_k : --_k) {
              if (idRangeOffset[i] === 0) {
                glyphId = code + idDelta[i];
              } else {
                index = idRangeOffset[i] / 2 + (code - start) - (segCount - i);
                glyphId = glyphIds[index] || 0;
                if (glyphId !== 0) {
                  glyphId += idDelta[i];
                }
              }
              this.codeMap[code] = glyphId & 0xFFFF;
            }
          }
      }
      data.pos = saveOffset;
    }

    CmapEntry.encode = function(charmap, encoding) {
      var charMap, code, codeMap, codes, delta, deltas, diff, endCode, endCodes, entrySelector, glyphIDs, i, id, indexes, last, map, nextID, offset, old, rangeOffsets, rangeShift, result, searchRange, segCount, segCountX2, startCode, startCodes, startGlyph, subtable, _i, _j, _k, _l, _len, _len1, _len2, _len3, _len4, _len5, _len6, _len7, _m, _n, _name, _o, _p, _q;
      subtable = new Data;
      codes = Object.keys(charmap).sort(function(a, b) {
        return a - b;
      });
      switch (encoding) {
        case 'macroman':
          id = 0;
          indexes = (function() {
            var _i, _results;
            _results = [];
            for (i = _i = 0; _i < 256; i = ++_i) {
              _results.push(0);
            }
            return _results;
          })();
          map = {
            0: 0
          };
          codeMap = {};
          for (_i = 0, _len = codes.length; _i < _len; _i++) {
            code = codes[_i];
            if (map[_name = charmap[code]] == null) {
              map[_name] = ++id;
            }
            codeMap[code] = {
              old: charmap[code],
              "new": map[charmap[code]]
            };
            indexes[code] = map[charmap[code]];
          }
          subtable.writeUInt16(1);
          subtable.writeUInt16(0);
          subtable.writeUInt32(12);
          subtable.writeUInt16(0);
          subtable.writeUInt16(262);
          subtable.writeUInt16(0);
          subtable.write(indexes);
          return result = {
            charMap: codeMap,
            subtable: subtable.data,
            maxGlyphID: id + 1
          };
        case 'unicode':
          startCodes = [];
          endCodes = [];
          nextID = 0;
          map = {};
          charMap = {};
          last = diff = null;
          for (_j = 0, _len1 = codes.length; _j < _len1; _j++) {
            code = codes[_j];
            old = charmap[code];
            if (map[old] == null) {
              map[old] = ++nextID;
            }
            charMap[code] = {
              old: old,
              "new": map[old]
            };
            delta = map[old] - code;
            if ((last == null) || delta !== diff) {
              if (last) {
                endCodes.push(last);
              }
              startCodes.push(code);
              diff = delta;
            }
            last = code;
          }
          if (last) {
            endCodes.push(last);
          }
          endCodes.push(0xFFFF);
          startCodes.push(0xFFFF);
          segCount = startCodes.length;
          segCountX2 = segCount * 2;
          searchRange = 2 * Math.pow(Math.log(segCount) / Math.LN2, 2);
          entrySelector = Math.log(searchRange / 2) / Math.LN2;
          rangeShift = 2 * segCount - searchRange;
          deltas = [];
          rangeOffsets = [];
          glyphIDs = [];
          for (i = _k = 0, _len2 = startCodes.length; _k < _len2; i = ++_k) {
            startCode = startCodes[i];
            endCode = endCodes[i];
            if (startCode === 0xFFFF) {
              deltas.push(0);
              rangeOffsets.push(0);
              break;
            }
            startGlyph = charMap[startCode]["new"];
            if (startCode - startGlyph >= 0x8000) {
              deltas.push(0);
              rangeOffsets.push(2 * (glyphIDs.length + segCount - i));
              for (code = _l = startCode; startCode <= endCode ? _l <= endCode : _l >= endCode; code = startCode <= endCode ? ++_l : --_l) {
                glyphIDs.push(charMap[code]["new"]);
              }
            } else {
              deltas.push(startGlyph - startCode);
              rangeOffsets.push(0);
            }
          }
          subtable.writeUInt16(3);
          subtable.writeUInt16(1);
          subtable.writeUInt32(12);
          subtable.writeUInt16(4);
          subtable.writeUInt16(16 + segCount * 8 + glyphIDs.length * 2);
          subtable.writeUInt16(0);
          subtable.writeUInt16(segCountX2);
          subtable.writeUInt16(searchRange);
          subtable.writeUInt16(entrySelector);
          subtable.writeUInt16(rangeShift);
          for (_m = 0, _len3 = endCodes.length; _m < _len3; _m++) {
            code = endCodes[_m];
            subtable.writeUInt16(code);
          }
          subtable.writeUInt16(0);
          for (_n = 0, _len4 = startCodes.length; _n < _len4; _n++) {
            code = startCodes[_n];
            subtable.writeUInt16(code);
          }
          for (_o = 0, _len5 = deltas.length; _o < _len5; _o++) {
            delta = deltas[_o];
            subtable.writeUInt16(delta);
          }
          for (_p = 0, _len6 = rangeOffsets.length; _p < _len6; _p++) {
            offset = rangeOffsets[_p];
            subtable.writeUInt16(offset);
          }
          for (_q = 0, _len7 = glyphIDs.length; _q < _len7; _q++) {
            id = glyphIDs[_q];
            subtable.writeUInt16(id);
          }
          return result = {
            charMap: charMap,
            subtable: subtable.data,
            maxGlyphID: nextID + 1
          };
      }
    };

    return CmapEntry;

  })();

  module.exports = CmapTable;

}).call(this);

},{"../../data":32,"../table":39}],41:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var CompoundGlyph, Data, GlyfTable, SimpleGlyph, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; },
    __slice = [].slice;

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  GlyfTable = (function(_super) {
    __extends(GlyfTable, _super);

    function GlyfTable() {
      return GlyfTable.__super__.constructor.apply(this, arguments);
    }

    GlyfTable.prototype.tag = 'glyf';

    GlyfTable.prototype.parse = function(data) {
      return this.cache = {};
    };

    GlyfTable.prototype.glyphFor = function(id) {
      var data, index, length, loca, numberOfContours, raw, xMax, xMin, yMax, yMin;
      if (id in this.cache) {
        return this.cache[id];
      }
      loca = this.file.loca;
      data = this.file.contents;
      index = loca.indexOf(id);
      length = loca.lengthOf(id);
      if (length === 0) {
        return this.cache[id] = null;
      }
      data.pos = this.offset + index;
      raw = new Data(data.read(length));
      numberOfContours = raw.readShort();
      xMin = raw.readShort();
      yMin = raw.readShort();
      xMax = raw.readShort();
      yMax = raw.readShort();
      if (numberOfContours === -1) {
        this.cache[id] = new CompoundGlyph(raw, xMin, yMin, xMax, yMax);
      } else {
        this.cache[id] = new SimpleGlyph(raw, numberOfContours, xMin, yMin, xMax, yMax);
      }
      return this.cache[id];
    };

    GlyfTable.prototype.encode = function(glyphs, mapping, old2new) {
      var glyph, id, offsets, table, _i, _len;
      table = [];
      offsets = [];
      for (_i = 0, _len = mapping.length; _i < _len; _i++) {
        id = mapping[_i];
        glyph = glyphs[id];
        offsets.push(table.length);
        if (glyph) {
          table = table.concat(glyph.encode(old2new));
        }
      }
      offsets.push(table.length);
      return {
        table: table,
        offsets: offsets
      };
    };

    return GlyfTable;

  })(Table);

  SimpleGlyph = (function() {
    function SimpleGlyph(raw, numberOfContours, xMin, yMin, xMax, yMax) {
      this.raw = raw;
      this.numberOfContours = numberOfContours;
      this.xMin = xMin;
      this.yMin = yMin;
      this.xMax = xMax;
      this.yMax = yMax;
      this.compound = false;
    }

    SimpleGlyph.prototype.encode = function() {
      return this.raw.data;
    };

    return SimpleGlyph;

  })();

  CompoundGlyph = (function() {
    var ARG_1_AND_2_ARE_WORDS, MORE_COMPONENTS, WE_HAVE_AN_X_AND_Y_SCALE, WE_HAVE_A_SCALE, WE_HAVE_A_TWO_BY_TWO, WE_HAVE_INSTRUCTIONS;

    ARG_1_AND_2_ARE_WORDS = 0x0001;

    WE_HAVE_A_SCALE = 0x0008;

    MORE_COMPONENTS = 0x0020;

    WE_HAVE_AN_X_AND_Y_SCALE = 0x0040;

    WE_HAVE_A_TWO_BY_TWO = 0x0080;

    WE_HAVE_INSTRUCTIONS = 0x0100;

    function CompoundGlyph(raw, xMin, yMin, xMax, yMax) {
      var data, flags;
      this.raw = raw;
      this.xMin = xMin;
      this.yMin = yMin;
      this.xMax = xMax;
      this.yMax = yMax;
      this.compound = true;
      this.glyphIDs = [];
      this.glyphOffsets = [];
      data = this.raw;
      while (true) {
        flags = data.readShort();
        this.glyphOffsets.push(data.pos);
        this.glyphIDs.push(data.readShort());
        if (!(flags & MORE_COMPONENTS)) {
          break;
        }
        if (flags & ARG_1_AND_2_ARE_WORDS) {
          data.pos += 4;
        } else {
          data.pos += 2;
        }
        if (flags & WE_HAVE_A_TWO_BY_TWO) {
          data.pos += 8;
        } else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
          data.pos += 4;
        } else if (flags & WE_HAVE_A_SCALE) {
          data.pos += 2;
        }
      }
    }

    CompoundGlyph.prototype.encode = function(mapping) {
      var i, id, result, _i, _len, _ref;
      result = new Data(__slice.call(this.raw.data));
      _ref = this.glyphIDs;
      for (i = _i = 0, _len = _ref.length; _i < _len; i = ++_i) {
        id = _ref[i];
        result.pos = this.glyphOffsets[i];
        result.writeShort(mapping[id]);
      }
      return result.data;
    };

    return CompoundGlyph;

  })();

  module.exports = GlyfTable;

}).call(this);

},{"../../data":32,"../table":39}],42:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, HeadTable, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  HeadTable = (function(_super) {
    __extends(HeadTable, _super);

    function HeadTable() {
      return HeadTable.__super__.constructor.apply(this, arguments);
    }

    HeadTable.prototype.tag = 'head';

    HeadTable.prototype.parse = function(data) {
      data.pos = this.offset;
      this.version = data.readInt();
      this.revision = data.readInt();
      this.checkSumAdjustment = data.readInt();
      this.magicNumber = data.readInt();
      this.flags = data.readShort();
      this.unitsPerEm = data.readShort();
      this.created = data.readLongLong();
      this.modified = data.readLongLong();
      this.xMin = data.readShort();
      this.yMin = data.readShort();
      this.xMax = data.readShort();
      this.yMax = data.readShort();
      this.macStyle = data.readShort();
      this.lowestRecPPEM = data.readShort();
      this.fontDirectionHint = data.readShort();
      this.indexToLocFormat = data.readShort();
      return this.glyphDataFormat = data.readShort();
    };

    HeadTable.prototype.encode = function(loca) {
      var table;
      table = new Data;
      table.writeInt(this.version);
      table.writeInt(this.revision);
      table.writeInt(this.checkSumAdjustment);
      table.writeInt(this.magicNumber);
      table.writeShort(this.flags);
      table.writeShort(this.unitsPerEm);
      table.writeLongLong(this.created);
      table.writeLongLong(this.modified);
      table.writeShort(this.xMin);
      table.writeShort(this.yMin);
      table.writeShort(this.xMax);
      table.writeShort(this.yMax);
      table.writeShort(this.macStyle);
      table.writeShort(this.lowestRecPPEM);
      table.writeShort(this.fontDirectionHint);
      table.writeShort(loca.type);
      table.writeShort(this.glyphDataFormat);
      return table.data;
    };

    return HeadTable;

  })(Table);

  module.exports = HeadTable;

}).call(this);

},{"../../data":32,"../table":39}],43:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, HheaTable, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  HheaTable = (function(_super) {
    __extends(HheaTable, _super);

    function HheaTable() {
      return HheaTable.__super__.constructor.apply(this, arguments);
    }

    HheaTable.prototype.tag = 'hhea';

    HheaTable.prototype.parse = function(data) {
      data.pos = this.offset;
      this.version = data.readInt();
      this.ascender = data.readShort();
      this.decender = data.readShort();
      this.lineGap = data.readShort();
      this.advanceWidthMax = data.readShort();
      this.minLeftSideBearing = data.readShort();
      this.minRightSideBearing = data.readShort();
      this.xMaxExtent = data.readShort();
      this.caretSlopeRise = data.readShort();
      this.caretSlopeRun = data.readShort();
      this.caretOffset = data.readShort();
      data.pos += 4 * 2;
      this.metricDataFormat = data.readShort();
      return this.numberOfMetrics = data.readUInt16();
    };

    HheaTable.prototype.encode = function(ids) {
      var i, table, _i, _ref;
      table = new Data;
      table.writeInt(this.version);
      table.writeShort(this.ascender);
      table.writeShort(this.decender);
      table.writeShort(this.lineGap);
      table.writeShort(this.advanceWidthMax);
      table.writeShort(this.minLeftSideBearing);
      table.writeShort(this.minRightSideBearing);
      table.writeShort(this.xMaxExtent);
      table.writeShort(this.caretSlopeRise);
      table.writeShort(this.caretSlopeRun);
      table.writeShort(this.caretOffset);
      for (i = _i = 0, _ref = 4 * 2; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        table.writeByte(0);
      }
      table.writeShort(this.metricDataFormat);
      table.writeUInt16(ids.length);
      return table.data;
    };

    return HheaTable;

  })(Table);

  module.exports = HheaTable;

}).call(this);

},{"../../data":32,"../table":39}],44:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, HmtxTable, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  HmtxTable = (function(_super) {
    __extends(HmtxTable, _super);

    function HmtxTable() {
      return HmtxTable.__super__.constructor.apply(this, arguments);
    }

    HmtxTable.prototype.tag = 'hmtx';

    HmtxTable.prototype.parse = function(data) {
      var i, last, lsbCount, m, _i, _j, _ref, _results;
      data.pos = this.offset;
      this.metrics = [];
      for (i = _i = 0, _ref = this.file.hhea.numberOfMetrics; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        this.metrics.push({
          advance: data.readUInt16(),
          lsb: data.readInt16()
        });
      }
      lsbCount = this.file.maxp.numGlyphs - this.file.hhea.numberOfMetrics;
      this.leftSideBearings = (function() {
        var _j, _results;
        _results = [];
        for (i = _j = 0; 0 <= lsbCount ? _j < lsbCount : _j > lsbCount; i = 0 <= lsbCount ? ++_j : --_j) {
          _results.push(data.readInt16());
        }
        return _results;
      })();
      this.widths = (function() {
        var _j, _len, _ref1, _results;
        _ref1 = this.metrics;
        _results = [];
        for (_j = 0, _len = _ref1.length; _j < _len; _j++) {
          m = _ref1[_j];
          _results.push(m.advance);
        }
        return _results;
      }).call(this);
      last = this.widths[this.widths.length - 1];
      _results = [];
      for (i = _j = 0; 0 <= lsbCount ? _j < lsbCount : _j > lsbCount; i = 0 <= lsbCount ? ++_j : --_j) {
        _results.push(this.widths.push(last));
      }
      return _results;
    };

    HmtxTable.prototype.forGlyph = function(id) {
      var metrics;
      if (id in this.metrics) {
        return this.metrics[id];
      }
      return metrics = {
        advance: this.metrics[this.metrics.length - 1].advance,
        lsb: this.leftSideBearings[id - this.metrics.length]
      };
    };

    HmtxTable.prototype.encode = function(mapping) {
      var id, metric, table, _i, _len;
      table = new Data;
      for (_i = 0, _len = mapping.length; _i < _len; _i++) {
        id = mapping[_i];
        metric = this.forGlyph(id);
        table.writeUInt16(metric.advance);
        table.writeUInt16(metric.lsb);
      }
      return table.data;
    };

    return HmtxTable;

  })(Table);

  module.exports = HmtxTable;

}).call(this);

},{"../../data":32,"../table":39}],45:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, LocaTable, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  LocaTable = (function(_super) {
    __extends(LocaTable, _super);

    function LocaTable() {
      return LocaTable.__super__.constructor.apply(this, arguments);
    }

    LocaTable.prototype.tag = 'loca';

    LocaTable.prototype.parse = function(data) {
      var format, i;
      data.pos = this.offset;
      format = this.file.head.indexToLocFormat;
      if (format === 0) {
        return this.offsets = (function() {
          var _i, _ref, _results;
          _results = [];
          for (i = _i = 0, _ref = this.length; _i < _ref; i = _i += 2) {
            _results.push(data.readUInt16() * 2);
          }
          return _results;
        }).call(this);
      } else {
        return this.offsets = (function() {
          var _i, _ref, _results;
          _results = [];
          for (i = _i = 0, _ref = this.length; _i < _ref; i = _i += 4) {
            _results.push(data.readUInt32());
          }
          return _results;
        }).call(this);
      }
    };

    LocaTable.prototype.indexOf = function(id) {
      return this.offsets[id];
    };

    LocaTable.prototype.lengthOf = function(id) {
      return this.offsets[id + 1] - this.offsets[id];
    };

    LocaTable.prototype.encode = function(offsets) {
      var o, offset, ret, table, _i, _j, _k, _len, _len1, _len2, _ref;
      table = new Data;
      for (_i = 0, _len = offsets.length; _i < _len; _i++) {
        offset = offsets[_i];
        if (!(offset > 0xFFFF)) {
          continue;
        }
        _ref = this.offsets;
        for (_j = 0, _len1 = _ref.length; _j < _len1; _j++) {
          o = _ref[_j];
          table.writeUInt32(o);
        }
        return ret = {
          format: 1,
          table: table.data
        };
      }
      for (_k = 0, _len2 = offsets.length; _k < _len2; _k++) {
        o = offsets[_k];
        table.writeUInt16(o / 2);
      }
      return ret = {
        format: 0,
        table: table.data
      };
    };

    return LocaTable;

  })(Table);

  module.exports = LocaTable;

}).call(this);

},{"../../data":32,"../table":39}],46:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, MaxpTable, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  MaxpTable = (function(_super) {
    __extends(MaxpTable, _super);

    function MaxpTable() {
      return MaxpTable.__super__.constructor.apply(this, arguments);
    }

    MaxpTable.prototype.tag = 'maxp';

    MaxpTable.prototype.parse = function(data) {
      data.pos = this.offset;
      this.version = data.readInt();
      this.numGlyphs = data.readUInt16();
      this.maxPoints = data.readUInt16();
      this.maxContours = data.readUInt16();
      this.maxCompositePoints = data.readUInt16();
      this.maxComponentContours = data.readUInt16();
      this.maxZones = data.readUInt16();
      this.maxTwilightPoints = data.readUInt16();
      this.maxStorage = data.readUInt16();
      this.maxFunctionDefs = data.readUInt16();
      this.maxInstructionDefs = data.readUInt16();
      this.maxStackElements = data.readUInt16();
      this.maxSizeOfInstructions = data.readUInt16();
      this.maxComponentElements = data.readUInt16();
      return this.maxComponentDepth = data.readUInt16();
    };

    MaxpTable.prototype.encode = function(ids) {
      var table;
      table = new Data;
      table.writeInt(this.version);
      table.writeUInt16(ids.length);
      table.writeUInt16(this.maxPoints);
      table.writeUInt16(this.maxContours);
      table.writeUInt16(this.maxCompositePoints);
      table.writeUInt16(this.maxComponentContours);
      table.writeUInt16(this.maxZones);
      table.writeUInt16(this.maxTwilightPoints);
      table.writeUInt16(this.maxStorage);
      table.writeUInt16(this.maxFunctionDefs);
      table.writeUInt16(this.maxInstructionDefs);
      table.writeUInt16(this.maxStackElements);
      table.writeUInt16(this.maxSizeOfInstructions);
      table.writeUInt16(this.maxComponentElements);
      table.writeUInt16(this.maxComponentDepth);
      return table.data;
    };

    return MaxpTable;

  })(Table);

  module.exports = MaxpTable;

}).call(this);

},{"../../data":32,"../table":39}],47:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, NameEntry, NameTable, Table, utils,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  utils = _dereq_('../utils');

  NameTable = (function(_super) {
    var subsetTag;

    __extends(NameTable, _super);

    function NameTable() {
      return NameTable.__super__.constructor.apply(this, arguments);
    }

    NameTable.prototype.tag = 'name';

    NameTable.prototype.parse = function(data) {
      var count, entries, entry, format, i, name, stringOffset, strings, text, _i, _j, _len, _name;
      data.pos = this.offset;
      format = data.readShort();
      count = data.readShort();
      stringOffset = data.readShort();
      entries = [];
      for (i = _i = 0; 0 <= count ? _i < count : _i > count; i = 0 <= count ? ++_i : --_i) {
        entries.push({
          platformID: data.readShort(),
          encodingID: data.readShort(),
          languageID: data.readShort(),
          nameID: data.readShort(),
          length: data.readShort(),
          offset: this.offset + stringOffset + data.readShort()
        });
      }
      strings = {};
      for (i = _j = 0, _len = entries.length; _j < _len; i = ++_j) {
        entry = entries[i];
        data.pos = entry.offset;
        text = data.readString(entry.length);
        name = new NameEntry(text, entry);
        if (strings[_name = entry.nameID] == null) {
          strings[_name] = [];
        }
        strings[entry.nameID].push(name);
      }
      this.strings = strings;
      this.copyright = strings[0];
      this.fontFamily = strings[1];
      this.fontSubfamily = strings[2];
      this.uniqueSubfamily = strings[3];
      this.fontName = strings[4];
      this.version = strings[5];
      this.postscriptName = strings[6][0].raw.replace(/[\x00-\x19\x80-\xff]/g, "");
      this.trademark = strings[7];
      this.manufacturer = strings[8];
      this.designer = strings[9];
      this.description = strings[10];
      this.vendorUrl = strings[11];
      this.designerUrl = strings[12];
      this.license = strings[13];
      this.licenseUrl = strings[14];
      this.preferredFamily = strings[15];
      this.preferredSubfamily = strings[17];
      this.compatibleFull = strings[18];
      return this.sampleText = strings[19];
    };

    subsetTag = "AAAAAA";

    NameTable.prototype.encode = function() {
      var id, list, nameID, nameTable, postscriptName, strCount, strTable, string, strings, table, val, _i, _len, _ref;
      strings = {};
      _ref = this.strings;
      for (id in _ref) {
        val = _ref[id];
        strings[id] = val;
      }
      postscriptName = new NameEntry("" + subsetTag + "+" + this.postscriptName, {
        platformID: 1,
        encodingID: 0,
        languageID: 0
      });
      strings[6] = [postscriptName];
      subsetTag = utils.successorOf(subsetTag);
      strCount = 0;
      for (id in strings) {
        list = strings[id];
        if (list != null) {
          strCount += list.length;
        }
      }
      table = new Data;
      strTable = new Data;
      table.writeShort(0);
      table.writeShort(strCount);
      table.writeShort(6 + 12 * strCount);
      for (nameID in strings) {
        list = strings[nameID];
        if (list != null) {
          for (_i = 0, _len = list.length; _i < _len; _i++) {
            string = list[_i];
            table.writeShort(string.platformID);
            table.writeShort(string.encodingID);
            table.writeShort(string.languageID);
            table.writeShort(nameID);
            table.writeShort(string.length);
            table.writeShort(strTable.pos);
            strTable.writeString(string.raw);
          }
        }
      }
      return nameTable = {
        postscriptName: postscriptName.raw,
        table: table.data.concat(strTable.data)
      };
    };

    return NameTable;

  })(Table);

  module.exports = NameTable;

  NameEntry = (function() {
    function NameEntry(raw, entry) {
      this.raw = raw;
      this.length = raw.length;
      this.platformID = entry.platformID;
      this.encodingID = entry.encodingID;
      this.languageID = entry.languageID;
    }

    return NameEntry;

  })();

}).call(this);

},{"../../data":32,"../table":39,"../utils":51}],48:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var OS2Table, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  OS2Table = (function(_super) {
    __extends(OS2Table, _super);

    function OS2Table() {
      return OS2Table.__super__.constructor.apply(this, arguments);
    }

    OS2Table.prototype.tag = 'OS/2';

    OS2Table.prototype.parse = function(data) {
      var i;
      data.pos = this.offset;
      this.version = data.readUInt16();
      this.averageCharWidth = data.readShort();
      this.weightClass = data.readUInt16();
      this.widthClass = data.readUInt16();
      this.type = data.readShort();
      this.ySubscriptXSize = data.readShort();
      this.ySubscriptYSize = data.readShort();
      this.ySubscriptXOffset = data.readShort();
      this.ySubscriptYOffset = data.readShort();
      this.ySuperscriptXSize = data.readShort();
      this.ySuperscriptYSize = data.readShort();
      this.ySuperscriptXOffset = data.readShort();
      this.ySuperscriptYOffset = data.readShort();
      this.yStrikeoutSize = data.readShort();
      this.yStrikeoutPosition = data.readShort();
      this.familyClass = data.readShort();
      this.panose = (function() {
        var _i, _results;
        _results = [];
        for (i = _i = 0; _i < 10; i = ++_i) {
          _results.push(data.readByte());
        }
        return _results;
      })();
      this.charRange = (function() {
        var _i, _results;
        _results = [];
        for (i = _i = 0; _i < 4; i = ++_i) {
          _results.push(data.readInt());
        }
        return _results;
      })();
      this.vendorID = data.readString(4);
      this.selection = data.readShort();
      this.firstCharIndex = data.readShort();
      this.lastCharIndex = data.readShort();
      if (this.version > 0) {
        this.ascent = data.readShort();
        this.descent = data.readShort();
        this.lineGap = data.readShort();
        this.winAscent = data.readShort();
        this.winDescent = data.readShort();
        this.codePageRange = (function() {
          var _i, _results;
          _results = [];
          for (i = _i = 0; _i < 2; i = ++_i) {
            _results.push(data.readInt());
          }
          return _results;
        })();
        if (this.version > 1) {
          this.xHeight = data.readShort();
          this.capHeight = data.readShort();
          this.defaultChar = data.readShort();
          this.breakChar = data.readShort();
          return this.maxContext = data.readShort();
        }
      }
    };

    OS2Table.prototype.encode = function() {
      return this.raw();
    };

    return OS2Table;

  })(Table);

  module.exports = OS2Table;

}).call(this);

},{"../table":39}],49:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var Data, PostTable, Table,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  Table = _dereq_('../table');

  Data = _dereq_('../../data');

  PostTable = (function(_super) {
    var POSTSCRIPT_GLYPHS;

    __extends(PostTable, _super);

    function PostTable() {
      return PostTable.__super__.constructor.apply(this, arguments);
    }

    PostTable.prototype.tag = 'post';

    PostTable.prototype.parse = function(data) {
      var i, length, numberOfGlyphs, _i, _results;
      data.pos = this.offset;
      this.format = data.readInt();
      this.italicAngle = data.readInt();
      this.underlinePosition = data.readShort();
      this.underlineThickness = data.readShort();
      this.isFixedPitch = data.readInt();
      this.minMemType42 = data.readInt();
      this.maxMemType42 = data.readInt();
      this.minMemType1 = data.readInt();
      this.maxMemType1 = data.readInt();
      switch (this.format) {
        case 0x00010000:
          break;
        case 0x00020000:
          numberOfGlyphs = data.readUInt16();
          this.glyphNameIndex = [];
          for (i = _i = 0; 0 <= numberOfGlyphs ? _i < numberOfGlyphs : _i > numberOfGlyphs; i = 0 <= numberOfGlyphs ? ++_i : --_i) {
            this.glyphNameIndex.push(data.readUInt16());
          }
          this.names = [];
          _results = [];
          while (data.pos < this.offset + this.length) {
            length = data.readByte();
            _results.push(this.names.push(data.readString(length)));
          }
          return _results;
          break;
        case 0x00025000:
          numberOfGlyphs = data.readUInt16();
          return this.offsets = data.read(numberOfGlyphs);
        case 0x00030000:
          break;
        case 0x00040000:
          return this.map = (function() {
            var _j, _ref, _results1;
            _results1 = [];
            for (i = _j = 0, _ref = this.file.maxp.numGlyphs; 0 <= _ref ? _j < _ref : _j > _ref; i = 0 <= _ref ? ++_j : --_j) {
              _results1.push(data.readUInt32());
            }
            return _results1;
          }).call(this);
      }
    };

    PostTable.prototype.glyphFor = function(code) {
      var index;
      switch (this.format) {
        case 0x00010000:
          return POSTSCRIPT_GLYPHS[code] || '.notdef';
        case 0x00020000:
          index = this.glyphNameIndex[code];
          if (index <= 257) {
            return POSTSCRIPT_GLYPHS[index];
          } else {
            return this.names[index - 258] || '.notdef';
          }
          break;
        case 0x00025000:
          return POSTSCRIPT_GLYPHS[code + this.offsets[code]] || '.notdef';
        case 0x00030000:
          return '.notdef';
        case 0x00040000:
          return this.map[code] || 0xFFFF;
      }
    };

    PostTable.prototype.encode = function(mapping) {
      var id, index, indexes, position, post, raw, string, strings, table, _i, _j, _k, _len, _len1, _len2;
      if (!this.exists) {
        return null;
      }
      raw = this.raw();
      if (this.format === 0x00030000) {
        return raw;
      }
      table = new Data(raw.slice(0, 32));
      table.writeUInt32(0x00020000);
      table.pos = 32;
      indexes = [];
      strings = [];
      for (_i = 0, _len = mapping.length; _i < _len; _i++) {
        id = mapping[_i];
        post = this.glyphFor(id);
        position = POSTSCRIPT_GLYPHS.indexOf(post);
        if (position !== -1) {
          indexes.push(position);
        } else {
          indexes.push(257 + strings.length);
          strings.push(post);
        }
      }
      table.writeUInt16(Object.keys(mapping).length);
      for (_j = 0, _len1 = indexes.length; _j < _len1; _j++) {
        index = indexes[_j];
        table.writeUInt16(index);
      }
      for (_k = 0, _len2 = strings.length; _k < _len2; _k++) {
        string = strings[_k];
        table.writeByte(string.length);
        table.writeString(string);
      }
      return table.data;
    };

    POSTSCRIPT_GLYPHS = '.notdef .null nonmarkingreturn space exclam quotedbl numbersign dollar percent\nampersand quotesingle parenleft parenright asterisk plus comma hyphen period slash\nzero one two three four five six seven eight nine colon semicolon less equal greater\nquestion at A B C D E F G H I J K L M N O P Q R S T U V W X Y Z\nbracketleft backslash bracketright asciicircum underscore grave\na b c d e f g h i j k l m n o p q r s t u v w x y z\nbraceleft bar braceright asciitilde Adieresis Aring Ccedilla Eacute Ntilde Odieresis\nUdieresis aacute agrave acircumflex adieresis atilde aring ccedilla eacute egrave\necircumflex edieresis iacute igrave icircumflex idieresis ntilde oacute ograve\nocircumflex odieresis otilde uacute ugrave ucircumflex udieresis dagger degree cent\nsterling section bullet paragraph germandbls registered copyright trademark acute\ndieresis notequal AE Oslash infinity plusminus lessequal greaterequal yen mu\npartialdiff summation product pi integral ordfeminine ordmasculine Omega ae oslash\nquestiondown exclamdown logicalnot radical florin approxequal Delta guillemotleft\nguillemotright ellipsis nonbreakingspace Agrave Atilde Otilde OE oe endash emdash\nquotedblleft quotedblright quoteleft quoteright divide lozenge ydieresis Ydieresis\nfraction currency guilsinglleft guilsinglright fi fl daggerdbl periodcentered\nquotesinglbase quotedblbase perthousand Acircumflex Ecircumflex Aacute Edieresis\nEgrave Iacute Icircumflex Idieresis Igrave Oacute Ocircumflex apple Ograve Uacute\nUcircumflex Ugrave dotlessi circumflex tilde macron breve dotaccent ring cedilla\nhungarumlaut ogonek caron Lslash lslash Scaron scaron Zcaron zcaron brokenbar Eth\neth Yacute yacute Thorn thorn minus multiply onesuperior twosuperior threesuperior\nonehalf onequarter threequarters franc Gbreve gbreve Idotaccent Scedilla scedilla\nCacute cacute Ccaron ccaron dcroat'.split(/\s+/g);

    return PostTable;

  })(Table);

  module.exports = PostTable;

}).call(this);

},{"../../data":32,"../table":39}],50:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var CmapTable, DFont, Data, Directory, GlyfTable, HeadTable, HheaTable, HmtxTable, LocaTable, MaxpTable, NameTable, OS2Table, PostTable, TTFFont, fs;

  fs = _dereq_('fs');

  Data = _dereq_('../data');

  DFont = _dereq_('./dfont');

  Directory = _dereq_('./directory');

  NameTable = _dereq_('./tables/name');

  HeadTable = _dereq_('./tables/head');

  CmapTable = _dereq_('./tables/cmap');

  HmtxTable = _dereq_('./tables/hmtx');

  HheaTable = _dereq_('./tables/hhea');

  MaxpTable = _dereq_('./tables/maxp');

  PostTable = _dereq_('./tables/post');

  OS2Table = _dereq_('./tables/os2');

  LocaTable = _dereq_('./tables/loca');

  GlyfTable = _dereq_('./tables/glyf');

  TTFFont = (function() {
    TTFFont.open = function(filename, name) {
      var contents;
      contents = fs.readFileSync(filename);
      return new TTFFont(contents, name);
    };

    TTFFont.fromDFont = function(filename, family) {
      var dfont;
      dfont = DFont.open(filename);
      return new TTFFont(dfont.getNamedFont(family));
    };

    TTFFont.fromBuffer = function(buffer, family) {
      var dfont, e, ttf;
      try {
        ttf = new TTFFont(buffer, family);
        if (!(ttf.head.exists && ttf.name.exists && ttf.cmap.exists)) {
          dfont = new DFont(buffer);
          ttf = new TTFFont(dfont.getNamedFont(family));
          if (!(ttf.head.exists && ttf.name.exists && ttf.cmap.exists)) {
            throw new Error('Invalid TTF file in DFont');
          }
        }
        return ttf;
      } catch (_error) {
        e = _error;
        throw new Error('Unknown font format in buffer: ' + e.message);
      }
    };

    function TTFFont(rawData, name) {
      var data, i, numFonts, offset, offsets, version, _i, _j, _len;
      this.rawData = rawData;
      data = this.contents = new Data(rawData);
      if (data.readString(4) === 'ttcf') {
        if (!name) {
          throw new Error("Must specify a font name for TTC files.");
        }
        version = data.readInt();
        numFonts = data.readInt();
        offsets = [];
        for (i = _i = 0; 0 <= numFonts ? _i < numFonts : _i > numFonts; i = 0 <= numFonts ? ++_i : --_i) {
          offsets[i] = data.readInt();
        }
        for (i = _j = 0, _len = offsets.length; _j < _len; i = ++_j) {
          offset = offsets[i];
          data.pos = offset;
          this.parse();
          if (this.name.postscriptName === name) {
            return;
          }
        }
        throw new Error("Font " + name + " not found in TTC file.");
      } else {
        data.pos = 0;
        this.parse();
      }
    }

    TTFFont.prototype.parse = function() {
      this.directory = new Directory(this.contents);
      this.head = new HeadTable(this);
      this.name = new NameTable(this);
      this.cmap = new CmapTable(this);
      this.hhea = new HheaTable(this);
      this.maxp = new MaxpTable(this);
      this.hmtx = new HmtxTable(this);
      this.post = new PostTable(this);
      this.os2 = new OS2Table(this);
      this.loca = new LocaTable(this);
      this.glyf = new GlyfTable(this);
      this.ascender = (this.os2.exists && this.os2.ascender) || this.hhea.ascender;
      this.decender = (this.os2.exists && this.os2.decender) || this.hhea.decender;
      this.lineGap = (this.os2.exists && this.os2.lineGap) || this.hhea.lineGap;
      return this.bbox = [this.head.xMin, this.head.yMin, this.head.xMax, this.head.yMax];
    };

    TTFFont.prototype.characterToGlyph = function(character) {
      var _ref;
      return ((_ref = this.cmap.unicode) != null ? _ref.codeMap[character] : void 0) || 0;
    };

    TTFFont.prototype.widthOfGlyph = function(glyph) {
      var scale;
      scale = 1000.0 / this.head.unitsPerEm;
      return this.hmtx.forGlyph(glyph).advance * scale;
    };

    return TTFFont;

  })();

  module.exports = TTFFont;

}).call(this);

},{"../data":32,"./dfont":36,"./directory":37,"./tables/cmap":40,"./tables/glyf":41,"./tables/head":42,"./tables/hhea":43,"./tables/hmtx":44,"./tables/loca":45,"./tables/maxp":46,"./tables/name":47,"./tables/os2":48,"./tables/post":49,"fs":"x/K9gc"}],51:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1

/*
 * An implementation of Ruby's string.succ method.
 * By Devon Govett
 *
 * Returns the successor to str. The successor is calculated by incrementing characters starting 
 * from the rightmost alphanumeric (or the rightmost character if there are no alphanumerics) in the
 * string. Incrementing a digit always results in another digit, and incrementing a letter results in
 * another letter of the same case.
 *
 * If the increment generates a carry, the character to the left of it is incremented. This 
 * process repeats until there is no carry, adding an additional character if necessary.
 *
 * succ("abcd")      == "abce"
 * succ("THX1138")   == "THX1139"
 * succ("<<koala>>") == "<<koalb>>"
 * succ("1999zzz")   == "2000aaa"
 * succ("ZZZ9999")   == "AAAA0000"
 */

(function() {
  exports.successorOf = function(input) {
    var added, alphabet, carry, i, index, isUpperCase, last, length, next, result;
    alphabet = 'abcdefghijklmnopqrstuvwxyz';
    length = alphabet.length;
    result = input;
    i = input.length;
    while (i >= 0) {
      last = input.charAt(--i);
      if (isNaN(last)) {
        index = alphabet.indexOf(last.toLowerCase());
        if (index === -1) {
          next = last;
          carry = true;
        } else {
          next = alphabet.charAt((index + 1) % length);
          isUpperCase = last === last.toUpperCase();
          if (isUpperCase) {
            next = next.toUpperCase();
          }
          carry = index + 1 >= length;
          if (carry && i === 0) {
            added = isUpperCase ? 'A' : 'a';
            result = added + next + result.slice(1);
            break;
          }
        }
      } else {
        next = +last + 1;
        carry = next > 9;
        if (carry) {
          next = 0;
        }
        if (carry && i === 0) {
          result = '1' + next + result.slice(1);
          break;
        }
      }
      result = result.slice(0, i) + next + result.slice(i + 1);
      if (!carry) {
        break;
      }
    }
    return result;
  };

  exports.invert = function(object) {
    var key, ret, val;
    ret = {};
    for (key in object) {
      val = object[key];
      ret[val] = key;
    }
    return ret;
  };

}).call(this);

},{}],52:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var PDFGradient, PDFLinearGradient, PDFRadialGradient,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  PDFGradient = (function() {
    function PDFGradient(doc) {
      this.doc = doc;
      this.stops = [];
      this.embedded = false;
      this.transform = [1, 0, 0, 1, 0, 0];
      this._colorSpace = 'DeviceRGB';
    }

    PDFGradient.prototype.stop = function(pos, color, opacity) {
      if (opacity == null) {
        opacity = 1;
      }
      opacity = Math.max(0, Math.min(1, opacity));
      this.stops.push([pos, this.doc._normalizeColor(color), opacity]);
      return this;
    };

    PDFGradient.prototype.embed = function() {
      var bounds, dx, dy, encode, fn, form, grad, group, gstate, i, last, m, m0, m1, m11, m12, m2, m21, m22, m3, m4, m5, name, pattern, resources, sMask, shader, stop, stops, v, _i, _j, _len, _ref, _ref1, _ref2;
      if (this.embedded || this.stops.length === 0) {
        return;
      }
      this.embedded = true;
      last = this.stops[this.stops.length - 1];
      if (last[0] < 1) {
        this.stops.push([1, last[1], last[2]]);
      }
      bounds = [];
      encode = [];
      stops = [];
      for (i = _i = 0, _ref = this.stops.length - 1; 0 <= _ref ? _i < _ref : _i > _ref; i = 0 <= _ref ? ++_i : --_i) {
        encode.push(0, 1);
        if (i + 2 !== this.stops.length) {
          bounds.push(this.stops[i + 1][0]);
        }
        fn = this.doc.ref({
          FunctionType: 2,
          Domain: [0, 1],
          C0: this.stops[i + 0][1],
          C1: this.stops[i + 1][1],
          N: 1
        });
        stops.push(fn);
        fn.end();
      }
      if (stops.length === 1) {
        fn = stops[0];
      } else {
        fn = this.doc.ref({
          FunctionType: 3,
          Domain: [0, 1],
          Functions: stops,
          Bounds: bounds,
          Encode: encode
        });
        fn.end();
      }
      this.id = 'Sh' + (++this.doc._gradCount);
      m = this.doc._ctm.slice();
      m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3], m4 = m[4], m5 = m[5];
      _ref1 = this.transform, m11 = _ref1[0], m12 = _ref1[1], m21 = _ref1[2], m22 = _ref1[3], dx = _ref1[4], dy = _ref1[5];
      m[0] = m0 * m11 + m2 * m12;
      m[1] = m1 * m11 + m3 * m12;
      m[2] = m0 * m21 + m2 * m22;
      m[3] = m1 * m21 + m3 * m22;
      m[4] = m0 * dx + m2 * dy + m4;
      m[5] = m1 * dx + m3 * dy + m5;
      shader = this.shader(fn);
      shader.end();
      pattern = this.doc.ref({
        Type: 'Pattern',
        PatternType: 2,
        Shading: shader,
        Matrix: (function() {
          var _j, _len, _results;
          _results = [];
          for (_j = 0, _len = m.length; _j < _len; _j++) {
            v = m[_j];
            _results.push(+v.toFixed(5));
          }
          return _results;
        })()
      });
      this.doc.page.patterns[this.id] = pattern;
      pattern.end();
      if (this.stops.some(function(stop) {
        return stop[2] < 1;
      })) {
        grad = this.opacityGradient();
        grad._colorSpace = 'DeviceGray';
        _ref2 = this.stops;
        for (_j = 0, _len = _ref2.length; _j < _len; _j++) {
          stop = _ref2[_j];
          grad.stop(stop[0], [stop[2]]);
        }
        grad = grad.embed();
        group = this.doc.ref({
          Type: 'Group',
          S: 'Transparency',
          CS: 'DeviceGray'
        });
        group.end();
        resources = this.doc.ref({
          ProcSet: ['PDF', 'Text', 'ImageB', 'ImageC', 'ImageI'],
          Shading: {
            Sh1: grad.data.Shading
          }
        });
        resources.end();
        form = this.doc.ref({
          Type: 'XObject',
          Subtype: 'Form',
          FormType: 1,
          BBox: [0, 0, this.doc.page.width, this.doc.page.height],
          Group: group,
          Resources: resources
        });
        form.end("/Sh1 sh");
        sMask = this.doc.ref({
          Type: 'Mask',
          S: 'Luminosity',
          G: form
        });
        sMask.end();
        gstate = this.doc.ref({
          Type: 'ExtGState',
          SMask: sMask
        });
        this.opacity_id = ++this.doc._opacityCount;
        name = "Gs" + this.opacity_id;
        this.doc.page.ext_gstates[name] = gstate;
        gstate.end();
      }
      return pattern;
    };

    PDFGradient.prototype.apply = function(op) {
      if (!this.embedded) {
        this.embed();
      }
      this.doc.addContent("/" + this.id + " " + op);
      if (this.opacity_id) {
        this.doc.addContent("/Gs" + this.opacity_id + " gs");
        return this.doc._sMasked = true;
      }
    };

    return PDFGradient;

  })();

  PDFLinearGradient = (function(_super) {
    __extends(PDFLinearGradient, _super);

    function PDFLinearGradient(doc, x1, y1, x2, y2) {
      this.doc = doc;
      this.x1 = x1;
      this.y1 = y1;
      this.x2 = x2;
      this.y2 = y2;
      PDFLinearGradient.__super__.constructor.apply(this, arguments);
    }

    PDFLinearGradient.prototype.shader = function(fn) {
      return this.doc.ref({
        ShadingType: 2,
        ColorSpace: this._colorSpace,
        Coords: [this.x1, this.y1, this.x2, this.y2],
        Function: fn,
        Extend: [true, true]
      });
    };

    PDFLinearGradient.prototype.opacityGradient = function() {
      return new PDFLinearGradient(this.doc, this.x1, this.y1, this.x2, this.y2);
    };

    return PDFLinearGradient;

  })(PDFGradient);

  PDFRadialGradient = (function(_super) {
    __extends(PDFRadialGradient, _super);

    function PDFRadialGradient(doc, x1, y1, r1, x2, y2, r2) {
      this.doc = doc;
      this.x1 = x1;
      this.y1 = y1;
      this.r1 = r1;
      this.x2 = x2;
      this.y2 = y2;
      this.r2 = r2;
      PDFRadialGradient.__super__.constructor.apply(this, arguments);
    }

    PDFRadialGradient.prototype.shader = function(fn) {
      return this.doc.ref({
        ShadingType: 3,
        ColorSpace: this._colorSpace,
        Coords: [this.x1, this.y1, this.r1, this.x2, this.y2, this.r2],
        Function: fn,
        Extend: [true, true]
      });
    };

    PDFRadialGradient.prototype.opacityGradient = function() {
      return new PDFRadialGradient(this.doc, this.x1, this.y1, this.r1, this.x2, this.y2, this.r2);
    };

    return PDFRadialGradient;

  })(PDFGradient);

  module.exports = {
    PDFGradient: PDFGradient,
    PDFLinearGradient: PDFLinearGradient,
    PDFRadialGradient: PDFRadialGradient
  };

}).call(this);

},{}],53:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1

/*
PDFImage - embeds images in PDF documents
By Devon Govett
 */

(function() {
  var Data, JPEG, PDFImage, PNG, fs;

  fs = _dereq_('fs');

  Data = _dereq_('./data');

  JPEG = _dereq_('./image/jpeg');

  PNG = _dereq_('./image/png');

  PDFImage = (function() {
    function PDFImage() {}

    PDFImage.open = function(src, label) {
      var data, match;
      if (Buffer.isBuffer(src)) {
        data = src;
      } else {
        if (match = /^data:.+;base64,(.*)$/.exec(src)) {
          data = new Buffer(match[1], 'base64');
        } else {
          data = fs.readFileSync(src);
          if (!data) {
            return;
          }
        }
      }
      if (data[0] === 0xff && data[1] === 0xd8) {
        return new JPEG(data, label);
      } else if (data[0] === 0x89 && data.toString('ascii', 1, 4) === 'PNG') {
        return new PNG(data, label);
      } else {
        throw new Error('Unknown image format.');
      }
    };

    return PDFImage;

  })();

  module.exports = PDFImage;

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"./data":32,"./image/jpeg":54,"./image/png":55,"buffer":16,"fs":"x/K9gc"}],54:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var JPEG, fs,
    __indexOf = [].indexOf || function(item) { for (var i = 0, l = this.length; i < l; i++) { if (i in this && this[i] === item) return i; } return -1; };

  fs = _dereq_('fs');

  JPEG = (function() {
    var MARKERS;

    MARKERS = [0xFFC0, 0xFFC1, 0xFFC2, 0xFFC3, 0xFFC5, 0xFFC6, 0xFFC7, 0xFFC8, 0xFFC9, 0xFFCA, 0xFFCB, 0xFFCC, 0xFFCD, 0xFFCE, 0xFFCF];

    function JPEG(data, label) {
      var channels, marker, pos;
      this.data = data;
      this.label = label;
      if (data.readUInt16BE(0) !== 0xFFD8) {
        throw "SOI not found in JPEG";
      }
      pos = 2;
      while (pos < data.length) {
        marker = data.readUInt16BE(pos);
        pos += 2;
        if (__indexOf.call(MARKERS, marker) >= 0) {
          break;
        }
        pos += data.readUInt16BE(pos);
      }
      if (__indexOf.call(MARKERS, marker) < 0) {
        throw "Invalid JPEG.";
      }
      pos += 2;
      this.bits = data[pos++];
      this.height = data.readUInt16BE(pos);
      pos += 2;
      this.width = data.readUInt16BE(pos);
      pos += 2;
      channels = data[pos++];
      this.colorSpace = (function() {
        switch (channels) {
          case 1:
            return 'DeviceGray';
          case 3:
            return 'DeviceRGB';
          case 4:
            return 'DeviceCMYK';
        }
      })();
      this.obj = null;
    }

    JPEG.prototype.embed = function(document) {
      if (this.obj) {
        return;
      }
      this.obj = document.ref({
        Type: 'XObject',
        Subtype: 'Image',
        BitsPerComponent: this.bits,
        Width: this.width,
        Height: this.height,
        ColorSpace: this.colorSpace,
        Filter: 'DCTDecode'
      });
      if (this.colorSpace === 'DeviceCMYK') {
        this.obj.data['Decode'] = [1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0];
      }
      this.obj.end(this.data);
      return this.data = null;
    };

    return JPEG;

  })();

  module.exports = JPEG;

}).call(this);

},{"fs":"x/K9gc"}],55:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1
(function() {
  var PNG, PNGImage, zlib;

  zlib = _dereq_('zlib');

  PNG = _dereq_('png-js');

  PNGImage = (function() {
    function PNGImage(data, label) {
      this.label = label;
      this.image = new PNG(data);
      this.width = this.image.width;
      this.height = this.image.height;
      this.imgData = this.image.imgData;
      this.obj = null;
    }

    PNGImage.prototype.embed = function(document) {
      var mask, palette, params, rgb, val, x, _i, _len;
      this.document = document;
      if (this.obj) {
        return;
      }
      this.obj = document.ref({
        Type: 'XObject',
        Subtype: 'Image',
        BitsPerComponent: this.image.bits,
        Width: this.width,
        Height: this.height,
        Filter: 'FlateDecode'
      });
      if (!this.image.hasAlphaChannel) {
        params = document.ref({
          Predictor: 15,
          Colors: this.image.colors,
          BitsPerComponent: this.image.bits,
          Columns: this.width
        });
        this.obj.data['DecodeParms'] = params;
        params.end();
      }
      if (this.image.palette.length === 0) {
        this.obj.data['ColorSpace'] = this.image.colorSpace;
      } else {
        palette = document.ref();
        palette.end(new Buffer(this.image.palette));
        this.obj.data['ColorSpace'] = ['Indexed', 'DeviceRGB', (this.image.palette.length / 3) - 1, palette];
      }
      if (this.image.transparency.grayscale) {
        val = this.image.transparency.greyscale;
        return this.obj.data['Mask'] = [val, val];
      } else if (this.image.transparency.rgb) {
        rgb = this.image.transparency.rgb;
        mask = [];
        for (_i = 0, _len = rgb.length; _i < _len; _i++) {
          x = rgb[_i];
          mask.push(x, x);
        }
        return this.obj.data['Mask'] = mask;
      } else if (this.image.transparency.indexed) {
        return this.loadIndexedAlphaChannel();
      } else if (this.image.hasAlphaChannel) {
        return this.splitAlphaChannel();
      } else {
        return this.finalize();
      }
    };

    PNGImage.prototype.finalize = function() {
      var sMask;
      if (this.alphaChannel) {
        sMask = this.document.ref({
          Type: 'XObject',
          Subtype: 'Image',
          Height: this.height,
          Width: this.width,
          BitsPerComponent: 8,
          Filter: 'FlateDecode',
          ColorSpace: 'DeviceGray',
          Decode: [0, 1]
        });
        sMask.end(this.alphaChannel);
        this.obj.data['SMask'] = sMask;
      }
      this.obj.end(this.imgData);
      this.image = null;
      return this.imgData = null;
    };

    PNGImage.prototype.splitAlphaChannel = function() {
      return this.image.decodePixels((function(_this) {
        return function(pixels) {
          var a, alphaChannel, colorByteSize, done, i, imgData, len, p, pixelCount;
          colorByteSize = _this.image.colors * _this.image.bits / 8;
          pixelCount = _this.width * _this.height;
          imgData = new Buffer(pixelCount * colorByteSize);
          alphaChannel = new Buffer(pixelCount);
          i = p = a = 0;
          len = pixels.length;
          while (i < len) {
            imgData[p++] = pixels[i++];
            imgData[p++] = pixels[i++];
            imgData[p++] = pixels[i++];
            alphaChannel[a++] = pixels[i++];
          }
          done = 0;
          zlib.deflate(imgData, function(err, imgData) {
            _this.imgData = imgData;
            if (err) {
              throw err;
            }
            if (++done === 2) {
              return _this.finalize();
            }
          });
          return zlib.deflate(alphaChannel, function(err, alphaChannel) {
            _this.alphaChannel = alphaChannel;
            if (err) {
              throw err;
            }
            if (++done === 2) {
              return _this.finalize();
            }
          });
        };
      })(this));
    };

    PNGImage.prototype.loadIndexedAlphaChannel = function(fn) {
      var transparency;
      transparency = this.image.transparency.indexed;
      return this.image.decodePixels((function(_this) {
        return function(pixels) {
          var alphaChannel, i, j, _i, _ref;
          alphaChannel = new Buffer(_this.width * _this.height);
          i = 0;
          for (j = _i = 0, _ref = pixels.length; _i < _ref; j = _i += 1) {
            alphaChannel[i++] = transparency[pixels[j]];
          }
          return zlib.deflate(alphaChannel, function(err, alphaChannel) {
            _this.alphaChannel = alphaChannel;
            if (err) {
              throw err;
            }
            return _this.finalize();
          });
        };
      })(this));
    };

    return PNGImage;

  })();

  module.exports = PNGImage;

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"buffer":16,"png-js":72,"zlib":15}],56:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var EventEmitter, LineBreaker, LineWrapper,
    __hasProp = {}.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor(); child.__super__ = parent.prototype; return child; };

  EventEmitter = _dereq_('events').EventEmitter;

  LineBreaker = _dereq_('linebreak');

  LineWrapper = (function(_super) {
    __extends(LineWrapper, _super);

    function LineWrapper(document, options) {
      var _ref;
      this.document = document;
      this.indent = options.indent || 0;
      this.characterSpacing = options.characterSpacing || 0;
      this.wordSpacing = options.wordSpacing === 0;
      this.columns = options.columns || 1;
      this.columnGap = (_ref = options.columnGap) != null ? _ref : 18;
      this.lineWidth = (options.width - (this.columnGap * (this.columns - 1))) / this.columns;
      this.spaceLeft = this.lineWidth;
      this.startX = this.document.x;
      this.startY = this.document.y;
      this.column = 1;
      this.ellipsis = options.ellipsis;
      this.continuedX = 0;
      if (options.height != null) {
        this.height = options.height;
        this.maxY = this.startY + options.height;
      } else {
        this.maxY = this.document.page.maxY();
      }
      this.on('firstLine', (function(_this) {
        return function(options) {
          var indent;
          indent = _this.continuedX || _this.indent;
          _this.document.x += indent;
          _this.lineWidth -= indent;
          return _this.once('line', function() {
            _this.document.x -= indent;
            _this.lineWidth += indent;
            if (options.continued && !_this.continuedX) {
              _this.continuedX = _this.indent;
            }
            if (!options.continued) {
              return _this.continuedX = 0;
            }
          });
        };
      })(this));
      this.on('lastLine', (function(_this) {
        return function(options) {
          var align;
          align = options.align;
          if (align === 'justify') {
            options.align = 'left';
          }
          _this.lastLine = true;
          return _this.once('line', function() {
            _this.document.y += options.paragraphGap || 0;
            options.align = align;
            return _this.lastLine = false;
          });
        };
      })(this));
    }

    LineWrapper.prototype.wordWidth = function(word) {
      return this.document.widthOfString(word, this) + this.characterSpacing + this.wordSpacing;
    };

    LineWrapper.prototype.eachWord = function(text, fn) {
      var bk, breaker, fbk, l, last, lbk, shouldContinue, w, word, wordWidths;
      breaker = new LineBreaker(text);
      last = null;
      wordWidths = {};
      while (bk = breaker.nextBreak()) {
        word = text.slice((last != null ? last.position : void 0) || 0, bk.position);
        w = wordWidths[word] != null ? wordWidths[word] : wordWidths[word] = this.wordWidth(word);
        if (w > this.lineWidth + this.continuedX) {
          lbk = last;
          fbk = {};
          while (word.length) {
            l = word.length;
            while (w > this.spaceLeft) {
              w = this.wordWidth(word.slice(0, --l));
            }
            fbk.required = l < word.length;
            shouldContinue = fn(word.slice(0, l), w, fbk, lbk);
            lbk = {
              required: false
            };
            word = word.slice(l);
            w = this.wordWidth(word);
            if (shouldContinue === false) {
              break;
            }
          }
        } else {
          shouldContinue = fn(word, w, bk, last);
        }
        if (shouldContinue === false) {
          break;
        }
        last = bk;
      }
    };

    LineWrapper.prototype.wrap = function(text, options) {
      var buffer, emitLine, lc, nextY, textWidth, wc, y;
      if (options.indent != null) {
        this.indent = options.indent;
      }
      if (options.characterSpacing != null) {
        this.characterSpacing = options.characterSpacing;
      }
      if (options.wordSpacing != null) {
        this.wordSpacing = options.wordSpacing;
      }
      if (options.ellipsis != null) {
        this.ellipsis = options.ellipsis;
      }
      nextY = this.document.y + this.document.currentLineHeight(true);
      if (this.document.y > this.maxY || nextY > this.maxY) {
        this.nextSection();
      }
      buffer = '';
      textWidth = 0;
      wc = 0;
      lc = 0;
      y = this.document.y;
      emitLine = (function(_this) {
        return function() {
          options.textWidth = textWidth + _this.wordSpacing * (wc - 1);
          options.wordCount = wc;
          options.lineWidth = _this.lineWidth;
          y = _this.document.y;
          _this.emit('line', buffer, options, _this);
          return lc++;
        };
      })(this);
      this.emit('sectionStart', options, this);
      this.eachWord(text, (function(_this) {
        return function(word, w, bk, last) {
          var lh, shouldContinue;
          if ((last == null) || last.required) {
            _this.emit('firstLine', options, _this);
            _this.spaceLeft = _this.lineWidth;
          }
          if (w <= _this.spaceLeft) {
            buffer += word;
            textWidth += w;
            wc++;
          }
          if (bk.required || w > _this.spaceLeft) {
            if (bk.required) {
              _this.emit('lastLine', options, _this);
            }
            lh = _this.document.currentLineHeight(true);
            if ((_this.height != null) && _this.ellipsis && _this.document.y + lh * 2 > _this.maxY && _this.column >= _this.columns) {
              if (_this.ellipsis === true) {
                _this.ellipsis = '…';
              }
              buffer = buffer.replace(/\s+$/, '');
              textWidth = _this.wordWidth(buffer + _this.ellipsis);
              while (textWidth > _this.lineWidth) {
                buffer = buffer.slice(0, -1).replace(/\s+$/, '');
                textWidth = _this.wordWidth(buffer + _this.ellipsis);
              }
              buffer = buffer + _this.ellipsis;
            }
            emitLine();
            if (_this.document.y + lh > _this.maxY) {
              shouldContinue = _this.nextSection();
              if (!shouldContinue) {
                wc = 0;
                buffer = '';
                return false;
              }
            }
            if (bk.required) {
              if (w > _this.spaceLeft) {
                buffer = word;
                textWidth = w;
                wc = 1;
                emitLine();
              }
              _this.spaceLeft = _this.lineWidth;
              buffer = '';
              textWidth = 0;
              return wc = 0;
            } else {
              _this.spaceLeft = _this.lineWidth - w;
              buffer = word;
              textWidth = w;
              return wc = 1;
            }
          } else {
            return _this.spaceLeft -= w;
          }
        };
      })(this));
      if (wc > 0) {
        this.emit('lastLine', options, this);
        emitLine();
      }
      this.emit('sectionEnd', options, this);
      if (options.continued === true) {
        if (lc > 1) {
          this.continuedX = 0;
        }
        this.continuedX += options.textWidth;
        return this.document.y = y;
      } else {
        return this.document.x = this.startX;
      }
    };

    LineWrapper.prototype.nextSection = function(options) {
      var _ref;
      this.emit('sectionEnd', options, this);
      if (++this.column > this.columns) {
        if (this.height != null) {
          return false;
        }
        this.document.addPage();
        this.column = 1;
        this.startY = this.document.page.margins.top;
        this.maxY = this.document.page.maxY();
        this.document.x = this.startX;
        if (this.document._fillColor) {
          (_ref = this.document).fillColor.apply(_ref, this.document._fillColor);
        }
        this.emit('pageBreak', options, this);
      } else {
        this.document.x += this.lineWidth + this.columnGap;
        this.document.y = this.startY;
        this.emit('columnBreak', options, this);
      }
      this.emit('sectionStart', options, this);
      return true;
    };

    return LineWrapper;

  })(EventEmitter);

  module.exports = LineWrapper;

}).call(this);

},{"events":19,"linebreak":70}],57:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var PDFObject;

  PDFObject = _dereq_('../object');

  module.exports = {
    annotate: function(x, y, w, h, options) {
      var key, ref, val;
      options.Type = 'Annot';
      options.Rect = this._convertRect(x, y, w, h);
      options.Border = [0, 0, 0];
      if (options.Subtype !== 'Link') {
        if (options.C == null) {
          options.C = this._normalizeColor(options.color || [0, 0, 0]);
        }
      }
      delete options.color;
      if (typeof options.Dest === 'string') {
        options.Dest = PDFObject.s(options.Dest);
      }
      for (key in options) {
        val = options[key];
        options[key[0].toUpperCase() + key.slice(1)] = val;
      }
      ref = this.ref(options);
      this.page.annotations.push(ref);
      ref.end();
      return this;
    },
    note: function(x, y, w, h, contents, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Text';
      options.Contents = PDFObject.s(contents, true);
      options.Name = 'Comment';
      if (options.color == null) {
        options.color = [243, 223, 92];
      }
      return this.annotate(x, y, w, h, options);
    },
    link: function(x, y, w, h, url, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Link';
      options.A = this.ref({
        S: 'URI',
        URI: PDFObject.s(url)
      });
      options.A.end();
      return this.annotate(x, y, w, h, options);
    },
    _markup: function(x, y, w, h, options) {
      var x1, x2, y1, y2, _ref;
      if (options == null) {
        options = {};
      }
      _ref = this._convertRect(x, y, w, h), x1 = _ref[0], y1 = _ref[1], x2 = _ref[2], y2 = _ref[3];
      options.QuadPoints = [x1, y2, x2, y2, x1, y1, x2, y1];
      options.Contents = PDFObject.s('');
      return this.annotate(x, y, w, h, options);
    },
    highlight: function(x, y, w, h, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Highlight';
      if (options.color == null) {
        options.color = [241, 238, 148];
      }
      return this._markup(x, y, w, h, options);
    },
    underline: function(x, y, w, h, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Underline';
      return this._markup(x, y, w, h, options);
    },
    strike: function(x, y, w, h, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'StrikeOut';
      return this._markup(x, y, w, h, options);
    },
    lineAnnotation: function(x1, y1, x2, y2, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Line';
      options.Contents = PDFObject.s('');
      options.L = [x1, this.page.height - y1, x2, this.page.height - y2];
      return this.annotate(x1, y1, x2, y2, options);
    },
    rectAnnotation: function(x, y, w, h, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Square';
      options.Contents = PDFObject.s('');
      return this.annotate(x, y, w, h, options);
    },
    ellipseAnnotation: function(x, y, w, h, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'Circle';
      options.Contents = PDFObject.s('');
      return this.annotate(x, y, w, h, options);
    },
    textAnnotation: function(x, y, w, h, text, options) {
      if (options == null) {
        options = {};
      }
      options.Subtype = 'FreeText';
      options.Contents = PDFObject.s(text, true);
      options.DA = PDFObject.s('');
      return this.annotate(x, y, w, h, options);
    },
    _convertRect: function(x1, y1, w, h) {
      var m0, m1, m2, m3, m4, m5, x2, y2, _ref;
      y2 = y1;
      y1 += h;
      x2 = x1 + w;
      _ref = this._ctm, m0 = _ref[0], m1 = _ref[1], m2 = _ref[2], m3 = _ref[3], m4 = _ref[4], m5 = _ref[5];
      x1 = m0 * x1 + m2 * y1 + m4;
      y1 = m1 * x1 + m3 * y1 + m5;
      x2 = m0 * x2 + m2 * y2 + m4;
      y2 = m1 * x2 + m3 * y2 + m5;
      return [x1, y1, x2, y2];
    }
  };

}).call(this);

},{"../object":63}],58:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var PDFGradient, PDFLinearGradient, PDFRadialGradient, namedColors, _ref;

  _ref = _dereq_('../gradient'), PDFGradient = _ref.PDFGradient, PDFLinearGradient = _ref.PDFLinearGradient, PDFRadialGradient = _ref.PDFRadialGradient;

  module.exports = {
    initColor: function() {
      this._opacityRegistry = {};
      this._opacityCount = 0;
      return this._gradCount = 0;
    },
    _normalizeColor: function(color) {
      var hex, part;
      if (color instanceof PDFGradient) {
        return color;
      }
      if (typeof color === 'string') {
        if (color.charAt(0) === '#') {
          if (color.length === 4) {
            color = color.replace(/#([0-9A-F])([0-9A-F])([0-9A-F])/i, "#$1$1$2$2$3$3");
          }
          hex = parseInt(color.slice(1), 16);
          color = [hex >> 16, hex >> 8 & 0xff, hex & 0xff];
        } else if (namedColors[color]) {
          color = namedColors[color];
        }
      }
      if (Array.isArray(color)) {
        if (color.length === 3) {
          color = (function() {
            var _i, _len, _results;
            _results = [];
            for (_i = 0, _len = color.length; _i < _len; _i++) {
              part = color[_i];
              _results.push(part / 255);
            }
            return _results;
          })();
        } else if (color.length === 4) {
          color = (function() {
            var _i, _len, _results;
            _results = [];
            for (_i = 0, _len = color.length; _i < _len; _i++) {
              part = color[_i];
              _results.push(part / 100);
            }
            return _results;
          })();
        }
        return color;
      }
      return null;
    },
    _setColor: function(color, stroke) {
      var gstate, name, op, space;
      color = this._normalizeColor(color);
      if (!color) {
        return false;
      }
      if (this._sMasked) {
        gstate = this.ref({
          Type: 'ExtGState',
          SMask: 'None'
        });
        gstate.end();
        name = "Gs" + (++this._opacityCount);
        this.page.ext_gstates[name] = gstate;
        this.addContent("/" + name + " gs");
        this._sMasked = false;
      }
      op = stroke ? 'SCN' : 'scn';
      if (color instanceof PDFGradient) {
        this._setColorSpace('Pattern', stroke);
        color.apply(op);
      } else {
        space = color.length === 4 ? 'DeviceCMYK' : 'DeviceRGB';
        this._setColorSpace(space, stroke);
        color = color.join(' ');
        this.addContent("" + color + " " + op);
      }
      return true;
    },
    _setColorSpace: function(space, stroke) {
      var op;
      op = stroke ? 'CS' : 'cs';
      return this.addContent("/" + space + " " + op);
    },
    fillColor: function(color, opacity) {
      var set;
      if (opacity == null) {
        opacity = 1;
      }
      set = this._setColor(color, false);
      if (set) {
        this.fillOpacity(opacity);
      }
      this._fillColor = [color, opacity];
      return this;
    },
    strokeColor: function(color, opacity) {
      var set;
      if (opacity == null) {
        opacity = 1;
      }
      set = this._setColor(color, true);
      if (set) {
        this.strokeOpacity(opacity);
      }
      return this;
    },
    opacity: function(opacity) {
      this._doOpacity(opacity, opacity);
      return this;
    },
    fillOpacity: function(opacity) {
      this._doOpacity(opacity, null);
      return this;
    },
    strokeOpacity: function(opacity) {
      this._doOpacity(null, opacity);
      return this;
    },
    _doOpacity: function(fillOpacity, strokeOpacity) {
      var dictionary, id, key, name, _ref1;
      if (!((fillOpacity != null) || (strokeOpacity != null))) {
        return;
      }
      if (fillOpacity != null) {
        fillOpacity = Math.max(0, Math.min(1, fillOpacity));
      }
      if (strokeOpacity != null) {
        strokeOpacity = Math.max(0, Math.min(1, strokeOpacity));
      }
      key = "" + fillOpacity + "_" + strokeOpacity;
      if (this._opacityRegistry[key]) {
        _ref1 = this._opacityRegistry[key], dictionary = _ref1[0], name = _ref1[1];
      } else {
        dictionary = {
          Type: 'ExtGState'
        };
        if (fillOpacity != null) {
          dictionary.ca = fillOpacity;
        }
        if (strokeOpacity != null) {
          dictionary.CA = strokeOpacity;
        }
        dictionary = this.ref(dictionary);
        dictionary.end();
        id = ++this._opacityCount;
        name = "Gs" + id;
        this._opacityRegistry[key] = [dictionary, name];
      }
      this.page.ext_gstates[name] = dictionary;
      return this.addContent("/" + name + " gs");
    },
    linearGradient: function(x1, y1, x2, y2) {
      return new PDFLinearGradient(this, x1, y1, x2, y2);
    },
    radialGradient: function(x1, y1, r1, x2, y2, r2) {
      return new PDFRadialGradient(this, x1, y1, r1, x2, y2, r2);
    }
  };

  namedColors = {
    aliceblue: [240, 248, 255],
    antiquewhite: [250, 235, 215],
    aqua: [0, 255, 255],
    aquamarine: [127, 255, 212],
    azure: [240, 255, 255],
    beige: [245, 245, 220],
    bisque: [255, 228, 196],
    black: [0, 0, 0],
    blanchedalmond: [255, 235, 205],
    blue: [0, 0, 255],
    blueviolet: [138, 43, 226],
    brown: [165, 42, 42],
    burlywood: [222, 184, 135],
    cadetblue: [95, 158, 160],
    chartreuse: [127, 255, 0],
    chocolate: [210, 105, 30],
    coral: [255, 127, 80],
    cornflowerblue: [100, 149, 237],
    cornsilk: [255, 248, 220],
    crimson: [220, 20, 60],
    cyan: [0, 255, 255],
    darkblue: [0, 0, 139],
    darkcyan: [0, 139, 139],
    darkgoldenrod: [184, 134, 11],
    darkgray: [169, 169, 169],
    darkgreen: [0, 100, 0],
    darkgrey: [169, 169, 169],
    darkkhaki: [189, 183, 107],
    darkmagenta: [139, 0, 139],
    darkolivegreen: [85, 107, 47],
    darkorange: [255, 140, 0],
    darkorchid: [153, 50, 204],
    darkred: [139, 0, 0],
    darksalmon: [233, 150, 122],
    darkseagreen: [143, 188, 143],
    darkslateblue: [72, 61, 139],
    darkslategray: [47, 79, 79],
    darkslategrey: [47, 79, 79],
    darkturquoise: [0, 206, 209],
    darkviolet: [148, 0, 211],
    deeppink: [255, 20, 147],
    deepskyblue: [0, 191, 255],
    dimgray: [105, 105, 105],
    dimgrey: [105, 105, 105],
    dodgerblue: [30, 144, 255],
    firebrick: [178, 34, 34],
    floralwhite: [255, 250, 240],
    forestgreen: [34, 139, 34],
    fuchsia: [255, 0, 255],
    gainsboro: [220, 220, 220],
    ghostwhite: [248, 248, 255],
    gold: [255, 215, 0],
    goldenrod: [218, 165, 32],
    gray: [128, 128, 128],
    grey: [128, 128, 128],
    green: [0, 128, 0],
    greenyellow: [173, 255, 47],
    honeydew: [240, 255, 240],
    hotpink: [255, 105, 180],
    indianred: [205, 92, 92],
    indigo: [75, 0, 130],
    ivory: [255, 255, 240],
    khaki: [240, 230, 140],
    lavender: [230, 230, 250],
    lavenderblush: [255, 240, 245],
    lawngreen: [124, 252, 0],
    lemonchiffon: [255, 250, 205],
    lightblue: [173, 216, 230],
    lightcoral: [240, 128, 128],
    lightcyan: [224, 255, 255],
    lightgoldenrodyellow: [250, 250, 210],
    lightgray: [211, 211, 211],
    lightgreen: [144, 238, 144],
    lightgrey: [211, 211, 211],
    lightpink: [255, 182, 193],
    lightsalmon: [255, 160, 122],
    lightseagreen: [32, 178, 170],
    lightskyblue: [135, 206, 250],
    lightslategray: [119, 136, 153],
    lightslategrey: [119, 136, 153],
    lightsteelblue: [176, 196, 222],
    lightyellow: [255, 255, 224],
    lime: [0, 255, 0],
    limegreen: [50, 205, 50],
    linen: [250, 240, 230],
    magenta: [255, 0, 255],
    maroon: [128, 0, 0],
    mediumaquamarine: [102, 205, 170],
    mediumblue: [0, 0, 205],
    mediumorchid: [186, 85, 211],
    mediumpurple: [147, 112, 219],
    mediumseagreen: [60, 179, 113],
    mediumslateblue: [123, 104, 238],
    mediumspringgreen: [0, 250, 154],
    mediumturquoise: [72, 209, 204],
    mediumvioletred: [199, 21, 133],
    midnightblue: [25, 25, 112],
    mintcream: [245, 255, 250],
    mistyrose: [255, 228, 225],
    moccasin: [255, 228, 181],
    navajowhite: [255, 222, 173],
    navy: [0, 0, 128],
    oldlace: [253, 245, 230],
    olive: [128, 128, 0],
    olivedrab: [107, 142, 35],
    orange: [255, 165, 0],
    orangered: [255, 69, 0],
    orchid: [218, 112, 214],
    palegoldenrod: [238, 232, 170],
    palegreen: [152, 251, 152],
    paleturquoise: [175, 238, 238],
    palevioletred: [219, 112, 147],
    papayawhip: [255, 239, 213],
    peachpuff: [255, 218, 185],
    peru: [205, 133, 63],
    pink: [255, 192, 203],
    plum: [221, 160, 221],
    powderblue: [176, 224, 230],
    purple: [128, 0, 128],
    red: [255, 0, 0],
    rosybrown: [188, 143, 143],
    royalblue: [65, 105, 225],
    saddlebrown: [139, 69, 19],
    salmon: [250, 128, 114],
    sandybrown: [244, 164, 96],
    seagreen: [46, 139, 87],
    seashell: [255, 245, 238],
    sienna: [160, 82, 45],
    silver: [192, 192, 192],
    skyblue: [135, 206, 235],
    slateblue: [106, 90, 205],
    slategray: [112, 128, 144],
    slategrey: [112, 128, 144],
    snow: [255, 250, 250],
    springgreen: [0, 255, 127],
    steelblue: [70, 130, 180],
    tan: [210, 180, 140],
    teal: [0, 128, 128],
    thistle: [216, 191, 216],
    tomato: [255, 99, 71],
    turquoise: [64, 224, 208],
    violet: [238, 130, 238],
    wheat: [245, 222, 179],
    white: [255, 255, 255],
    whitesmoke: [245, 245, 245],
    yellow: [255, 255, 0],
    yellowgreen: [154, 205, 50]
  };

}).call(this);

},{"../gradient":52}],59:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var PDFFont;

  PDFFont = _dereq_('../font');

  module.exports = {
    initFonts: function() {
      this._fontFamilies = {};
      this._fontCount = 0;
      this._fontSize = 12;
      this._font = null;
      this._registeredFonts = {};
      
    },
    font: function(src, family, size) {
      var cacheKey, font, id, _ref;
      if (typeof family === 'number') {
        size = family;
        family = null;
      }
      if (typeof src === 'string' && this._registeredFonts[src]) {
        cacheKey = src;
        _ref = this._registeredFonts[src], src = _ref.src, family = _ref.family;
      } else {
        cacheKey = family || src;
        if (typeof cacheKey !== 'string') {
          cacheKey = null;
        }
      }
      if (size != null) {
        this.fontSize(size);
      }
      if (font = this._fontFamilies[cacheKey]) {
        this._font = font;
        return this;
      }
      id = 'F' + (++this._fontCount);
      this._font = new PDFFont(this, src, family, id);
      if (font = this._fontFamilies[this._font.name]) {
        this._font = font;
        return this;
      }
      if (cacheKey) {
        this._fontFamilies[cacheKey] = this._font;
      }
      this._fontFamilies[this._font.name] = this._font;
      return this;
    },
    fontSize: function(_fontSize) {
      this._fontSize = _fontSize;
      return this;
    },
    currentLineHeight: function(includeGap) {
      if (includeGap == null) {
        includeGap = false;
      }
      return this._font.lineHeight(this._fontSize, includeGap);
    },
    registerFont: function(name, src, family) {
      this._registeredFonts[name] = {
        src: src,
        family: family
      };
      return this;
    }
  };

}).call(this);

},{"../font":34}],60:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1
(function() {
  var PDFImage;

  PDFImage = _dereq_('../image');

  module.exports = {
    initImages: function() {
      this._imageRegistry = {};
      return this._imageCount = 0;
    },
    image: function(src, x, y, options) {
      var bh, bp, bw, h, hp, image, ip, w, wp, _base, _name, _ref, _ref1, _ref2;
      if (options == null) {
        options = {};
      }
      if (typeof x === 'object') {
        options = x;
        x = null;
      }
      x = (_ref = x != null ? x : options.x) != null ? _ref : this.x;
      y = (_ref1 = y != null ? y : options.y) != null ? _ref1 : this.y;
      if (!Buffer.isBuffer(src)) {
        image = this._imageRegistry[src];
      }
      if (!image) {
        image = PDFImage.open(src, 'I' + (++this._imageCount));
        image.embed(this);
        if (!Buffer.isBuffer(src)) {
          this._imageRegistry[src] = image;
        }
      }
      if ((_base = this.page.xobjects)[_name = image.label] == null) {
        _base[_name] = image.obj;
      }
      w = options.width || image.width;
      h = options.height || image.height;
      if (options.width && !options.height) {
        wp = w / image.width;
        w = image.width * wp;
        h = image.height * wp;
      } else if (options.height && !options.width) {
        hp = h / image.height;
        w = image.width * hp;
        h = image.height * hp;
      } else if (options.scale) {
        w = image.width * options.scale;
        h = image.height * options.scale;
      } else if (options.fit) {
        _ref2 = options.fit, bw = _ref2[0], bh = _ref2[1];
        bp = bw / bh;
        ip = image.width / image.height;
        if (ip > bp) {
          w = bw;
          h = bw / ip;
        } else {
          h = bh;
          w = bh * ip;
        }
        if (options.align === 'center') {
          x = x + bw / 2 - w / 2;
        } else if (options.align === 'right') {
          x = x + bw - w;
        }
        if (options.valign === 'center') {
          y = y + bh / 2 - h / 2;
        } else if (options.valign === 'bottom') {
          y = y + bh - h;
        }
      }
      if (this.y === y) {
        this.y += h;
      }
      this.save();
      this.transform(w, 0, 0, -h, x, y + h);
      this.addContent("/" + image.label + " Do");
      this.restore();
      return this;
    }
  };

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"../image":53,"buffer":16}],61:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var LineWrapper;

  LineWrapper = _dereq_('../line_wrapper');

  module.exports = {
    initText: function() {
      this.x = 0;
      this.y = 0;
      return this._lineGap = 0;
    },
    lineGap: function(_lineGap) {
      this._lineGap = _lineGap;
      return this;
    },
    moveDown: function(lines) {
      if (lines == null) {
        lines = 1;
      }
      this.y += this.currentLineHeight(true) * lines + this._lineGap;
      return this;
    },
    moveUp: function(lines) {
      if (lines == null) {
        lines = 1;
      }
      this.y -= this.currentLineHeight(true) * lines + this._lineGap;
      return this;
    },
    _text: function(text, x, y, options, lineCallback) {
      var line, wrapper, _i, _len, _ref;
      options = this._initOptions(x, y, options);
      text = '' + text;
      if (options.wordSpacing) {
        text = text.replace(/\s{2,}/g, ' ');
      }
      if (options.width) {
        wrapper = this._wrapper;
        if (!wrapper) {
          wrapper = new LineWrapper(this, options);
          wrapper.on('line', lineCallback);
        }
        this._wrapper = options.continued ? wrapper : null;
        this._textOptions = options.continued ? options : null;
        wrapper.wrap(text, options);
      } else {
        _ref = text.split('\n');
        for (_i = 0, _len = _ref.length; _i < _len; _i++) {
          line = _ref[_i];
          lineCallback(line, options);
        }
      }
      return this;
    },
    text: function(text, x, y, options) {
      return this._text(text, x, y, options, this._line.bind(this));
    },
    widthOfString: function(string, options) {
      if (options == null) {
        options = {};
      }
      return this._font.widthOfString(string, this._fontSize) + (options.characterSpacing || 0) * (string.length - 1);
    },
    heightOfString: function(text, options) {
      var height, lineGap, x, y;
      if (options == null) {
        options = {};
      }
      x = this.x, y = this.y;
      options = this._initOptions(options);
      options.height = Infinity;
      lineGap = options.lineGap || this._lineGap || 0;
      this._text(text, this.x, this.y, options, (function(_this) {
        return function(line, options) {
          return _this.y += _this.currentLineHeight(true) + lineGap;
        };
      })(this));
      height = this.y - y;
      this.x = x;
      this.y = y;
      return height;
    },
    list: function(list, x, y, options, wrapper) {
      var flatten, i, indent, itemIndent, items, level, levels, r;
      options = this._initOptions(x, y, options);
      r = Math.round((this._font.ascender / 1000 * this._fontSize) / 3);
      indent = options.textIndent || r * 5;
      itemIndent = options.bulletIndent || r * 8;
      level = 1;
      items = [];
      levels = [];
      flatten = function(list) {
        var i, item, _i, _len, _results;
        _results = [];
        for (i = _i = 0, _len = list.length; _i < _len; i = ++_i) {
          item = list[i];
          if (Array.isArray(item)) {
            level++;
            flatten(item);
            _results.push(level--);
          } else {
            items.push(item);
            _results.push(levels.push(level));
          }
        }
        return _results;
      };
      flatten(list);
      wrapper = new LineWrapper(this, options);
      wrapper.on('line', this._line.bind(this));
      level = 1;
      i = 0;
      wrapper.on('firstLine', (function(_this) {
        return function() {
          var diff, l;
          if ((l = levels[i++]) !== level) {
            diff = itemIndent * (l - level);
            _this.x += diff;
            wrapper.lineWidth -= diff;
            level = l;
          }
          _this.circle(_this.x - indent + r, _this.y + r + (r / 2), r);
          return _this.fill();
        };
      })(this));
      wrapper.on('sectionStart', (function(_this) {
        return function() {
          var pos;
          pos = indent + itemIndent * (level - 1);
          _this.x += pos;
          return wrapper.lineWidth -= pos;
        };
      })(this));
      wrapper.on('sectionEnd', (function(_this) {
        return function() {
          var pos;
          pos = indent + itemIndent * (level - 1);
          _this.x -= pos;
          return wrapper.lineWidth += pos;
        };
      })(this));
      wrapper.wrap(items.join('\n'), options);
      this.x -= indent;
      return this;
    },
    _initOptions: function(x, y, options) {
      var key, margins, val, _ref;
      if (x == null) {
        x = {};
      }
      if (options == null) {
        options = {};
      }
      if (typeof x === 'object') {
        options = x;
        x = null;
      }
      options = (function() {
        var k, opts, v;
        opts = {};
        for (k in options) {
          v = options[k];
          opts[k] = v;
        }
        return opts;
      })();
      if (this._textOptions) {
        _ref = this._textOptions;
        for (key in _ref) {
          val = _ref[key];
          if (key !== 'continued') {
            if (options[key] == null) {
              options[key] = val;
            }
          }
        }
      }
      if (x != null) {
        this.x = x;
      }
      if (y != null) {
        this.y = y;
      }
      if (options.lineBreak !== false) {
        margins = this.page.margins;
        if (options.width == null) {
          options.width = this.page.width - this.x - margins.right;
        }
      }
      options.columns || (options.columns = 0);
      if (options.columnGap == null) {
        options.columnGap = 18;
      }
      return options;
    },
    _line: function(text, options, wrapper) {
      var lineGap;
      if (options == null) {
        options = {};
      }
      this._fragment(text, this.x, this.y, options);
      lineGap = options.lineGap || this._lineGap || 0;
      if (!wrapper) {
        return this.x += this.widthOfString(text);
      } else {
        return this.y += this.currentLineHeight(true) + lineGap;
      }
    },
    _fragment: function(text, x, y, options) {
      var align, characterSpacing, commands, d, encoded, i, lineWidth, lineY, mode, renderedWidth, spaceWidth, textWidth, word, wordSpacing, words, _base, _i, _len, _name;
      text = '' + text;
      if (text.length === 0) {
        return;
      }
      align = options.align || 'left';
      wordSpacing = options.wordSpacing || 0;
      characterSpacing = options.characterSpacing || 0;
      if (options.width) {
        switch (align) {
          case 'right':
            textWidth = this.widthOfString(text.replace(/\s+$/, ''), options);
            x += options.lineWidth - textWidth;
            break;
          case 'center':
            x += options.lineWidth / 2 - options.textWidth / 2;
            break;
          case 'justify':
            words = text.trim().split(/\s+/);
            textWidth = this.widthOfString(text.replace(/\s+/g, ''), options);
            spaceWidth = this.widthOfString(' ') + characterSpacing;
            wordSpacing = Math.max(0, (options.lineWidth - textWidth) / Math.max(1, words.length - 1) - spaceWidth);
        }
      }
      renderedWidth = options.textWidth + (wordSpacing * (options.wordCount - 1)) + (characterSpacing * (text.length - 1));
      if (options.link) {
        this.link(x, y, renderedWidth, this.currentLineHeight(), options.link);
      }
      if (options.underline || options.strike) {
        this.save();
        if (!options.stroke) {
          this.strokeColor.apply(this, this._fillColor);
        }
        lineWidth = this._fontSize < 10 ? 0.5 : Math.floor(this._fontSize / 10);
        this.lineWidth(lineWidth);
        d = options.underline ? 1 : 2;
        lineY = y + this.currentLineHeight() / d;
        if (options.underline) {
          lineY -= lineWidth;
        }
        this.moveTo(x, lineY);
        this.lineTo(x + renderedWidth, lineY);
        this.stroke();
        this.restore();
      }
      this.save();
      this.transform(1, 0, 0, -1, 0, this.page.height);
      y = this.page.height - y - (this._font.ascender / 1000 * this._fontSize);
      if ((_base = this.page.fonts)[_name = this._font.id] == null) {
        _base[_name] = this._font.ref();
      }
      this._font.use(text);
      this.addContent("BT");
      this.addContent("" + x + " " + y + " Td");
      this.addContent("/" + this._font.id + " " + this._fontSize + " Tf");
      mode = options.fill && options.stroke ? 2 : options.stroke ? 1 : 0;
      if (mode) {
        this.addContent("" + mode + " Tr");
      }
      if (characterSpacing) {
        this.addContent("" + characterSpacing + " Tc");
      }
      if (wordSpacing) {
        words = text.trim().split(/\s+/);
        wordSpacing += this.widthOfString(' ') + characterSpacing;
        wordSpacing *= 1000 / this._fontSize;
        commands = [];
        for (_i = 0, _len = words.length; _i < _len; _i++) {
          word = words[_i];
          encoded = this._font.encode(word);
          encoded = ((function() {
            var _j, _ref, _results;
            _results = [];
            for (i = _j = 0, _ref = encoded.length; _j < _ref; i = _j += 1) {
              _results.push(encoded.charCodeAt(i).toString(16));
            }
            return _results;
          })()).join('');
          commands.push("<" + encoded + "> " + (-wordSpacing));
        }
        this.addContent("[" + (commands.join(' ')) + "] TJ");
      } else {
        encoded = this._font.encode(text);
        encoded = ((function() {
          var _j, _ref, _results;
          _results = [];
          for (i = _j = 0, _ref = encoded.length; _j < _ref; i = _j += 1) {
            _results.push(encoded.charCodeAt(i).toString(16));
          }
          return _results;
        })()).join('');
        this.addContent("<" + encoded + "> Tj");
      }
      this.addContent("ET");
      return this.restore();
    }
  };

}).call(this);

},{"../line_wrapper":56}],62:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var KAPPA, SVGPath,
    __slice = [].slice;

  SVGPath = _dereq_('../path');

  KAPPA = 4.0 * ((Math.sqrt(2) - 1.0) / 3.0);

  module.exports = {
    initVector: function() {
      this._ctm = [1, 0, 0, 1, 0, 0];
      return this._ctmStack = [];
    },
    save: function() {
      this._ctmStack.push(this._ctm.slice());
      return this.addContent('q');
    },
    restore: function() {
      this._ctm = this._ctmStack.pop() || [1, 0, 0, 1, 0, 0];
      return this.addContent('Q');
    },
    closePath: function() {
      return this.addContent('h');
    },
    lineWidth: function(w) {
      return this.addContent("" + w + " w");
    },
    _CAP_STYLES: {
      BUTT: 0,
      ROUND: 1,
      SQUARE: 2
    },
    lineCap: function(c) {
      if (typeof c === 'string') {
        c = this._CAP_STYLES[c.toUpperCase()];
      }
      return this.addContent("" + c + " J");
    },
    _JOIN_STYLES: {
      MITER: 0,
      ROUND: 1,
      BEVEL: 2
    },
    lineJoin: function(j) {
      if (typeof j === 'string') {
        j = this._JOIN_STYLES[j.toUpperCase()];
      }
      return this.addContent("" + j + " j");
    },
    miterLimit: function(m) {
      return this.addContent("" + m + " M");
    },
    dash: function(length, options) {
      var phase, space, _ref;
      if (options == null) {
        options = {};
      }
      if (length == null) {
        return this;
      }
      space = (_ref = options.space) != null ? _ref : length;
      phase = options.phase || 0;
      return this.addContent("[" + length + " " + space + "] " + phase + " d");
    },
    undash: function() {
      return this.addContent("[] 0 d");
    },
    moveTo: function(x, y) {
      return this.addContent("" + x + " " + y + " m");
    },
    lineTo: function(x, y) {
      return this.addContent("" + x + " " + y + " l");
    },
    bezierCurveTo: function(cp1x, cp1y, cp2x, cp2y, x, y) {
      return this.addContent("" + cp1x + " " + cp1y + " " + cp2x + " " + cp2y + " " + x + " " + y + " c");
    },
    quadraticCurveTo: function(cpx, cpy, x, y) {
      return this.addContent("" + cpx + " " + cpy + " " + x + " " + y + " v");
    },
    rect: function(x, y, w, h) {
      return this.addContent("" + x + " " + y + " " + w + " " + h + " re");
    },
    roundedRect: function(x, y, w, h, r) {
      if (r == null) {
        r = 0;
      }
      this.moveTo(x + r, y);
      this.lineTo(x + w - r, y);
      this.quadraticCurveTo(x + w, y, x + w, y + r);
      this.lineTo(x + w, y + h - r);
      this.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
      this.lineTo(x + r, y + h);
      this.quadraticCurveTo(x, y + h, x, y + h - r);
      this.lineTo(x, y + r);
      return this.quadraticCurveTo(x, y, x + r, y);
    },
    ellipse: function(x, y, r1, r2) {
      var ox, oy, xe, xm, ye, ym;
      if (r2 == null) {
        r2 = r1;
      }
      x -= r1;
      y -= r2;
      ox = r1 * KAPPA;
      oy = r2 * KAPPA;
      xe = x + r1 * 2;
      ye = y + r2 * 2;
      xm = x + r1;
      ym = y + r2;
      this.moveTo(x, ym);
      this.bezierCurveTo(x, ym - oy, xm - ox, y, xm, y);
      this.bezierCurveTo(xm + ox, y, xe, ym - oy, xe, ym);
      this.bezierCurveTo(xe, ym + oy, xm + ox, ye, xm, ye);
      this.bezierCurveTo(xm - ox, ye, x, ym + oy, x, ym);
      return this.closePath();
    },
    circle: function(x, y, radius) {
      return this.ellipse(x, y, radius);
    },
    polygon: function() {
      var point, points, _i, _len;
      points = 1 <= arguments.length ? __slice.call(arguments, 0) : [];
      this.moveTo.apply(this, points.shift());
      for (_i = 0, _len = points.length; _i < _len; _i++) {
        point = points[_i];
        this.lineTo.apply(this, point);
      }
      return this.closePath();
    },
    path: function(path) {
      SVGPath.apply(this, path);
      return this;
    },
    _windingRule: function(rule) {
      if (/even-?odd/.test(rule)) {
        return '*';
      }
      return '';
    },
    fill: function(color, rule) {
      if (/(even-?odd)|(non-?zero)/.test(color)) {
        rule = color;
        color = null;
      }
      if (color) {
        this.fillColor(color);
      }
      return this.addContent('f' + this._windingRule(rule));
    },
    stroke: function(color) {
      if (color) {
        this.strokeColor(color);
      }
      return this.addContent('S');
    },
    fillAndStroke: function(fillColor, strokeColor, rule) {
      var isFillRule;
      if (strokeColor == null) {
        strokeColor = fillColor;
      }
      isFillRule = /(even-?odd)|(non-?zero)/;
      if (isFillRule.test(fillColor)) {
        rule = fillColor;
        fillColor = null;
      }
      if (isFillRule.test(strokeColor)) {
        rule = strokeColor;
        strokeColor = fillColor;
      }
      if (fillColor) {
        this.fillColor(fillColor);
        this.strokeColor(strokeColor);
      }
      return this.addContent('B' + this._windingRule(rule));
    },
    clip: function(rule) {
      return this.addContent('W' + this._windingRule(rule) + ' n');
    },
    transform: function(m11, m12, m21, m22, dx, dy) {
      var m, m0, m1, m2, m3, m4, m5, v, values;
      m = this._ctm;
      m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3], m4 = m[4], m5 = m[5];
      m[0] = m0 * m11 + m2 * m12;
      m[1] = m1 * m11 + m3 * m12;
      m[2] = m0 * m21 + m2 * m22;
      m[3] = m1 * m21 + m3 * m22;
      m[4] = m0 * dx + m2 * dy + m4;
      m[5] = m1 * dx + m3 * dy + m5;
      values = ((function() {
        var _i, _len, _ref, _results;
        _ref = [m11, m12, m21, m22, dx, dy];
        _results = [];
        for (_i = 0, _len = _ref.length; _i < _len; _i++) {
          v = _ref[_i];
          _results.push(+v.toFixed(5));
        }
        return _results;
      })()).join(' ');
      return this.addContent("" + values + " cm");
    },
    translate: function(x, y) {
      return this.transform(1, 0, 0, 1, x, y);
    },
    rotate: function(angle, options) {
      var cos, rad, sin, x, x1, y, y1, _ref;
      if (options == null) {
        options = {};
      }
      rad = angle * Math.PI / 180;
      cos = Math.cos(rad);
      sin = Math.sin(rad);
      x = y = 0;
      if (options.origin != null) {
        _ref = options.origin, x = _ref[0], y = _ref[1];
        x1 = x * cos - y * sin;
        y1 = x * sin + y * cos;
        x -= x1;
        y -= y1;
      }
      return this.transform(cos, sin, -sin, cos, x, y);
    },
    scale: function(xFactor, yFactor, options) {
      var x, y, _ref;
      if (yFactor == null) {
        yFactor = xFactor;
      }
      if (options == null) {
        options = {};
      }
      if (arguments.length === 2) {
        yFactor = xFactor;
        options = yFactor;
      }
      x = y = 0;
      if (options.origin != null) {
        _ref = options.origin, x = _ref[0], y = _ref[1];
        x -= xFactor * x;
        y -= yFactor * y;
      }
      return this.transform(xFactor, 0, 0, yFactor, x, y);
    }
  };

}).call(this);

},{"../path":65}],63:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1

/*
PDFObject - converts JavaScript types into their corrisponding PDF types.
By Devon Govett
 */

(function() {
  var PDFObject, PDFReference;

  PDFObject = (function() {
    var pad, swapBytes;

    function PDFObject() {}

    pad = function(str, length) {
      return (Array(length + 1).join('0') + str).slice(-length);
    };

    PDFObject.convert = function(object) {
      var e, items, key, out, val;
      if (Array.isArray(object)) {
        items = ((function() {
          var _i, _len, _results;
          _results = [];
          for (_i = 0, _len = object.length; _i < _len; _i++) {
            e = object[_i];
            _results.push(PDFObject.convert(e));
          }
          return _results;
        })()).join(' ');
        return '[' + items + ']';
      } else if (typeof object === 'string') {
        return '/' + object;
      } else if (object != null ? object.isString : void 0) {
        return '(' + object + ')';
      } else if (object instanceof PDFReference) {
        return object.toString();
      } else if (object instanceof Date) {
        return '(D:' + pad(object.getUTCFullYear(), 4) + pad(object.getUTCMonth(), 2) + pad(object.getUTCDate(), 2) + pad(object.getUTCHours(), 2) + pad(object.getUTCMinutes(), 2) + pad(object.getUTCSeconds(), 2) + 'Z)';
      } else if ({}.toString.call(object) === '[object Object]') {
        out = ['<<'];
        for (key in object) {
          val = object[key];
          out.push('/' + key + ' ' + PDFObject.convert(val));
        }
        out.push('>>');
        return out.join('\n');
      } else {
        return '' + object;
      }
    };

    swapBytes = function(buff) {
      var a, i, l, _i, _ref;
      l = buff.length;
      if (l & 0x01) {
        throw new Error("Buffer length must be even");
      } else {
        for (i = _i = 0, _ref = l - 1; _i < _ref; i = _i += 2) {
          a = buff[i];
          buff[i] = buff[i + 1];
          buff[i + 1] = a;
        }
      }
      return buff;
    };

    PDFObject.s = function(string, swap) {
      if (swap == null) {
        swap = false;
      }
      string = string.replace(/\\/g, '\\\\\\\\').replace(/\(/g, '\\(').replace(/\)/g, '\\)').replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&');
      if (swap) {
        string = swapBytes(new Buffer('\ufeff' + string, 'ucs-2')).toString('binary');
      }
      return {
        isString: true,
        toString: function() {
          return string;
        }
      };
    };

    return PDFObject;

  })();

  module.exports = PDFObject;

  PDFReference = _dereq_('./reference');

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"./reference":66,"buffer":16}],64:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1

/*
PDFPage - represents a single page in the PDF document
By Devon Govett
 */

(function() {
  var PDFPage;

  PDFPage = (function() {
    var DEFAULT_MARGINS, SIZES;

    function PDFPage(document, options) {
      var dimensions;
      this.document = document;
      if (options == null) {
        options = {};
      }
      this.size = options.size || 'letter';
      this.layout = options.layout || 'portrait';
      if (typeof options.margin === 'number') {
        this.margins = {
          top: options.margin,
          left: options.margin,
          bottom: options.margin,
          right: options.margin
        };
      } else {
        this.margins = options.margins || DEFAULT_MARGINS;
      }
      dimensions = Array.isArray(this.size) ? this.size : SIZES[this.size.toUpperCase()];
      this.width = dimensions[this.layout === 'portrait' ? 0 : 1];
      this.height = dimensions[this.layout === 'portrait' ? 1 : 0];
      this.content = this.document.ref();
      this.resources = this.document.ref({
        ProcSet: ['PDF', 'Text', 'ImageB', 'ImageC', 'ImageI']
      });
      Object.defineProperties(this, {
        fonts: {
          get: (function(_this) {
            return function() {
              var _base;
              return (_base = _this.resources.data).Font != null ? _base.Font : _base.Font = {};
            };
          })(this)
        },
        xobjects: {
          get: (function(_this) {
            return function() {
              var _base;
              return (_base = _this.resources.data).XObject != null ? _base.XObject : _base.XObject = {};
            };
          })(this)
        },
        ext_gstates: {
          get: (function(_this) {
            return function() {
              var _base;
              return (_base = _this.resources.data).ExtGState != null ? _base.ExtGState : _base.ExtGState = {};
            };
          })(this)
        },
        patterns: {
          get: (function(_this) {
            return function() {
              var _base;
              return (_base = _this.resources.data).Pattern != null ? _base.Pattern : _base.Pattern = {};
            };
          })(this)
        },
        annotations: {
          get: (function(_this) {
            return function() {
              var _base;
              return (_base = _this.dictionary.data).Annots != null ? _base.Annots : _base.Annots = [];
            };
          })(this)
        }
      });
      this.dictionary = this.document.ref({
        Type: 'Page',
        Parent: this.document._root.data.Pages,
        MediaBox: [0, 0, this.width, this.height],
        Contents: this.content,
        Resources: this.resources
      });
    }

    PDFPage.prototype.maxY = function() {
      return this.height - this.margins.bottom;
    };

    PDFPage.prototype.write = function(chunk) {
      return this.content.write(chunk);
    };

    PDFPage.prototype.end = function() {
      this.dictionary.end();
      this.resources.end();
      return this.content.end();
    };

    DEFAULT_MARGINS = {
      top: 72,
      left: 72,
      bottom: 72,
      right: 72
    };

    SIZES = {
      '4A0': [4767.87, 6740.79],
      '2A0': [3370.39, 4767.87],
      A0: [2383.94, 3370.39],
      A1: [1683.78, 2383.94],
      A2: [1190.55, 1683.78],
      A3: [841.89, 1190.55],
      A4: [595.28, 841.89],
      A5: [419.53, 595.28],
      A6: [297.64, 419.53],
      A7: [209.76, 297.64],
      A8: [147.40, 209.76],
      A9: [104.88, 147.40],
      A10: [73.70, 104.88],
      B0: [2834.65, 4008.19],
      B1: [2004.09, 2834.65],
      B2: [1417.32, 2004.09],
      B3: [1000.63, 1417.32],
      B4: [708.66, 1000.63],
      B5: [498.90, 708.66],
      B6: [354.33, 498.90],
      B7: [249.45, 354.33],
      B8: [175.75, 249.45],
      B9: [124.72, 175.75],
      B10: [87.87, 124.72],
      C0: [2599.37, 3676.54],
      C1: [1836.85, 2599.37],
      C2: [1298.27, 1836.85],
      C3: [918.43, 1298.27],
      C4: [649.13, 918.43],
      C5: [459.21, 649.13],
      C6: [323.15, 459.21],
      C7: [229.61, 323.15],
      C8: [161.57, 229.61],
      C9: [113.39, 161.57],
      C10: [79.37, 113.39],
      RA0: [2437.80, 3458.27],
      RA1: [1729.13, 2437.80],
      RA2: [1218.90, 1729.13],
      RA3: [864.57, 1218.90],
      RA4: [609.45, 864.57],
      SRA0: [2551.18, 3628.35],
      SRA1: [1814.17, 2551.18],
      SRA2: [1275.59, 1814.17],
      SRA3: [907.09, 1275.59],
      SRA4: [637.80, 907.09],
      EXECUTIVE: [521.86, 756.00],
      FOLIO: [612.00, 936.00],
      LEGAL: [612.00, 1008.00],
      LETTER: [612.00, 792.00],
      TABLOID: [792.00, 1224.00]
    };

    return PDFPage;

  })();

  module.exports = PDFPage;

}).call(this);

},{}],65:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var SVGPath;

  SVGPath = (function() {
    var apply, arcToSegments, cx, cy, parameters, parse, px, py, runners, segmentToBezier, solveArc, sx, sy;

    function SVGPath() {}

    SVGPath.apply = function(doc, path) {
      var commands;
      commands = parse(path);
      return apply(commands, doc);
    };

    parameters = {
      A: 7,
      a: 7,
      C: 6,
      c: 6,
      H: 1,
      h: 1,
      L: 2,
      l: 2,
      M: 2,
      m: 2,
      Q: 4,
      q: 4,
      S: 4,
      s: 4,
      T: 2,
      t: 2,
      V: 1,
      v: 1,
      Z: 0,
      z: 0
    };

    parse = function(path) {
      var args, c, cmd, curArg, foundDecimal, params, ret, _i, _len;
      ret = [];
      args = [];
      curArg = "";
      foundDecimal = false;
      params = 0;
      for (_i = 0, _len = path.length; _i < _len; _i++) {
        c = path[_i];
        if (parameters[c] != null) {
          params = parameters[c];
          if (cmd) {
            if (curArg.length > 0) {
              args[args.length] = +curArg;
            }
            ret[ret.length] = {
              cmd: cmd,
              args: args
            };
            args = [];
            curArg = "";
            foundDecimal = false;
          }
          cmd = c;
        } else if ((c === " " || c === ",") || (c === "-" && curArg.length > 0 && curArg[curArg.length - 1] !== 'e') || (c === "." && foundDecimal)) {
          if (curArg.length === 0) {
            continue;
          }
          if (args.length === params) {
            ret[ret.length] = {
              cmd: cmd,
              args: args
            };
            args = [+curArg];
            if (cmd === "M") {
              cmd = "L";
            }
            if (cmd === "m") {
              cmd = "l";
            }
          } else {
            args[args.length] = +curArg;
          }
          foundDecimal = c === ".";
          curArg = c === '-' || c === '.' ? c : '';
        } else {
          curArg += c;
          if (c === '.') {
            foundDecimal = true;
          }
        }
      }
      if (curArg.length > 0) {
        if (args.length === params) {
          ret[ret.length] = {
            cmd: cmd,
            args: args
          };
          args = [+curArg];
          if (cmd === "M") {
            cmd = "L";
          }
          if (cmd === "m") {
            cmd = "l";
          }
        } else {
          args[args.length] = +curArg;
        }
      }
      ret[ret.length] = {
        cmd: cmd,
        args: args
      };
      return ret;
    };

    cx = cy = px = py = sx = sy = 0;

    apply = function(commands, doc) {
      var c, i, _i, _len, _name;
      cx = cy = px = py = sx = sy = 0;
      for (i = _i = 0, _len = commands.length; _i < _len; i = ++_i) {
        c = commands[i];
        if (typeof runners[_name = c.cmd] === "function") {
          runners[_name](doc, c.args);
        }
      }
      return cx = cy = px = py = 0;
    };

    runners = {
      M: function(doc, a) {
        cx = a[0];
        cy = a[1];
        px = py = null;
        sx = cx;
        sy = cy;
        return doc.moveTo(cx, cy);
      },
      m: function(doc, a) {
        cx += a[0];
        cy += a[1];
        px = py = null;
        sx = cx;
        sy = cy;
        return doc.moveTo(cx, cy);
      },
      C: function(doc, a) {
        cx = a[4];
        cy = a[5];
        px = a[2];
        py = a[3];
        return doc.bezierCurveTo.apply(doc, a);
      },
      c: function(doc, a) {
        doc.bezierCurveTo(a[0] + cx, a[1] + cy, a[2] + cx, a[3] + cy, a[4] + cx, a[5] + cy);
        px = cx + a[2];
        py = cy + a[3];
        cx += a[4];
        return cy += a[5];
      },
      S: function(doc, a) {
        if (px === null) {
          px = cx;
          py = cy;
        }
        doc.bezierCurveTo(cx - (px - cx), cy - (py - cy), a[0], a[1], a[2], a[3]);
        px = a[0];
        py = a[1];
        cx = a[2];
        return cy = a[3];
      },
      s: function(doc, a) {
        if (px === null) {
          px = cx;
          py = cy;
        }
        doc.bezierCurveTo(cx - (px - cx), cy - (py - cy), cx + a[0], cy + a[1], cx + a[2], cy + a[3]);
        px = cx + a[0];
        py = cy + a[1];
        cx += a[2];
        return cy += a[3];
      },
      Q: function(doc, a) {
        px = a[0];
        py = a[1];
        cx = a[2];
        cy = a[3];
        return doc.quadraticCurveTo(a[0], a[1], cx, cy);
      },
      q: function(doc, a) {
        doc.quadraticCurveTo(a[0] + cx, a[1] + cy, a[2] + cx, a[3] + cy);
        px = cx + a[0];
        py = cy + a[1];
        cx += a[2];
        return cy += a[3];
      },
      T: function(doc, a) {
        if (px === null) {
          px = cx;
          py = cy;
        } else {
          px = cx - (px - cx);
          py = cy - (py - cy);
        }
        doc.quadraticCurveTo(px, py, a[0], a[1]);
        px = cx - (px - cx);
        py = cy - (py - cy);
        cx = a[0];
        return cy = a[1];
      },
      t: function(doc, a) {
        if (px === null) {
          px = cx;
          py = cy;
        } else {
          px = cx - (px - cx);
          py = cy - (py - cy);
        }
        doc.quadraticCurveTo(px, py, cx + a[0], cy + a[1]);
        cx += a[0];
        return cy += a[1];
      },
      A: function(doc, a) {
        solveArc(doc, cx, cy, a);
        cx = a[5];
        return cy = a[6];
      },
      a: function(doc, a) {
        a[5] += cx;
        a[6] += cy;
        solveArc(doc, cx, cy, a);
        cx = a[5];
        return cy = a[6];
      },
      L: function(doc, a) {
        cx = a[0];
        cy = a[1];
        px = py = null;
        return doc.lineTo(cx, cy);
      },
      l: function(doc, a) {
        cx += a[0];
        cy += a[1];
        px = py = null;
        return doc.lineTo(cx, cy);
      },
      H: function(doc, a) {
        cx = a[0];
        px = py = null;
        return doc.lineTo(cx, cy);
      },
      h: function(doc, a) {
        cx += a[0];
        px = py = null;
        return doc.lineTo(cx, cy);
      },
      V: function(doc, a) {
        cy = a[0];
        px = py = null;
        return doc.lineTo(cx, cy);
      },
      v: function(doc, a) {
        cy += a[0];
        px = py = null;
        return doc.lineTo(cx, cy);
      },
      Z: function(doc) {
        doc.closePath();
        cx = sx;
        return cy = sy;
      },
      z: function(doc) {
        doc.closePath();
        cx = sx;
        return cy = sy;
      }
    };

    solveArc = function(doc, x, y, coords) {
      var bez, ex, ey, large, rot, rx, ry, seg, segs, sweep, _i, _len, _results;
      rx = coords[0], ry = coords[1], rot = coords[2], large = coords[3], sweep = coords[4], ex = coords[5], ey = coords[6];
      segs = arcToSegments(ex, ey, rx, ry, large, sweep, rot, x, y);
      _results = [];
      for (_i = 0, _len = segs.length; _i < _len; _i++) {
        seg = segs[_i];
        bez = segmentToBezier.apply(null, seg);
        _results.push(doc.bezierCurveTo.apply(doc, bez));
      }
      return _results;
    };

    arcToSegments = function(x, y, rx, ry, large, sweep, rotateX, ox, oy) {
      var a00, a01, a10, a11, cos_th, d, i, pl, result, segments, sfactor, sfactor_sq, sin_th, th, th0, th1, th2, th3, th_arc, x0, x1, xc, y0, y1, yc, _i;
      th = rotateX * (Math.PI / 180);
      sin_th = Math.sin(th);
      cos_th = Math.cos(th);
      rx = Math.abs(rx);
      ry = Math.abs(ry);
      px = cos_th * (ox - x) * 0.5 + sin_th * (oy - y) * 0.5;
      py = cos_th * (oy - y) * 0.5 - sin_th * (ox - x) * 0.5;
      pl = (px * px) / (rx * rx) + (py * py) / (ry * ry);
      if (pl > 1) {
        pl = Math.sqrt(pl);
        rx *= pl;
        ry *= pl;
      }
      a00 = cos_th / rx;
      a01 = sin_th / rx;
      a10 = (-sin_th) / ry;
      a11 = cos_th / ry;
      x0 = a00 * ox + a01 * oy;
      y0 = a10 * ox + a11 * oy;
      x1 = a00 * x + a01 * y;
      y1 = a10 * x + a11 * y;
      d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
      sfactor_sq = 1 / d - 0.25;
      if (sfactor_sq < 0) {
        sfactor_sq = 0;
      }
      sfactor = Math.sqrt(sfactor_sq);
      if (sweep === large) {
        sfactor = -sfactor;
      }
      xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
      yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);
      th0 = Math.atan2(y0 - yc, x0 - xc);
      th1 = Math.atan2(y1 - yc, x1 - xc);
      th_arc = th1 - th0;
      if (th_arc < 0 && sweep === 1) {
        th_arc += 2 * Math.PI;
      } else if (th_arc > 0 && sweep === 0) {
        th_arc -= 2 * Math.PI;
      }
      segments = Math.ceil(Math.abs(th_arc / (Math.PI * 0.5 + 0.001)));
      result = [];
      for (i = _i = 0; 0 <= segments ? _i < segments : _i > segments; i = 0 <= segments ? ++_i : --_i) {
        th2 = th0 + i * th_arc / segments;
        th3 = th0 + (i + 1) * th_arc / segments;
        result[i] = [xc, yc, th2, th3, rx, ry, sin_th, cos_th];
      }
      return result;
    };

    segmentToBezier = function(cx, cy, th0, th1, rx, ry, sin_th, cos_th) {
      var a00, a01, a10, a11, t, th_half, x1, x2, x3, y1, y2, y3;
      a00 = cos_th * rx;
      a01 = -sin_th * ry;
      a10 = sin_th * rx;
      a11 = cos_th * ry;
      th_half = 0.5 * (th1 - th0);
      t = (8 / 3) * Math.sin(th_half * 0.5) * Math.sin(th_half * 0.5) / Math.sin(th_half);
      x1 = cx + Math.cos(th0) - t * Math.sin(th0);
      y1 = cy + Math.sin(th0) + t * Math.cos(th0);
      x3 = cx + Math.cos(th1);
      y3 = cy + Math.sin(th1);
      x2 = x3 + t * Math.sin(th1);
      y2 = y3 - t * Math.cos(th1);
      return [a00 * x1 + a01 * y1, a10 * x1 + a11 * y1, a00 * x2 + a01 * y2, a10 * x2 + a11 * y2, a00 * x3 + a01 * y3, a10 * x3 + a11 * y3];
    };

    return SVGPath;

  })();

  module.exports = SVGPath;

}).call(this);

},{}],66:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.7.1

/*
PDFReference - represents a reference to another object in the PDF object heirarchy
By Devon Govett
 */

(function() {
  var PDFObject, PDFReference, zlib,
    __bind = function(fn, me){ return function(){ return fn.apply(me, arguments); }; };

  zlib = _dereq_('zlib');

  PDFReference = (function() {
    function PDFReference(document, id, data) {
      this.document = document;
      this.id = id;
      this.data = data != null ? data : {};
      this.finalize = __bind(this.finalize, this);
      this.gen = 0;
      this.deflate = null;
      this.compress = this.document.compress && !this.data.Filter;
      this.uncompressedLength = 0;
      this.chunks = [];
    }

    PDFReference.prototype.initDeflate = function() {
      this.data.Filter = 'FlateDecode';
      this.deflate = zlib.createDeflate();
      this.deflate.on('data', (function(_this) {
        return function(chunk) {
          _this.chunks.push(chunk);
          return _this.data.Length += chunk.length;
        };
      })(this));
      return this.deflate.on('end', this.finalize);
    };

    PDFReference.prototype.write = function(chunk) {
      var _base;
      if (!Buffer.isBuffer(chunk)) {
        chunk = new Buffer(chunk + '\n', 'binary');
      }
      this.uncompressedLength += chunk.length;
      if ((_base = this.data).Length == null) {
        _base.Length = 0;
      }
      if (this.compress) {
        if (!this.deflate) {
          this.initDeflate();
        }
        return this.deflate.write(chunk);
      } else {
        this.chunks.push(chunk);
        return this.data.Length += chunk.length;
      }
    };

    PDFReference.prototype.end = function(chunk) {
      if (typeof chunk === 'string' || Buffer.isBuffer(chunk)) {
        this.write(chunk);
      }
      if (this.deflate) {
        return this.deflate.end();
      } else {
        return this.finalize();
      }
    };

    PDFReference.prototype.finalize = function() {
      var chunk, _i, _len, _ref;
      this.offset = this.document._offset;
      this.document._write("" + this.id + " " + this.gen + " obj");
      this.document._write(PDFObject.convert(this.data));
      if (this.chunks.length) {
        this.document._write('stream');
        _ref = this.chunks;
        for (_i = 0, _len = _ref.length; _i < _len; _i++) {
          chunk = _ref[_i];
          this.document._write(chunk);
        }
        this.chunks.length = 0;
        this.document._write('\nendstream');
      }
      this.document._write('endobj');
      return this.document._refEnd(this);
    };

    PDFReference.prototype.toString = function() {
      return "" + this.id + " " + this.gen + " R";
    };

    return PDFReference;

  })();

  module.exports = PDFReference;

  PDFObject = _dereq_('./object');

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"./object":63,"buffer":16,"zlib":15}],67:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
var UnicodeTrie;

UnicodeTrie = (function() {
  var DATA_BLOCK_LENGTH, DATA_GRANULARITY, DATA_MASK, INDEX_1_OFFSET, INDEX_2_BLOCK_LENGTH, INDEX_2_BMP_LENGTH, INDEX_2_MASK, INDEX_SHIFT, LSCP_INDEX_2_LENGTH, LSCP_INDEX_2_OFFSET, OMITTED_BMP_INDEX_1_LENGTH, SHIFT_1, SHIFT_1_2, SHIFT_2, UTF8_2B_INDEX_2_LENGTH, UTF8_2B_INDEX_2_OFFSET;

  SHIFT_1 = 6 + 5;

  SHIFT_2 = 5;

  SHIFT_1_2 = SHIFT_1 - SHIFT_2;

  OMITTED_BMP_INDEX_1_LENGTH = 0x10000 >> SHIFT_1;

  INDEX_2_BLOCK_LENGTH = 1 << SHIFT_1_2;

  INDEX_2_MASK = INDEX_2_BLOCK_LENGTH - 1;

  INDEX_SHIFT = 2;

  DATA_BLOCK_LENGTH = 1 << SHIFT_2;

  DATA_MASK = DATA_BLOCK_LENGTH - 1;

  LSCP_INDEX_2_OFFSET = 0x10000 >> SHIFT_2;

  LSCP_INDEX_2_LENGTH = 0x400 >> SHIFT_2;

  INDEX_2_BMP_LENGTH = LSCP_INDEX_2_OFFSET + LSCP_INDEX_2_LENGTH;

  UTF8_2B_INDEX_2_OFFSET = INDEX_2_BMP_LENGTH;

  UTF8_2B_INDEX_2_LENGTH = 0x800 >> 6;

  INDEX_1_OFFSET = UTF8_2B_INDEX_2_OFFSET + UTF8_2B_INDEX_2_LENGTH;

  DATA_GRANULARITY = 1 << INDEX_SHIFT;

  function UnicodeTrie(json) {
    var _ref, _ref1;
    if (json == null) {
      json = {};
    }
    this.data = json.data || [];
    this.highStart = (_ref = json.highStart) != null ? _ref : 0;
    this.errorValue = (_ref1 = json.errorValue) != null ? _ref1 : -1;
  }

  UnicodeTrie.prototype.get = function(codePoint) {
    var index;
    if (codePoint < 0 || codePoint > 0x10ffff) {
      return this.errorValue;
    }
    if (codePoint < 0xd800 || (codePoint > 0xdbff && codePoint <= 0xffff)) {
      index = (this.data[codePoint >> SHIFT_2] << INDEX_SHIFT) + (codePoint & DATA_MASK);
      return this.data[index];
    }
    if (codePoint <= 0xffff) {
      index = (this.data[LSCP_INDEX_2_OFFSET + ((codePoint - 0xd800) >> SHIFT_2)] << INDEX_SHIFT) + (codePoint & DATA_MASK);
      return this.data[index];
    }
    if (codePoint < this.highStart) {
      index = this.data[(INDEX_1_OFFSET - OMITTED_BMP_INDEX_1_LENGTH) + (codePoint >> SHIFT_1)];
      index = this.data[index + ((codePoint >> SHIFT_2) & INDEX_2_MASK)];
      index = (index << INDEX_SHIFT) + (codePoint & DATA_MASK);
      return this.data[index];
    }
    return this.data[this.data.length - DATA_GRANULARITY];
  };

  return UnicodeTrie;

})();

module.exports = UnicodeTrie;

},{}],68:[function(_dereq_,module,exports){
module.exports={"data":[1961,1969,1977,1985,2025,2033,2041,2049,2057,2065,2073,2081,2089,2097,2105,2113,2121,2129,2137,2145,2153,2161,2169,2177,2185,2193,2201,2209,2217,2225,2233,2241,2249,2257,2265,2273,2281,2289,2297,2305,2313,2321,2329,2337,2345,2353,2361,2369,2377,2385,2393,2401,2409,2417,2425,2433,2441,2449,2457,2465,2473,2481,2489,2497,2505,2513,2521,2529,2529,2537,2009,2545,2553,2561,2569,2577,2585,2593,2601,2609,2617,2625,2633,2641,2649,2657,2665,2673,2681,2689,2697,2705,2713,2721,2729,2737,2745,2753,2761,2769,2777,2785,2793,2801,2809,2817,2825,2833,2841,2849,2857,2865,2873,2881,2889,2009,2897,2905,2913,2009,2921,2929,2937,2945,2953,2961,2969,2009,2977,2977,2985,2993,3001,3009,3009,3009,3017,3017,3017,3025,3025,3033,3041,3041,3049,3049,3049,3049,3049,3049,3049,3049,3049,3049,3057,3065,3073,3073,3073,3081,3089,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3097,3105,3113,3113,3121,3129,3137,3145,3153,3161,3161,3169,3177,3185,3193,3193,3193,3193,3201,3209,3209,3217,3225,3233,3241,3241,3241,3249,3257,3265,3273,3273,3281,3289,3297,2009,2009,3305,3313,3321,3329,3337,3345,3353,3361,3369,3377,3385,3393,2009,2009,3401,3409,3417,3417,3417,3417,3417,3417,3425,3425,3433,3433,3433,3433,3433,3433,3433,3433,3433,3433,3433,3433,3433,3433,3433,3441,3449,3457,3465,3473,3481,3489,3497,3505,3513,3521,3529,3537,3545,3553,3561,3569,3577,3585,3593,3601,3609,3617,3625,3625,3633,3641,3649,3649,3649,3649,3649,3657,3665,3665,3673,3681,3681,3681,3681,3689,3697,3697,3705,3713,3721,3729,3737,3745,3753,3761,3769,3777,3785,3793,3801,3809,3817,3825,3833,3841,3849,3857,3865,3873,3881,3881,3881,3881,3881,3881,3881,3881,3881,3881,3881,3881,3889,3897,3905,3913,3921,3921,3921,3921,3921,3921,3921,3921,3921,3921,3929,2009,2009,2009,2009,2009,3937,3937,3937,3937,3937,3937,3937,3945,3953,3953,3953,3961,3969,3969,3977,3985,3993,4001,2009,2009,4009,4009,4009,4009,4009,4009,4009,4009,4009,4009,4009,4009,4017,4025,4033,4041,4049,4057,4065,4073,4081,4081,4081,4081,4081,4081,4081,4089,4097,4097,4105,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4113,4121,4121,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4129,4137,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4145,4153,4161,4169,4169,4169,4169,4169,4169,4169,4169,4177,4185,4193,4201,4209,4217,4217,4225,4233,4233,4233,4233,4233,4233,4233,4233,4241,4249,4257,4265,4273,4281,4289,4297,4305,4313,4321,4329,4337,4345,4353,4361,4361,4369,4377,4385,4385,4385,4385,4393,4401,4409,4409,4409,4409,4409,4409,4417,4425,4433,4441,4449,4457,4465,4473,4481,4489,4497,4505,4513,4521,4529,4537,4545,4553,4561,4569,4577,4585,4593,4601,4609,4617,4625,4633,4641,4649,4657,4665,4673,4681,4689,4697,4705,4713,4721,4729,4737,4745,4753,4761,4769,4777,4785,4793,4801,4809,4817,4825,4833,4841,4849,4857,4865,4873,4881,4889,4897,4905,4913,4921,4929,4937,4945,4953,4961,4969,4977,4985,4993,5001,5009,5017,5025,5033,5041,5049,5057,5065,5073,5081,5089,5097,5105,5113,5121,5129,5137,5145,5153,5161,5169,5177,5185,5193,5201,5209,5217,5225,5233,5241,5249,5257,5265,5273,5281,5289,5297,5305,5313,5321,5329,5337,5345,5353,5361,5369,5377,5385,5393,5401,5409,5417,5425,5433,5441,5449,5457,5465,5473,5481,5489,5497,5505,5513,5521,5529,5537,5545,5553,5561,5569,5577,5585,5593,5601,5609,5617,5625,5633,5641,5649,5657,5665,5673,5681,5689,5697,5705,5713,5721,5729,5737,5745,5753,5761,5769,5777,5785,5793,5801,5809,5817,5825,5833,5841,5849,5857,5865,5873,5881,5889,5897,5905,5913,5921,5929,5937,5945,5953,5961,5969,5977,5985,5993,6001,6009,6017,6025,6033,6041,6049,6057,6065,6073,6081,6089,6097,6105,6113,6121,6129,6137,6145,6153,6161,6169,6177,6185,6193,6201,6209,6217,6225,6233,6241,6249,6257,6265,6273,6281,6289,6297,6305,6313,6321,6329,6337,6345,6353,6361,6369,6377,6385,6393,6401,6409,6417,6425,6433,6441,6449,6457,6465,6473,6481,6489,6497,6505,6513,6521,6529,6537,6545,6553,6561,6569,6577,6585,6593,6601,6609,6617,6625,6633,6641,6649,6657,6665,6673,6681,6689,6697,6705,6713,6721,6729,6737,6745,6753,6761,6769,6777,6785,6793,6801,6809,6817,6825,6833,6841,6849,6857,6865,6873,6881,6889,6897,6905,6913,6921,6929,6937,6945,6953,6961,6969,6977,6985,6993,7001,7009,7017,7025,7033,7041,7049,7057,7065,7073,7081,7089,7097,7105,7113,7121,7129,7137,7145,7153,7161,7169,7177,7185,7193,7201,7209,7217,7225,7233,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7249,7257,7265,7273,7281,7281,7281,7281,7281,7281,7281,7281,7281,7281,7281,7281,7281,7281,7289,7297,7305,7305,7305,7305,7313,7321,7329,7337,7345,7353,7353,7353,7361,7369,7377,7385,7393,7401,7409,7417,7425,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7241,7972,7972,8100,8164,8228,8292,8356,8420,8484,8548,8612,8676,8740,8804,8868,8932,8996,9060,9124,9188,9252,9316,9380,9444,9508,9572,9636,9700,9764,9828,9892,9956,2593,2657,2721,2529,2785,2529,2849,2913,2977,3041,3105,3169,3233,3297,2529,2529,2529,2529,2529,2529,2529,2529,3361,2529,2529,2529,3425,2529,2529,3489,3553,2529,3617,3681,3745,3809,3873,3937,4001,4065,4129,4193,4257,4321,4385,4449,4513,4577,4641,4705,4769,4833,4897,4961,5025,5089,5153,5217,5281,5345,5409,5473,5537,5601,5665,5729,5793,5857,5921,5985,6049,6113,6177,6241,6305,6369,6433,6497,6561,6625,6689,6753,6817,6881,6945,7009,7073,7137,7201,7265,7329,7393,7457,7521,7585,7649,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,2529,7713,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7433,7433,7433,7433,7433,7433,7433,7441,7449,7457,7457,7457,7457,7457,7457,7465,2009,2009,2009,2009,7473,7473,7473,7473,7473,7473,7473,7473,7481,7489,7497,7505,7505,7505,7505,7505,7513,7521,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7529,7529,7537,7545,7545,7545,7545,7545,7553,7561,7561,7561,7561,7561,7561,7561,7569,7577,7585,7593,7593,7593,7593,7593,7593,7601,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7609,7617,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7625,7633,7641,7649,7657,7665,7673,7681,7689,7697,7705,2009,7713,7721,7729,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7737,7745,7753,2009,2009,2009,2009,2009,2009,2009,2009,2009,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7761,7769,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7777,7785,7793,7801,7809,7809,7809,7809,7809,7809,7817,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7825,7833,7841,7849,2009,2009,2009,7857,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7865,7865,7865,7865,7865,7865,7865,7865,7865,7865,7865,7873,7881,7889,7897,7897,7897,7897,7905,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7913,7921,7929,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,7937,7937,7937,7937,7937,7937,7937,7945,2009,2009,2009,2009,2009,2009,2009,2009,7953,7953,7953,7953,7953,7953,7953,2009,7961,7969,7977,7985,7993,2009,2009,8001,8009,8009,8009,8009,8009,8009,8009,8009,8009,8009,8009,8009,8009,8017,8025,8025,8025,8025,8025,8025,8025,8033,8041,8049,8057,8065,8073,8081,8081,8081,8081,8081,8081,8081,8081,8081,8081,8081,8089,2009,8097,8097,8097,8105,2009,2009,2009,2009,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8113,8121,8129,8137,8137,8137,8137,8137,8137,8137,8137,8137,8137,8137,8137,8137,8137,8145,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,2009,67496,67496,67496,21,21,21,21,21,21,21,21,21,17,34,30,30,33,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,38,6,3,12,9,10,12,3,0,2,12,9,8,16,8,7,11,11,11,11,11,11,11,11,11,11,8,8,12,12,12,6,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,9,2,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,17,1,12,21,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,21,21,35,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,4,0,10,9,9,9,12,29,29,12,29,3,12,17,12,12,10,9,29,29,18,12,29,29,29,29,29,3,29,29,29,0,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,18,29,29,29,18,29,12,12,29,12,12,12,12,12,12,12,29,29,29,29,12,29,12,18,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,4,21,21,21,21,21,21,21,21,21,21,21,21,4,4,4,4,4,4,4,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,8,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,8,17,39,39,39,39,9,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,17,21,12,21,21,12,21,21,6,21,39,39,39,39,39,39,39,39,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,10,10,10,8,8,12,12,21,21,21,21,21,21,21,21,21,21,21,6,6,6,6,6,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,11,11,11,11,11,11,11,11,11,11,10,11,11,12,12,12,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,6,12,21,21,21,21,21,21,21,12,12,21,21,21,21,21,21,12,12,21,21,12,21,21,21,21,12,12,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,12,39,39,39,39,39,39,39,39,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,12,12,12,12,8,6,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,12,21,21,21,21,21,21,21,21,21,12,21,21,21,12,21,21,21,21,21,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,21,21,17,17,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,21,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,21,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,39,39,39,39,39,39,39,39,21,39,39,39,39,12,12,12,12,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,12,12,10,10,12,12,12,12,12,10,12,9,39,39,39,39,39,21,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,12,12,12,12,12,12,39,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,21,21,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,21,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,12,9,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,21,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,12,12,12,12,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,12,39,39,39,39,39,39,21,39,39,39,39,39,39,39,39,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,9,12,39,39,39,39,39,39,21,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,12,12,12,12,12,12,12,12,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,39,39,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,21,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,12,12,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,39,12,12,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,39,39,39,39,39,39,39,39,21,39,39,39,39,39,39,39,39,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,39,39,39,10,12,12,12,12,12,12,39,39,21,21,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,39,39,39,39,39,39,39,39,39,39,39,39,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,39,39,39,39,9,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,12,11,11,11,11,11,11,11,11,11,11,17,17,39,39,39,39,39,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,39,39,11,11,11,11,11,11,11,11,11,11,39,39,36,36,36,36,12,18,18,18,18,12,18,18,4,18,18,17,4,6,6,6,6,6,4,12,6,12,12,12,21,21,12,12,12,12,12,12,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,17,21,12,21,12,21,0,1,0,1,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,17,21,21,21,21,21,17,21,21,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,17,17,12,12,12,12,12,12,21,12,12,12,12,12,12,12,12,12,18,18,17,18,12,12,12,12,12,4,4,39,39,39,39,39,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,11,11,11,11,11,11,11,11,11,11,17,17,12,12,12,12,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,11,11,11,11,11,11,11,11,11,11,36,36,36,36,36,36,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,21,21,21,12,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,17,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,17,17,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,21,21,39,39,39,39,39,39,39,39,39,39,39,39,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,17,17,5,36,17,12,17,9,36,36,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,6,6,17,17,18,12,6,6,12,21,21,21,4,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,12,39,39,39,6,6,11,11,11,11,11,11,11,11,11,11,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,36,36,36,36,36,36,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,39,39,12,12,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,39,39,21,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,36,36,36,36,36,36,36,36,36,36,36,36,36,36,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,39,39,39,39,11,11,11,11,11,11,11,11,11,11,17,17,12,17,17,17,17,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,39,39,39,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,17,17,17,17,17,11,11,11,11,11,11,11,11,11,11,39,39,39,12,12,12,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,17,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,21,21,21,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,21,12,12,12,12,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,18,12,39,17,17,17,17,17,17,17,4,17,17,17,20,21,21,21,21,17,4,17,17,19,29,29,12,3,3,0,3,3,3,0,3,29,29,12,12,15,15,15,17,30,30,21,21,21,21,21,4,10,10,10,10,10,10,10,10,12,3,3,29,5,5,12,12,12,12,12,12,8,0,1,5,5,5,12,12,12,12,12,12,12,12,12,12,12,12,17,12,17,17,17,17,12,17,17,17,22,12,12,12,12,39,39,39,39,39,21,21,21,21,21,21,12,12,39,39,29,12,12,12,12,12,12,12,12,0,1,29,12,29,29,29,29,12,12,12,12,12,12,12,12,0,1,39,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,9,9,9,9,9,9,9,10,9,9,9,9,9,9,9,9,9,9,9,9,9,9,10,9,9,9,9,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,10,12,29,12,12,12,10,12,12,12,12,12,12,12,12,12,29,12,12,9,12,12,12,12,12,12,12,12,12,12,29,29,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,12,12,12,12,12,29,12,12,29,12,29,29,29,29,29,29,29,29,29,29,29,29,12,12,12,12,29,29,29,29,29,29,29,29,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,29,29,12,12,12,29,29,12,12,29,12,12,12,29,12,29,9,9,12,29,12,12,12,12,29,12,12,29,29,29,29,12,12,29,12,29,12,29,29,29,29,29,29,12,29,12,12,12,12,12,29,29,29,29,12,12,12,12,29,29,12,12,12,12,12,12,12,12,12,12,29,12,12,12,29,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,12,12,29,29,29,29,12,12,29,29,12,12,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,12,12,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,14,14,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,12,12,12,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,12,12,12,12,12,12,12,12,12,12,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,12,29,29,29,29,12,12,12,12,12,12,12,12,12,12,29,29,12,29,29,29,29,29,29,29,12,12,12,12,12,12,12,12,29,29,12,12,29,29,12,12,12,12,29,29,12,12,29,29,12,12,12,12,29,29,29,12,12,29,12,12,29,29,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,29,29,12,12,12,12,12,12,12,12,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,14,14,12,29,29,12,12,29,12,12,12,12,29,29,12,12,12,12,14,14,29,29,14,12,14,14,14,14,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,14,12,12,12,12,29,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,12,29,29,29,12,29,14,29,29,12,29,29,12,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,14,14,14,14,14,14,14,14,14,14,29,29,29,29,14,12,14,14,14,29,14,14,29,29,29,14,14,29,29,14,29,29,14,14,14,12,29,12,12,12,12,29,29,14,29,29,29,29,29,29,14,14,14,14,14,29,14,14,14,14,29,29,14,14,14,14,14,14,14,14,12,12,12,14,14,14,14,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,12,12,12,3,3,3,3,12,12,12,6,6,12,12,12,12,0,1,0,1,0,1,0,1,0,1,0,1,0,1,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,0,1,0,1,0,1,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,29,29,29,29,29,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,12,12,39,39,39,39,39,6,17,17,17,12,6,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,3,3,3,3,3,3,3,3,3,3,3,3,3,3,17,17,17,17,17,17,17,17,12,17,0,17,12,12,3,3,12,12,3,3,0,1,0,1,0,1,0,1,17,17,17,17,6,12,17,17,12,17,17,12,12,12,12,12,19,19,39,39,39,39,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,1,1,14,14,5,14,14,0,1,0,1,0,1,0,1,0,1,14,14,0,1,0,1,0,1,0,1,5,0,1,1,14,14,14,14,14,14,14,14,14,14,21,21,21,21,21,21,14,14,14,14,14,14,14,14,14,14,14,5,5,14,14,14,39,32,14,32,14,32,14,32,14,32,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,32,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,32,14,32,14,32,14,14,14,14,14,14,32,14,14,14,14,14,14,32,32,39,39,21,21,5,5,5,5,14,5,32,14,32,14,32,14,32,14,32,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,32,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,32,14,32,14,32,14,14,14,14,14,14,32,14,14,14,14,14,14,32,32,14,14,14,14,5,32,5,5,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,39,39,39,39,39,39,39,39,39,39,39,39,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,29,29,29,29,29,29,29,29,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,5,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,6,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,12,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,12,17,17,17,17,17,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,12,12,12,21,12,12,12,12,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,10,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,18,18,6,6,39,39,39,39,39,39,39,39,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,39,17,17,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,39,39,39,39,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,17,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,39,39,39,12,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,39,39,39,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,17,17,17,12,12,12,12,12,12,11,11,11,11,11,11,11,11,11,11,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,39,12,12,12,21,12,12,12,12,12,12,12,12,21,21,39,39,11,11,11,11,11,11,11,11,11,11,39,39,12,17,17,17,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,36,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,17,17,12,12,12,21,21,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,17,21,21,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,39,39,39,39,39,39,39,39,39,39,39,39,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,39,39,39,39,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,39,39,39,39,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,13,21,13,13,13,13,13,13,13,13,13,13,12,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,10,12,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,8,1,1,8,8,6,6,0,1,15,39,39,39,39,39,39,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,39,14,14,14,14,14,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,14,14,0,1,14,14,14,14,14,14,14,1,14,1,39,5,5,6,6,14,0,1,0,1,0,1,14,14,14,14,14,14,14,14,14,14,9,10,14,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,22,39,6,14,14,9,10,14,14,0,1,14,14,1,14,1,14,14,14,14,14,14,14,14,14,14,14,5,5,14,14,14,6,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,0,14,1,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,0,14,1,14,0,1,1,0,1,1,5,12,32,32,32,32,32,32,32,32,32,32,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,5,5,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,10,9,14,14,14,9,9,39,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,21,21,21,31,29,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,17,17,17,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,17,17,17,17,17,17,17,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,17,17,17,17,17,17,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,17,17,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,12,12,12,17,17,17,17,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,11,11,11,11,11,11,11,11,11,11,17,17,17,17,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,17,17,12,17,39,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,11,11,11,11,11,11,11,11,11,11,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,39,39,17,17,17,17,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,0,0,1,1,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,1,12,12,12,0,1,0,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,0,1,1,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,21,12,12,12,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,12,12,21,21,21,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,21,21,21,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,39,39,39,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,39,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,12,12,39,39,39,39,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,39,39,39,39,39,39,39,39,39,39,39,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,12,12,14,14,14,14,14,12,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,12,14,12,14,12,14,14,14,14,14,14,14,14,14,14,12,14,12,12,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,39,39,39,12,12,12,12,12,12,12,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,14,14,14,14,14,14,14,14,14,14,14,14,14,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,39,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,39,39,39,39,39,39,39,39,39,39,39,39,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,39,39,39,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39],"highStart":919552,"errorValue":0}
},{}],69:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var AI, AL, B2, BA, BB, BK, CB, CJ, CL, CM, CP, CR, EX, GL, H2, H3, HL, HY, ID, IN, IS, JL, JT, JV, LF, NL, NS, NU, OP, PO, PR, QU, RI, SA, SG, SP, SY, WJ, XX, ZW;

  exports.OP = OP = 0;

  exports.CL = CL = 1;

  exports.CP = CP = 2;

  exports.QU = QU = 3;

  exports.GL = GL = 4;

  exports.NS = NS = 5;

  exports.EX = EX = 6;

  exports.SY = SY = 7;

  exports.IS = IS = 8;

  exports.PR = PR = 9;

  exports.PO = PO = 10;

  exports.NU = NU = 11;

  exports.AL = AL = 12;

  exports.HL = HL = 13;

  exports.ID = ID = 14;

  exports.IN = IN = 15;

  exports.HY = HY = 16;

  exports.BA = BA = 17;

  exports.BB = BB = 18;

  exports.B2 = B2 = 19;

  exports.ZW = ZW = 20;

  exports.CM = CM = 21;

  exports.WJ = WJ = 22;

  exports.H2 = H2 = 23;

  exports.H3 = H3 = 24;

  exports.JL = JL = 25;

  exports.JV = JV = 26;

  exports.JT = JT = 27;

  exports.RI = RI = 28;

  exports.AI = AI = 29;

  exports.BK = BK = 30;

  exports.CB = CB = 31;

  exports.CJ = CJ = 32;

  exports.CR = CR = 33;

  exports.LF = LF = 34;

  exports.NL = NL = 35;

  exports.SA = SA = 36;

  exports.SG = SG = 37;

  exports.SP = SP = 38;

  exports.XX = XX = 39;

}).call(this);

},{}],70:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var AI, AL, BA, BK, CB, CI_BRK, CJ, CP_BRK, CR, DI_BRK, ID, IN_BRK, LF, LineBreaker, NL, NS, PR_BRK, SA, SG, SP, UnicodeTrie, WJ, XX, characterClasses, classTrie, pairTable, _ref, _ref1;

  UnicodeTrie = _dereq_('unicode-trie');

  classTrie = new UnicodeTrie(_dereq_('./class_trie.json'));

  _ref = _dereq_('./classes'), BK = _ref.BK, CR = _ref.CR, LF = _ref.LF, NL = _ref.NL, CB = _ref.CB, BA = _ref.BA, SP = _ref.SP, WJ = _ref.WJ, SP = _ref.SP, BK = _ref.BK, LF = _ref.LF, NL = _ref.NL, AI = _ref.AI, AL = _ref.AL, SA = _ref.SA, SG = _ref.SG, XX = _ref.XX, CJ = _ref.CJ, ID = _ref.ID, NS = _ref.NS, characterClasses = _ref.characterClasses;

  _ref1 = _dereq_('./pairs'), DI_BRK = _ref1.DI_BRK, IN_BRK = _ref1.IN_BRK, CI_BRK = _ref1.CI_BRK, CP_BRK = _ref1.CP_BRK, PR_BRK = _ref1.PR_BRK, pairTable = _ref1.pairTable;

  LineBreaker = (function() {
    var Break, mapClass, mapFirst;

    function LineBreaker(string) {
      this.string = string;
      this.pos = 0;
      this.lastPos = 0;
      this.curClass = null;
      this.nextClass = null;
    }

    LineBreaker.prototype.nextCodePoint = function() {
      var code, next;
      code = this.string.charCodeAt(this.pos++);
      next = this.string.charCodeAt(this.pos);
      if ((0xd800 <= code && code <= 0xdbff) && (0xdc00 <= next && next <= 0xdfff)) {
        this.pos++;
        return ((code - 0xd800) * 0x400) + (next - 0xdc00) + 0x10000;
      }
      return code;
    };

    mapClass = function(c) {
      switch (c) {
        case AI:
          return AL;
        case SA:
        case SG:
        case XX:
          return AL;
        case CJ:
          return NS;
        default:
          return c;
      }
    };

    mapFirst = function(c) {
      switch (c) {
        case LF:
        case NL:
          return BK;
        case CB:
          return BA;
        case SP:
          return WJ;
        default:
          return c;
      }
    };

    LineBreaker.prototype.nextCharClass = function(first) {
      if (first == null) {
        first = false;
      }
      return mapClass(classTrie.get(this.nextCodePoint()));
    };

    Break = (function() {
      function Break(position, required) {
        this.position = position;
        this.required = required != null ? required : false;
      }

      return Break;

    })();

    LineBreaker.prototype.nextBreak = function() {
      var cur, lastClass, shouldBreak;
      if (this.curClass == null) {
        this.curClass = mapFirst(this.nextCharClass());
      }
      while (this.pos < this.string.length) {
        this.lastPos = this.pos;
        lastClass = this.nextClass;
        this.nextClass = this.nextCharClass();
        if (this.curClass === BK || (this.curClass === CR && this.nextClass !== LF)) {
          this.curClass = mapFirst(mapClass(this.nextClass));
          return new Break(this.lastPos, true);
        }
        cur = (function() {
          switch (this.nextClass) {
            case SP:
              return this.curClass;
            case BK:
            case LF:
            case NL:
              return BK;
            case CR:
              return CR;
            case CB:
              return BA;
          }
        }).call(this);
        if (cur != null) {
          this.curClass = cur;
          if (this.nextClass === CB) {
            return new Break(this.lastPos);
          }
          continue;
        }
        shouldBreak = false;
        switch (pairTable[this.curClass][this.nextClass]) {
          case DI_BRK:
            shouldBreak = true;
            break;
          case IN_BRK:
            shouldBreak = lastClass === SP;
            break;
          case CI_BRK:
            shouldBreak = lastClass === SP;
            if (!shouldBreak) {
              continue;
            }
            break;
          case CP_BRK:
            if (lastClass !== SP) {
              continue;
            }
        }
        this.curClass = this.nextClass;
        if (shouldBreak) {
          return new Break(this.lastPos);
        }
      }
      if (this.pos >= this.string.length) {
        if (this.lastPos < this.string.length) {
          this.lastPos = this.string.length;
          return new Break(this.string.length);
        } else {
          return null;
        }
      }
    };

    return LineBreaker;

  })();

  module.exports = LineBreaker;

}).call(this);

},{"./class_trie.json":68,"./classes":69,"./pairs":71,"unicode-trie":67}],71:[function(_dereq_,module,exports){
// Generated by CoffeeScript 1.7.1
(function() {
  var CI_BRK, CP_BRK, DI_BRK, IN_BRK, PR_BRK;

  exports.DI_BRK = DI_BRK = 0;

  exports.IN_BRK = IN_BRK = 1;

  exports.CI_BRK = CI_BRK = 2;

  exports.CP_BRK = CP_BRK = 3;

  exports.PR_BRK = PR_BRK = 4;

  exports.pairTable = [[PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, CP_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, CI_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, CI_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, DI_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, DI_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, CI_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, PR_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK], [IN_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, CI_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, IN_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, DI_BRK], [DI_BRK, PR_BRK, PR_BRK, IN_BRK, IN_BRK, IN_BRK, PR_BRK, PR_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK, IN_BRK, DI_BRK, DI_BRK, PR_BRK, CI_BRK, PR_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, DI_BRK, IN_BRK]];

}).call(this);

},{}],72:[function(_dereq_,module,exports){
(function (Buffer){
// Generated by CoffeeScript 1.4.0

/*
# MIT LICENSE
# Copyright (c) 2011 Devon Govett
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy of this 
# software and associated documentation files (the "Software"), to deal in the Software 
# without restriction, including without limitation the rights to use, copy, modify, merge, 
# publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons 
# to whom the Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all copies or 
# substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
# DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


(function() {
  var PNG, fs, zlib;

  fs = _dereq_('fs');

  zlib = _dereq_('zlib');

  module.exports = PNG = (function() {

    PNG.decode = function(path, fn) {
      return fs.readFile(path, function(err, file) {
        var png;
        png = new PNG(file);
        return png.decode(function(pixels) {
          return fn(pixels);
        });
      });
    };

    PNG.load = function(path) {
      var file;
      file = fs.readFileSync(path);
      return new PNG(file);
    };

    function PNG(data) {
      var chunkSize, colors, i, index, key, section, short, text, _i, _j, _ref;
      this.data = data;
      this.pos = 8;
      this.palette = [];
      this.imgData = [];
      this.transparency = {};
      this.text = {};
      while (true) {
        chunkSize = this.readUInt32();
        section = ((function() {
          var _i, _results;
          _results = [];
          for (i = _i = 0; _i < 4; i = ++_i) {
            _results.push(String.fromCharCode(this.data[this.pos++]));
          }
          return _results;
        }).call(this)).join('');
        switch (section) {
          case 'IHDR':
            this.width = this.readUInt32();
            this.height = this.readUInt32();
            this.bits = this.data[this.pos++];
            this.colorType = this.data[this.pos++];
            this.compressionMethod = this.data[this.pos++];
            this.filterMethod = this.data[this.pos++];
            this.interlaceMethod = this.data[this.pos++];
            break;
          case 'PLTE':
            this.palette = this.read(chunkSize);
            break;
          case 'IDAT':
            for (i = _i = 0; _i < chunkSize; i = _i += 1) {
              this.imgData.push(this.data[this.pos++]);
            }
            break;
          case 'tRNS':
            this.transparency = {};
            switch (this.colorType) {
              case 3:
                this.transparency.indexed = this.read(chunkSize);
                short = 255 - this.transparency.indexed.length;
                if (short > 0) {
                  for (i = _j = 0; 0 <= short ? _j < short : _j > short; i = 0 <= short ? ++_j : --_j) {
                    this.transparency.indexed.push(255);
                  }
                }
                break;
              case 0:
                this.transparency.grayscale = this.read(chunkSize)[0];
                break;
              case 2:
                this.transparency.rgb = this.read(chunkSize);
            }
            break;
          case 'tEXt':
            text = this.read(chunkSize);
            index = text.indexOf(0);
            key = String.fromCharCode.apply(String, text.slice(0, index));
            this.text[key] = String.fromCharCode.apply(String, text.slice(index + 1));
            break;
          case 'IEND':
            this.colors = (function() {
              switch (this.colorType) {
                case 0:
                case 3:
                case 4:
                  return 1;
                case 2:
                case 6:
                  return 3;
              }
            }).call(this);
            this.hasAlphaChannel = (_ref = this.colorType) === 4 || _ref === 6;
            colors = this.colors + (this.hasAlphaChannel ? 1 : 0);
            this.pixelBitlength = this.bits * colors;
            this.colorSpace = (function() {
              switch (this.colors) {
                case 1:
                  return 'DeviceGray';
                case 3:
                  return 'DeviceRGB';
              }
            }).call(this);
            this.imgData = new Buffer(this.imgData);
            return;
          default:
            this.pos += chunkSize;
        }
        this.pos += 4;
        if (this.pos > this.data.length) {
          throw new Error("Incomplete or corrupt PNG file");
        }
      }
      return;
    }

    PNG.prototype.read = function(bytes) {
      var i, _i, _results;
      _results = [];
      for (i = _i = 0; 0 <= bytes ? _i < bytes : _i > bytes; i = 0 <= bytes ? ++_i : --_i) {
        _results.push(this.data[this.pos++]);
      }
      return _results;
    };

    PNG.prototype.readUInt32 = function() {
      var b1, b2, b3, b4;
      b1 = this.data[this.pos++] << 24;
      b2 = this.data[this.pos++] << 16;
      b3 = this.data[this.pos++] << 8;
      b4 = this.data[this.pos++];
      return b1 | b2 | b3 | b4;
    };

    PNG.prototype.readUInt16 = function() {
      var b1, b2;
      b1 = this.data[this.pos++] << 8;
      b2 = this.data[this.pos++];
      return b1 | b2;
    };

    PNG.prototype.decodePixels = function(fn) {
      var _this = this;
      return zlib.inflate(this.imgData, function(err, data) {
        var byte, c, col, i, left, length, p, pa, paeth, pb, pc, pixelBytes, pixels, pos, row, scanlineLength, upper, upperLeft, _i, _j, _k, _l, _m;
        if (err) {
          throw err;
        }
        pixelBytes = _this.pixelBitlength / 8;
        scanlineLength = pixelBytes * _this.width;
        pixels = new Buffer(scanlineLength * _this.height);
        length = data.length;
        row = 0;
        pos = 0;
        c = 0;
        while (pos < length) {
          switch (data[pos++]) {
            case 0:
              for (i = _i = 0; _i < scanlineLength; i = _i += 1) {
                pixels[c++] = data[pos++];
              }
              break;
            case 1:
              for (i = _j = 0; _j < scanlineLength; i = _j += 1) {
                byte = data[pos++];
                left = i < pixelBytes ? 0 : pixels[c - pixelBytes];
                pixels[c++] = (byte + left) % 256;
              }
              break;
            case 2:
              for (i = _k = 0; _k < scanlineLength; i = _k += 1) {
                byte = data[pos++];
                col = (i - (i % pixelBytes)) / pixelBytes;
                upper = row && pixels[(row - 1) * scanlineLength + col * pixelBytes + (i % pixelBytes)];
                pixels[c++] = (upper + byte) % 256;
              }
              break;
            case 3:
              for (i = _l = 0; _l < scanlineLength; i = _l += 1) {
                byte = data[pos++];
                col = (i - (i % pixelBytes)) / pixelBytes;
                left = i < pixelBytes ? 0 : pixels[c - pixelBytes];
                upper = row && pixels[(row - 1) * scanlineLength + col * pixelBytes + (i % pixelBytes)];
                pixels[c++] = (byte + Math.floor((left + upper) / 2)) % 256;
              }
              break;
            case 4:
              for (i = _m = 0; _m < scanlineLength; i = _m += 1) {
                byte = data[pos++];
                col = (i - (i % pixelBytes)) / pixelBytes;
                left = i < pixelBytes ? 0 : pixels[c - pixelBytes];
                if (row === 0) {
                  upper = upperLeft = 0;
                } else {
                  upper = pixels[(row - 1) * scanlineLength + col * pixelBytes + (i % pixelBytes)];
                  upperLeft = col && pixels[(row - 1) * scanlineLength + (col - 1) * pixelBytes + (i % pixelBytes)];
                }
                p = left + upper - upperLeft;
                pa = Math.abs(p - left);
                pb = Math.abs(p - upper);
                pc = Math.abs(p - upperLeft);
                if (pa <= pb && pa <= pc) {
                  paeth = left;
                } else if (pb <= pc) {
                  paeth = upper;
                } else {
                  paeth = upperLeft;
                }
                pixels[c++] = (byte + paeth) % 256;
              }
              break;
            default:
              throw new Error("Invalid filter algorithm: " + data[pos - 1]);
          }
          row++;
        }
        return fn(pixels);
      });
    };

    PNG.prototype.decodePalette = function() {
      var c, i, length, palette, pos, ret, transparency, _i, _ref, _ref1;
      palette = this.palette;
      transparency = this.transparency.indexed || [];
      ret = new Buffer(transparency.length + palette.length);
      pos = 0;
      length = palette.length;
      c = 0;
      for (i = _i = 0, _ref = palette.length; _i < _ref; i = _i += 3) {
        ret[pos++] = palette[i];
        ret[pos++] = palette[i + 1];
        ret[pos++] = palette[i + 2];
        ret[pos++] = (_ref1 = transparency[c++]) != null ? _ref1 : 255;
      }
      return ret;
    };

    PNG.prototype.copyToImageData = function(imageData, pixels) {
      var alpha, colors, data, i, input, j, k, length, palette, v, _ref;
      colors = this.colors;
      palette = null;
      alpha = this.hasAlphaChannel;
      if (this.palette.length) {
        palette = (_ref = this._decodedPalette) != null ? _ref : this._decodedPalette = this.decodePalette();
        colors = 4;
        alpha = true;
      }
      data = (imageData != null ? imageData.data : void 0) || imageData;
      length = data.length;
      input = palette || pixels;
      i = j = 0;
      if (colors === 1) {
        while (i < length) {
          k = palette ? pixels[i / 4] * 4 : j;
          v = input[k++];
          data[i++] = v;
          data[i++] = v;
          data[i++] = v;
          data[i++] = alpha ? input[k++] : 255;
          j = k;
        }
      } else {
        while (i < length) {
          k = palette ? pixels[i / 4] * 4 : j;
          data[i++] = input[k++];
          data[i++] = input[k++];
          data[i++] = input[k++];
          data[i++] = alpha ? input[k++] : 255;
          j = k;
        }
      }
    };

    PNG.prototype.decode = function(fn) {
      var ret,
        _this = this;
      ret = new Buffer(this.width * this.height * 4);
      return this.decodePixels(function(pixels) {
        _this.copyToImageData(ret, pixels);
        return fn(ret);
      });
    };

    return PNG;

  })();

}).call(this);

}).call(this,_dereq_("buffer").Buffer)
},{"buffer":16,"fs":"x/K9gc","zlib":15}],73:[function(_dereq_,module,exports){
(function (Buffer){
/* jslint node: true */
/* jslint browser: true */
/* global BlobBuilder */
'use strict';

var PdfPrinter = _dereq_('../printer');
var saveAs = _dereq_('../../libs/fileSaver');

var defaultClientFonts = {
	Roboto: {
		normal: 'Roboto-Regular.ttf',
		bold: 'Roboto-Medium.ttf',
		italics: 'Roboto-Italic.ttf',
		bolditalics: 'Roboto-Italic.ttf'
	}
};

function Document(docDefinition, fonts, vfs) {
	this.docDefinition = docDefinition;
	this.fonts = fonts || defaultClientFonts;
	this.vfs = vfs;
}

Document.prototype._createDoc = function(options, callback) {
	var printer = new PdfPrinter(this.fonts);
	printer.fs.bindFS(this.vfs);

	var doc = printer.createPdfKitDocument(this.docDefinition, options);
	var chunks = [];
	var result;

	doc.on('data', function(chunk) {
		chunks.push(chunk);
	});
	doc.on('end', function() {
		result = Buffer.concat(chunks);
		callback(result);
	});
	doc.end();
};

Document.prototype.open = function(message) {
	// we have to open the window immediately and store the reference
	// otherwise popup blockers will stop us
	var win = window.open('', '_blank');

	try {
		this.getDataUrl(function(result) {
			win.location.href = result;
		});
	} catch(e) {
		win.close();
		return false;
	}
};


Document.prototype.print = function() {
  this.getDataUrl(function(dataUrl) {
    var iFrame = document.createElement('iframe');
    iFrame.style.position = 'absolute';
    iFrame.style.left = '-99999px';
    iFrame.src = dataUrl;
    iFrame.onload = function() {
      function removeIFrame(){
        document.body.removeChild(iFrame);
        document.removeEventListener('click', removeIFrame);
      }
      document.addEventListener('click', removeIFrame, false);
    };

    document.body.appendChild(iFrame);
  }, { autoPrint: true });
};

Document.prototype.download = function(defaultFileName, cb) {
   if(typeof defaultFileName === "function") {
      cb = defaultFileName;
      defaultFileName = null;
   }

   defaultFileName = defaultFileName || 'file.pdf';
   this.getBuffer(function(result) {
       saveAs(new Blob([result], {type: 'application/pdf'}), defaultFileName);
       if (typeof cb === "function") {
           cb();
       }
   });
};

Document.prototype.getBase64 = function(cb, options) {
	if (!cb) throw 'getBase64 is an async method and needs a callback argument';
	this._createDoc(options, function(buffer) {
		cb(buffer.toString('base64'));
	});
};

Document.prototype.getDataUrl = function(cb, options) {
	if (!cb) throw 'getDataUrl is an async method and needs a callback argument';
	this._createDoc(options, function(buffer) {
		cb('data:application/pdf;base64,' + buffer.toString('base64'));
	});
};

Document.prototype.getBuffer = function(cb, options) {
	if (!cb) throw 'getBuffer is an async method and needs a callback argument';
	this._createDoc(options, cb);
};

module.exports = {
	createPdf: function(docDefinition) {
		return new Document(docDefinition, window.pdfMake.fonts, window.pdfMake.vfs);
	}
};

}).call(this,_dereq_("buffer").Buffer)
},{"../../libs/fileSaver":1,"../printer":85,"buffer":16}],"x/K9gc":[function(_dereq_,module,exports){
(function (Buffer,__dirname){
/* jslint node: true */
'use strict';

// var b64 = require('./base64.js').base64DecToArr;
function VirtualFileSystem() {
	this.fileSystem = {};
	this.baseSystem = {};
}

VirtualFileSystem.prototype.readFileSync = function(filename) {
	filename = fixFilename(filename);

	var base64content = this.baseSystem[filename];
	if (base64content) {
		return new Buffer(base64content, 'base64');
	}

	return this.fileSystem[filename];
};

VirtualFileSystem.prototype.writeFileSync = function(filename, content) {
	this.fileSystem[fixFilename(filename)] = content;
};

VirtualFileSystem.prototype.bindFS = function(data) {
	this.baseSystem = data;
};


function fixFilename(filename) {
	if (filename.indexOf(__dirname) === 0) {
		filename = filename.substring(__dirname.length);
	}

	if (filename.indexOf('/') === 0) {
		filename = filename.substring(1);
	}

	return filename;
}

module.exports = new VirtualFileSystem();

}).call(this,_dereq_("buffer").Buffer,"/")
},{"buffer":16}],"fs":[function(_dereq_,module,exports){
module.exports=_dereq_('x/K9gc');
},{}],76:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

function buildColumnWidths(columns, availableWidth) {
	var autoColumns = [],
		autoMin = 0, autoMax = 0,
		starColumns = [],
		starMaxMin = 0,
		starMaxMax = 0,
		fixedColumns = [],
		initial_availableWidth = availableWidth;

	columns.forEach(function(column) {
		if (isAutoColumn(column)) {
			autoColumns.push(column);
			autoMin += column._minWidth;
			autoMax += column._maxWidth;
		} else if (isStarColumn(column)) {
			starColumns.push(column);
			starMaxMin = Math.max(starMaxMin, column._minWidth);
			starMaxMax = Math.max(starMaxMax, column._maxWidth);
		} else {
			fixedColumns.push(column);
		}
	});

	fixedColumns.forEach(function(col) {
		// width specified as %
		if (typeof col.width === 'string' && /\d+%/.test(col.width) ) {
			col.width = parseFloat(col.width)*initial_availableWidth/100;
		}
		if (col.width < (col._minWidth) && col.elasticWidth) {
			col._calcWidth = col._minWidth;
		} else {
			col._calcWidth = col.width;
		}

		availableWidth -= col._calcWidth;
	});

	// http://www.freesoft.org/CIE/RFC/1942/18.htm
	// http://www.w3.org/TR/CSS2/tables.html#width-layout
	// http://dev.w3.org/csswg/css3-tables-algorithms/Overview.src.htm
	var minW = autoMin + starMaxMin * starColumns.length;
	var maxW = autoMax + starMaxMax * starColumns.length;
	if (minW >= availableWidth) {
		// case 1 - there's no way to fit all columns within available width
		// that's actually pretty bad situation with PDF as we have no horizontal scroll
		// no easy workaround (unless we decide, in the future, to split single words)
		// currently we simply use minWidths for all columns
		autoColumns.forEach(function(col) {
			col._calcWidth = col._minWidth;
		});

		starColumns.forEach(function(col) {
			col._calcWidth = starMaxMin; // starMaxMin already contains padding
		});
	} else {
		if (maxW < availableWidth) {
			// case 2 - we can fit rest of the table within available space
			autoColumns.forEach(function(col) {
				col._calcWidth = col._maxWidth;
				availableWidth -= col._calcWidth;
			});
		} else {
			// maxW is too large, but minW fits within available width
			var W = availableWidth - minW;
			var D = maxW - minW;

			autoColumns.forEach(function(col) {
				var d = col._maxWidth - col._minWidth;
				col._calcWidth = col._minWidth + d * W / D;
				availableWidth -= col._calcWidth;
			});
		}

		if (starColumns.length > 0) {
			var starSize = availableWidth / starColumns.length;

			starColumns.forEach(function(col) {
				col._calcWidth = starSize;
			});
		}
	}
}

function isAutoColumn(column) {
	return column.width === 'auto';
}

function isStarColumn(column) {
	return column.width === null || column.width === undefined || column.width === '*' || column.width === 'star';
}

//TODO: refactor and reuse in measureTable
function measureMinMax(columns) {
	var result = { min: 0, max: 0 };

	var maxStar = { min: 0, max: 0 };
	var starCount = 0;

	for(var i = 0, l = columns.length; i < l; i++) {
		var c = columns[i];

		if (isStarColumn(c)) {
			maxStar.min = Math.max(maxStar.min, c._minWidth);
			maxStar.max = Math.max(maxStar.max, c._maxWidth);
			starCount++;
		} else if (isAutoColumn(c)) {
			result.min += c._minWidth;
			result.max += c._maxWidth;
		} else {
			result.min += ((c.width !== undefined && c.width) || c._minWidth);
			result.max += ((c.width  !== undefined && c.width) || c._maxWidth);
		}
	}

	if (starCount) {
		result.min += starCount * maxStar.min;
		result.max += starCount * maxStar.max;
	}

	return result;
}

/**
* Calculates column widths
* @private
*/
module.exports = {
	buildColumnWidths: buildColumnWidths,
	measureMinMax: measureMinMax,
	isAutoColumn: isAutoColumn,
	isStarColumn: isStarColumn
};

},{}],77:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

var TextTools = _dereq_('./textTools');
var StyleContextStack = _dereq_('./styleContextStack');
var ColumnCalculator = _dereq_('./columnCalculator');
var fontStringify = _dereq_('./helpers').fontStringify;
var pack = _dereq_('./helpers').pack;
var qrEncoder = _dereq_('./qrEnc.js');

/**
* @private
*/
function DocMeasure(fontProvider, styleDictionary, defaultStyle, imageMeasure, tableLayouts, images) {
	this.textTools = new TextTools(fontProvider);
	this.styleStack = new StyleContextStack(styleDictionary, defaultStyle);
	this.imageMeasure = imageMeasure;
	this.tableLayouts = tableLayouts;
	this.images = images;
	this.autoImageIndex = 1;
}

/**
* Measures all nodes and sets min/max-width properties required for the second
* layout-pass.
* @param  {Object} docStructure document-definition-object
* @return {Object}              document-measurement-object
*/
DocMeasure.prototype.measureDocument = function(docStructure) {
	return this.measureNode(docStructure);
};

DocMeasure.prototype.measureNode = function(node) {
	// expand shortcuts
	if (node instanceof Array) {
		node = { stack: node };
	} else if (typeof node == 'string' || node instanceof String) {
		node = { text: node };
	}

	var self = this;

	return this.styleStack.auto(node, function() {
		// TODO: refactor + rethink whether this is the proper way to handle margins
		node._margin = getNodeMargin(node);

		if (node.columns) {
			return extendMargins(self.measureColumns(node));
		} else if (node.stack) {
			return extendMargins(self.measureVerticalContainer(node));
		} else if (node.ul) {
			return extendMargins(self.measureList(false, node));
		} else if (node.ol) {
			return extendMargins(self.measureList(true, node));
		} else if (node.table) {
			return extendMargins(self.measureTable(node));
		} else if (node.text !== undefined) {
			return extendMargins(self.measureLeaf(node));
		} else if (node.image) {
			return extendMargins(self.measureImage(node));
		} else if (node.canvas) {
			return extendMargins(self.measureCanvas(node));
		} else if (node.qr) {
			return extendMargins(self.measureQr(node));
		} else {
			throw 'Unrecognized document structure: ' + JSON.stringify(node, fontStringify);
		}
	});

	function extendMargins(node) {
		var margin = node._margin;

		if (margin) {
			node._minWidth += margin[0] + margin[2];
			node._maxWidth += margin[0] + margin[2];
		}

		return node;
	}

	function getNodeMargin() {
		var margin = node.margin;

		if (!margin && node.style) {
			var styleArray = (node.style instanceof Array) ? node.style : [ node.style ];

			for(var i = styleArray.length - 1; i >= 0; i--) {
				var styleName = styleArray[i];
				var style = self.styleStack.styleDictionary[styleName];
				if (style && style.margin) {
					margin = style.margin;
					break;
				}
			}
		}

		if (!margin) return null;

		if (typeof margin === 'number' || margin instanceof Number) {
			margin = [ margin, margin, margin, margin ];
		} else if (margin instanceof Array) {
			if (margin.length === 2) {
				margin = [ margin[0], margin[1], margin[0], margin[1] ];
			}
		}

		return margin;
	}
};

DocMeasure.prototype.convertIfBase64Image = function(node) {
	if (/^data:image\/(jpeg|jpg|png);base64,/.test(node.image)) {
		var label = '$$pdfmake$$' + this.autoImageIndex++;
		this.images[label] = node.image;
		node.image = label;
}
};

DocMeasure.prototype.measureImage = function(node) {
	if (this.images) {
		this.convertIfBase64Image(node);
	}

	var imageSize = this.imageMeasure.measureImage(node.image);

	if (node.fit) {
		var factor = (imageSize.width / imageSize.height > node.fit[0] / node.fit[1]) ? node.fit[0] / imageSize.width : node.fit[1] / imageSize.height;
		node._width = node._minWidth = node._maxWidth = imageSize.width * factor;
		node._height = imageSize.height * factor;
	} else {
		node._width = node._minWidth = node._maxWidth = node.width || imageSize.width;
		node._height = node.height || (imageSize.height * node._width / imageSize.width);
	}

	node._alignment = this.styleStack.getProperty('alignment');
	return node;
};

DocMeasure.prototype.measureLeaf = function(node) {
	var data = this.textTools.buildInlines(node.text, this.styleStack);

	node._inlines = data.items;
	node._minWidth = data.minWidth;
	node._maxWidth = data.maxWidth;

	return node;
};

DocMeasure.prototype.measureVerticalContainer = function(node) {
	var items = node.stack;

	node._minWidth = 0;
	node._maxWidth = 0;

	for(var i = 0, l = items.length; i < l; i++) {
		items[i] = this.measureNode(items[i]);

		node._minWidth = Math.max(node._minWidth, items[i]._minWidth);
		node._maxWidth = Math.max(node._maxWidth, items[i]._maxWidth);
	}

	return node;
};

DocMeasure.prototype.gapSizeForList = function(isOrderedList, listItems) {
	if (isOrderedList) {
		var longestNo = (listItems.length).toString().replace(/./g, '9');
		return this.textTools.sizeOfString(longestNo + '. ', this.styleStack);
	} else {
		return this.textTools.sizeOfString('9. ', this.styleStack);
	}
};

DocMeasure.prototype.buildMarker = function(isOrderedList, counter, styleStack, gapSize) {
	var marker;

	if (isOrderedList) {
		marker = { _inlines: this.textTools.buildInlines(counter, styleStack).items };
	}
	else {
		// TODO: ascender-based calculations
		var radius = gapSize.fontSize / 6;

		marker = {
			canvas: [ {
				x: radius,
				y: gapSize.height + gapSize.decender - gapSize.fontSize / 3,//0,// gapSize.fontSize * 2 / 3,
				r1: radius,
				r2: radius,
				type: 'ellipse',
				color: 'black'
			} ]
		};
	}

	marker._minWidth = marker._maxWidth = gapSize.width;
	marker._minHeight = marker._maxHeight = gapSize.height;

	return marker;
};

DocMeasure.prototype.measureList = function(isOrdered, node) {
	var style = this.styleStack.clone();

	var items = isOrdered ? node.ol : node.ul;
	node._gapSize = this.gapSizeForList(isOrdered, items);
	node._minWidth = 0;
	node._maxWidth = 0;

	var counter = 1;

	for(var i = 0, l = items.length; i < l; i++) {
		var nextItem = items[i] = this.measureNode(items[i]);

		var marker = counter++ + '. ';

		if (!nextItem.ol && !nextItem.ul) {
			nextItem.listMarker = this.buildMarker(isOrdered, nextItem.counter || marker, style, node._gapSize);
		}  // TODO: else - nested lists numbering

		node._minWidth = Math.max(node._minWidth, items[i]._minWidth + node._gapSize.width);
		node._maxWidth = Math.max(node._maxWidth, items[i]._maxWidth + node._gapSize.width);
	}

	return node;
};

DocMeasure.prototype.measureColumns = function(node) {
	var columns = node.columns;
	node._gap = this.styleStack.getProperty('columnGap') || 0;

	for(var i = 0, l = columns.length; i < l; i++) {
		columns[i] = this.measureNode(columns[i]);
	}

	var measures = ColumnCalculator.measureMinMax(columns);

	node._minWidth = measures.min + node._gap * (columns.length - 1);
	node._maxWidth = measures.max + node._gap * (columns.length - 1);

	return node;
};

DocMeasure.prototype.measureTable = function(node) {
	extendTableWidths(node);
	node._layout = getLayout(this.tableLayouts);
	node._offsets = getOffsets(node._layout);

	var colSpans = [];
	var col, row, cols, rows;

	for(col = 0, cols = node.table.body[0].length; col < cols; col++) {
		var c = node.table.widths[col];
		c._minWidth = 0;
		c._maxWidth = 0;

		for(row = 0, rows = node.table.body.length; row < rows; row++) {
			var rowData = node.table.body[row];
			var data = rowData[col];
			if (!data._span) {
				var _this = this;
				data = rowData[col] = this.styleStack.auto(data, measureCb(this, data));

				if (data.colSpan && data.colSpan > 1) {
					markSpans(rowData, col, data.colSpan);
					colSpans.push({ col: col, span: data.colSpan, minWidth: data._minWidth, maxWidth: data._maxWidth });
				} else {
					c._minWidth = Math.max(c._minWidth, data._minWidth);
					c._maxWidth = Math.max(c._maxWidth, data._maxWidth);
				}
			}

			if (data.rowSpan && data.rowSpan > 1) {
				markVSpans(node.table, row, col, data.rowSpan);
			}
		}
	}

	extendWidthsForColSpans();

	var measures = ColumnCalculator.measureMinMax(node.table.widths);

	node._minWidth = measures.min + node._offsets.total;
	node._maxWidth = measures.max + node._offsets.total;

	return node;

	function measureCb(_this, data) {
		return function() {
			if (data !== null && typeof data === 'object') {
				data.fillColor = _this.styleStack.getProperty('fillColor');
			}
			return _this.measureNode(data);
		};
	}

	function getLayout(tableLayouts) {
		var layout = node.layout;

		if (typeof node.layout === 'string' || node instanceof String) {
			layout = tableLayouts[layout];
		}

		var defaultLayout = {
			hLineWidth: function(i, node) { return 1; }, //return node.table.headerRows && i === node.table.headerRows && 3 || 0; },
			vLineWidth: function(i, node) { return 1; },
			hLineColor: function(i, node) { return 'black'; },
			vLineColor: function(i, node) { return 'black'; },
			paddingLeft: function(i, node) { return 4; }, //i && 4 || 0; },
			paddingRight: function(i, node) { return 4; }, //(i < node.table.widths.length - 1) ? 4 : 0; },
			paddingTop: function(i, node) { return 2; },
			paddingBottom: function(i, node) { return 2; }
		};

		return pack(defaultLayout, layout);
	}

	function getOffsets(layout) {
		var offsets = [];
		var totalOffset = 0;
		var prevRightPadding = 0;

		for(var i = 0, l = node.table.widths.length; i < l; i++) {
			var lOffset = prevRightPadding + layout.vLineWidth(i, node) + layout.paddingLeft(i, node);
			offsets.push(lOffset);
			totalOffset += lOffset;
			prevRightPadding = layout.paddingRight(i, node);
		}

		totalOffset += prevRightPadding + layout.vLineWidth(node.table.widths.length, node);

		return {
			total: totalOffset,
			offsets: offsets
		};
	}

	function extendWidthsForColSpans() {
		var q, j;

		for (var i = 0, l = colSpans.length; i < l; i++) {
			var span = colSpans[i];

			var currentMinMax = getMinMax(span.col, span.span, node._offsets);
			var minDifference = span.minWidth - currentMinMax.minWidth;
			var maxDifference = span.maxWidth - currentMinMax.maxWidth;

			if (minDifference > 0) {
				q = minDifference / span.span;

				for(j = 0; j < span.span; j++) {
					node.table.widths[span.col + j]._minWidth += q;
				}
			}

			if (maxDifference > 0) {
				q = maxDifference / span.span;

				for(j = 0; j < span.span; j++) {
					node.table.widths[span.col + j]._maxWidth += q;
				}
			}
		}
	}

	function getMinMax(col, span, offsets) {
		var result = { minWidth: 0, maxWidth: 0 };

		for(var i = 0; i < span; i++) {
			result.minWidth += node.table.widths[col + i]._minWidth + (i? offsets.offsets[col + i] : 0);
			result.maxWidth += node.table.widths[col + i]._maxWidth + (i? offsets.offsets[col + i] : 0);
		}

		return result;
	}

	function markSpans(rowData, col, span) {
		for (var i = 1; i < span; i++) {
			rowData[col + i] = {
				_span: true,
				_minWidth: 0,
				_maxWidth: 0,
				rowSpan: rowData[col].rowSpan
			};
		}
	}

	function markVSpans(table, row, col, span) {
		for (var i = 1; i < span; i++) {
			table.body[row + i][col] = {
				_span: true,
				_minWidth: 0,
				_maxWidth: 0,
				fillColor: table.body[row][col].fillColor
			};
		}
	}

	function extendTableWidths(node) {
		if (!node.table.widths) {
			node.table.widths = 'auto';
		}

		if (typeof node.table.widths === 'string' || node.table.widths instanceof String) {
			node.table.widths = [ node.table.widths ];

			while(node.table.widths.length < node.table.body[0].length) {
				node.table.widths.push(node.table.widths[node.table.widths.length - 1]);
			}
		}

		for(var i = 0, l = node.table.widths.length; i < l; i++) {
			var w = node.table.widths[i];
			if (typeof w === 'number' || w instanceof Number || typeof w === 'string' || w instanceof String) {
				node.table.widths[i] = { width: w };
			}
		}
	}
};

DocMeasure.prototype.measureCanvas = function(node) {
	var w = 0, h = 0;

	for(var i = 0, l = node.canvas.length; i < l; i++) {
		var vector = node.canvas[i];

		switch(vector.type) {
		case 'ellipse':
			w = Math.max(w, vector.x + vector.r1);
			h = Math.max(h, vector.y + vector.r2);
			break;
		case 'rect':
			w = Math.max(w, vector.x + vector.w);
			h = Math.max(h, vector.y + vector.h);
			break;
		case 'line':
			w = Math.max(w, vector.x1, vector.x2);
			h = Math.max(h, vector.y1, vector.y2);
			break;
		case 'polyline':
			for(var i2 = 0, l2 = vector.points.length; i2 < l2; i2++) {
				w = Math.max(w, vector.points[i2].x);
				h = Math.max(h, vector.points[i2].y);
			}
			break;
		}
	}

	node._minWidth = node._maxWidth = w;
	node._minHeight = node._maxHeight = h;

	return node;
};

DocMeasure.prototype.measureQr = function(node) {
	node = qrEncoder.measure(node);
	node._alignment = this.styleStack.getProperty('alignment');
	return node;
};

module.exports = DocMeasure;

},{"./columnCalculator":76,"./helpers":80,"./qrEnc.js":86,"./styleContextStack":88,"./textTools":91}],78:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

var TraversalTracker = _dereq_('./traversalTracker');

/**
* Creates an instance of DocumentContext - a store for current x, y positions and available width/height.
* It facilitates column divisions and vertical sync
*/
function DocumentContext(pageSize, pageMargins) {
	this.pages = [];

	this.pageSize = pageSize;
	this.pageMargins = pageMargins;

	this.x = pageMargins.left;
	this.availableWidth = pageSize.width - pageMargins.left - pageMargins.right;
	this.availableHeight = 0;
	this.page = -1;

	this.snapshots = [];

	this.endingCell = null;

    this.defaultPage = { items: [] };
    
    this.tracker = new TraversalTracker();
    
	this.addPage();
}

DocumentContext.prototype.beginColumnGroup = function() {
	this.snapshots.push({
		x: this.x,
		y: this.y,
		availableHeight: this.availableHeight,
		availableWidth: this.availableWidth,
		page: this.page,
		bottomMost: { y: this.y, page: this.page },
		endingCell: this.endingCell,
		lastColumnWidth: this.lastColumnWidth
	});

	this.lastColumnWidth = 0;
};

DocumentContext.prototype.beginColumn = function(width, offset, endingCell) {
	var saved = this.snapshots[this.snapshots.length - 1];

	this.calculateBottomMost(saved);

  this.endingCell = endingCell;
	this.page = saved.page;
	this.x = this.x + this.lastColumnWidth + (offset || 0);
	this.y = saved.y;
	this.availableWidth = width;	//saved.availableWidth - offset;
	this.availableHeight = saved.availableHeight;

	this.lastColumnWidth = width;
};

DocumentContext.prototype.calculateBottomMost = function(destContext) {
	if (this.endingCell) {
		this.saveContextInEndingCell(this.endingCell);
		this.endingCell = null;
	} else {
		destContext.bottomMost = bottomMostContext(this, destContext.bottomMost);
	}
};

DocumentContext.prototype.markEnding = function(endingCell) {
	this.page = endingCell._columnEndingContext.page;
	this.x = endingCell._columnEndingContext.x;
	this.y = endingCell._columnEndingContext.y;
	this.availableWidth = endingCell._columnEndingContext.availableWidth;
	this.availableHeight = endingCell._columnEndingContext.availableHeight;
	this.lastColumnWidth = endingCell._columnEndingContext.lastColumnWidth;
};

DocumentContext.prototype.saveContextInEndingCell = function(endingCell) {
	endingCell._columnEndingContext = {
		page: this.page,
		x: this.x,
		y: this.y,
		availableHeight: this.availableHeight,
		availableWidth: this.availableWidth,
		lastColumnWidth: this.lastColumnWidth
	};
};

DocumentContext.prototype.completeColumnGroup = function() {
	var saved = this.snapshots.pop();

	this.calculateBottomMost(saved);

	this.endingCell = null;
	this.x = saved.x;
	this.y = saved.bottomMost.y;
	this.page = saved.bottomMost.page;
	this.availableWidth = saved.availableWidth;
	this.availableHeight = saved.bottomMost.availableHeight;
	this.lastColumnWidth = saved.lastColumnWidth;
};

DocumentContext.prototype.addMargin = function(left, right) {
	this.x += left;
	this.availableWidth -= left + (right || 0);
};

DocumentContext.prototype.moveDown = function(offset) {
	this.y += offset;
	this.availableHeight -= offset;

	return this.availableHeight > 0;
};

DocumentContext.prototype.moveToPageTop = function() {
	this.y = this.pageMargins.top;
	this.availableHeight = this.pageSize.height - this.pageMargins.top - this.pageMargins.bottom;
};

DocumentContext.prototype.addPage = function() {
	var page = this.getDefaultPage();
    this.pages.push(page);
	this.page = this.pages.length - 1;
	this.moveToPageTop();

    this.tracker.emit('pageAdded');
    
	return page;
};

DocumentContext.prototype.getCurrentPage = function() {
	if (this.page < 0 || this.page >= this.pages.length) return null;

	return this.pages[this.page];
};

DocumentContext.prototype.setDefaultPage = function(defaultPage) {
    // copy the items without deep-copying the object (which is not possible due to circular structures)
    this.defaultPage = { items: (defaultPage || this.pages[this.page]).items.slice() };
};

DocumentContext.prototype.getDefaultPage = function() {
    return { items: this.defaultPage.items.slice() };
};

function bottomMostContext(c1, c2) {
	var r;

	if (c1.page > c2.page) r = c1;
	else if (c2.page > c1.page) r = c2;
	else r = (c1.y > c2.y) ? c1 : c2;

	return {
		page: r.page,
		x: r.x,
		y: r.y,
		availableHeight: r.availableHeight,
		availableWidth: r.availableWidth
	};
}

/****TESTS**** (add a leading '/' to uncomment)
DocumentContext.bottomMostContext = bottomMostContext;
// */

module.exports = DocumentContext;

},{"./traversalTracker":92}],79:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

var Line = _dereq_('./line');
var pack = _dereq_('./helpers').pack;
var offsetVector = _dereq_('./helpers').offsetVector;
var DocumentContext = _dereq_('./documentContext');

/**
* Creates an instance of ElementWriter - a line/vector writer, which adds
* elements to current page and sets their positions based on the context
*/
function ElementWriter(context, tracker) {
	this.context = context;
	this.contextStack = [];
	this.tracker = tracker || { emit: function() { } };
}

function addPageItem(page, item, index) {
	if(index === null || index === undefined || index < 0 || index > page.items.length) {
		page.items.push(item);
	} else {
		page.items.splice(index, 0, item);
	}
}

ElementWriter.prototype.addLine = function(line, dontUpdateContextPosition, index) {
	var height = line.getHeight();
	var context = this.context;
	var page = context.getCurrentPage();

	if (context.availableHeight < height || !page) {
		return false;
	}

	line.x = context.x + (line.x || 0);
	line.y = context.y + (line.y || 0);

	this.alignLine(line);

    addPageItem(page, {
        type: 'line',
        item: line
    }, index);
	this.tracker.emit('lineAdded', line);

	if (!dontUpdateContextPosition) context.moveDown(height);

	return true;
};

ElementWriter.prototype.alignLine = function(line) {
	var width = this.context.availableWidth;
	var lineWidth = line.getWidth();

	var alignment = line.inlines && line.inlines.length > 0 && line.inlines[0].alignment;

	var offset = 0;
	switch(alignment) {
		case 'right':
			offset = width - lineWidth;
			break;
		case 'center':
			offset = (width - lineWidth) / 2;
			break;
	}

	if (offset) {
		line.x = (line.x || 0) + offset;
	}

	if (alignment === 'justify' &&
		!line.newLineForced &&
		!line.lastLineInParagraph &&
		line.inlines.length > 1) {
		var additionalSpacing = (width - lineWidth) / (line.inlines.length - 1);

		for(var i = 1, l = line.inlines.length; i < l; i++) {
			offset = i * additionalSpacing;

			line.inlines[i].x += offset;
		}
	}
};

ElementWriter.prototype.addImage = function(image, index) {
	var context = this.context;
	var page = context.getCurrentPage();

	if (context.availableHeight < image._height || !page) {
		return false;
	}

	image.x = context.x + (image.x || 0);
	image.y = context.y;

	this.alignImage(image);

	addPageItem(page, {
        type: 'image',
        item: image
    }, index);

	context.moveDown(image._height);

	return true;
};

ElementWriter.prototype.addQr = function(qr, index) {
	var context = this.context;
	var page = context.getCurrentPage();

	if (context.availableHeight < qr._height || !page) {
		return false;
	}

	qr.x = context.x + (qr.x || 0);
	qr.y = context.y;
	
	this.alignImage(qr);
	
	for (var i=0, l=qr._canvas.length; i < l; i++) {
		var vector = qr._canvas[i];
		vector.x += qr.x;
		vector.y += qr.y;
		this.addVector(vector, true, true, index);
	}

	context.moveDown(qr._height);

	return true;
};

ElementWriter.prototype.alignImage = function(image) {
	var width = this.context.availableWidth;
	var imageWidth = image._minWidth;
	var offset = 0;
	switch(image._alignment) {
		case 'right':
			offset = width - imageWidth;
			break;
		case 'center':
			offset = (width - imageWidth) / 2;
			break;
	}

	if (offset) {
		image.x = (image.x || 0) + offset;
	}
};

ElementWriter.prototype.addVector = function(vector, ignoreContextX, ignoreContextY, index) {
	var context = this.context;
	var page = context.getCurrentPage();

	if (page) {
		offsetVector(vector, ignoreContextX ? 0 : context.x, ignoreContextY ? 0 : context.y);
        addPageItem(page, {
            type: 'vector',
            item: vector
        }, index);
		return true;
	}
};

function cloneLine(line) {
	var result = new Line(line.maxWidth);

	for(var key in line) {
		if (line.hasOwnProperty(key)) {
			result[key] = line[key];
		}
	}

	return result;
}

ElementWriter.prototype.addFragment = function(block, useBlockXOffset, useBlockYOffset, dontUpdateContextPosition) {
	var ctx = this.context;
	var page = ctx.getCurrentPage();

	if (!useBlockXOffset && block.height > ctx.availableHeight) return false;

	block.items.forEach(function(item) {
        switch(item.type) {
            case 'line':
                var l = cloneLine(item.item);

                l.x = (l.x || 0) + (useBlockXOffset ? (block.xOffset || 0) : ctx.x);
                l.y = (l.y || 0) + (useBlockYOffset ? (block.yOffset || 0) : ctx.y);

                page.items.push({
                    type: 'line',
                    item: l
                });
                break;
                
            case 'vector':
                var v = pack(item.item);

                offsetVector(v, useBlockXOffset ? (block.xOffset || 0) : ctx.x, useBlockYOffset ? (block.yOffset || 0) : ctx.y);
                page.items.push({
                    type: 'vector',
                    item: v
                });
                break;
                
            case 'image':
                var img = pack(item.item);

                img.x = (img.x || 0) + (useBlockXOffset ? (block.xOffset || 0) : ctx.x);
                img.y = (img.y || 0) + (useBlockYOffset ? (block.yOffset || 0) : ctx.y);

                page.items.push({
                    type: 'image',
                    item: img
                });
                break;
        }
	});

	if (!dontUpdateContextPosition) ctx.moveDown(block.height);

	return true;
};

/**
* Pushes the provided context onto the stack or creates a new one
*
* pushContext(context) - pushes the provided context and makes it current
* pushContext(width, height) - creates and pushes a new context with the specified width and height
* pushContext() - creates a new context for unbreakable blocks (with current availableWidth and full-page-height)
*/
ElementWriter.prototype.pushContext = function(contextOrWidth, height) {
	if (contextOrWidth === undefined) {
		height = this.context.pageSize.height - this.context.pageMargins.top - this.context.pageMargins.bottom;
		contextOrWidth = this.context.availableWidth;
	}

	if (typeof contextOrWidth === 'number' || contextOrWidth instanceof Number) {
		contextOrWidth = new DocumentContext({ width: contextOrWidth, height: height }, { left: 0, right: 0, top: 0, bottom: 0 });
	}

	this.contextStack.push(this.context);
	this.context = contextOrWidth;
};

ElementWriter.prototype.popContext = function() {
	this.context = this.contextStack.pop();
};


module.exports = ElementWriter;

},{"./documentContext":78,"./helpers":80,"./line":83}],80:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

function pack() {
	var result = {};

	for(var i = 0, l = arguments.length; i < l; i++) {
		var obj = arguments[i];

		if (obj) {
			for(var key in obj) {
				if (obj.hasOwnProperty(key)) {
					result[key] = obj[key];
				}
			}
		}
	}

	return result;
}

function offsetVector(vector, x, y) {
	switch(vector.type) {
	case 'ellipse':
	case 'rect':
		vector.x += x;
		vector.y += y;
		break;
	case 'line':
		vector.x1 += x;
		vector.x2 += x;
		vector.y1 += y;
		vector.y2 += y;
		break;
	case 'polyline':
		for(var i = 0, l = vector.points.length; i < l; i++) {
			vector.points[i].x += x;
			vector.points[i].y += y;
		}
		break;
	}
}

function fontStringify(key, val) {
	if (key === 'font') {
		return 'font';
	}
	return val;
}

function isFunction(functionToCheck) {
	var getType = {};
	return functionToCheck && getType.toString.call(functionToCheck) === '[object Function]';
}


module.exports = {
	pack: pack,
	fontStringify: fontStringify,
	offsetVector: offsetVector,
	isFunction: isFunction
};

},{}],81:[function(_dereq_,module,exports){
(function (Buffer){
var pdfKit = _dereq_('pdfkit');
var PDFImage = _dereq_('../node_modules/pdfkit/js/image');

function ImageMeasure(pdfDoc, imageDictionary) {
	this.pdfDoc = pdfDoc;
	this.imageDictionary = imageDictionary || {};
}

ImageMeasure.prototype.measureImage = function(src) {
	var image, label;
	var that = this;

	if (!this.pdfDoc._imageRegistry[src]) {
		label = "I" + (++this.pdfDoc._imageCount);
		image = PDFImage.open(realImageSrc(src), label);
		image.embed(this.pdfDoc);
		this.pdfDoc._imageRegistry[src] = image;
	} else {
		image = this.pdfDoc._imageRegistry[src];
	}

	return { width: image.width, height: image.height };

	function realImageSrc(src) {
		var img = that.imageDictionary[src];

		if (!img) return src;

		var index = img.indexOf('base64,');
		if (index < 0) {
			throw 'invalid image format, images dictionary should contain dataURL entries';
		}

		return new Buffer(img.substring(index + 7), 'base64');
	}
};

module.exports = ImageMeasure;

}).call(this,_dereq_("buffer").Buffer)
},{"../node_modules/pdfkit/js/image":53,"buffer":16,"pdfkit":33}],82:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

var TraversalTracker = _dereq_('./traversalTracker');
var DocMeasure = _dereq_('./docMeasure');
var DocumentContext = _dereq_('./documentContext');
var PageElementWriter = _dereq_('./pageElementWriter');
var ColumnCalculator = _dereq_('./columnCalculator');
var TableProcessor = _dereq_('./tableProcessor');
var Line = _dereq_('./line');
var pack = _dereq_('./helpers').pack;
var offsetVector = _dereq_('./helpers').offsetVector;
var fontStringify = _dereq_('./helpers').fontStringify;
var isFunction = _dereq_('./helpers').isFunction;
var TextTools = _dereq_('./textTools');
var StyleContextStack = _dereq_('./styleContextStack');

/**
 * Creates an instance of LayoutBuilder - layout engine which turns document-definition-object
 * into a set of pages, lines, inlines and vectors ready to be rendered into a PDF
 *
 * @param {Object} pageSize - an object defining page width and height
 * @param {Object} pageMargins - an object defining top, left, right and bottom margins
 */
function LayoutBuilder(pageSize, pageMargins, imageMeasure) {
	this.pageSize = pageSize;
	this.pageMargins = pageMargins;
	this.tracker = new TraversalTracker();
    this.imageMeasure = imageMeasure;
    this.tableLayouts = {};
}

LayoutBuilder.prototype.registerTableLayouts = function (tableLayouts) {
  this.tableLayouts = pack(this.tableLayouts, tableLayouts);
};

/**
 * Executes layout engine on document-definition-object and creates an array of pages
 * containing positioned Blocks, Lines and inlines
 *
 * @param {Object} docStructure document-definition-object
 * @param {Object} fontProvider font provider
 * @param {Object} styleDictionary dictionary with style definitions
 * @param {Object} defaultStyle default style definition
 * @return {Array} an array of pages
 */
LayoutBuilder.prototype.layoutDocument = function (docStructure, fontProvider, styleDictionary, defaultStyle, background, header, footer, images, watermark) {
	this.docMeasure = new DocMeasure(fontProvider, styleDictionary, defaultStyle, this.imageMeasure, this.tableLayouts, images);

  docStructure = this.docMeasure.measureDocument(docStructure);

  this.writer = new PageElementWriter(
    new DocumentContext(this.pageSize, this.pageMargins), this.tracker);

  var _this = this;
  this.writer.context().tracker.startTracking('pageAdded', function() {
      _this.addBackground(background);
  });
    
  this.addBackground(background);
  this.processNode(docStructure);
  this.addHeadersAndFooters(header, footer);
  /* jshint eqnull:true */
  if(watermark != null)
    this.addWatermark(watermark, fontProvider);

	return this.writer.context().pages;
};

LayoutBuilder.prototype.addBackground = function(background) {
    var backgroundGetter = isFunction(background) ? background : function() { return background; };
    
    var pageBackground = backgroundGetter(this.writer.context().page + 1);
    
    if (pageBackground) {
      this.writer.beginUnbreakableBlock(this.pageSize.width, this.pageSize.height);
      this.processNode(this.docMeasure.measureDocument(pageBackground));
      this.writer.commitUnbreakableBlock(0, 0);
    }
};

LayoutBuilder.prototype.addStaticRepeatable = function(node, x, y, width, height) {
  var pages = this.writer.context().pages;
  this.writer.context().page = 0;
  
  this.writer.beginUnbreakableBlock(width, height);
  this.processNode(this.docMeasure.measureDocument(node));
  var repeatable = this.writer.currentBlockToRepeatable();
  repeatable.xOffset = x;
  repeatable.yOffset = y;
  this.writer.commitUnbreakableBlock(x, y);
  
  for(var i = 1, l = pages.length; i < l; i++) {
    this.writer.context().page = i;
    this.writer.addFragment(repeatable, true, true, true);
  }
};

LayoutBuilder.prototype.addDynamicRepeatable = function(nodeGetter, x, y, width, height) {
  var pages = this.writer.context().pages;
  
  for(var i = 0, l = pages.length; i < l; i++) {
    this.writer.context().page = i;

    var node = nodeGetter(i + 1, l);

    if (node) {
      this.writer.beginUnbreakableBlock(width, height);
      this.processNode(this.docMeasure.measureDocument(node));
      this.writer.commitUnbreakableBlock(x, y);
    }
  }
};

LayoutBuilder.prototype.addHeadersAndFooters = function(header, footer) {
  if(isFunction(header)) {
    this.addDynamicRepeatable(header, 0, 0, this.pageSize.width, this.pageMargins.top);
  } else if(header) {
    this.addStaticRepeatable(header, 0, 0, this.pageSize.width, this.pageMargins.top);
  }
  
  if(isFunction(footer)) {
    this.addDynamicRepeatable(footer, 0, this.pageSize.height - this.pageMargins.bottom, this.pageSize.width, this.pageMargins.bottom);
  } else if(footer) {
    this.addStaticRepeatable(footer, 0, this.pageSize.height - this.pageMargins.bottom, this.pageSize.width, this.pageMargins.bottom);
  }
};

LayoutBuilder.prototype.addWatermark = function(watermark, fontProvider){
  var defaultFont = Object.getOwnPropertyNames(fontProvider.fonts)[0]; // TODO allow selection of other font
  var watermarkObject = {
    text: watermark,
    font: fontProvider.provideFont(fontProvider[defaultFont], false, false),
    size: getSize(this.pageSize, watermark, fontProvider)
  };

  var pages = this.writer.context().pages;
  for(var i = 0, l = pages.length; i < l; i++) {
    pages[i].watermark = watermarkObject;
  }

  function getSize(pageSize, watermark, fontProvider){
    var width = pageSize.width;
    var height = pageSize.height;
    var targetWidth = Math.sqrt(width*width + height*height)*0.8; /* page diagnoal * sample factor */
    var textTools = new TextTools(fontProvider);
    var styleContextStack = new StyleContextStack();
    var size;

    /**
     * Binary search the best font size.
     * Initial bounds [0, 1000]
     * Break when range < 1
     */
    var a = 0;
    var b = 1000;
    var c = (a+b)/2;
    while(Math.abs(a - b) > 1){
      styleContextStack.push({
        fontSize: c
      });
      size = textTools.sizeOfString(watermark, styleContextStack);
      if(size.width > targetWidth){
        b = c;
        c = (a+b)/2;
      }
      else if(size.width < targetWidth){
        a = c;
        c = (a+b)/2;
      }
      styleContextStack.pop();
    }
    /*
      End binary search
     */
    return {size: size, fontSize: c};
  }
};

LayoutBuilder.prototype.processNode = function(node) {
  var self = this;

  applyMargins(function() {
    if (node.stack) {
      self.processVerticalContainer(node.stack);
    } else if (node.columns) {
      self.processColumns(node);
    } else if (node.ul) {
      self.processList(false, node.ul, node._gapSize);
    } else if (node.ol) {
      self.processList(true, node.ol, node._gapSize);
    } else if (node.table) {
      self.processTable(node);
    } else if (node.text !== undefined) {
      self.processLeaf(node);
    } else if (node.image) {
      self.processImage(node);
    } else if (node.canvas) {
      self.processCanvas(node);
    } else if (node.qr) {
      self.processQr(node);
    }else if (!node._span) {
		throw 'Unrecognized document structure: ' + JSON.stringify(node, fontStringify);
		}
	});

	function applyMargins(callback) {
		var margin = node._margin;

    if (node.pageBreak === 'before') {
        self.writer.moveToNextPage();
    }

		if (margin) {
			self.writer.context().moveDown(margin[1]);
			self.writer.context().addMargin(margin[0], margin[2]);
		}

		callback();

		if(margin) {
			self.writer.context().addMargin(-margin[0], -margin[2]);
			self.writer.context().moveDown(margin[3]);
		}

    if (node.pageBreak === 'after') {
        self.writer.moveToNextPage();
    }
	}
};

// vertical container
LayoutBuilder.prototype.processVerticalContainer = function(items) {
	var self = this;
	items.forEach(function(item) {
		self.processNode(item);

		//TODO: paragraph gap
	});
};

// columns
LayoutBuilder.prototype.processColumns = function(columnNode) {
	var columns = columnNode.columns;
	var availableWidth = this.writer.context().availableWidth;
	var gaps = gapArray(columnNode._gap);

	if (gaps) availableWidth -= (gaps.length - 1) * columnNode._gap;

	ColumnCalculator.buildColumnWidths(columns, availableWidth);
	this.processRow(columns, columns, gaps);


	function gapArray(gap) {
		if (!gap) return null;

		var gaps = [];
		gaps.push(0);

		for(var i = columns.length - 1; i > 0; i--) {
			gaps.push(gap);
		}

		return gaps;
	}
};

LayoutBuilder.prototype.processRow = function(columns, widths, gaps, tableBody, tableRow) {
  var self = this;
  var pageBreaks = [];

  this.tracker.auto('pageChanged', storePageBreakData, function() {
    widths = widths || columns;

    self.writer.context().beginColumnGroup();

    for(var i = 0, l = columns.length; i < l; i++) {
      var column = columns[i];
      var width = widths[i]._calcWidth;
      var leftOffset = colLeftOffset(i);

      if (column.colSpan && column.colSpan > 1) {
          for(var j = 1; j < column.colSpan; j++) {
              width += widths[++i]._calcWidth + gaps[i];
          }
      }

      self.writer.context().beginColumn(width, leftOffset, getEndingCell(column, i));
      if (!column._span) {
        self.processNode(column);
      } else if (column._columnEndingContext) {
        // row-span ending
        self.writer.context().markEnding(column);
      }
    }

    self.writer.context().completeColumnGroup();
  });

  return pageBreaks;

  function storePageBreakData(data) {
    var pageDesc;

    for(var i = 0, l = pageBreaks.length; i < l; i++) {
      var desc = pageBreaks[i];
      if (desc.prevPage === data.prevPage) {
        pageDesc = desc;
        break;
      }
    }

    if (!pageDesc) {
      pageDesc = data;
      pageBreaks.push(pageDesc);
    }
    pageDesc.prevY = Math.max(pageDesc.prevY, data.prevY);
    pageDesc.y = Math.min(pageDesc.y, data.y);
  }

	function colLeftOffset(i) {
		if (gaps && gaps.length > i) return gaps[i];
		return 0;
	}

  function getEndingCell(column, columnIndex) {
    if (column.rowSpan && column.rowSpan > 1) {
      var endingRow = tableRow + column.rowSpan - 1;
      if (endingRow >= tableBody.length) throw 'Row span for column ' + columnIndex + ' (with indexes starting from 0) exceeded row count';
      return tableBody[endingRow][columnIndex];
    }

    return null;
  }
};

// lists
LayoutBuilder.prototype.processList = function(orderedList, items, gapSize) {
	var self = this;

	this.writer.context().addMargin(gapSize.width);

	var nextMarker;
	this.tracker.auto('lineAdded', addMarkerToFirstLeaf, function() {
		items.forEach(function(item) {
			nextMarker = item.listMarker;
			self.processNode(item);
		});
	});

	this.writer.context().addMargin(-gapSize.width);

	function addMarkerToFirstLeaf(line) {
		// I'm not very happy with the way list processing is implemented
		// (both code and algorithm should be rethinked)
		if (nextMarker) {
			var marker = nextMarker;
			nextMarker = null;

			if (marker.canvas) {
				var vector = marker.canvas[0];

				offsetVector(vector, -marker._minWidth, 0);
				self.writer.addVector(vector);
			} else {
				var markerLine = new Line(self.pageSize.width);
				markerLine.addInline(marker._inlines[0]);
				markerLine.x = -marker._minWidth;
				markerLine.y = line.getAscenderHeight() - markerLine.getAscenderHeight();
				self.writer.addLine(markerLine, true);
			}
		}
	}
};

// tables
LayoutBuilder.prototype.processTable = function(tableNode) {
  var processor = new TableProcessor(tableNode);

  processor.beginTable(this.writer);

  for(var i = 0, l = tableNode.table.body.length; i < l; i++) {
    processor.beginRow(i, this.writer);

    var pageBreaks = this.processRow(tableNode.table.body[i], tableNode.table.widths, tableNode._offsets.offsets, tableNode.table.body, i);

    processor.endRow(i, this.writer, pageBreaks);
  }

  processor.endTable(this.writer);
};

// leafs (texts)
LayoutBuilder.prototype.processLeaf = function(node) {
	var line = this.buildNextLine(node);

	while (line) {
		this.writer.addLine(line);
		line = this.buildNextLine(node);
	}
};

LayoutBuilder.prototype.buildNextLine = function(textNode) {
	if (!textNode._inlines || textNode._inlines.length === 0) return null;

	var line = new Line(this.writer.context().availableWidth);

	while(textNode._inlines && textNode._inlines.length > 0 && line.hasEnoughSpaceForInline(textNode._inlines[0])) {
		line.addInline(textNode._inlines.shift());
	}

	line.lastLineInParagraph = textNode._inlines.length === 0;
	return line;
};

// images
LayoutBuilder.prototype.processImage = function(node) {
    this.writer.addImage(node);
};

LayoutBuilder.prototype.processCanvas = function(node) {
	var height = node._minHeight;

	if (this.writer.context().availableHeight < height) {
		// TODO: support for canvas larger than a page
		// TODO: support for other overflow methods

		this.writer.moveToNextPage();
	}

	node.canvas.forEach(function(vector) {
		this.writer.addVector(vector);
	}, this);

	this.writer.context().moveDown(height);
};

LayoutBuilder.prototype.processQr = function(node) {
	this.writer.addQr(node);
};


module.exports = LayoutBuilder;

},{"./columnCalculator":76,"./docMeasure":77,"./documentContext":78,"./helpers":80,"./line":83,"./pageElementWriter":84,"./styleContextStack":88,"./tableProcessor":89,"./textTools":91,"./traversalTracker":92}],83:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

/**
* Creates an instance of Line
*
* @constructor
* @this {Line}
* @param {Number} Maximum width this line can have
*/
function Line(maxWidth) {
	this.maxWidth = maxWidth;
	this.leadingCut = 0;
	this.trailingCut = 0;
	this.inlineWidths = 0;
	this.inlines = [];
}

Line.prototype.getAscenderHeight = function() {
	var y = 0;

	this.inlines.forEach(function(inline) {
		y = Math.max(y, inline.font.ascender / 1000 * inline.fontSize);
	});
	return y;
};

Line.prototype.hasEnoughSpaceForInline = function(inline) {
	if (this.inlines.length === 0) return true;
	if (this.newLineForced) return false;

	return this.inlineWidths + inline.width - this.leadingCut - (inline.trailingCut || 0) <= this.maxWidth;
};

Line.prototype.addInline = function(inline) {
	if (this.inlines.length === 0) {
		this.leadingCut = inline.leadingCut || 0;
	}
	this.trailingCut = inline.trailingCut || 0;

	inline.x = this.inlineWidths - this.leadingCut;

	this.inlines.push(inline);
	this.inlineWidths += inline.width;

	if (inline.lineEnd) {
		this.newLineForced = true;
	}
};

Line.prototype.getWidth = function() {
	return this.inlineWidths - this.leadingCut - this.trailingCut;
};

/**
* Returns line height
* @return {Number}
*/
Line.prototype.getHeight = function() {
	var max = 0;

	this.inlines.forEach(function(item) {
		max = Math.max(max, item.height || 0);
	});

	return max;
};

module.exports = Line;

},{}],84:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

var ElementWriter = _dereq_('./elementWriter');

/**
* Creates an instance of PageElementWriter - an extended ElementWriter
* which can handle:
* - page-breaks (it adds new pages when there's not enough space left),
* - repeatable fragments (like table-headers, which are repeated everytime
*                         a page-break occurs)
* - transactions (used for unbreakable-blocks when we want to make sure
*                 whole block will be rendered on the same page)
*/
function PageElementWriter(context, tracker) {
	this.transactionLevel = 0;
	this.repeatables = [];
	this.tracker = tracker;
	this.writer = new ElementWriter(context, tracker);
}

PageElementWriter.prototype.addLine = function(line, dontUpdateContextPosition, index) {
	if (!this.writer.addLine(line, dontUpdateContextPosition, index)) {
		this.moveToNextPage();
		this.writer.addLine(line, dontUpdateContextPosition, index);
	}
};

PageElementWriter.prototype.addImage = function(image, index) {
	if(!this.writer.addImage(image, index)) {
		this.moveToNextPage();
		this.writer.addImage(image, index);
	}
};

PageElementWriter.prototype.addQr = function(qr, index) {
	if(!this.writer.addQr(qr, index)) {
		this.moveToNextPage();
		this.writer.addQr(qr, index);
	}
};

PageElementWriter.prototype.addVector = function(vector, ignoreContextX, ignoreContextY, index) {
	this.writer.addVector(vector, ignoreContextX, ignoreContextY, index);
};

PageElementWriter.prototype.addFragment = function(fragment, useBlockXOffset, useBlockYOffset, dontUpdateContextPosition) {
	if (!this.writer.addFragment(fragment, useBlockXOffset, useBlockYOffset, dontUpdateContextPosition)) {
		this.moveToNextPage();
		this.writer.addFragment(fragment, useBlockXOffset, useBlockYOffset, dontUpdateContextPosition);
	}
};

PageElementWriter.prototype.moveToNextPage = function() {
	var nextPageIndex = this.writer.context.page + 1;

	var prevPage = this.writer.context.page;
	var prevY = this.writer.context.y;

	if (nextPageIndex >= this.writer.context.pages.length) {
		// create new Page
        this.writer.context.addPage();

		// add repeatable fragments
		this.repeatables.forEach(function(rep) {
			this.writer.addFragment(rep, true);
		}, this);
	} else {
		this.writer.context.page = nextPageIndex;
		this.writer.context.moveToPageTop();

		this.repeatables.forEach(function(rep) {
			this.writer.context.moveDown(rep.height);
		}, this);
	}

	this.writer.tracker.emit('pageChanged', {
		prevPage: prevPage,
		prevY: prevY,
		y: this.writer.context.y
	});
};

PageElementWriter.prototype.beginUnbreakableBlock = function(width, height) {
	if (this.transactionLevel++ === 0) {
		this.originalX = this.writer.context.x;
		this.writer.pushContext(width, height);
	}
};

PageElementWriter.prototype.commitUnbreakableBlock = function(forcedX, forcedY) {
	if (--this.transactionLevel === 0) {
		var unbreakableContext = this.writer.context;
		this.writer.popContext();

		var nbPages = unbreakableContext.pages.length;
		if(nbPages > 0) {
			// no support for multi-page unbreakableBlocks
			var fragment = unbreakableContext.pages[0];
			fragment.xOffset = forcedX;
			fragment.yOffset = forcedY;

			//TODO: vectors can influence height in some situations
			if(nbPages > 1) {
				// on out-of-context blocs (headers, footers, background) height should be the whole DocumentContext height
				if (forcedX !== undefined || forcedY !== undefined) {
					fragment.height = unbreakableContext.pageSize.height - unbreakableContext.pageMargins.top - unbreakableContext.pageMargins.bottom;
				} else {
					fragment.height = this.writer.context.pageSize.height - this.writer.context.pageMargins.top - this.writer.context.pageMargins.bottom;
					for (var i = 0, l = this.repeatables.length; i < l; i++) {
						fragment.height -= this.repeatables[i].height;
					}
				}
			} else {
				fragment.height = unbreakableContext.y;	
			}

			if (forcedX !== undefined || forcedY !== undefined) {
				this.writer.addFragment(fragment, true, true, true);
			} else {
				this.addFragment(fragment);
			}
		}
	}
};

PageElementWriter.prototype.currentBlockToRepeatable = function() {
	var unbreakableContext = this.writer.context;
	var rep = { items: [] };

    unbreakableContext.pages[0].items.forEach(function(item) {
        rep.items.push(item);
    });

	rep.xOffset = this.originalX;

	//TODO: vectors can influence height in some situations
	rep.height = unbreakableContext.y;

	return rep;
};

PageElementWriter.prototype.pushToRepeatables = function(rep) {
	this.repeatables.push(rep);
};

PageElementWriter.prototype.popFromRepeatables = function() {
	this.repeatables.pop();
};

PageElementWriter.prototype.context = function() {
	return this.writer.context;
};

module.exports = PageElementWriter;

},{"./elementWriter":79}],85:[function(_dereq_,module,exports){
/* jslint node: true */
/* global window */
'use strict';

var LayoutBuilder = _dereq_('./layoutBuilder');
var PdfKit = _dereq_('pdfkit');
var PDFReference = _dereq_('../node_modules/pdfkit/js/reference');
var sizes = _dereq_('./standardPageSizes');
var ImageMeasure = _dereq_('./imageMeasure');
var textDecorator = _dereq_('./textDecorator');

////////////////////////////////////////
// PdfPrinter

/**
 * @class Creates an instance of a PdfPrinter which turns document definition into a pdf
 *
 * @param {Object} fontDescriptors font definition dictionary
 *
 * @example
 * var fontDescriptors = {
 *	Roboto: {
 *		normal: 'fonts/Roboto-Regular.ttf',
 *		bold: 'fonts/Roboto-Medium.ttf',
 *		italics: 'fonts/Roboto-Italic.ttf',
 *		bolditalics: 'fonts/Roboto-Italic.ttf'
 *	}
 * };
 *
 * var printer = new PdfPrinter(fontDescriptors);
 */
function PdfPrinter(fontDescriptors) {
	this.fontDescriptors = fontDescriptors;
}

/**
 * Executes layout engine for the specified document and renders it into a pdfkit document
 * ready to be saved.
 *
 * @param {Object} docDefinition document definition
 * @param {Object} docDefinition.content an array describing the pdf structure (for more information take a look at the examples in the /examples folder)
 * @param {Object} [docDefinition.defaultStyle] default (implicit) style definition
 * @param {Object} [docDefinition.styles] dictionary defining all styles which can be used in the document
 * @param {Object} [docDefinition.pageSize] page size (pdfkit units, A4 dimensions by default)
 * @param {Number} docDefinition.pageSize.width width
 * @param {Number} docDefinition.pageSize.height height
 * @param {Object} [docDefinition.pageMargins] page margins (pdfkit units)
 *
 * @example
 *
 * var docDefinition = {
 *	content: [
 *		'First paragraph',
 *		'Second paragraph, this time a little bit longer',
 *		{ text: 'Third paragraph, slightly bigger font size', fontSize: 20 },
 *		{ text: 'Another paragraph using a named style', style: 'header' },
 *		{ text: ['playing with ', 'inlines' ] },
 *		{ text: ['and ', { text: 'restyling ', bold: true }, 'them'] },
 *	],
 *	styles: {
 *		header: { fontSize: 30, bold: true }
 *	}
 * }
 *
 * var pdfDoc = printer.createPdfKitDocument(docDefinition);
 *
 * pdfDoc.pipe(fs.createWriteStream('sample.pdf'));
 * pdfDoc.end();
 *
 * @return {Object} a pdfKit document object which can be saved or encode to data-url
 */
PdfPrinter.prototype.createPdfKitDocument = function(docDefinition, options) {
	options = options || {};

	var pageSize = pageSize2widthAndHeight(docDefinition.pageSize || 'a4');

  if(docDefinition.pageOrientation === 'landscape') {
    pageSize = { width: pageSize.height, height: pageSize.width };
  }

	this.pdfKitDoc = new PdfKit({ size: [ pageSize.width, pageSize.height ], compress: false});
	this.pdfKitDoc.info.Producer = 'pdfmake';
	this.pdfKitDoc.info.Creator = 'pdfmake';
	this.fontProvider = new FontProvider(this.fontDescriptors, this.pdfKitDoc);

  docDefinition.images = docDefinition.images || {};

	var builder = new LayoutBuilder(
		pageSize,
		fixPageMargins(docDefinition.pageMargins || 40),
        new ImageMeasure(this.pdfKitDoc, docDefinition.images));

  registerDefaultTableLayouts(builder);
  if (options.tableLayouts) {
    builder.registerTableLayouts(options.tableLayouts);
  }

	var pages = builder.layoutDocument(docDefinition.content, this.fontProvider, docDefinition.styles || {}, docDefinition.defaultStyle || { fontSize: 12, font: 'Roboto' }, docDefinition.background, docDefinition.header, docDefinition.footer, docDefinition.images, docDefinition.watermark);

	renderPages(pages, this.fontProvider, this.pdfKitDoc);

	if(options.autoPrint){
        var jsRef = this.pdfKitDoc.ref({
			S: 'JavaScript',
			JS: new StringObject('this.print\\(true\\);')
		});
		var namesRef = this.pdfKitDoc.ref({
			Names: [new StringObject('EmbeddedJS'), new PDFReference(this.pdfKitDoc, jsRef.id)],
		});

		jsRef.end();
		namesRef.end();

		this.pdfKitDoc._root.data.Names = {
			JavaScript: new PDFReference(this.pdfKitDoc, namesRef.id)
		};
	}
	return this.pdfKitDoc;
};

function fixPageMargins(margin) {
    if (!margin) return null;

    if (typeof margin === 'number' || margin instanceof Number) {
        margin = { left: margin, right: margin, top: margin, bottom: margin };
    } else if (margin instanceof Array) {
        if (margin.length === 2) {
            margin = { left: margin[0], top: margin[1], right: margin[0], bottom: margin[1] };
        } else if (margin.length === 4) {
            margin = { left: margin[0], top: margin[1], right: margin[2], bottom: margin[3] };
        } else throw 'Invalid pageMargins definition';
    }

    return margin;
}

function registerDefaultTableLayouts(layoutBuilder) {
  layoutBuilder.registerTableLayouts({
    noBorders: {
      hLineWidth: function(i) { return 0; },
      vLineWidth: function(i) { return 0; },
      paddingLeft: function(i) { return i && 4 || 0; },
      paddingRight: function(i, node) { return (i < node.table.widths.length - 1) ? 4 : 0; },
    },
    headerLineOnly: {
      hLineWidth: function(i, node) {
        if (i === 0 || i === node.table.body.length) return 0;
        return (i === node.table.headerRows) ? 2 : 0;
      },
      vLineWidth: function(i) { return 0; },
      paddingLeft: function(i) {
        return i === 0 ? 0 : 8;
      },
      paddingRight: function(i, node) {
        return (i === node.table.widths.length - 1) ? 0 : 8;
      }
    },
    lightHorizontalLines: {
      hLineWidth: function(i, node) {
        if (i === 0 || i === node.table.body.length) return 0;
        return (i === node.table.headerRows) ? 2 : 1;
      },
      vLineWidth: function(i) { return 0; },
      hLineColor: function(i) { return i === 1 ? 'black' : '#aaa'; },
      paddingLeft: function(i) {
        return i === 0 ? 0 : 8;
      },
      paddingRight: function(i, node) {
        return (i === node.table.widths.length - 1) ? 0 : 8;
      }
    }
  });
}

var defaultLayout = {
  hLineWidth: function(i, node) { return 1; }, //return node.table.headerRows && i === node.table.headerRows && 3 || 0; },
  vLineWidth: function(i, node) { return 1; },
  hLineColor: function(i, node) { return 'black'; },
  vLineColor: function(i, node) { return 'black'; },
  paddingLeft: function(i, node) { return 4; }, //i && 4 || 0; },
  paddingRight: function(i, node) { return 4; }, //(i < node.table.widths.length - 1) ? 4 : 0; },
  paddingTop: function(i, node) { return 2; },
  paddingBottom: function(i, node) { return 2; }
};

function pageSize2widthAndHeight(pageSize) {
    if (typeof pageSize == 'string' || pageSize instanceof String) {
        var size = sizes[pageSize.toUpperCase()];
        if (!size) throw ('Page size ' + pageSize + ' not recognized');
        return { width: size[0], height: size[1] };
    }

    return pageSize;
}

function StringObject(str){
	this.isString = true;
	this.toString = function(){
		return str;
	};
}

function renderPages(pages, fontProvider, pdfKitDoc) {
	for(var i = 0, l = pages.length; i < l; i++) {
		if (i > 0) {
			pdfKitDoc.addPage();
		}

		setFontRefs(fontProvider, pdfKitDoc);

		var page = pages[i];
    for(var ii = 0, il = page.items.length; ii < il; ii++) {
        var item = page.items[ii];
        switch(item.type) {
          case 'vector':
              renderVector(item.item, pdfKitDoc);
              break;
          case 'line':
              renderLine(item.item, item.item.x, item.item.y, pdfKitDoc);
              break;
          case 'image':
              renderImage(item.item, item.item.x, item.item.y, pdfKitDoc);
              break;
				}
    }
    if(page.watermark){
			renderWatermark(page, pdfKitDoc, fontProvider);
		}
	}
}

function setFontRefs(fontProvider, pdfKitDoc) {
	for(var fontName in fontProvider.cache) {
		var desc = fontProvider.cache[fontName];

		for (var fontType in desc) {
			var font = desc[fontType];
			var _ref, _base, _name;

			if (!(_ref = (_base = pdfKitDoc.page.fonts)[_name = font.id])) {
				_base[_name] = font.ref();
			}
		}
	}
}

function renderLine(line, x, y, pdfKitDoc) {
	x = x || 0;
	y = y || 0;

	var ascenderHeight = line.getAscenderHeight();
	var lineHeight = line.getHeight();

	textDecorator.drawBackground(line, x, y, pdfKitDoc);

	//TODO: line.optimizeInlines();
	for(var i = 0, l = line.inlines.length; i < l; i++) {
		var inline = line.inlines[i];

		pdfKitDoc.fill(inline.color || 'black');

		pdfKitDoc.save();
		pdfKitDoc.transform(1, 0, 0, -1, 0, pdfKitDoc.page.height);

		pdfKitDoc.addContent('BT');
		var a = (inline.font.ascender / 1000 * inline.fontSize);

		pdfKitDoc.addContent('' + (x + inline.x) + ' ' + (pdfKitDoc.page.height - y - ascenderHeight) + ' Td');
		pdfKitDoc.addContent('/' + inline.font.id + ' ' + inline.fontSize + ' Tf');

		pdfKitDoc.addContent('<' + encode(inline.font, inline.text) + '> Tj');

		pdfKitDoc.addContent('ET');
		pdfKitDoc.restore();
	}

	textDecorator.drawDecorations(line, x, y, pdfKitDoc);

}

function renderWatermark(page, pdfKitDoc, fontProvider){
	var watermark = page.watermark;

	pdfKitDoc.fill('black');
	pdfKitDoc.opacity(0.6);

	pdfKitDoc.save();
	pdfKitDoc.transform(1, 0, 0, -1, 0, pdfKitDoc.page.height);

	var angle = Math.atan2(pdfKitDoc.page.height, pdfKitDoc.page.width) * 180/Math.PI;
	pdfKitDoc.rotate(angle, {origin: [pdfKitDoc.page.width/2, pdfKitDoc.page.height/2]});

	pdfKitDoc.addContent('BT');
	pdfKitDoc.addContent('' + (pdfKitDoc.page.width/2 - watermark.size.size.width/2) + ' ' + (pdfKitDoc.page.height/2 - watermark.size.size.height/4) + ' Td');
	pdfKitDoc.addContent('/' + watermark.font.id + ' ' + watermark.size.fontSize + ' Tf');
	pdfKitDoc.addContent('<' + encode(watermark.font, watermark.text) + '> Tj');
	pdfKitDoc.addContent('ET');
	pdfKitDoc.restore();
}

function encode(font, text) {
	font.use(text);

	text = font.encode(text);
	text = ((function() {
		var _results = [];

		for (var i = 0, _ref2 = text.length; 0 <= _ref2 ? i < _ref2 : i > _ref2; 0 <= _ref2 ? i++ : i--) {
			_results.push(text.charCodeAt(i).toString(16));
		}
		return _results;
	})()).join('');

	return text;
}

function renderVector(vector, pdfDoc) {
	//TODO: pdf optimization (there's no need to write all properties everytime)
	pdfDoc.lineWidth(vector.lineWidth || 1);
	if (vector.dash) {
		pdfDoc.dash(vector.dash.length, { space: vector.dash.space || vector.dash.length });
	} else {
		pdfDoc.undash();
	}
	pdfDoc.fillOpacity(vector.fillOpacity || 1);
	pdfDoc.strokeOpacity(vector.strokeOpacity || 1);
	pdfDoc.lineJoin(vector.lineJoin || 'miter');

	//TODO: clipping

	switch(vector.type) {
		case 'ellipse':
			pdfDoc.ellipse(vector.x, vector.y, vector.r1, vector.r2);
			break;
		case 'rect':
			if (vector.r) {
				pdfDoc.roundedRect(vector.x, vector.y, vector.w, vector.h, vector.r);
			} else {
				pdfDoc.rect(vector.x, vector.y, vector.w, vector.h);
			}
			break;
		case 'line':
			pdfDoc.moveTo(vector.x1, vector.y1);
			pdfDoc.lineTo(vector.x2, vector.y2);
			break;
		case 'polyline':
			if (vector.points.length === 0) break;

			pdfDoc.moveTo(vector.points[0].x, vector.points[0].y);
			for(var i = 1, l = vector.points.length; i < l; i++) {
				pdfDoc.lineTo(vector.points[i].x, vector.points[i].y);
			}

			if (vector.points.length > 1) {
				var p1 = vector.points[0];
				var pn = vector.points[vector.points.length - 1];

				if (vector.closePath || p1.x === pn.x && p1.y === pn.y) {
					pdfDoc.closePath();
				}
			}
			break;
	}

	if (vector.color && vector.lineColor) {
		pdfDoc.fillAndStroke(vector.color, vector.lineColor);
	} else if (vector.color) {
		pdfDoc.fill(vector.color);
	} else {
		pdfDoc.stroke(vector.lineColor || 'black');
	}
}

function renderImage(image, x, y, pdfKitDoc) {
    pdfKitDoc.image(image.image, image.x, image.y, { width: image._width, height: image._height });
}

function FontProvider(fontDescriptors, pdfDoc) {
	this.fonts = {};
	this.pdfDoc = pdfDoc;
	this.cache = {};

	for(var font in fontDescriptors) {
		if (fontDescriptors.hasOwnProperty(font)) {
			var fontDef = fontDescriptors[font];

			this.fonts[font] = {
				normal: fontDef.normal,
				bold: fontDef.bold,
				italics: fontDef.italics,
				bolditalics: fontDef.bolditalics
			};
		}
	}
}

FontProvider.prototype.provideFont = function(familyName, bold, italics) {
	if (!this.fonts[familyName]) return this.pdfDoc._font;

	var type = 'normal';

	if (bold && italics) type = 'bolditalics';
	else if (bold) type = 'bold';
	else if (italics) type = 'italics';

	if (!this.cache[familyName]) this.cache[familyName] = {};

	var cached = this.cache[familyName] && this.cache[familyName][type];

	if (cached) return cached;

	var fontCache = (this.cache[familyName] = this.cache[familyName] || {});
	fontCache[type] = this.pdfDoc.font(this.fonts[familyName][type], familyName)._font;
	return fontCache[type];
};

module.exports = PdfPrinter;


/* temporary browser extension */
PdfPrinter.prototype.fs = _dereq_('fs');

},{"../node_modules/pdfkit/js/reference":66,"./imageMeasure":81,"./layoutBuilder":82,"./standardPageSizes":87,"./textDecorator":90,"fs":"x/K9gc","pdfkit":33}],86:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';
/*jshint -W004 */
/* qr.js -- QR code generator in Javascript (revision 2011-01-19)
 * Written by Kang Seonghoon <public+qrjs@mearie.org>.
 *
 * This source code is in the public domain; if your jurisdiction does not
 * recognize the public domain the terms of Creative Commons CC0 license
 * apply. In the other words, you can always do what you want.
 */


// per-version information (cf. JIS X 0510:2004 pp. 30--36, 71)
//
// [0]: the degree of generator polynomial by ECC levels
// [1]: # of code blocks by ECC levels
// [2]: left-top positions of alignment patterns
//
// the number in this table (in particular, [0]) does not exactly match with
// the numbers in the specficiation. see augumenteccs below for the reason.
var VERSIONS = [
	null,
	[[10, 7,17,13], [ 1, 1, 1, 1], []],
	[[16,10,28,22], [ 1, 1, 1, 1], [4,16]],
	[[26,15,22,18], [ 1, 1, 2, 2], [4,20]],
	[[18,20,16,26], [ 2, 1, 4, 2], [4,24]],
	[[24,26,22,18], [ 2, 1, 4, 4], [4,28]],
	[[16,18,28,24], [ 4, 2, 4, 4], [4,32]],
	[[18,20,26,18], [ 4, 2, 5, 6], [4,20,36]],
	[[22,24,26,22], [ 4, 2, 6, 6], [4,22,40]],
	[[22,30,24,20], [ 5, 2, 8, 8], [4,24,44]],
	[[26,18,28,24], [ 5, 4, 8, 8], [4,26,48]],
	[[30,20,24,28], [ 5, 4,11, 8], [4,28,52]],
	[[22,24,28,26], [ 8, 4,11,10], [4,30,56]],
	[[22,26,22,24], [ 9, 4,16,12], [4,32,60]],
	[[24,30,24,20], [ 9, 4,16,16], [4,24,44,64]],
	[[24,22,24,30], [10, 6,18,12], [4,24,46,68]],
	[[28,24,30,24], [10, 6,16,17], [4,24,48,72]],
	[[28,28,28,28], [11, 6,19,16], [4,28,52,76]],
	[[26,30,28,28], [13, 6,21,18], [4,28,54,80]],
	[[26,28,26,26], [14, 7,25,21], [4,28,56,84]],
	[[26,28,28,30], [16, 8,25,20], [4,32,60,88]],
	[[26,28,30,28], [17, 8,25,23], [4,26,48,70,92]],
	[[28,28,24,30], [17, 9,34,23], [4,24,48,72,96]],
	[[28,30,30,30], [18, 9,30,25], [4,28,52,76,100]],
	[[28,30,30,30], [20,10,32,27], [4,26,52,78,104]],
	[[28,26,30,30], [21,12,35,29], [4,30,56,82,108]],
	[[28,28,30,28], [23,12,37,34], [4,28,56,84,112]],
	[[28,30,30,30], [25,12,40,34], [4,32,60,88,116]],
	[[28,30,30,30], [26,13,42,35], [4,24,48,72,96,120]],
	[[28,30,30,30], [28,14,45,38], [4,28,52,76,100,124]],
	[[28,30,30,30], [29,15,48,40], [4,24,50,76,102,128]],
	[[28,30,30,30], [31,16,51,43], [4,28,54,80,106,132]],
	[[28,30,30,30], [33,17,54,45], [4,32,58,84,110,136]],
	[[28,30,30,30], [35,18,57,48], [4,28,56,84,112,140]],
	[[28,30,30,30], [37,19,60,51], [4,32,60,88,116,144]],
	[[28,30,30,30], [38,19,63,53], [4,28,52,76,100,124,148]],
	[[28,30,30,30], [40,20,66,56], [4,22,48,74,100,126,152]],
	[[28,30,30,30], [43,21,70,59], [4,26,52,78,104,130,156]],
	[[28,30,30,30], [45,22,74,62], [4,30,56,82,108,134,160]],
	[[28,30,30,30], [47,24,77,65], [4,24,52,80,108,136,164]],
	[[28,30,30,30], [49,25,81,68], [4,28,56,84,112,140,168]]];

// mode constants (cf. Table 2 in JIS X 0510:2004 p. 16)
var MODE_TERMINATOR = 0;
var MODE_NUMERIC = 1, MODE_ALPHANUMERIC = 2, MODE_OCTET = 4, MODE_KANJI = 8;

// validation regexps
var NUMERIC_REGEXP = /^\d*$/;
var ALPHANUMERIC_REGEXP = /^[A-Za-z0-9 $%*+\-./:]*$/;
var ALPHANUMERIC_OUT_REGEXP = /^[A-Z0-9 $%*+\-./:]*$/;

// ECC levels (cf. Table 22 in JIS X 0510:2004 p. 45)
var ECCLEVEL_L = 1, ECCLEVEL_M = 0, ECCLEVEL_Q = 3, ECCLEVEL_H = 2;

// GF(2^8)-to-integer mapping with a reducing polynomial x^8+x^4+x^3+x^2+1
// invariant: GF256_MAP[GF256_INVMAP[i]] == i for all i in [1,256)
var GF256_MAP = [], GF256_INVMAP = [-1];
for (var i = 0, v = 1; i < 255; ++i) {
	GF256_MAP.push(v);
	GF256_INVMAP[v] = i;
	v = (v * 2) ^ (v >= 128 ? 0x11d : 0);
}

// generator polynomials up to degree 30
// (should match with polynomials in JIS X 0510:2004 Appendix A)
//
// generator polynomial of degree K is product of (x-\alpha^0), (x-\alpha^1),
// ..., (x-\alpha^(K-1)). by convention, we omit the K-th coefficient (always 1)
// from the result; also other coefficients are written in terms of the exponent
// to \alpha to avoid the redundant calculation. (see also calculateecc below.)
var GF256_GENPOLY = [[]];
for (var i = 0; i < 30; ++i) {
	var prevpoly = GF256_GENPOLY[i], poly = [];
	for (var j = 0; j <= i; ++j) {
		var a = (j < i ? GF256_MAP[prevpoly[j]] : 0);
		var b = GF256_MAP[(i + (prevpoly[j-1] || 0)) % 255];
		poly.push(GF256_INVMAP[a ^ b]);
	}
	GF256_GENPOLY.push(poly);
}

// alphanumeric character mapping (cf. Table 5 in JIS X 0510:2004 p. 19)
var ALPHANUMERIC_MAP = {};
for (var i = 0; i < 45; ++i) {
	ALPHANUMERIC_MAP['0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:'.charAt(i)] = i;
}

// mask functions in terms of row # and column #
// (cf. Table 20 in JIS X 0510:2004 p. 42)
var MASKFUNCS = [
	function(i,j) { return (i+j) % 2 === 0; },
	function(i,j) { return i % 2 === 0; },
	function(i,j) { return j % 3 === 0; },
	function(i,j) { return (i+j) % 3 === 0; },
	function(i,j) { return (((i/2)|0) + ((j/3)|0)) % 2 === 0; },
	function(i,j) { return (i*j) % 2 + (i*j) % 3 === 0; },
	function(i,j) { return ((i*j) % 2 + (i*j) % 3) % 2 === 0; },
	function(i,j) { return ((i+j) % 2 + (i*j) % 3) % 2 === 0; }];

// returns true when the version information has to be embeded.
var needsverinfo = function(ver) { return ver > 6; };

// returns the size of entire QR code for given version.
var getsizebyver = function(ver) { return 4 * ver + 17; };

// returns the number of bits available for code words in this version.
var nfullbits = function(ver) {
	/*
	 * |<--------------- n --------------->|
	 * |        |<----- n-17 ---->|        |
	 * +-------+                ///+-------+ ----
	 * |       |                ///|       |    ^
	 * |  9x9  |       @@@@@    ///|  9x8  |    |
	 * |       | # # # @5x5@ # # # |       |    |
	 * +-------+       @@@@@       +-------+    |
	 *       #                               ---|
	 *                                        ^ |
	 *       #                                |
	 *     @@@@@       @@@@@       @@@@@      | n
	 *     @5x5@       @5x5@       @5x5@   n-17
	 *     @@@@@       @@@@@       @@@@@      | |
	 *       #                                | |
	 * //////                                 v |
	 * //////#                               ---|
	 * +-------+       @@@@@       @@@@@        |
	 * |       |       @5x5@       @5x5@        |
	 * |  8x9  |       @@@@@       @@@@@        |
	 * |       |                                v
	 * +-------+                             ----
	 *
	 * when the entire code has n^2 modules and there are m^2-3 alignment
	 * patterns, we have:
	 * - 225 (= 9x9 + 9x8 + 8x9) modules for finder patterns and
	 *   format information;
	 * - 2n-34 (= 2(n-17)) modules for timing patterns;
	 * - 36 (= 3x6 + 6x3) modules for version information, if any;
	 * - 25m^2-75 (= (m^2-3)(5x5)) modules for alignment patterns
	 *   if any, but 10m-20 (= 2(m-2)x5) of them overlaps with
	 *   timing patterns.
	 */
	var v = VERSIONS[ver];
	var nbits = 16*ver*ver + 128*ver + 64; // finder, timing and format info.
	if (needsverinfo(ver)) nbits -= 36; // version information
	if (v[2].length) { // alignment patterns
		nbits -= 25 * v[2].length * v[2].length - 10 * v[2].length - 55;
	}
	return nbits;
};

// returns the number of bits available for data portions (i.e. excludes ECC
// bits but includes mode and length bits) in this version and ECC level.
var ndatabits = function(ver, ecclevel) {
	var nbits = nfullbits(ver) & ~7; // no sub-octet code words
	var v = VERSIONS[ver];
	nbits -= 8 * v[0][ecclevel] * v[1][ecclevel]; // ecc bits
	return nbits;
};

// returns the number of bits required for the length of data.
// (cf. Table 3 in JIS X 0510:2004 p. 16)
var ndatalenbits = function(ver, mode) {
	switch (mode) {
	case MODE_NUMERIC: return (ver < 10 ? 10 : ver < 27 ? 12 : 14);
	case MODE_ALPHANUMERIC: return (ver < 10 ? 9 : ver < 27 ? 11 : 13);
	case MODE_OCTET: return (ver < 10 ? 8 : 16);
	case MODE_KANJI: return (ver < 10 ? 8 : ver < 27 ? 10 : 12);
	}
};

// returns the maximum length of data possible in given configuration.
var getmaxdatalen = function(ver, mode, ecclevel) {
	var nbits = ndatabits(ver, ecclevel) - 4 - ndatalenbits(ver, mode); // 4 for mode bits
	switch (mode) {
	case MODE_NUMERIC:
		return ((nbits/10) | 0) * 3 + (nbits%10 < 4 ? 0 : nbits%10 < 7 ? 1 : 2);
	case MODE_ALPHANUMERIC:
		return ((nbits/11) | 0) * 2 + (nbits%11 < 6 ? 0 : 1);
	case MODE_OCTET:
		return (nbits/8) | 0;
	case MODE_KANJI:
		return (nbits/13) | 0;
	}
};

// checks if the given data can be encoded in given mode, and returns
// the converted data for the further processing if possible. otherwise
// returns null.
//
// this function does not check the length of data; it is a duty of
// encode function below (as it depends on the version and ECC level too).
var validatedata = function(mode, data) {
	switch (mode) {
	case MODE_NUMERIC:
		if (!data.match(NUMERIC_REGEXP)) return null;
		return data;

	case MODE_ALPHANUMERIC:
		if (!data.match(ALPHANUMERIC_REGEXP)) return null;
		return data.toUpperCase();

	case MODE_OCTET:
		if (typeof data === 'string') { // encode as utf-8 string
			var newdata = [];
			for (var i = 0; i < data.length; ++i) {
				var ch = data.charCodeAt(i);
				if (ch < 0x80) {
					newdata.push(ch);
				} else if (ch < 0x800) {
					newdata.push(0xc0 | (ch >> 6),
						0x80 | (ch & 0x3f));
				} else if (ch < 0x10000) {
					newdata.push(0xe0 | (ch >> 12),
						0x80 | ((ch >> 6) & 0x3f),
						0x80 | (ch & 0x3f));
				} else {
					newdata.push(0xf0 | (ch >> 18),
						0x80 | ((ch >> 12) & 0x3f),
						0x80 | ((ch >> 6) & 0x3f),
						0x80 | (ch & 0x3f));
				}
			}
			return newdata;
		} else {
			return data;
		}
	}
};

// returns the code words (sans ECC bits) for given data and configurations.
// requires data to be preprocessed by validatedata. no length check is
// performed, and everything has to be checked before calling this function.
var encode = function(ver, mode, data, maxbuflen) {
	var buf = [];
	var bits = 0, remaining = 8;
	var datalen = data.length;

	// this function is intentionally no-op when n=0.
	var pack = function(x, n) {
		if (n >= remaining) {
			buf.push(bits | (x >> (n -= remaining)));
			while (n >= 8) buf.push((x >> (n -= 8)) & 255);
			bits = 0;
			remaining = 8;
		}
		if (n > 0) bits |= (x & ((1 << n) - 1)) << (remaining -= n);
	};

	var nlenbits = ndatalenbits(ver, mode);
	pack(mode, 4);
	pack(datalen, nlenbits);

	switch (mode) {
	case MODE_NUMERIC:
		for (var i = 2; i < datalen; i += 3) {
			pack(parseInt(data.substring(i-2,i+1), 10), 10);
		}
		pack(parseInt(data.substring(i-2), 10), [0,4,7][datalen%3]);
		break;

	case MODE_ALPHANUMERIC:
		for (var i = 1; i < datalen; i += 2) {
			pack(ALPHANUMERIC_MAP[data.charAt(i-1)] * 45 +
				ALPHANUMERIC_MAP[data.charAt(i)], 11);
		}
		if (datalen % 2 == 1) {
			pack(ALPHANUMERIC_MAP[data.charAt(i-1)], 6);
		}
		break;

	case MODE_OCTET:
		for (var i = 0; i < datalen; ++i) {
			pack(data[i], 8);
		}
		break;
	}

	// final bits. it is possible that adding terminator causes the buffer
	// to overflow, but then the buffer truncated to the maximum size will
	// be valid as the truncated terminator mode bits and padding is
	// identical in appearance (cf. JIS X 0510:2004 sec 8.4.8).
	pack(MODE_TERMINATOR, 4);
	if (remaining < 8) buf.push(bits);

	// the padding to fill up the remaining space. we should not add any
	// words when the overflow already occurred.
	while (buf.length + 1 < maxbuflen) buf.push(0xec, 0x11);
	if (buf.length < maxbuflen) buf.push(0xec);
	return buf;
};

// calculates ECC code words for given code words and generator polynomial.
//
// this is quite similar to CRC calculation as both Reed-Solomon and CRC use
// the certain kind of cyclic codes, which is effectively the division of
// zero-augumented polynomial by the generator polynomial. the only difference
// is that Reed-Solomon uses GF(2^8), instead of CRC's GF(2), and Reed-Solomon
// uses the different generator polynomial than CRC's.
var calculateecc = function(poly, genpoly) {
	var modulus = poly.slice(0);
	var polylen = poly.length, genpolylen = genpoly.length;
	for (var i = 0; i < genpolylen; ++i) modulus.push(0);
	for (var i = 0; i < polylen; ) {
		var quotient = GF256_INVMAP[modulus[i++]];
		if (quotient >= 0) {
			for (var j = 0; j < genpolylen; ++j) {
				modulus[i+j] ^= GF256_MAP[(quotient + genpoly[j]) % 255];
			}
		}
	}
	return modulus.slice(polylen);
};

// auguments ECC code words to given code words. the resulting words are
// ready to be encoded in the matrix.
//
// the much of actual augumenting procedure follows JIS X 0510:2004 sec 8.7.
// the code is simplified using the fact that the size of each code & ECC
// blocks is almost same; for example, when we have 4 blocks and 46 data words
// the number of code words in those blocks are 11, 11, 12, 12 respectively.
var augumenteccs = function(poly, nblocks, genpoly) {
	var subsizes = [];
	var subsize = (poly.length / nblocks) | 0, subsize0 = 0;
	var pivot = nblocks - poly.length % nblocks;
	for (var i = 0; i < pivot; ++i) {
		subsizes.push(subsize0);
		subsize0 += subsize;
	}
	for (var i = pivot; i < nblocks; ++i) {
		subsizes.push(subsize0);
		subsize0 += subsize+1;
	}
	subsizes.push(subsize0);

	var eccs = [];
	for (var i = 0; i < nblocks; ++i) {
		eccs.push(calculateecc(poly.slice(subsizes[i], subsizes[i+1]), genpoly));
	}

	var result = [];
	var nitemsperblock = (poly.length / nblocks) | 0;
	for (var i = 0; i < nitemsperblock; ++i) {
		for (var j = 0; j < nblocks; ++j) {
			result.push(poly[subsizes[j] + i]);
		}
	}
	for (var j = pivot; j < nblocks; ++j) {
		result.push(poly[subsizes[j+1] - 1]);
	}
	for (var i = 0; i < genpoly.length; ++i) {
		for (var j = 0; j < nblocks; ++j) {
			result.push(eccs[j][i]);
		}
	}
	return result;
};

// auguments BCH(p+q,q) code to the polynomial over GF(2), given the proper
// genpoly. the both input and output are in binary numbers, and unlike
// calculateecc genpoly should include the 1 bit for the highest degree.
//
// actual polynomials used for this procedure are as follows:
// - p=10, q=5, genpoly=x^10+x^8+x^5+x^4+x^2+x+1 (JIS X 0510:2004 Appendix C)
// - p=18, q=6, genpoly=x^12+x^11+x^10+x^9+x^8+x^5+x^2+1 (ibid. Appendix D)
var augumentbch = function(poly, p, genpoly, q) {
	var modulus = poly << q;
	for (var i = p - 1; i >= 0; --i) {
		if ((modulus >> (q+i)) & 1) modulus ^= genpoly << i;
	}
	return (poly << q) | modulus;
};

// creates the base matrix for given version. it returns two matrices, one of
// them is the actual one and the another represents the "reserved" portion
// (e.g. finder and timing patterns) of the matrix.
//
// some entries in the matrix may be undefined, rather than 0 or 1. this is
// intentional (no initialization needed!), and putdata below will fill
// the remaining ones.
var makebasematrix = function(ver) {
	var v = VERSIONS[ver], n = getsizebyver(ver);
	var matrix = [], reserved = [];
	for (var i = 0; i < n; ++i) {
		matrix.push([]);
		reserved.push([]);
	}

	var blit = function(y, x, h, w, bits) {
		for (var i = 0; i < h; ++i) {
			for (var j = 0; j < w; ++j) {
				matrix[y+i][x+j] = (bits[i] >> j) & 1;
				reserved[y+i][x+j] = 1;
			}
		}
	};

	// finder patterns and a part of timing patterns
	// will also mark the format information area (not yet written) as reserved.
	blit(0, 0, 9, 9, [0x7f, 0x41, 0x5d, 0x5d, 0x5d, 0x41, 0x17f, 0x00, 0x40]);
	blit(n-8, 0, 8, 9, [0x100, 0x7f, 0x41, 0x5d, 0x5d, 0x5d, 0x41, 0x7f]);
	blit(0, n-8, 9, 8, [0xfe, 0x82, 0xba, 0xba, 0xba, 0x82, 0xfe, 0x00, 0x00]);

	// the rest of timing patterns
	for (var i = 9; i < n-8; ++i) {
		matrix[6][i] = matrix[i][6] = ~i & 1;
		reserved[6][i] = reserved[i][6] = 1;
	}

	// alignment patterns
	var aligns = v[2], m = aligns.length;
	for (var i = 0; i < m; ++i) {
		var minj = (i===0 || i===m-1 ? 1 : 0), maxj = (i===0 ? m-1 : m);
		for (var j = minj; j < maxj; ++j) {
			blit(aligns[i], aligns[j], 5, 5, [0x1f, 0x11, 0x15, 0x11, 0x1f]);
		}
	}

	// version information
	if (needsverinfo(ver)) {
		var code = augumentbch(ver, 6, 0x1f25, 12);
		var k = 0;
		for (var i = 0; i < 6; ++i) {
			for (var j = 0; j < 3; ++j) {
				matrix[i][(n-11)+j] = matrix[(n-11)+j][i] = (code >> k++) & 1;
				reserved[i][(n-11)+j] = reserved[(n-11)+j][i] = 1;
			}
		}
	}

	return {matrix: matrix, reserved: reserved};
};

// fills the data portion (i.e. unmarked in reserved) of the matrix with given
// code words. the size of code words should be no more than available bits,
// and remaining bits are padded to 0 (cf. JIS X 0510:2004 sec 8.7.3).
var putdata = function(matrix, reserved, buf) {
	var n = matrix.length;
	var k = 0, dir = -1;
	for (var i = n-1; i >= 0; i -= 2) {
		if (i == 6) --i; // skip the entire timing pattern column
		var jj = (dir < 0 ? n-1 : 0);
		for (var j = 0; j < n; ++j) {
			for (var ii = i; ii > i-2; --ii) {
				if (!reserved[jj][ii]) {
					// may overflow, but (undefined >> x)
					// is 0 so it will auto-pad to zero.
					matrix[jj][ii] = (buf[k >> 3] >> (~k&7)) & 1;
					++k;
				}
			}
			jj += dir;
		}
		dir = -dir;
	}
	return matrix;
};

// XOR-masks the data portion of the matrix. repeating the call with the same
// arguments will revert the prior call (convenient in the matrix evaluation).
var maskdata = function(matrix, reserved, mask) {
	var maskf = MASKFUNCS[mask];
	var n = matrix.length;
	for (var i = 0; i < n; ++i) {
		for (var j = 0; j < n; ++j) {
			if (!reserved[i][j]) matrix[i][j] ^= maskf(i,j);
		}
	}
	return matrix;
};

// puts the format information.
var putformatinfo = function(matrix, reserved, ecclevel, mask) {
	var n = matrix.length;
	var code = augumentbch((ecclevel << 3) | mask, 5, 0x537, 10) ^ 0x5412;
	for (var i = 0; i < 15; ++i) {
		var r = [0,1,2,3,4,5,7,8,n-7,n-6,n-5,n-4,n-3,n-2,n-1][i];
		var c = [n-1,n-2,n-3,n-4,n-5,n-6,n-7,n-8,7,5,4,3,2,1,0][i];
		matrix[r][8] = matrix[8][c] = (code >> i) & 1;
		// we don't have to mark those bits reserved; always done
		// in makebasematrix above.
	}
	return matrix;
};

// evaluates the resulting matrix and returns the score (lower is better).
// (cf. JIS X 0510:2004 sec 8.8.2)
//
// the evaluation procedure tries to avoid the problematic patterns naturally
// occuring from the original matrix. for example, it penaltizes the patterns
// which just look like the finder pattern which will confuse the decoder.
// we choose the mask which results in the lowest score among 8 possible ones.
//
// note: zxing seems to use the same procedure and in many cases its choice
// agrees to ours, but sometimes it does not. practically it doesn't matter.
var evaluatematrix = function(matrix) {
	// N1+(k-5) points for each consecutive row of k same-colored modules,
	// where k >= 5. no overlapping row counts.
	var PENALTY_CONSECUTIVE = 3;
	// N2 points for each 2x2 block of same-colored modules.
	// overlapping block does count.
	var PENALTY_TWOBYTWO = 3;
	// N3 points for each pattern with >4W:1B:1W:3B:1W:1B or
	// 1B:1W:3B:1W:1B:>4W, or their multiples (e.g. highly unlikely,
	// but 13W:3B:3W:9B:3W:3B counts).
	var PENALTY_FINDERLIKE = 40;
	// N4*k points for every (5*k)% deviation from 50% black density.
	// i.e. k=1 for 55~60% and 40~45%, k=2 for 60~65% and 35~40%, etc.
	var PENALTY_DENSITY = 10;

	var evaluategroup = function(groups) { // assumes [W,B,W,B,W,...,B,W]
		var score = 0;
		for (var i = 0; i < groups.length; ++i) {
			if (groups[i] >= 5) score += PENALTY_CONSECUTIVE + (groups[i]-5);
		}
		for (var i = 5; i < groups.length; i += 2) {
			var p = groups[i];
			if (groups[i-1] == p && groups[i-2] == 3*p && groups[i-3] == p &&
					groups[i-4] == p && (groups[i-5] >= 4*p || groups[i+1] >= 4*p)) {
				// this part differs from zxing...
				score += PENALTY_FINDERLIKE;
			}
		}
		return score;
	};

	var n = matrix.length;
	var score = 0, nblacks = 0;
	for (var i = 0; i < n; ++i) {
		var row = matrix[i];
		var groups;

		// evaluate the current row
		groups = [0]; // the first empty group of white
		for (var j = 0; j < n; ) {
			var k;
			for (k = 0; j < n && row[j]; ++k) ++j;
			groups.push(k);
			for (k = 0; j < n && !row[j]; ++k) ++j;
			groups.push(k);
		}
		score += evaluategroup(groups);

		// evaluate the current column
		groups = [0];
		for (var j = 0; j < n; ) {
			var k;
			for (k = 0; j < n && matrix[j][i]; ++k) ++j;
			groups.push(k);
			for (k = 0; j < n && !matrix[j][i]; ++k) ++j;
			groups.push(k);
		}
		score += evaluategroup(groups);

		// check the 2x2 box and calculate the density
		var nextrow = matrix[i+1] || [];
		nblacks += row[0];
		for (var j = 1; j < n; ++j) {
			var p = row[j];
			nblacks += p;
			// at least comparison with next row should be strict...
			if (row[j-1] == p && nextrow[j] === p && nextrow[j-1] === p) {
				score += PENALTY_TWOBYTWO;
			}
		}
	}

	score += PENALTY_DENSITY * ((Math.abs(nblacks / n / n - 0.5) / 0.05) | 0);
	return score;
};

// returns the fully encoded QR code matrix which contains given data.
// it also chooses the best mask automatically when mask is -1.
var generate = function(data, ver, mode, ecclevel, mask) {
	var v = VERSIONS[ver];
	var buf = encode(ver, mode, data, ndatabits(ver, ecclevel) >> 3);
	buf = augumenteccs(buf, v[1][ecclevel], GF256_GENPOLY[v[0][ecclevel]]);

	var result = makebasematrix(ver);
	var matrix = result.matrix, reserved = result.reserved;
	putdata(matrix, reserved, buf);

	if (mask < 0) {
		// find the best mask
		maskdata(matrix, reserved, 0);
		putformatinfo(matrix, reserved, ecclevel, 0);
		var bestmask = 0, bestscore = evaluatematrix(matrix);
		maskdata(matrix, reserved, 0);
		for (mask = 1; mask < 8; ++mask) {
			maskdata(matrix, reserved, mask);
			putformatinfo(matrix, reserved, ecclevel, mask);
			var score = evaluatematrix(matrix);
			if (bestscore > score) {
				bestscore = score;
				bestmask = mask;
			}
			maskdata(matrix, reserved, mask);
		}
		mask = bestmask;
	}

	maskdata(matrix, reserved, mask);
	putformatinfo(matrix, reserved, ecclevel, mask);
	return matrix;
};

// the public interface is trivial; the options available are as follows:
//
// - version: an integer in [1,40]. when omitted (or -1) the smallest possible
//   version is chosen.
// - mode: one of 'numeric', 'alphanumeric', 'octet'. when omitted the smallest
//   possible mode is chosen.
// - eccLevel: one of 'L', 'M', 'Q', 'H'. defaults to 'L'.
// - mask: an integer in [0,7]. when omitted (or -1) the best mask is chosen.
//

function generateFrame(data, options) {
		var MODES = {'numeric': MODE_NUMERIC, 'alphanumeric': MODE_ALPHANUMERIC,
			'octet': MODE_OCTET};
		var ECCLEVELS = {'L': ECCLEVEL_L, 'M': ECCLEVEL_M, 'Q': ECCLEVEL_Q,
			'H': ECCLEVEL_H};

		options = options || {};
		var ver = options.version || -1;
		var ecclevel = ECCLEVELS[(options.eccLevel || 'L').toUpperCase()];
		var mode = options.mode ? MODES[options.mode.toLowerCase()] : -1;
		var mask = 'mask' in options ? options.mask : -1;

		if (mode < 0) {
			if (typeof data === 'string') {
				if (data.match(NUMERIC_REGEXP)) {
					mode = MODE_NUMERIC;
				} else if (data.match(ALPHANUMERIC_OUT_REGEXP)) {
					// while encode supports case-insensitive encoding, we restrict the data to be uppercased when auto-selecting the mode.
					mode = MODE_ALPHANUMERIC;
				} else {
					mode = MODE_OCTET;
				}
			} else {
				mode = MODE_OCTET;
			}
		} else if (!(mode == MODE_NUMERIC || mode == MODE_ALPHANUMERIC ||
				mode == MODE_OCTET)) {
			throw 'invalid or unsupported mode';
		}

		data = validatedata(mode, data);
		if (data === null) throw 'invalid data format';

		if (ecclevel < 0 || ecclevel > 3) throw 'invalid ECC level';

		if (ver < 0) {
			for (ver = 1; ver <= 40; ++ver) {
				if (data.length <= getmaxdatalen(ver, mode, ecclevel)) break;
			}
			if (ver > 40) throw 'too large data for the Qr format';
		} else if (ver < 1 || ver > 40) {
			throw 'invalid Qr version! should be between 1 and 40';
		}

		if (mask != -1 && (mask < 0 || mask > 8)) throw 'invalid mask';
        //console.log('version:', ver, 'mode:', mode, 'ECC:', ecclevel, 'mask:', mask )
		return generate(data, ver, mode, ecclevel, mask);
	}


// options
// - modulesize: a number. this is a size of each modules in pixels, and
//   defaults to 5px.
// - margin: a number. this is a size of margin in *modules*, and defaults to
//   4 (white modules). the specficiation mandates the margin no less than 4
//   modules, so it is better not to alter this value unless you know what
//   you're doing.
function buildCanvas(data, options) {
   
    var canvas = [];
    var background = data.background || '#fff';
    var foreground = data.foreground || '#000';
    //var margin = options.margin || 4;
	var matrix = generateFrame(data, options);
	var n = matrix.length;
	var modSize = Math.floor( options.fit ? options.fit/n : 5 );
	var size = n * modSize;
	
    canvas.push({
      type: 'rect',
      x: 0, y: 0, w: size, h: size, lineWidth: 0, color: background
    });
    
	for (var i = 0; i < n; ++i) {
		for (var j = 0; j < n; ++j) {
            if(matrix[i][j]) {
              canvas.push({
                type: 'rect',
                x: modSize * i,
                y: modSize * j,
                w: modSize,
                h: modSize,
                lineWidth: 0,
                color: foreground
              });
            }
        }
    }
    
    return {
        canvas: canvas,
        size: size
    };
		
}

function measure(node) {
    var cd = buildCanvas(node.qr, node);
    node._canvas = cd.canvas;
    node._width = node._height = node._minWidth = node._maxWidth = node._minHeight = node._maxHeight = cd.size;
    return node;
}

module.exports = {
  measure: measure
};
},{}],87:[function(_dereq_,module,exports){
module.exports = {
	'4A0': [4767.87, 6740.79],
	'2A0': [3370.39, 4767.87],
	A0: [2383.94, 3370.39],
	A1: [1683.78, 2383.94],
	A2: [1190.55, 1683.78],
	A3: [841.89, 1190.55],
	A4: [595.28, 841.89],
	A5: [419.53, 595.28],
	A6: [297.64, 419.53],
	A7: [209.76, 297.64],
	A8: [147.40, 209.76],
	A9: [104.88, 147.40],
	A10: [73.70, 104.88],
	B0: [2834.65, 4008.19],
	B1: [2004.09, 2834.65],
	B2: [1417.32, 2004.09],
	B3: [1000.63, 1417.32],
	B4: [708.66, 1000.63],
	B5: [498.90, 708.66],
	B6: [354.33, 498.90],
	B7: [249.45, 354.33],
	B8: [175.75, 249.45],
	B9: [124.72, 175.75],
	B10: [87.87, 124.72],
	C0: [2599.37, 3676.54],
	C1: [1836.85, 2599.37],
	C2: [1298.27, 1836.85],
	C3: [918.43, 1298.27],
	C4: [649.13, 918.43],
	C5: [459.21, 649.13],
	C6: [323.15, 459.21],
	C7: [229.61, 323.15],
	C8: [161.57, 229.61],
	C9: [113.39, 161.57],
	C10: [79.37, 113.39],
	RA0: [2437.80, 3458.27],
	RA1: [1729.13, 2437.80],
	RA2: [1218.90, 1729.13],
	RA3: [864.57, 1218.90],
	RA4: [609.45, 864.57],
	SRA0: [2551.18, 3628.35],
	SRA1: [1814.17, 2551.18],
	SRA2: [1275.59, 1814.17],
	SRA3: [907.09, 1275.59],
	SRA4: [637.80, 907.09],
	EXECUTIVE: [521.86, 756.00],
	FOLIO: [612.00, 936.00],
	LEGAL: [612.00, 1008.00],
	LETTER: [612.00, 792.00],
	TABLOID: [792.00, 1224.00]
};

},{}],88:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

/**
* Creates an instance of StyleContextStack used for style inheritance and style overrides
*
* @constructor
* @this {StyleContextStack}
* @param {Object} named styles dictionary
* @param {Object} optional default style definition
*/
function StyleContextStack (styleDictionary, defaultStyle) {
	this.defaultStyle = defaultStyle || {};
	this.styleDictionary = styleDictionary;
	this.styleOverrides = [];
}

/**
* Creates cloned version of current stack
* @return {StyleContextStack} current stack snapshot
*/
StyleContextStack.prototype.clone = function() {
	var stack = new StyleContextStack(this.styleDictionary, this.defaultStyle);

	this.styleOverrides.forEach(function(item) {
		stack.styleOverrides.push(item);
	});

	return stack;
};

/**
* Pushes style-name or style-overrides-object onto the stack for future evaluation
*
* @param {String|Object} styleNameOrOverride style-name (referring to styleDictionary) or
*                                            a new dictionary defining overriding properties
*/
StyleContextStack.prototype.push = function(styleNameOrOverride) {
	this.styleOverrides.push(styleNameOrOverride);
};

/**
* Removes last style-name or style-overrides-object from the stack
*
* @param {Number} howMany - optional number of elements to be popped (if not specified,
*                           one element will be removed from the stack)
*/
StyleContextStack.prototype.pop = function(howMany) {
	howMany = howMany || 1;

	while(howMany-- > 0) {
		this.styleOverrides.pop();
	}
};

/**
* Creates a set of named styles or/and a style-overrides-object based on the item,
* pushes those elements onto the stack for future evaluation and returns the number
* of elements pushed, so they can be easily poped then.
*
* @param {Object} item - an object with optional style property and/or style overrides
* @return the number of items pushed onto the stack
*/
StyleContextStack.prototype.autopush = function(item) {
	if (typeof item === 'string' || item instanceof String) return 0;

	var styleNames = [];

	if (item.style) {
		if (item.style instanceof Array) {
			styleNames = item.style;
		} else {
			styleNames = [ item.style ];
		}
	}

	for(var i = 0, l = styleNames.length; i < l; i++) {
		this.push(styleNames[i]);
	}

	var styleOverrideObject = {};
	var pushSOO = false;

	[
		'font',
		'fontSize',
		'bold',
		'italics',
		'alignment',
		'color',
		'columnGap',
		'fillColor',
		'decoration',
		'decorationStyle',
		'decorationColor',
		'background'
		//'tableCellPadding'
		// 'cellBorder',
		// 'headerCellBorder',
		// 'oddRowCellBorder',
		// 'evenRowCellBorder',
		// 'tableBorder'
	].forEach(function(key) {
		if (item[key] !== undefined && item[key] !== null) {
			styleOverrideObject[key] = item[key];
			pushSOO = true;
		}
	});

	if (pushSOO) {
		this.push(styleOverrideObject);
	}

	return styleNames.length + (pushSOO ? 1 : 0);
};

/**
* Automatically pushes elements onto the stack, using autopush based on item,
* executes callback and then pops elements back. Returns value returned by callback
*
* @param  {Object}   item - an object with optional style property and/or style overrides
* @param  {Function} function to be called between autopush and pop
* @return {Object} value returned by callback
*/
StyleContextStack.prototype.auto = function(item, callback) {
	var pushedItems = this.autopush(item);
	var result = callback();

	if (pushedItems > 0) {
		this.pop(pushedItems);
	}

	return result;
};

/**
* Evaluates stack and returns value of a named property
*
* @param {String} property - property name
* @return property value or null if not found
*/
StyleContextStack.prototype.getProperty = function(property) {
	if (this.styleOverrides) {
		for(var i = this.styleOverrides.length - 1; i >= 0; i--) {
			var item = this.styleOverrides[i];

			if (typeof item == 'string' || item instanceof String) {
				// named-style-override

				var style = this.styleDictionary[item];
				if (style && style[property] !== null && style[property] !== undefined) {
					return style[property];
				}
			} else {
				// style-overrides-object
				if (item[property] !== undefined && item[property] !== null) {
					return item[property];
				}
			}
		}
	}

	return this.defaultStyle && this.defaultStyle[property];
};

module.exports = StyleContextStack;

},{}],89:[function(_dereq_,module,exports){
var ColumnCalculator = _dereq_('./columnCalculator');

function TableProcessor(tableNode) {
  this.tableNode = tableNode;
}

TableProcessor.prototype.beginTable = function(writer) {
  var tableNode;
  var availableWidth;
  var self = this;

  tableNode = this.tableNode;
  this.offsets = tableNode._offsets;
  this.layout = tableNode._layout;

  availableWidth = writer.context().availableWidth - this.offsets.total;
  ColumnCalculator.buildColumnWidths(tableNode.table.widths, availableWidth);

  this.tableWidth = tableNode._offsets.total + getTableInnerContentWidth();
  this.rowSpanData = prepareRowSpanData();

  this.headerRows = tableNode.table.headerRows || 0;
  this.rowsWithoutPageBreak = this.headerRows + (tableNode.table.keepWithHeaderRows || 0);
  this.dontBreakRows = tableNode.table.dontBreakRows || false;

  if (this.rowsWithoutPageBreak) {
    writer.beginUnbreakableBlock();
  }

  this.drawHorizontalLine(0, writer);

  function getTableInnerContentWidth() {
    var width = 0;

    tableNode.table.widths.forEach(function(w) {
      width += w._calcWidth;
    });

    return width;
  }

  function prepareRowSpanData() {
    var rsd = [];
    var x = 0;
    var lastWidth = 0;

    rsd.push({ left: 0, rowSpan: 0 });

    for(var i = 0, l = self.tableNode.table.body[0].length; i < l; i++) {
      var paddings = self.layout.paddingLeft(i, self.tableNode) + self.layout.paddingRight(i, self.tableNode);
      var lBorder = self.layout.vLineWidth(i, self.tableNode);
      lastWidth = paddings + lBorder + self.tableNode.table.widths[i]._calcWidth;
      rsd[rsd.length - 1].width = lastWidth;
      x += lastWidth;
      rsd.push({ left: x, rowSpan: 0, width: 0 });
    }

    return rsd;
  }
};

TableProcessor.prototype.onRowBreak = function(rowIndex, writer) {
  var self = this;
  return function() {
    //console.log('moving by : ', topLineWidth, rowPaddingTop);
    var offset = self.rowPaddingTop + (!self.headerRows ? self.topLineWidth : 0);
    writer.context().moveDown(offset);  
  };
  
};

TableProcessor.prototype.beginRow = function(rowIndex, writer) {
  this.topLineWidth = this.layout.hLineWidth(rowIndex, this.tableNode);
  this.rowPaddingTop = this.layout.paddingTop(rowIndex, this.tableNode);
  this.bottomLineWidth = this.layout.hLineWidth(rowIndex+1, this.tableNode);
  this.rowPaddingBottom = this.layout.paddingBottom(rowIndex, this.tableNode);
  
  this.rowCallback = this.onRowBreak(rowIndex, writer);
  writer.tracker.startTracking('pageChanged', this.rowCallback );
    if(this.dontBreakRows) {
        writer.beginUnbreakableBlock();
    }
  this.rowTopY = writer.context().y;
  this.reservedAtBottom = this.bottomLineWidth + this.rowPaddingBottom;

  writer.context().availableHeight -= this.reservedAtBottom;

  writer.context().moveDown(this.rowPaddingTop);
};

TableProcessor.prototype.drawHorizontalLine = function(lineIndex, writer, overrideY) {
  var lineWidth = this.layout.hLineWidth(lineIndex, this.tableNode);
  if (lineWidth) {
    var offset = lineWidth / 2;
    var currentLine = null;

    for(var i = 0, l = this.rowSpanData.length; i < l; i++) {
      var data = this.rowSpanData[i];
      var shouldDrawLine = !data.rowSpan;

      if (!currentLine && shouldDrawLine) {
        currentLine = { left: data.left, width: 0 };
      }

      if (shouldDrawLine) {
        currentLine.width += (data.width || 0);
      }

      var y = (overrideY || 0) + offset;

      if (!shouldDrawLine || i === l - 1) {
        if (currentLine) {
          writer.addVector({
            type: 'line',
            x1: currentLine.left,
            x2: currentLine.left + currentLine.width,
            y1: y,
            y2: y,
            lineWidth: lineWidth,
            lineColor: typeof this.layout.hLineColor === 'function' ? this.layout.hLineColor(lineIndex, this.tableNode) : this.layout.hLineColor
          }, false, overrideY);
          currentLine = null;
        }
      }
    }

    writer.context().moveDown(lineWidth);
  }
};

TableProcessor.prototype.drawVerticalLine = function(x, y0, y1, vLineIndex, writer) {
  var width = this.layout.vLineWidth(vLineIndex, this.tableNode);
  if (width === 0) return;  
  writer.addVector({
    type: 'line',
    x1: x + width/2,
    x2: x + width/2,
    y1: y0,
    y2: y1,
    lineWidth: width,
    lineColor: typeof this.layout.vlineColor === 'function' ? this.layout.vLineColor(vLineIndex, this.tableNode) : this.layout.vLineColor
  }, false, true);
};

TableProcessor.prototype.endTable = function(writer) {
  writer.popFromRepeatables();
};

TableProcessor.prototype.endRow = function(rowIndex, writer, pageBreaks) {
    var i;
    var self = this;
    writer.tracker.stopTracking('pageChanged', this.rowCallback);
    writer.context().moveDown(this.layout.paddingBottom(rowIndex, this.tableNode));
    writer.context().availableHeight += this.reservedAtBottom;

    var endingPage = writer.context().page;
    var endingY = writer.context().y;

    var xs = getLineXs();

    var ys = [];

    var hasBreaks = pageBreaks && pageBreaks.length > 0;

    ys.push({
      y0: this.rowTopY,
      page: hasBreaks ? pageBreaks[0].prevPage : endingPage
    });

    if (hasBreaks) {
      for(i = 0, l = pageBreaks.length; i < l; i++) {
        var pageBreak = pageBreaks[i];
        ys[ys.length - 1].y1 = pageBreak.prevY;

        ys.push({y0: pageBreak.y, page: pageBreak.prevPage + 1});
      }
    }

    ys[ys.length - 1].y1 = endingY;

    var skipOrphanePadding = (ys[0].y1 - ys[0].y0 === this.rowPaddingTop);
    for(var yi = (skipOrphanePadding ? 1 : 0), yl = ys.length; yi < yl; yi++) {
      var willBreak = yi < ys.length - 1;
      var rowBreakWithoutHeader = (yi > 0 && !this.headerRows);
      var hzLineOffset =  rowBreakWithoutHeader ? 0 : this.topLineWidth;
      var y1 = ys[yi].y0;
      var y2 = ys[yi].y1;
      if (writer.context().page != ys[yi].page) {
        writer.context().page = ys[yi].page;

        //TODO: buggy, availableHeight should be updated on every pageChanged event
        // TableProcessor should be pageChanged listener, instead of processRow
        this.reservedAtBottom = 0;
      }

      for(i = 0, l = xs.length; i < l; i++) {
        this.drawVerticalLine(xs[i].x, y1 - hzLineOffset, y2 + this.bottomLineWidth, xs[i].index, writer);
        if(i < l-1) {
          var colIndex = xs[i].index;
          var fillColor=  this.tableNode.table.body[rowIndex][colIndex].fillColor;
          if(fillColor ) {
            var wBorder = this.layout.vLineWidth(colIndex, this.tableNode);
            var xf = xs[i].x+wBorder;
            var yf = y1 - hzLineOffset;
            writer.addVector({
              type: 'rect',
              x: xf,
              y: yf,
              w: xs[i+1].x-xf,
              h: y2+this.bottomLineWidth-yf,
              lineWidth: 0,
              color: fillColor
            }, false, true, 0);
          }
        }
      }

      if (willBreak) {
        this.drawHorizontalLine(rowIndex + 1, writer, y2);
      }
      if(rowBreakWithoutHeader) {
        this.drawHorizontalLine(rowIndex, writer, y1);
      }
    }

    writer.context().page = endingPage;
    writer.context().y = endingY;

    var row = this.tableNode.table.body[rowIndex];
    for(i = 0, l = row.length; i < l; i++) {
      if (row[i].rowSpan) {
        this.rowSpanData[i].rowSpan = row[i].rowSpan;

        // fix colSpans
        if (row[i].colSpan && row[i].colSpan > 1) {
          for(var j = 1; j < row[i].colSpan; j++) {
            this.tableNode.table.body[rowIndex + j][i]._colSpan = row[i].colSpan;
          }
        }
      }

      if(this.rowSpanData[i].rowSpan > 0) {
        this.rowSpanData[i].rowSpan--;
      }
    }

    this.drawHorizontalLine(rowIndex + 1, writer);

    if(this.headerRows && rowIndex === this.headerRows - 1) {
      this.headerRepeatable = writer.currentBlockToRepeatable();
    }

    if(this.dontBreakRows) {
      writer.tracker.auto('pageChanged', 
        function() {
          self.drawHorizontalLine(rowIndex, writer);
        },
        function() {
          writer.commitUnbreakableBlock();
        }
      );
    }

    if(this.headerRepeatable && (rowIndex === (this.rowsWithoutPageBreak - 1) || rowIndex === this.tableNode.table.body.length - 1)) {
      writer.commitUnbreakableBlock();
      writer.pushToRepeatables(this.headerRepeatable);
      this.headerRepeatable = null;
    }

    function getLineXs() {
      var result = [];
      var cols = 0;

      for(var i = 0, l = self.tableNode.table.body[rowIndex].length; i < l; i++) {
        if (!cols) {
          result.push({ x: self.rowSpanData[i].left, index: i});

          var item = self.tableNode.table.body[rowIndex][i];
          cols = (item._colSpan || item.colSpan || 0);
        }
        if (cols > 0) {
          cols--;
        }
      }

      result.push({ x: self.rowSpanData[self.rowSpanData.length - 1].left, index: self.rowSpanData.length - 1});

      return result;
    }
};

module.exports = TableProcessor;

},{"./columnCalculator":76}],90:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';


function groupDecorations(line) {
	var groups = [], curGroup = null;
	for(var i = 0, l = line.inlines.length; i < l; i++) {
		var inline = line.inlines[i];
		var decoration = inline.decoration;
		if(!decoration) {
			curGroup = null;
			continue;
		}
		var color = inline.decorationColor || inline.color || 'black';
		var style = inline.decorationStyle || 'solid';
		decoration = Array.isArray(decoration) ? decoration : [ decoration ];
		for(var ii = 0, ll = decoration.length; ii < ll; ii++) {
			var deco = decoration[ii];
			if(!curGroup || deco !== curGroup.decoration ||
					style !== curGroup.decorationStyle || color !== curGroup.decorationColor ||
					deco === 'lineThrough') {
		
				curGroup = {
					line: line,
					decoration: deco, 
					decorationColor: color, 
					decorationStyle: style,
					inlines: [ inline ]
				};
				groups.push(curGroup);
			} else {
				curGroup.inlines.push(inline);
			}
		}
	}
	
	return groups;
}

function drawDecoration(group, x, y, pdfKitDoc) {
	function maxInline() {
		var max = 0;
		for (var i = 0, l = group.inlines.length; i < l; i++) {
			var inl = group.inlines[i];
			max = inl.fontSize > max ? i : max;
		}
		return group.inlines[max];
	}
	function width() {
		var sum = 0;
		for (var i = 0, l = group.inlines.length; i < l; i++) {
			sum += group.inlines[i].width;
		}
		return sum;
	}
	var firstInline = group.inlines[0],
		biggerInline = maxInline(),
		totalWidth = width(),
		lineAscent = group.line.getAscenderHeight(),
		ascent = biggerInline.font.ascender / 1000 * biggerInline.fontSize,
		height = biggerInline.height,
		descent = height - ascent;
	
	var lw = 0.5 + Math.floor(Math.max(biggerInline.fontSize - 8, 0) / 2) * 0.12;
	
	switch (group.decoration) {
		case 'underline':
			y += lineAscent + descent * 0.45;
			break;
		case 'overline':
			y += lineAscent - (ascent * 0.85);
			break;
		case 'lineThrough':
			y += lineAscent - (ascent * 0.25);
			break;
		default:
			throw 'Unkown decoration : ' + group.decoration;
	}
	pdfKitDoc.save();
	
	if(group.decorationStyle === 'double') {
		var gap = Math.max(0.5, lw*2);
		pdfKitDoc	.fillColor(group.decorationColor)
					.rect(x + firstInline.x, y-lw/2, totalWidth, lw/2).fill()
					.rect(x + firstInline.x, y+gap-lw/2, totalWidth, lw/2).fill();
	} else if(group.decorationStyle === 'dashed') {
		var nbDashes = Math.ceil(totalWidth / (3.96+2.84));
		var rdx = x + firstInline.x;
		pdfKitDoc.rect(rdx, y, totalWidth, lw).clip();
		pdfKitDoc.fillColor(group.decorationColor);
		for (var i = 0; i < nbDashes; i++) {
			pdfKitDoc.rect(rdx, y-lw/2, 3.96, lw).fill();
			rdx += 3.96 + 2.84;
		}
	} else if(group.decorationStyle === 'dotted') {
		var nbDots = Math.ceil(totalWidth / (lw*3));
		var rx = x + firstInline.x;
		pdfKitDoc.rect(rx, y, totalWidth, lw).clip();
		pdfKitDoc.fillColor(group.decorationColor);
		for (var ii = 0; ii < nbDots; ii++) {
			pdfKitDoc.rect(rx, y-lw/2, lw, lw).fill();
			rx += (lw*3);
		}
	} else if(group.decorationStyle === 'wavy') {
		var sh = 0.7, sv = 1;
		var nbWaves = Math.ceil(totalWidth / (sh*2))+1;
		var rwx = x + firstInline.x - 1;
		pdfKitDoc.rect(x + firstInline.x, y-sv, totalWidth, y+sv).clip();
		pdfKitDoc.lineWidth(0.24);
		pdfKitDoc.moveTo(rwx, y);
		for(var iii = 0; iii < nbWaves; iii++) {
			pdfKitDoc   .bezierCurveTo(rwx+sh, y-sv, rwx+sh*2, y-sv, rwx+sh*3, y)
						.bezierCurveTo(rwx+sh*4, y+sv, rwx+sh*5, y+sv, rwx+sh*6, y);
				rwx += sh*6;
			}
		pdfKitDoc.stroke(group.decorationColor);
		
	} else {
		pdfKitDoc	.fillColor(group.decorationColor)
					.rect(x + firstInline.x, y-lw/2, totalWidth, lw)
					.fill();
	}
	pdfKitDoc.restore();
}

function drawDecorations(line, x, y, pdfKitDoc) {
	var groups = groupDecorations(line);
	for (var i = 0, l = groups.length; i < l; i++) {
		drawDecoration(groups[i], x, y, pdfKitDoc);
	}
}

function drawBackground(line, x, y, pdfKitDoc) {
	var height = line.getHeight();
	for(var i = 0, l = line.inlines.length; i < l; i++) {
		var inline = line.inlines[i];
			if(inline.background) {
				pdfKitDoc	.fillColor(inline.background)
							.rect(x + inline.x, y, inline.width, height)
							.fill();
			}
	}
}

module.exports = {
	drawBackground: drawBackground,
	drawDecorations: drawDecorations
};
},{}],91:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

var WORD_RE = /([^ ,\/!.?:;\-\n]*[ ,\/!.?:;\-]*)|\n/g;
// /\S*\s*/g to be considered (I'm not sure however - we shouldn't split 'aaa !!!!')

var LEADING = /^(\s)+/g;
var TRAILING = /(\s)+$/g;

/**
* Creates an instance of TextTools - text measurement utility
*
* @constructor
* @param {FontProvider} fontProvider
*/
function TextTools(fontProvider) {
	this.fontProvider = fontProvider;
}

/**
* Converts an array of strings (or inline-definition-objects) into a set of inlines
* and their min/max widths
* @param  {Object} textArray - an array of inline-definition-objects (or strings)
* @param  {Number} maxWidth - max width a single Line should have
* @return {Array} an array of Lines
*/
TextTools.prototype.buildInlines = function(textArray, styleContextStack) {
	var measured = measure(this.fontProvider, textArray, styleContextStack);

	var minWidth = 0,
		maxWidth = 0,
		currentLineWidth;

	measured.forEach(function (inline) {
		minWidth = Math.max(minWidth, inline.width - inline.leadingCut - inline.trailingCut);

		if (!currentLineWidth) {
			currentLineWidth = { width: 0, leadingCut: inline.leadingCut, trailingCut: 0 };
		}

		currentLineWidth.width += inline.width;
		currentLineWidth.trailingCut = inline.trailingCut;

		maxWidth = Math.max(maxWidth, getTrimmedWidth(currentLineWidth));

		if (inline.lineEnd) {
			currentLineWidth = null;
		}
	});

	return {
		items: measured,
		minWidth: minWidth,
		maxWidth: maxWidth
	};

	function getTrimmedWidth(item) {
		return Math.max(0, item.width - item.leadingCut - item.trailingCut);
	}
};

/**
* Returns size of the specified string (without breaking it) using the current style
* @param  {String} text              text to be measured
* @param  {Object} styleContextStack current style stack
* @return {Object}                   size of the specified string
*/
TextTools.prototype.sizeOfString = function(text, styleContextStack) {
	text = text.replace('\t', '    ');

	//TODO: refactor - extract from measure
	var fontName = getStyleProperty({}, styleContextStack, 'font', 'Roboto');
	var fontSize = getStyleProperty({}, styleContextStack, 'fontSize', 12);
	var bold = getStyleProperty({}, styleContextStack, 'bold', false);
	var italics = getStyleProperty({}, styleContextStack, 'italics', false);

	var font = this.fontProvider.provideFont(fontName, bold, italics);

	return {
		width: font.widthOfString(removeDiacritics(text), fontSize),
		height: font.lineHeight(fontSize),
		fontSize: fontSize,
		ascender: font.ascender / 1000 * fontSize,
		decender: font.decender / 1000 * fontSize
	};
};

function splitWords(text) {
	var results = [];
	text = text.replace('\t', '    ');

	var array = text.match(WORD_RE);

	// i < l - 1, because the last match is always an empty string
	// other empty strings however are treated as new-lines
	for(var i = 0, l = array.length; i < l - 1; i++) {
		var item = array[i];

		var isNewLine = item.length === 0;

		if (!isNewLine) {
			results.push({text: item});
		}
		else {
			var shouldAddLine = (results.length === 0 || results[results.length - 1].lineEnd);

			if (shouldAddLine) {
				results.push({ text: '', lineEnd: true });
			}
			else {
				results[results.length - 1].lineEnd = true;
			}
		}
	}

	return results;
}

function copyStyle(source, destination) {
	destination = destination || {};
	source = source || {}; //TODO: default style

	for(var key in source) {
		if (key != 'text' && source.hasOwnProperty(key)) {
			destination[key] = source[key];
		}
	}

	return destination;
}

function normalizeTextArray(array) {
	var results = [];

	if (typeof array == 'string' || array instanceof String) {
		array = [ array ];
	}

	for(var i = 0, l = array.length; i < l; i++) {
		var item = array[i];
		var style = null;
		var words;

		if (typeof item == 'string' || item instanceof String) {
			words = splitWords(item);
		} else {
			words = splitWords(item.text);
			style = copyStyle(item);
		}

		for(var i2 = 0, l2 = words.length; i2 < l2; i2++) {
			var result = {
				text: words[i2].text
			};

			if (words[i2].lineEnd) {
				result.lineEnd = true;
			}

			copyStyle(style, result);

			results.push(result);
		}
	}

	return results;
}

//TODO: support for other languages (currently only polish is supported)
var diacriticsMap = { 'Ą': 'A', 'Ć': 'C', 'Ę': 'E', 'Ł': 'L', 'Ń': 'N', 'Ó': 'O', 'Ś': 'S', 'Ź': 'Z', 'Ż': 'Z', 'ą': 'a', 'ć': 'c', 'ę': 'e', 'ł': 'l', 'ń': 'n', 'ó': 'o', 'ś': 's', 'ź': 'z', 'ż': 'z' };
// '  << atom.io workaround

function removeDiacritics(text) {
	return text.replace(/[^A-Za-z0-9\[\] ]/g, function(a) {
		return diacriticsMap[a] || a;
	});
}

function getStyleProperty(item, styleContextStack, property, defaultValue) {
	var value;

	if (item[property] !== undefined && item[property] !== null) {
		// item defines this property
		return item[property];
	}

	if (!styleContextStack) return defaultValue;

	styleContextStack.auto(item, function() {
		value = styleContextStack.getProperty(property);
	});

	if (value !== null && value !== undefined) {
		return value;
	} else {
		return defaultValue;
	}
}

function measure(fontProvider, textArray, styleContextStack) {
	var normalized = normalizeTextArray(textArray);

	normalized.forEach(function(item) {
		var fontName = getStyleProperty(item, styleContextStack, 'font', 'Roboto');
		var fontSize = getStyleProperty(item, styleContextStack, 'fontSize', 12);
		var bold = getStyleProperty(item, styleContextStack, 'bold', false);
		var italics = getStyleProperty(item, styleContextStack, 'italics', false);
		var color = getStyleProperty(item, styleContextStack, 'color', 'black');
		var decoration = getStyleProperty(item, styleContextStack, 'decoration', null);
		var decorationColor = getStyleProperty(item, styleContextStack, 'decorationColor', null);
		var decorationStyle = getStyleProperty(item, styleContextStack, 'decorationStyle', null);
		var background = getStyleProperty(item, styleContextStack, 'background', null);

		var font = fontProvider.provideFont(fontName, bold, italics);

		// TODO: character spacing
		item.width = font.widthOfString(removeDiacritics(item.text), fontSize);
		item.height = font.lineHeight(fontSize);

		var leadingSpaces = item.text.match(LEADING);
		var trailingSpaces = item.text.match(TRAILING);
		if (leadingSpaces) {
			item.leadingCut = font.widthOfString(leadingSpaces[0], fontSize);
		}
		else {
			item.leadingCut = 0;
		}

		if (trailingSpaces) {
			item.trailingCut = font.widthOfString(trailingSpaces[0], fontSize);
		}
		else {
			item.trailingCut = 0;
		}

		item.alignment = getStyleProperty(item, styleContextStack, 'alignment', 'left');
		item.font = font;
		item.fontSize = fontSize;
		item.color = color;
		item.decoration = decoration;
		item.decorationColor = decorationColor;
		item.decorationStyle = decorationStyle;
		item.background = background;
	});

	return normalized;
}

/****TESTS**** (add a leading '/' to uncomment)
TextTools.prototype.splitWords = splitWords;
TextTools.prototype.normalizeTextArray = normalizeTextArray;
TextTools.prototype.measure = measure;
// */


module.exports = TextTools;

},{}],92:[function(_dereq_,module,exports){
/* jslint node: true */
'use strict';

/**
* Creates an instance of TraversalTracker
*
* @constructor
*/
function TraversalTracker() {
	this.events = {};
}

TraversalTracker.prototype.startTracking = function(event, cb) {
	var callbacks = (this.events[event] || (this.events[event] = []));

	if (callbacks.indexOf(cb) < 0) {
		callbacks.push(cb);
	}
};

TraversalTracker.prototype.stopTracking = function(event, cb) {
	var callbacks = this.events[event];

	if (callbacks) {
		var index = callbacks.indexOf(cb);
		if (index >= 0) {
			callbacks.splice(index, 1);
		}
	}
};

TraversalTracker.prototype.emit = function(event) {
	var args = Array.prototype.slice.call(arguments, 1);

	var callbacks = this.events[event];

	if (callbacks) {
		callbacks.forEach(function(cb) {
			cb.apply(this, args);
		});
	}
};

TraversalTracker.prototype.auto = function(event, cb, innerBlock) {
	this.startTracking(event, cb);
	innerBlock();
	this.stopTracking(event, cb);
};

module.exports = TraversalTracker;

},{}]},{},[73])
(73)
});