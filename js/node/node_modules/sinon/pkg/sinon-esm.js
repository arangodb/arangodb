let sinon;/* Sinon.JS 21.0.1, 2025-12-19, @license BSD-3 */
var __getOwnPropNames = Object.getOwnPropertyNames;
var __require = /* @__PURE__ */ ((x) => typeof require !== "undefined" ? require : typeof Proxy !== "undefined" ? new Proxy(x, {
  get: (a, b) => (typeof require !== "undefined" ? require : a)[b]
}) : x)(function(x) {
  if (typeof require !== "undefined") return require.apply(this, arguments);
  throw Error('Dynamic require of "' + x + '" is not supported');
});
var __commonJS = (cb, mod) => function __require2() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};

// node_modules/@sinonjs/commons/lib/global.js
var require_global = __commonJS({
  "node_modules/@sinonjs/commons/lib/global.js"(exports, module) {
    "use strict";
    var globalObject;
    if (typeof global !== "undefined") {
      globalObject = global;
    } else if (typeof window !== "undefined") {
      globalObject = window;
    } else {
      globalObject = self;
    }
    module.exports = globalObject;
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/throws-on-proto.js
var require_throws_on_proto = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/throws-on-proto.js"(exports, module) {
    "use strict";
    var throwsOnProto;
    try {
      const object = {};
      object.__proto__;
      throwsOnProto = false;
    } catch (_) {
      throwsOnProto = true;
    }
    module.exports = throwsOnProto;
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/copy-prototype-methods.js
var require_copy_prototype_methods = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/copy-prototype-methods.js"(exports, module) {
    "use strict";
    var call = Function.call;
    var throwsOnProto = require_throws_on_proto();
    var disallowedProperties = [
      // ignore size because it throws from Map
      "size",
      "caller",
      "callee",
      "arguments"
    ];
    if (throwsOnProto) {
      disallowedProperties.push("__proto__");
    }
    module.exports = function copyPrototypeMethods(prototype) {
      return Object.getOwnPropertyNames(prototype).reduce(
        function(result, name) {
          if (disallowedProperties.includes(name)) {
            return result;
          }
          if (typeof prototype[name] !== "function") {
            return result;
          }
          result[name] = call.bind(prototype[name]);
          return result;
        },
        /* @__PURE__ */ Object.create(null)
      );
    };
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/array.js
var require_array = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/array.js"(exports, module) {
    "use strict";
    var copyPrototype = require_copy_prototype_methods();
    module.exports = copyPrototype(Array.prototype);
  }
});

// node_modules/@sinonjs/commons/lib/called-in-order.js
var require_called_in_order = __commonJS({
  "node_modules/@sinonjs/commons/lib/called-in-order.js"(exports, module) {
    "use strict";
    var every = require_array().every;
    function hasCallsLeft(callMap, spy) {
      if (callMap[spy.id] === void 0) {
        callMap[spy.id] = 0;
      }
      return callMap[spy.id] < spy.callCount;
    }
    function checkAdjacentCalls(callMap, spy, index, spies) {
      var calledBeforeNext = true;
      if (index !== spies.length - 1) {
        calledBeforeNext = spy.calledBefore(spies[index + 1]);
      }
      if (hasCallsLeft(callMap, spy) && calledBeforeNext) {
        callMap[spy.id] += 1;
        return true;
      }
      return false;
    }
    function calledInOrder(spies) {
      var callMap = {};
      var _spies = arguments.length > 1 ? arguments : spies;
      return every(_spies, checkAdjacentCalls.bind(null, callMap));
    }
    module.exports = calledInOrder;
  }
});

// node_modules/@sinonjs/commons/lib/class-name.js
var require_class_name = __commonJS({
  "node_modules/@sinonjs/commons/lib/class-name.js"(exports, module) {
    "use strict";
    function className(value) {
      const name = value.constructor && value.constructor.name;
      return name || null;
    }
    module.exports = className;
  }
});

// node_modules/@sinonjs/commons/lib/deprecated.js
var require_deprecated = __commonJS({
  "node_modules/@sinonjs/commons/lib/deprecated.js"(exports) {
    "use strict";
    exports.wrap = function(func, msg) {
      var wrapped = function() {
        exports.printWarning(msg);
        return func.apply(this, arguments);
      };
      if (func.prototype) {
        wrapped.prototype = func.prototype;
      }
      return wrapped;
    };
    exports.defaultMsg = function(packageName, funcName) {
      return `${packageName}.${funcName} is deprecated and will be removed from the public API in a future version of ${packageName}.`;
    };
    exports.printWarning = function(msg) {
      if (typeof process === "object" && process.emitWarning) {
        process.emitWarning(msg);
      } else if (console.info) {
        console.info(msg);
      } else {
        console.log(msg);
      }
    };
  }
});

// node_modules/@sinonjs/commons/lib/every.js
var require_every = __commonJS({
  "node_modules/@sinonjs/commons/lib/every.js"(exports, module) {
    "use strict";
    module.exports = function every(obj, fn) {
      var pass = true;
      try {
        obj.forEach(function() {
          if (!fn.apply(this, arguments)) {
            throw new Error();
          }
        });
      } catch (e) {
        pass = false;
      }
      return pass;
    };
  }
});

// node_modules/@sinonjs/commons/lib/function-name.js
var require_function_name = __commonJS({
  "node_modules/@sinonjs/commons/lib/function-name.js"(exports, module) {
    "use strict";
    module.exports = function functionName(func) {
      if (!func) {
        return "";
      }
      try {
        return func.displayName || func.name || // Use function decomposition as a last resort to get function
        // name. Does not rely on function decomposition to work - if it
        // doesn't debugging will be slightly less informative
        // (i.e. toString will say 'spy' rather than 'myFunc').
        (String(func).match(/function ([^\s(]+)/) || [])[1];
      } catch (e) {
        return "";
      }
    };
  }
});

// node_modules/@sinonjs/commons/lib/order-by-first-call.js
var require_order_by_first_call = __commonJS({
  "node_modules/@sinonjs/commons/lib/order-by-first-call.js"(exports, module) {
    "use strict";
    var sort = require_array().sort;
    var slice = require_array().slice;
    function comparator(a, b) {
      var aCall = a.getCall(0);
      var bCall = b.getCall(0);
      var aId = aCall && aCall.callId || -1;
      var bId = bCall && bCall.callId || -1;
      return aId < bId ? -1 : 1;
    }
    function orderByFirstCall(spies) {
      return sort(slice(spies), comparator);
    }
    module.exports = orderByFirstCall;
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/function.js
var require_function = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/function.js"(exports, module) {
    "use strict";
    var copyPrototype = require_copy_prototype_methods();
    module.exports = copyPrototype(Function.prototype);
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/map.js
var require_map = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/map.js"(exports, module) {
    "use strict";
    var copyPrototype = require_copy_prototype_methods();
    module.exports = copyPrototype(Map.prototype);
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/object.js
var require_object = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/object.js"(exports, module) {
    "use strict";
    var copyPrototype = require_copy_prototype_methods();
    module.exports = copyPrototype(Object.prototype);
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/set.js
var require_set = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/set.js"(exports, module) {
    "use strict";
    var copyPrototype = require_copy_prototype_methods();
    module.exports = copyPrototype(Set.prototype);
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/string.js
var require_string = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/string.js"(exports, module) {
    "use strict";
    var copyPrototype = require_copy_prototype_methods();
    module.exports = copyPrototype(String.prototype);
  }
});

// node_modules/@sinonjs/commons/lib/prototypes/index.js
var require_prototypes = __commonJS({
  "node_modules/@sinonjs/commons/lib/prototypes/index.js"(exports, module) {
    "use strict";
    module.exports = {
      array: require_array(),
      function: require_function(),
      map: require_map(),
      object: require_object(),
      set: require_set(),
      string: require_string()
    };
  }
});

// node_modules/type-detect/type-detect.js
var require_type_detect = __commonJS({
  "node_modules/type-detect/type-detect.js"(exports, module) {
    (function(global2, factory) {
      typeof exports === "object" && typeof module !== "undefined" ? module.exports = factory() : typeof define === "function" && define.amd ? define(factory) : global2.typeDetect = factory();
    })(exports, (function() {
      "use strict";
      var promiseExists = typeof Promise === "function";
      var globalObject = typeof self === "object" ? self : global;
      var symbolExists = typeof Symbol !== "undefined";
      var mapExists = typeof Map !== "undefined";
      var setExists = typeof Set !== "undefined";
      var weakMapExists = typeof WeakMap !== "undefined";
      var weakSetExists = typeof WeakSet !== "undefined";
      var dataViewExists = typeof DataView !== "undefined";
      var symbolIteratorExists = symbolExists && typeof Symbol.iterator !== "undefined";
      var symbolToStringTagExists = symbolExists && typeof Symbol.toStringTag !== "undefined";
      var setEntriesExists = setExists && typeof Set.prototype.entries === "function";
      var mapEntriesExists = mapExists && typeof Map.prototype.entries === "function";
      var setIteratorPrototype = setEntriesExists && Object.getPrototypeOf((/* @__PURE__ */ new Set()).entries());
      var mapIteratorPrototype = mapEntriesExists && Object.getPrototypeOf((/* @__PURE__ */ new Map()).entries());
      var arrayIteratorExists = symbolIteratorExists && typeof Array.prototype[Symbol.iterator] === "function";
      var arrayIteratorPrototype = arrayIteratorExists && Object.getPrototypeOf([][Symbol.iterator]());
      var stringIteratorExists = symbolIteratorExists && typeof String.prototype[Symbol.iterator] === "function";
      var stringIteratorPrototype = stringIteratorExists && Object.getPrototypeOf(""[Symbol.iterator]());
      var toStringLeftSliceLength = 8;
      var toStringRightSliceLength = -1;
      function typeDetect(obj) {
        var typeofObj = typeof obj;
        if (typeofObj !== "object") {
          return typeofObj;
        }
        if (obj === null) {
          return "null";
        }
        if (obj === globalObject) {
          return "global";
        }
        if (Array.isArray(obj) && (symbolToStringTagExists === false || !(Symbol.toStringTag in obj))) {
          return "Array";
        }
        if (typeof window === "object" && window !== null) {
          if (typeof window.location === "object" && obj === window.location) {
            return "Location";
          }
          if (typeof window.document === "object" && obj === window.document) {
            return "Document";
          }
          if (typeof window.navigator === "object") {
            if (typeof window.navigator.mimeTypes === "object" && obj === window.navigator.mimeTypes) {
              return "MimeTypeArray";
            }
            if (typeof window.navigator.plugins === "object" && obj === window.navigator.plugins) {
              return "PluginArray";
            }
          }
          if ((typeof window.HTMLElement === "function" || typeof window.HTMLElement === "object") && obj instanceof window.HTMLElement) {
            if (obj.tagName === "BLOCKQUOTE") {
              return "HTMLQuoteElement";
            }
            if (obj.tagName === "TD") {
              return "HTMLTableDataCellElement";
            }
            if (obj.tagName === "TH") {
              return "HTMLTableHeaderCellElement";
            }
          }
        }
        var stringTag = symbolToStringTagExists && obj[Symbol.toStringTag];
        if (typeof stringTag === "string") {
          return stringTag;
        }
        var objPrototype = Object.getPrototypeOf(obj);
        if (objPrototype === RegExp.prototype) {
          return "RegExp";
        }
        if (objPrototype === Date.prototype) {
          return "Date";
        }
        if (promiseExists && objPrototype === Promise.prototype) {
          return "Promise";
        }
        if (setExists && objPrototype === Set.prototype) {
          return "Set";
        }
        if (mapExists && objPrototype === Map.prototype) {
          return "Map";
        }
        if (weakSetExists && objPrototype === WeakSet.prototype) {
          return "WeakSet";
        }
        if (weakMapExists && objPrototype === WeakMap.prototype) {
          return "WeakMap";
        }
        if (dataViewExists && objPrototype === DataView.prototype) {
          return "DataView";
        }
        if (mapExists && objPrototype === mapIteratorPrototype) {
          return "Map Iterator";
        }
        if (setExists && objPrototype === setIteratorPrototype) {
          return "Set Iterator";
        }
        if (arrayIteratorExists && objPrototype === arrayIteratorPrototype) {
          return "Array Iterator";
        }
        if (stringIteratorExists && objPrototype === stringIteratorPrototype) {
          return "String Iterator";
        }
        if (objPrototype === null) {
          return "Object";
        }
        return Object.prototype.toString.call(obj).slice(toStringLeftSliceLength, toStringRightSliceLength);
      }
      return typeDetect;
    }));
  }
});

// node_modules/@sinonjs/commons/lib/type-of.js
var require_type_of = __commonJS({
  "node_modules/@sinonjs/commons/lib/type-of.js"(exports, module) {
    "use strict";
    var type = require_type_detect();
    module.exports = function typeOf(value) {
      return type(value).toLowerCase();
    };
  }
});

// node_modules/@sinonjs/commons/lib/value-to-string.js
var require_value_to_string = __commonJS({
  "node_modules/@sinonjs/commons/lib/value-to-string.js"(exports, module) {
    "use strict";
    function valueToString(value) {
      if (value && value.toString) {
        return value.toString();
      }
      return String(value);
    }
    module.exports = valueToString;
  }
});

// node_modules/@sinonjs/commons/lib/index.js
var require_lib = __commonJS({
  "node_modules/@sinonjs/commons/lib/index.js"(exports, module) {
    "use strict";
    module.exports = {
      global: require_global(),
      calledInOrder: require_called_in_order(),
      className: require_class_name(),
      deprecated: require_deprecated(),
      every: require_every(),
      functionName: require_function_name(),
      orderByFirstCall: require_order_by_first_call(),
      prototypes: require_prototypes(),
      typeOf: require_type_of(),
      valueToString: require_value_to_string()
    };
  }
});

// lib/sinon/util/core/extend.js
var require_extend = __commonJS({
  "lib/sinon/util/core/extend.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var hasOwnProperty = require_lib().prototypes.object.hasOwnProperty;
    var join = arrayProto.join;
    var push = arrayProto.push;
    var hasDontEnumBug = (function() {
      const obj = {
        constructor: function() {
          return "0";
        },
        toString: function() {
          return "1";
        },
        valueOf: function() {
          return "2";
        },
        toLocaleString: function() {
          return "3";
        },
        prototype: function() {
          return "4";
        },
        isPrototypeOf: function() {
          return "5";
        },
        propertyIsEnumerable: function() {
          return "6";
        },
        hasOwnProperty: function() {
          return "7";
        },
        length: function() {
          return "8";
        },
        unique: function() {
          return "9";
        }
      };
      const result = [];
      for (const prop in obj) {
        if (hasOwnProperty(obj, prop)) {
          push(result, obj[prop]());
        }
      }
      return join(result, "") !== "0123456789";
    })();
    function extendCommon(target, sources, doCopy) {
      let source, i, prop;
      for (i = 0; i < sources.length; i++) {
        source = sources[i];
        for (prop in source) {
          if (hasOwnProperty(source, prop)) {
            doCopy(target, source, prop);
          }
        }
        if (hasDontEnumBug && hasOwnProperty(source, "toString") && source.toString !== target.toString) {
          target.toString = source.toString;
        }
      }
      return target;
    }
    module.exports = function extend(target, ...sources) {
      return extendCommon(
        target,
        sources,
        function copyValue(dest, source, prop) {
          const destOwnPropertyDescriptor = Object.getOwnPropertyDescriptor(
            dest,
            prop
          );
          const sourceOwnPropertyDescriptor = Object.getOwnPropertyDescriptor(
            source,
            prop
          );
          if (prop === "name" && !destOwnPropertyDescriptor.writable) {
            return;
          }
          const descriptors = {
            configurable: sourceOwnPropertyDescriptor.configurable,
            enumerable: sourceOwnPropertyDescriptor.enumerable
          };
          if (hasOwnProperty(sourceOwnPropertyDescriptor, "writable")) {
            descriptors.writable = sourceOwnPropertyDescriptor.writable;
            descriptors.value = sourceOwnPropertyDescriptor.value;
          } else {
            if (sourceOwnPropertyDescriptor.get) {
              descriptors.get = sourceOwnPropertyDescriptor.get.bind(dest);
            }
            if (sourceOwnPropertyDescriptor.set) {
              descriptors.set = sourceOwnPropertyDescriptor.set.bind(dest);
            }
          }
          Object.defineProperty(dest, prop, descriptors);
        }
      );
    };
    module.exports.nonEnum = function extendNonEnum(target, ...sources) {
      return extendCommon(
        target,
        sources,
        function copyProperty(dest, source, prop) {
          Object.defineProperty(dest, prop, {
            value: source[prop],
            enumerable: false,
            configurable: true,
            writable: true
          });
        }
      );
    };
  }
});

// lib/sinon/util/core/get-next-tick.js
var require_get_next_tick = __commonJS({
  "lib/sinon/util/core/get-next-tick.js"(exports, module) {
    "use strict";
    function nextTick(callback) {
      setTimeout(callback, 0);
    }
    module.exports = function getNextTick(process2, setImmediate) {
      if (typeof process2 === "object" && typeof process2.nextTick === "function") {
        return process2.nextTick;
      }
      if (typeof setImmediate === "function") {
        return setImmediate;
      }
      return nextTick;
    };
  }
});

// lib/sinon/util/core/next-tick.js
var require_next_tick = __commonJS({
  "lib/sinon/util/core/next-tick.js"(exports, module) {
    "use strict";
    var globalObject = require_lib().global;
    var getNextTick = require_get_next_tick();
    module.exports = getNextTick(globalObject.process, globalObject.setImmediate);
  }
});

// lib/sinon/util/core/export-async-behaviors.js
var require_export_async_behaviors = __commonJS({
  "lib/sinon/util/core/export-async-behaviors.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var reduce = arrayProto.reduce;
    module.exports = function exportAsyncBehaviors(behaviorMethods) {
      return reduce(
        Object.keys(behaviorMethods),
        function(acc, method) {
          if (method.match(/^(callsArg|yields)/) && !method.match(/Async/)) {
            acc[`${method}Async`] = function() {
              const result = behaviorMethods[method].apply(
                this,
                arguments
              );
              this.callbackAsync = true;
              return result;
            };
          }
          return acc;
        },
        {}
      );
    };
  }
});

// lib/sinon/behavior.js
var require_behavior = __commonJS({
  "lib/sinon/behavior.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var extend = require_extend();
    var functionName = require_lib().functionName;
    var nextTick = require_next_tick();
    var valueToString = require_lib().valueToString;
    var exportAsyncBehaviors = require_export_async_behaviors();
    var concat = arrayProto.concat;
    var join = arrayProto.join;
    var reverse = arrayProto.reverse;
    var slice = arrayProto.slice;
    var useLeftMostCallback = -1;
    var useRightMostCallback = -2;
    function getCallback(behavior, args) {
      const callArgAt = behavior.callArgAt;
      if (callArgAt >= 0) {
        return args[callArgAt];
      }
      let argumentList;
      if (callArgAt === useLeftMostCallback) {
        argumentList = args;
      }
      if (callArgAt === useRightMostCallback) {
        argumentList = reverse(slice(args));
      }
      const callArgProp = behavior.callArgProp;
      for (let i = 0, l = argumentList.length; i < l; ++i) {
        if (!callArgProp && typeof argumentList[i] === "function") {
          return argumentList[i];
        }
        if (callArgProp && argumentList[i] && typeof argumentList[i][callArgProp] === "function") {
          return argumentList[i][callArgProp];
        }
      }
      return null;
    }
    function getCallbackError(behavior, func, args) {
      if (behavior.callArgAt < 0) {
        let msg;
        if (behavior.callArgProp) {
          msg = `${functionName(
            behavior.stub
          )} expected to yield to '${valueToString(
            behavior.callArgProp
          )}', but no object with such a property was passed.`;
        } else {
          msg = `${functionName(
            behavior.stub
          )} expected to yield, but no callback was passed.`;
        }
        if (args.length > 0) {
          msg += ` Received [${join(args, ", ")}]`;
        }
        return msg;
      }
      return `argument at index ${behavior.callArgAt} is not a function: ${func}`;
    }
    function ensureArgs(name, behavior, args) {
      const property = name.replace(/sArg/, "ArgAt");
      const index = behavior[property];
      if (index >= args.length) {
        throw new TypeError(
          `${name} failed: ${index + 1} arguments required but only ${args.length} present`
        );
      }
    }
    function callCallback(behavior, args) {
      if (typeof behavior.callArgAt === "number") {
        ensureArgs("callsArg", behavior, args);
        const func = getCallback(behavior, args);
        if (typeof func !== "function") {
          throw new TypeError(getCallbackError(behavior, func, args));
        }
        if (behavior.callbackAsync) {
          nextTick(function() {
            func.apply(
              behavior.callbackContext,
              behavior.callbackArguments
            );
          });
        } else {
          return func.apply(
            behavior.callbackContext,
            behavior.callbackArguments
          );
        }
      }
      return void 0;
    }
    var proto = {
      create: function create(stub) {
        const behavior = extend({}, proto);
        delete behavior.create;
        delete behavior.addBehavior;
        delete behavior.createBehavior;
        behavior.stub = stub;
        if (stub.defaultBehavior && stub.defaultBehavior.promiseLibrary) {
          behavior.promiseLibrary = stub.defaultBehavior.promiseLibrary;
        }
        return behavior;
      },
      isPresent: function isPresent() {
        return typeof this.callArgAt === "number" || this.exception || this.exceptionCreator || typeof this.returnArgAt === "number" || this.returnThis || typeof this.resolveArgAt === "number" || this.resolveThis || typeof this.throwArgAt === "number" || this.fakeFn || this.returnValueDefined;
      },
      /*eslint complexity: ["error", 20]*/
      invoke: function invoke(context, args) {
        const returnValue = callCallback(this, args);
        if (this.exception) {
          throw this.exception;
        } else if (this.exceptionCreator) {
          this.exception = this.exceptionCreator();
          this.exceptionCreator = void 0;
          throw this.exception;
        } else if (typeof this.returnArgAt === "number") {
          ensureArgs("returnsArg", this, args);
          return args[this.returnArgAt];
        } else if (this.returnThis) {
          return context;
        } else if (typeof this.throwArgAt === "number") {
          ensureArgs("throwsArg", this, args);
          throw args[this.throwArgAt];
        } else if (this.fakeFn) {
          return this.fakeFn.apply(context, args);
        } else if (typeof this.resolveArgAt === "number") {
          ensureArgs("resolvesArg", this, args);
          return (this.promiseLibrary || Promise).resolve(
            args[this.resolveArgAt]
          );
        } else if (this.resolveThis) {
          return (this.promiseLibrary || Promise).resolve(context);
        } else if (this.resolve) {
          return (this.promiseLibrary || Promise).resolve(this.returnValue);
        } else if (this.reject) {
          return (this.promiseLibrary || Promise).reject(this.returnValue);
        } else if (this.callsThrough) {
          const wrappedMethod = this.effectiveWrappedMethod();
          return wrappedMethod.apply(context, args);
        } else if (this.callsThroughWithNew) {
          const WrappedClass = this.effectiveWrappedMethod();
          const argsArray = slice(args);
          const F = WrappedClass.bind.apply(
            WrappedClass,
            concat([null], argsArray)
          );
          return new F();
        } else if (typeof this.returnValue !== "undefined") {
          return this.returnValue;
        } else if (typeof this.callArgAt === "number") {
          return returnValue;
        }
        return this.returnValue;
      },
      effectiveWrappedMethod: function effectiveWrappedMethod() {
        for (let stubb = this.stub; stubb; stubb = stubb.parent) {
          if (stubb.wrappedMethod) {
            return stubb.wrappedMethod;
          }
        }
        throw new Error("Unable to find wrapped method");
      },
      onCall: function onCall(index) {
        return this.stub.onCall(index);
      },
      onFirstCall: function onFirstCall() {
        return this.stub.onFirstCall();
      },
      onSecondCall: function onSecondCall() {
        return this.stub.onSecondCall();
      },
      onThirdCall: function onThirdCall() {
        return this.stub.onThirdCall();
      },
      withArgs: function withArgs() {
        throw new Error(
          'Defining a stub by invoking "stub.onCall(...).withArgs(...)" is not supported. Use "stub.withArgs(...).onCall(...)" to define sequential behavior for calls with certain arguments.'
        );
      }
    };
    function createBehavior(behaviorMethod) {
      return function() {
        this.defaultBehavior = this.defaultBehavior || proto.create(this);
        this.defaultBehavior[behaviorMethod].apply(
          this.defaultBehavior,
          arguments
        );
        return this;
      };
    }
    function addBehavior(stub, name, fn) {
      proto[name] = function() {
        fn.apply(this, concat([this], slice(arguments)));
        return this.stub || this;
      };
      stub[name] = createBehavior(name);
    }
    proto.addBehavior = addBehavior;
    proto.createBehavior = createBehavior;
    var asyncBehaviors = exportAsyncBehaviors(proto);
    module.exports = extend.nonEnum({}, proto, asyncBehaviors);
  }
});

// lib/sinon/util/core/walk.js
var require_walk = __commonJS({
  "lib/sinon/util/core/walk.js"(exports, module) {
    "use strict";
    var forEach = require_lib().prototypes.array.forEach;
    function walkInternal(obj, iterator, context, originalObj, seen) {
      let prop;
      const proto = Object.getPrototypeOf(obj);
      if (typeof Object.getOwnPropertyNames !== "function") {
        for (prop in obj) {
          iterator.call(context, obj[prop], prop, obj);
        }
        return;
      }
      forEach(Object.getOwnPropertyNames(obj), function(k) {
        if (seen[k] !== true) {
          seen[k] = true;
          const target = typeof Object.getOwnPropertyDescriptor(obj, k).get === "function" ? originalObj : obj;
          iterator.call(context, k, target);
        }
      });
      if (proto) {
        walkInternal(proto, iterator, context, originalObj, seen);
      }
    }
    module.exports = function walk(obj, iterator, context) {
      return walkInternal(obj, iterator, context, obj, {});
    };
  }
});

// lib/sinon/util/core/get-property-descriptor.js
var require_get_property_descriptor = __commonJS({
  "lib/sinon/util/core/get-property-descriptor.js"(exports, module) {
    "use strict";
    function getPropertyDescriptor(object, property) {
      let proto = object;
      let descriptor;
      const isOwn = Boolean(
        object && Object.getOwnPropertyDescriptor(object, property)
      );
      while (proto && !(descriptor = Object.getOwnPropertyDescriptor(proto, property))) {
        proto = Object.getPrototypeOf(proto);
      }
      if (descriptor) {
        descriptor.isOwn = isOwn;
      }
      return descriptor;
    }
    module.exports = getPropertyDescriptor;
  }
});

// lib/sinon/collect-own-methods.js
var require_collect_own_methods = __commonJS({
  "lib/sinon/collect-own-methods.js"(exports, module) {
    "use strict";
    var walk = require_walk();
    var getPropertyDescriptor = require_get_property_descriptor();
    var hasOwnProperty = require_lib().prototypes.object.hasOwnProperty;
    var push = require_lib().prototypes.array.push;
    function collectMethod(methods, object, prop, propOwner) {
      if (typeof getPropertyDescriptor(propOwner, prop).value === "function" && hasOwnProperty(object, prop)) {
        push(methods, object[prop]);
      }
    }
    function collectOwnMethods(object) {
      const methods = [];
      walk(object, collectMethod.bind(null, methods, object));
      return methods;
    }
    module.exports = collectOwnMethods;
  }
});

// lib/sinon/util/core/is-property-configurable.js
var require_is_property_configurable = __commonJS({
  "lib/sinon/util/core/is-property-configurable.js"(exports, module) {
    "use strict";
    var getPropertyDescriptor = require_get_property_descriptor();
    function isPropertyConfigurable(obj, propName) {
      const propertyDescriptor = getPropertyDescriptor(obj, propName);
      return propertyDescriptor ? propertyDescriptor.configurable : true;
    }
    module.exports = isPropertyConfigurable;
  }
});

// node_modules/@sinonjs/samsam/lib/is-nan.js
var require_is_nan = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-nan.js"(exports, module) {
    "use strict";
    function isNaN2(value) {
      return typeof value === "number" && value !== value;
    }
    module.exports = isNaN2;
  }
});

// node_modules/@sinonjs/samsam/lib/is-neg-zero.js
var require_is_neg_zero = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-neg-zero.js"(exports, module) {
    "use strict";
    function isNegZero(value) {
      return value === 0 && 1 / value === -Infinity;
    }
    module.exports = isNegZero;
  }
});

// node_modules/@sinonjs/samsam/lib/identical.js
var require_identical = __commonJS({
  "node_modules/@sinonjs/samsam/lib/identical.js"(exports, module) {
    "use strict";
    var isNaN2 = require_is_nan();
    var isNegZero = require_is_neg_zero();
    function identical(obj1, obj2) {
      if (obj1 === obj2 || isNaN2(obj1) && isNaN2(obj2)) {
        return obj1 !== 0 || isNegZero(obj1) === isNegZero(obj2);
      }
      return false;
    }
    module.exports = identical;
  }
});

// node_modules/@sinonjs/samsam/lib/get-class.js
var require_get_class = __commonJS({
  "node_modules/@sinonjs/samsam/lib/get-class.js"(exports, module) {
    "use strict";
    var toString = require_lib().prototypes.object.toString;
    function getClass(value) {
      return toString(value).split(/[ \]]/)[1];
    }
    module.exports = getClass;
  }
});

// node_modules/@sinonjs/samsam/lib/is-arguments.js
var require_is_arguments = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-arguments.js"(exports, module) {
    "use strict";
    var getClass = require_get_class();
    function isArguments(object) {
      return getClass(object) === "Arguments";
    }
    module.exports = isArguments;
  }
});

// node_modules/@sinonjs/samsam/lib/is-element.js
var require_is_element = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-element.js"(exports, module) {
    "use strict";
    var div = typeof document !== "undefined" && document.createElement("div");
    function isElement(object) {
      if (!object || object.nodeType !== 1 || !div) {
        return false;
      }
      try {
        object.appendChild(div);
        object.removeChild(div);
      } catch (e) {
        return false;
      }
      return true;
    }
    module.exports = isElement;
  }
});

// node_modules/@sinonjs/samsam/lib/is-set.js
var require_is_set = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-set.js"(exports, module) {
    "use strict";
    function isSet(val) {
      return typeof Set !== "undefined" && val instanceof Set || false;
    }
    module.exports = isSet;
  }
});

// node_modules/@sinonjs/samsam/lib/is-map.js
var require_is_map = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-map.js"(exports, module) {
    "use strict";
    function isMap(value) {
      return typeof Map !== "undefined" && value instanceof Map;
    }
    module.exports = isMap;
  }
});

// node_modules/@sinonjs/samsam/node_modules/type-detect/type-detect.js
var require_type_detect2 = __commonJS({
  "node_modules/@sinonjs/samsam/node_modules/type-detect/type-detect.js"(exports, module) {
    (function(global2, factory) {
      typeof exports === "object" && typeof module !== "undefined" ? module.exports = factory() : typeof define === "function" && define.amd ? define(factory) : (global2 = typeof globalThis !== "undefined" ? globalThis : global2 || self, global2.typeDetect = factory());
    })(exports, (function() {
      "use strict";
      var promiseExists = typeof Promise === "function";
      var globalObject = (function(Obj) {
        if (typeof globalThis === "object") {
          return globalThis;
        }
        Object.defineProperty(Obj, "typeDetectGlobalObject", {
          get: function get() {
            return this;
          },
          configurable: true
        });
        var global2 = typeDetectGlobalObject;
        delete Obj.typeDetectGlobalObject;
        return global2;
      })(Object.prototype);
      var symbolExists = typeof Symbol !== "undefined";
      var mapExists = typeof Map !== "undefined";
      var setExists = typeof Set !== "undefined";
      var weakMapExists = typeof WeakMap !== "undefined";
      var weakSetExists = typeof WeakSet !== "undefined";
      var dataViewExists = typeof DataView !== "undefined";
      var symbolIteratorExists = symbolExists && typeof Symbol.iterator !== "undefined";
      var symbolToStringTagExists = symbolExists && typeof Symbol.toStringTag !== "undefined";
      var setEntriesExists = setExists && typeof Set.prototype.entries === "function";
      var mapEntriesExists = mapExists && typeof Map.prototype.entries === "function";
      var setIteratorPrototype = setEntriesExists && Object.getPrototypeOf((/* @__PURE__ */ new Set()).entries());
      var mapIteratorPrototype = mapEntriesExists && Object.getPrototypeOf((/* @__PURE__ */ new Map()).entries());
      var arrayIteratorExists = symbolIteratorExists && typeof Array.prototype[Symbol.iterator] === "function";
      var arrayIteratorPrototype = arrayIteratorExists && Object.getPrototypeOf([][Symbol.iterator]());
      var stringIteratorExists = symbolIteratorExists && typeof String.prototype[Symbol.iterator] === "function";
      var stringIteratorPrototype = stringIteratorExists && Object.getPrototypeOf(""[Symbol.iterator]());
      var toStringLeftSliceLength = 8;
      var toStringRightSliceLength = -1;
      function typeDetect(obj) {
        var typeofObj = typeof obj;
        if (typeofObj !== "object") {
          return typeofObj;
        }
        if (obj === null) {
          return "null";
        }
        if (obj === globalObject) {
          return "global";
        }
        if (Array.isArray(obj) && (symbolToStringTagExists === false || !(Symbol.toStringTag in obj))) {
          return "Array";
        }
        if (typeof window === "object" && window !== null) {
          if (typeof window.location === "object" && obj === window.location) {
            return "Location";
          }
          if (typeof window.document === "object" && obj === window.document) {
            return "Document";
          }
          if (typeof window.navigator === "object") {
            if (typeof window.navigator.mimeTypes === "object" && obj === window.navigator.mimeTypes) {
              return "MimeTypeArray";
            }
            if (typeof window.navigator.plugins === "object" && obj === window.navigator.plugins) {
              return "PluginArray";
            }
          }
          if ((typeof window.HTMLElement === "function" || typeof window.HTMLElement === "object") && obj instanceof window.HTMLElement) {
            if (obj.tagName === "BLOCKQUOTE") {
              return "HTMLQuoteElement";
            }
            if (obj.tagName === "TD") {
              return "HTMLTableDataCellElement";
            }
            if (obj.tagName === "TH") {
              return "HTMLTableHeaderCellElement";
            }
          }
        }
        var stringTag = symbolToStringTagExists && obj[Symbol.toStringTag];
        if (typeof stringTag === "string") {
          return stringTag;
        }
        var objPrototype = Object.getPrototypeOf(obj);
        if (objPrototype === RegExp.prototype) {
          return "RegExp";
        }
        if (objPrototype === Date.prototype) {
          return "Date";
        }
        if (promiseExists && objPrototype === Promise.prototype) {
          return "Promise";
        }
        if (setExists && objPrototype === Set.prototype) {
          return "Set";
        }
        if (mapExists && objPrototype === Map.prototype) {
          return "Map";
        }
        if (weakSetExists && objPrototype === WeakSet.prototype) {
          return "WeakSet";
        }
        if (weakMapExists && objPrototype === WeakMap.prototype) {
          return "WeakMap";
        }
        if (dataViewExists && objPrototype === DataView.prototype) {
          return "DataView";
        }
        if (mapExists && objPrototype === mapIteratorPrototype) {
          return "Map Iterator";
        }
        if (setExists && objPrototype === setIteratorPrototype) {
          return "Set Iterator";
        }
        if (arrayIteratorExists && objPrototype === arrayIteratorPrototype) {
          return "Array Iterator";
        }
        if (stringIteratorExists && objPrototype === stringIteratorPrototype) {
          return "String Iterator";
        }
        if (objPrototype === null) {
          return "Object";
        }
        return Object.prototype.toString.call(obj).slice(toStringLeftSliceLength, toStringRightSliceLength);
      }
      return typeDetect;
    }));
  }
});

// node_modules/@sinonjs/samsam/lib/array-types.js
var require_array_types = __commonJS({
  "node_modules/@sinonjs/samsam/lib/array-types.js"(exports, module) {
    "use strict";
    var ARRAY_TYPES = [
      Array,
      Int8Array,
      Uint8Array,
      Uint8ClampedArray,
      Int16Array,
      Uint16Array,
      Int32Array,
      Uint32Array,
      Float32Array,
      Float64Array
    ];
    module.exports = ARRAY_TYPES;
  }
});

// node_modules/@sinonjs/samsam/lib/is-array-type.js
var require_is_array_type = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-array-type.js"(exports, module) {
    "use strict";
    var functionName = require_lib().functionName;
    var indexOf = require_lib().prototypes.array.indexOf;
    var map = require_lib().prototypes.array.map;
    var ARRAY_TYPES = require_array_types();
    var type = require_type_detect2();
    function isArrayType(object) {
      return indexOf(map(ARRAY_TYPES, functionName), type(object)) !== -1;
    }
    module.exports = isArrayType;
  }
});

// node_modules/@sinonjs/samsam/lib/is-date.js
var require_is_date = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-date.js"(exports, module) {
    "use strict";
    function isDate(value) {
      return value instanceof Date;
    }
    module.exports = isDate;
  }
});

// node_modules/@sinonjs/samsam/lib/is-iterable.js
var require_is_iterable = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-iterable.js"(exports, module) {
    "use strict";
    function isIterable(val) {
      if (typeof val !== "object") {
        return false;
      }
      return typeof val[Symbol.iterator] === "function";
    }
    module.exports = isIterable;
  }
});

// node_modules/@sinonjs/samsam/lib/is-object.js
var require_is_object = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-object.js"(exports, module) {
    "use strict";
    function isObject(value) {
      return typeof value === "object" && value !== null && // none of these are collection objects, so we can return false
      !(value instanceof Boolean) && !(value instanceof Date) && !(value instanceof Error) && !(value instanceof Number) && !(value instanceof RegExp) && !(value instanceof String);
    }
    module.exports = isObject;
  }
});

// node_modules/@sinonjs/samsam/lib/is-subset.js
var require_is_subset = __commonJS({
  "node_modules/@sinonjs/samsam/lib/is-subset.js"(exports, module) {
    "use strict";
    var forEach = require_lib().prototypes.set.forEach;
    function isSubset(s1, s2, compare) {
      var allContained = true;
      forEach(s1, function(v1) {
        var includes = false;
        forEach(s2, function(v2) {
          if (compare(v2, v1)) {
            includes = true;
          }
        });
        allContained = allContained && includes;
      });
      return allContained;
    }
    module.exports = isSubset;
  }
});

// node_modules/@sinonjs/samsam/lib/deep-equal.js
var require_deep_equal = __commonJS({
  "node_modules/@sinonjs/samsam/lib/deep-equal.js"(exports, module) {
    "use strict";
    var valueToString = require_lib().valueToString;
    var className = require_lib().className;
    var typeOf = require_lib().typeOf;
    var arrayProto = require_lib().prototypes.array;
    var objectProto = require_lib().prototypes.object;
    var mapForEach = require_lib().prototypes.map.forEach;
    var getClass = require_get_class();
    var identical = require_identical();
    var isArguments = require_is_arguments();
    var isArrayType = require_is_array_type();
    var isDate = require_is_date();
    var isElement = require_is_element();
    var isIterable = require_is_iterable();
    var isMap = require_is_map();
    var isNaN2 = require_is_nan();
    var isObject = require_is_object();
    var isSet = require_is_set();
    var isSubset = require_is_subset();
    var concat = arrayProto.concat;
    var every = arrayProto.every;
    var push = arrayProto.push;
    var getTime = Date.prototype.getTime;
    var hasOwnProperty = objectProto.hasOwnProperty;
    var indexOf = arrayProto.indexOf;
    var keys = Object.keys;
    var getOwnPropertySymbols = Object.getOwnPropertySymbols;
    function deepEqualCyclic(actual, expectation, match) {
      var actualObjects = [];
      var expectationObjects = [];
      var actualPaths = [];
      var expectationPaths = [];
      var compared = {};
      return (function deepEqual(actualObj, expectationObj, actualPath, expectationPath) {
        if (match && match.isMatcher(expectationObj)) {
          if (match.isMatcher(actualObj)) {
            return actualObj === expectationObj;
          }
          return expectationObj.test(actualObj);
        }
        var actualType = typeof actualObj;
        var expectationType = typeof expectationObj;
        if (actualObj === expectationObj || isNaN2(actualObj) || isNaN2(expectationObj) || actualObj === null || expectationObj === null || actualObj === void 0 || expectationObj === void 0 || actualType !== "object" || expectationType !== "object") {
          return identical(actualObj, expectationObj);
        }
        if (isElement(actualObj) || isElement(expectationObj)) {
          return false;
        }
        var isActualDate = isDate(actualObj);
        var isExpectationDate = isDate(expectationObj);
        if (isActualDate || isExpectationDate) {
          if (!isActualDate || !isExpectationDate || getTime.call(actualObj) !== getTime.call(expectationObj)) {
            return false;
          }
        }
        if (actualObj instanceof RegExp && expectationObj instanceof RegExp) {
          if (valueToString(actualObj) !== valueToString(expectationObj)) {
            return false;
          }
        }
        if (actualObj instanceof Promise && expectationObj instanceof Promise) {
          return actualObj === expectationObj;
        }
        if (actualObj instanceof Error && expectationObj instanceof Error) {
          return actualObj === expectationObj;
        }
        var actualClass = getClass(actualObj);
        var expectationClass = getClass(expectationObj);
        var actualKeys = keys(actualObj);
        var expectationKeys = keys(expectationObj);
        var actualName = className(actualObj);
        var expectationName = className(expectationObj);
        var expectationSymbols = typeOf(getOwnPropertySymbols) === "function" ? getOwnPropertySymbols(expectationObj) : (
          /* istanbul ignore next: cannot collect coverage for engine that doesn't support Symbol */
          []
        );
        var expectationKeysAndSymbols = concat(
          expectationKeys,
          expectationSymbols
        );
        if (isArguments(actualObj) || isArguments(expectationObj)) {
          if (actualObj.length !== expectationObj.length) {
            return false;
          }
        } else {
          if (actualType !== expectationType || actualClass !== expectationClass || actualKeys.length !== expectationKeys.length || actualName && expectationName && actualName !== expectationName) {
            return false;
          }
        }
        if (isSet(actualObj) || isSet(expectationObj)) {
          if (!isSet(actualObj) || !isSet(expectationObj) || actualObj.size !== expectationObj.size) {
            return false;
          }
          return isSubset(actualObj, expectationObj, deepEqual);
        }
        if (isMap(actualObj) || isMap(expectationObj)) {
          if (!isMap(actualObj) || !isMap(expectationObj) || actualObj.size !== expectationObj.size) {
            return false;
          }
          var mapsDeeplyEqual = true;
          mapForEach(actualObj, function(value, key) {
            mapsDeeplyEqual = mapsDeeplyEqual && deepEqualCyclic(value, expectationObj.get(key));
          });
          return mapsDeeplyEqual;
        }
        if (actualObj.constructor && actualObj.constructor.name === "jQuery" && typeof actualObj.is === "function") {
          return actualObj.is(expectationObj);
        }
        var isActualNonArrayIterable = isIterable(actualObj) && !isArrayType(actualObj) && !isArguments(actualObj);
        var isExpectationNonArrayIterable = isIterable(expectationObj) && !isArrayType(expectationObj) && !isArguments(expectationObj);
        if (isActualNonArrayIterable || isExpectationNonArrayIterable) {
          var actualArray = Array.from(actualObj);
          var expectationArray = Array.from(expectationObj);
          if (actualArray.length !== expectationArray.length) {
            return false;
          }
          var arrayDeeplyEquals = true;
          every(actualArray, function(key) {
            arrayDeeplyEquals = arrayDeeplyEquals && deepEqualCyclic(actualArray[key], expectationArray[key]);
          });
          return arrayDeeplyEquals;
        }
        return every(expectationKeysAndSymbols, function(key) {
          if (!hasOwnProperty(actualObj, key)) {
            return false;
          }
          var actualValue = actualObj[key];
          var expectationValue = expectationObj[key];
          var actualObject = isObject(actualValue);
          var expectationObject = isObject(expectationValue);
          var actualIndex = actualObject ? indexOf(actualObjects, actualValue) : -1;
          var expectationIndex = expectationObject ? indexOf(expectationObjects, expectationValue) : -1;
          var newActualPath = actualIndex !== -1 ? actualPaths[actualIndex] : `${actualPath}[${JSON.stringify(key)}]`;
          var newExpectationPath = expectationIndex !== -1 ? expectationPaths[expectationIndex] : `${expectationPath}[${JSON.stringify(key)}]`;
          var combinedPath = newActualPath + newExpectationPath;
          if (compared[combinedPath]) {
            return true;
          }
          if (actualIndex === -1 && actualObject) {
            push(actualObjects, actualValue);
            push(actualPaths, newActualPath);
          }
          if (expectationIndex === -1 && expectationObject) {
            push(expectationObjects, expectationValue);
            push(expectationPaths, newExpectationPath);
          }
          if (actualObject && expectationObject) {
            compared[combinedPath] = true;
          }
          return deepEqual(
            actualValue,
            expectationValue,
            newActualPath,
            newExpectationPath
          );
        });
      })(actual, expectation, "$1", "$2");
    }
    deepEqualCyclic.use = function(match) {
      return function deepEqual(a, b) {
        return deepEqualCyclic(a, b, match);
      };
    };
    module.exports = deepEqualCyclic;
  }
});

// node_modules/@sinonjs/samsam/lib/iterable-to-string.js
var require_iterable_to_string = __commonJS({
  "node_modules/@sinonjs/samsam/lib/iterable-to-string.js"(exports, module) {
    "use strict";
    var slice = require_lib().prototypes.string.slice;
    var typeOf = require_lib().typeOf;
    var valueToString = require_lib().valueToString;
    function iterableToString(obj) {
      if (typeOf(obj) === "map") {
        return mapToString(obj);
      }
      return genericIterableToString(obj);
    }
    function mapToString(map) {
      var representation = "";
      map.forEach(function(value, key) {
        representation += `[${stringify(key)},${stringify(value)}],`;
      });
      representation = slice(representation, 0, -1);
      return representation;
    }
    function genericIterableToString(iterable) {
      var representation = "";
      iterable.forEach(function(value) {
        representation += `${stringify(value)},`;
      });
      representation = slice(representation, 0, -1);
      return representation;
    }
    function stringify(item) {
      return typeof item === "string" ? `'${item}'` : valueToString(item);
    }
    module.exports = iterableToString;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/matcher-prototype.js
var require_matcher_prototype = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/matcher-prototype.js"(exports, module) {
    "use strict";
    var matcherPrototype = {
      toString: function() {
        return this.message;
      }
    };
    matcherPrototype.or = function(valueOrMatcher) {
      var createMatcher = require_create_matcher();
      var isMatcher = createMatcher.isMatcher;
      if (!arguments.length) {
        throw new TypeError("Matcher expected");
      }
      var m2 = isMatcher(valueOrMatcher) ? valueOrMatcher : createMatcher(valueOrMatcher);
      var m1 = this;
      var or = Object.create(matcherPrototype);
      or.test = function(actual) {
        return m1.test(actual) || m2.test(actual);
      };
      or.message = `${m1.message}.or(${m2.message})`;
      return or;
    };
    matcherPrototype.and = function(valueOrMatcher) {
      var createMatcher = require_create_matcher();
      var isMatcher = createMatcher.isMatcher;
      if (!arguments.length) {
        throw new TypeError("Matcher expected");
      }
      var m2 = isMatcher(valueOrMatcher) ? valueOrMatcher : createMatcher(valueOrMatcher);
      var m1 = this;
      var and = Object.create(matcherPrototype);
      and.test = function(actual) {
        return m1.test(actual) && m2.test(actual);
      };
      and.message = `${m1.message}.and(${m2.message})`;
      return and;
    };
    module.exports = matcherPrototype;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/is-matcher.js
var require_is_matcher = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/is-matcher.js"(exports, module) {
    "use strict";
    var isPrototypeOf = require_lib().prototypes.object.isPrototypeOf;
    var matcherPrototype = require_matcher_prototype();
    function isMatcher(object) {
      return isPrototypeOf(matcherPrototype, object);
    }
    module.exports = isMatcher;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/assert-matcher.js
var require_assert_matcher = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/assert-matcher.js"(exports, module) {
    "use strict";
    var isMatcher = require_is_matcher();
    function assertMatcher(value) {
      if (!isMatcher(value)) {
        throw new TypeError("Matcher expected");
      }
    }
    module.exports = assertMatcher;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/assert-method-exists.js
var require_assert_method_exists = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/assert-method-exists.js"(exports, module) {
    "use strict";
    function assertMethodExists(value, method, name, methodPath) {
      if (value[method] === null || value[method] === void 0) {
        throw new TypeError(`Expected ${name} to have method ${methodPath}`);
      }
    }
    module.exports = assertMethodExists;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/assert-type.js
var require_assert_type = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/assert-type.js"(exports, module) {
    "use strict";
    var typeOf = require_lib().typeOf;
    function assertType(value, type, name) {
      var actual = typeOf(value);
      if (actual !== type) {
        throw new TypeError(
          `Expected type of ${name} to be ${type}, but was ${actual}`
        );
      }
    }
    module.exports = assertType;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/is-iterable.js
var require_is_iterable2 = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/is-iterable.js"(exports, module) {
    "use strict";
    var typeOf = require_lib().typeOf;
    function isIterable(value) {
      return Boolean(value) && typeOf(value.forEach) === "function";
    }
    module.exports = isIterable;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/match-object.js
var require_match_object = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/match-object.js"(exports, module) {
    "use strict";
    var every = require_lib().prototypes.array.every;
    var concat = require_lib().prototypes.array.concat;
    var typeOf = require_lib().typeOf;
    var deepEqualFactory = require_deep_equal().use;
    var identical = require_identical();
    var isMatcher = require_is_matcher();
    var keys = Object.keys;
    var getOwnPropertySymbols = Object.getOwnPropertySymbols;
    function matchObject(actual, expectation, matcher) {
      var deepEqual = deepEqualFactory(matcher);
      if (actual === null || actual === void 0) {
        return false;
      }
      var expectedKeys = keys(expectation);
      if (typeOf(getOwnPropertySymbols) === "function") {
        expectedKeys = concat(expectedKeys, getOwnPropertySymbols(expectation));
      }
      return every(expectedKeys, function(key) {
        var exp = expectation[key];
        var act = actual[key];
        if (isMatcher(exp)) {
          if (!exp.test(act)) {
            return false;
          }
        } else if (typeOf(exp) === "object") {
          if (identical(exp, act)) {
            return true;
          }
          if (!matchObject(act, exp, matcher)) {
            return false;
          }
        } else if (!deepEqual(act, exp)) {
          return false;
        }
        return true;
      });
    }
    module.exports = matchObject;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher/type-map.js
var require_type_map = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher/type-map.js"(exports, module) {
    "use strict";
    var functionName = require_lib().functionName;
    var join = require_lib().prototypes.array.join;
    var map = require_lib().prototypes.array.map;
    var stringIndexOf = require_lib().prototypes.string.indexOf;
    var valueToString = require_lib().valueToString;
    var matchObject = require_match_object();
    var createTypeMap = function(match) {
      return {
        function: function(m, expectation, message) {
          m.test = expectation;
          m.message = message || `match(${functionName(expectation)})`;
        },
        number: function(m, expectation) {
          m.test = function(actual) {
            return expectation == actual;
          };
        },
        object: function(m, expectation) {
          var array = [];
          if (typeof expectation.test === "function") {
            m.test = function(actual) {
              return expectation.test(actual) === true;
            };
            m.message = `match(${functionName(expectation.test)})`;
            return m;
          }
          array = map(Object.keys(expectation), function(key) {
            return `${key}: ${valueToString(expectation[key])}`;
          });
          m.test = function(actual) {
            return matchObject(actual, expectation, match);
          };
          m.message = `match(${join(array, ", ")})`;
          return m;
        },
        regexp: function(m, expectation) {
          m.test = function(actual) {
            return typeof actual === "string" && expectation.test(actual);
          };
        },
        string: function(m, expectation) {
          m.test = function(actual) {
            return typeof actual === "string" && stringIndexOf(actual, expectation) !== -1;
          };
          m.message = `match("${expectation}")`;
        }
      };
    };
    module.exports = createTypeMap;
  }
});

// node_modules/@sinonjs/samsam/lib/create-matcher.js
var require_create_matcher = __commonJS({
  "node_modules/@sinonjs/samsam/lib/create-matcher.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var deepEqual = require_deep_equal().use(createMatcher);
    var every = require_lib().every;
    var functionName = require_lib().functionName;
    var iterableToString = require_iterable_to_string();
    var objectProto = require_lib().prototypes.object;
    var typeOf = require_lib().typeOf;
    var valueToString = require_lib().valueToString;
    var assertMatcher = require_assert_matcher();
    var assertMethodExists = require_assert_method_exists();
    var assertType = require_assert_type();
    var isIterable = require_is_iterable2();
    var isMatcher = require_is_matcher();
    var matcherPrototype = require_matcher_prototype();
    var arrayIndexOf = arrayProto.indexOf;
    var some = arrayProto.some;
    var hasOwnProperty = objectProto.hasOwnProperty;
    var objectToString = objectProto.toString;
    var TYPE_MAP = require_type_map()(createMatcher);
    function createMatcher(expectation, message) {
      var m = Object.create(matcherPrototype);
      var type = typeOf(expectation);
      if (message !== void 0 && typeof message !== "string") {
        throw new TypeError("Message should be a string");
      }
      if (arguments.length > 2) {
        throw new TypeError(
          `Expected 1 or 2 arguments, received ${arguments.length}`
        );
      }
      if (type in TYPE_MAP) {
        TYPE_MAP[type](m, expectation, message);
      } else {
        m.test = function(actual) {
          return deepEqual(actual, expectation);
        };
      }
      if (!m.message) {
        m.message = `match(${valueToString(expectation)})`;
      }
      Object.defineProperty(m, "message", {
        configurable: false,
        writable: false,
        value: m.message
      });
      return m;
    }
    createMatcher.isMatcher = isMatcher;
    createMatcher.any = createMatcher(function() {
      return true;
    }, "any");
    createMatcher.defined = createMatcher(function(actual) {
      return actual !== null && actual !== void 0;
    }, "defined");
    createMatcher.truthy = createMatcher(function(actual) {
      return Boolean(actual);
    }, "truthy");
    createMatcher.falsy = createMatcher(function(actual) {
      return !actual;
    }, "falsy");
    createMatcher.same = function(expectation) {
      return createMatcher(
        function(actual) {
          return expectation === actual;
        },
        `same(${valueToString(expectation)})`
      );
    };
    createMatcher.in = function(arrayOfExpectations) {
      if (typeOf(arrayOfExpectations) !== "array") {
        throw new TypeError("array expected");
      }
      return createMatcher(
        function(actual) {
          return some(arrayOfExpectations, function(expectation) {
            return expectation === actual;
          });
        },
        `in(${valueToString(arrayOfExpectations)})`
      );
    };
    createMatcher.typeOf = function(type) {
      assertType(type, "string", "type");
      return createMatcher(function(actual) {
        return typeOf(actual) === type;
      }, `typeOf("${type}")`);
    };
    createMatcher.instanceOf = function(type) {
      if (typeof Symbol === "undefined" || typeof Symbol.hasInstance === "undefined") {
        assertType(type, "function", "type");
      } else {
        assertMethodExists(
          type,
          Symbol.hasInstance,
          "type",
          "[Symbol.hasInstance]"
        );
      }
      return createMatcher(
        function(actual) {
          return actual instanceof type;
        },
        `instanceOf(${functionName(type) || objectToString(type)})`
      );
    };
    function createPropertyMatcher(propertyTest, messagePrefix) {
      return function(property, value) {
        assertType(property, "string", "property");
        var onlyProperty = arguments.length === 1;
        var message = `${messagePrefix}("${property}"`;
        if (!onlyProperty) {
          message += `, ${valueToString(value)}`;
        }
        message += ")";
        return createMatcher(function(actual) {
          if (actual === void 0 || actual === null || !propertyTest(actual, property)) {
            return false;
          }
          return onlyProperty || deepEqual(actual[property], value);
        }, message);
      };
    }
    createMatcher.has = createPropertyMatcher(function(actual, property) {
      if (typeof actual === "object") {
        return property in actual;
      }
      return actual[property] !== void 0;
    }, "has");
    createMatcher.hasOwn = createPropertyMatcher(function(actual, property) {
      return hasOwnProperty(actual, property);
    }, "hasOwn");
    createMatcher.hasNested = function(property, value) {
      assertType(property, "string", "property");
      var onlyProperty = arguments.length === 1;
      var message = `hasNested("${property}"`;
      if (!onlyProperty) {
        message += `, ${valueToString(value)}`;
      }
      message += ")";
      return createMatcher(function(actual) {
        const parts = property.split(/(?:\.|\[|\])+?/).filter(Boolean);
        let current = actual;
        for (const part of parts) {
          current = current?.[part];
          if (current === void 0) {
            return false;
          }
        }
        return onlyProperty || deepEqual(current, value);
      }, message);
    };
    var jsonParseResultTypes = {
      null: true,
      boolean: true,
      number: true,
      string: true,
      object: true,
      array: true
    };
    createMatcher.json = function(value) {
      if (!jsonParseResultTypes[typeOf(value)]) {
        throw new TypeError("Value cannot be the result of JSON.parse");
      }
      var message = `json(${JSON.stringify(value, null, "  ")})`;
      return createMatcher(function(actual) {
        var parsed;
        try {
          parsed = JSON.parse(actual);
        } catch (e) {
          return false;
        }
        return deepEqual(parsed, value);
      }, message);
    };
    createMatcher.every = function(predicate) {
      assertMatcher(predicate);
      return createMatcher(function(actual) {
        if (typeOf(actual) === "object") {
          return every(Object.keys(actual), function(key) {
            return predicate.test(actual[key]);
          });
        }
        return isIterable(actual) && every(actual, function(element) {
          return predicate.test(element);
        });
      }, `every(${predicate.message})`);
    };
    createMatcher.some = function(predicate) {
      assertMatcher(predicate);
      return createMatcher(function(actual) {
        if (typeOf(actual) === "object") {
          return !every(Object.keys(actual), function(key) {
            return !predicate.test(actual[key]);
          });
        }
        return isIterable(actual) && !every(actual, function(element) {
          return !predicate.test(element);
        });
      }, `some(${predicate.message})`);
    };
    createMatcher.array = createMatcher.typeOf("array");
    createMatcher.array.deepEquals = function(expectation) {
      return createMatcher(
        function(actual) {
          var sameLength = actual.length === expectation.length;
          return typeOf(actual) === "array" && sameLength && every(actual, function(element, index) {
            var expected = expectation[index];
            return typeOf(expected) === "array" && typeOf(element) === "array" ? createMatcher.array.deepEquals(expected).test(element) : deepEqual(expected, element);
          });
        },
        `deepEquals([${iterableToString(expectation)}])`
      );
    };
    createMatcher.array.startsWith = function(expectation) {
      return createMatcher(
        function(actual) {
          return typeOf(actual) === "array" && every(expectation, function(expectedElement, index) {
            return actual[index] === expectedElement;
          });
        },
        `startsWith([${iterableToString(expectation)}])`
      );
    };
    createMatcher.array.endsWith = function(expectation) {
      return createMatcher(
        function(actual) {
          var offset = actual.length - expectation.length;
          return typeOf(actual) === "array" && every(expectation, function(expectedElement, index) {
            return actual[offset + index] === expectedElement;
          });
        },
        `endsWith([${iterableToString(expectation)}])`
      );
    };
    createMatcher.array.contains = function(expectation) {
      return createMatcher(
        function(actual) {
          return typeOf(actual) === "array" && every(expectation, function(expectedElement) {
            return arrayIndexOf(actual, expectedElement) !== -1;
          });
        },
        `contains([${iterableToString(expectation)}])`
      );
    };
    createMatcher.map = createMatcher.typeOf("map");
    createMatcher.map.deepEquals = function mapDeepEquals(expectation) {
      return createMatcher(
        function(actual) {
          var sameLength = actual.size === expectation.size;
          return typeOf(actual) === "map" && sameLength && every(actual, function(element, key) {
            return expectation.has(key) && expectation.get(key) === element;
          });
        },
        `deepEquals(Map[${iterableToString(expectation)}])`
      );
    };
    createMatcher.map.contains = function mapContains(expectation) {
      return createMatcher(
        function(actual) {
          return typeOf(actual) === "map" && every(expectation, function(element, key) {
            return actual.has(key) && actual.get(key) === element;
          });
        },
        `contains(Map[${iterableToString(expectation)}])`
      );
    };
    createMatcher.set = createMatcher.typeOf("set");
    createMatcher.set.deepEquals = function setDeepEquals(expectation) {
      return createMatcher(
        function(actual) {
          var sameLength = actual.size === expectation.size;
          return typeOf(actual) === "set" && sameLength && every(actual, function(element) {
            return expectation.has(element);
          });
        },
        `deepEquals(Set[${iterableToString(expectation)}])`
      );
    };
    createMatcher.set.contains = function setContains(expectation) {
      return createMatcher(
        function(actual) {
          return typeOf(actual) === "set" && every(expectation, function(element) {
            return actual.has(element);
          });
        },
        `contains(Set[${iterableToString(expectation)}])`
      );
    };
    createMatcher.bool = createMatcher.typeOf("boolean");
    createMatcher.number = createMatcher.typeOf("number");
    createMatcher.string = createMatcher.typeOf("string");
    createMatcher.object = createMatcher.typeOf("object");
    createMatcher.func = createMatcher.typeOf("function");
    createMatcher.regexp = createMatcher.typeOf("regexp");
    createMatcher.date = createMatcher.typeOf("date");
    createMatcher.symbol = createMatcher.typeOf("symbol");
    module.exports = createMatcher;
  }
});

// node_modules/@sinonjs/samsam/lib/match.js
var require_match = __commonJS({
  "node_modules/@sinonjs/samsam/lib/match.js"(exports, module) {
    "use strict";
    var valueToString = require_lib().valueToString;
    var indexOf = require_lib().prototypes.string.indexOf;
    var forEach = require_lib().prototypes.array.forEach;
    var type = require_type_detect2();
    var engineCanCompareMaps = typeof Array.from === "function";
    var deepEqual = require_deep_equal().use(match);
    var isArrayType = require_is_array_type();
    var isSubset = require_is_subset();
    var createMatcher = require_create_matcher();
    function arrayContains(array, subset, compare) {
      if (subset.length === 0) {
        return true;
      }
      var i, l, j, k;
      for (i = 0, l = array.length; i < l; ++i) {
        if (compare(array[i], subset[0])) {
          for (j = 0, k = subset.length; j < k; ++j) {
            if (i + j >= l) {
              return false;
            }
            if (!compare(array[i + j], subset[j])) {
              return false;
            }
          }
          return true;
        }
      }
      return false;
    }
    function match(object, matcherOrValue) {
      if (matcherOrValue && typeof matcherOrValue.test === "function") {
        return matcherOrValue.test(object);
      }
      switch (type(matcherOrValue)) {
        case "bigint":
        case "boolean":
        case "number":
        case "symbol":
          return matcherOrValue === object;
        case "function":
          return matcherOrValue(object) === true;
        case "string":
          var notNull = typeof object === "string" || Boolean(object);
          return notNull && indexOf(
            valueToString(object).toLowerCase(),
            matcherOrValue.toLowerCase()
          ) >= 0;
        case "null":
          return object === null;
        case "undefined":
          return typeof object === "undefined";
        case "Date":
          if (type(object) === "Date") {
            return object.getTime() === matcherOrValue.getTime();
          }
          break;
        case "Array":
        case "Int8Array":
        case "Uint8Array":
        case "Uint8ClampedArray":
        case "Int16Array":
        case "Uint16Array":
        case "Int32Array":
        case "Uint32Array":
        case "Float32Array":
        case "Float64Array":
          return isArrayType(matcherOrValue) && arrayContains(object, matcherOrValue, match);
        case "Map":
          if (!engineCanCompareMaps) {
            throw new Error(
              "The JavaScript engine does not support Array.from and cannot reliably do value comparison of Map instances"
            );
          }
          return type(object) === "Map" && arrayContains(
            Array.from(object),
            Array.from(matcherOrValue),
            match
          );
        default:
          break;
      }
      switch (type(object)) {
        case "null":
          return false;
        case "Set":
          return isSubset(matcherOrValue, object, match);
        default:
          break;
      }
      if (matcherOrValue && typeof matcherOrValue === "object") {
        if (matcherOrValue === object) {
          return true;
        }
        if (typeof object !== "object") {
          return false;
        }
        var prop;
        for (prop in matcherOrValue) {
          var value = object[prop];
          if (typeof value === "undefined" && typeof object.getAttribute === "function") {
            value = object.getAttribute(prop);
          }
          if (matcherOrValue[prop] === null || typeof matcherOrValue[prop] === "undefined") {
            if (value !== matcherOrValue[prop]) {
              return false;
            }
          } else if (typeof value === "undefined" || !deepEqual(value, matcherOrValue[prop])) {
            return false;
          }
        }
        return true;
      }
      throw new Error("Matcher was an unknown or unsupported type");
    }
    forEach(Object.keys(createMatcher), function(key) {
      match[key] = createMatcher[key];
    });
    module.exports = match;
  }
});

// node_modules/@sinonjs/samsam/lib/samsam.js
var require_samsam = __commonJS({
  "node_modules/@sinonjs/samsam/lib/samsam.js"(exports, module) {
    "use strict";
    var identical = require_identical();
    var isArguments = require_is_arguments();
    var isElement = require_is_element();
    var isNegZero = require_is_neg_zero();
    var isSet = require_is_set();
    var isMap = require_is_map();
    var match = require_match();
    var deepEqualCyclic = require_deep_equal().use(match);
    var createMatcher = require_create_matcher();
    module.exports = {
      createMatcher,
      deepEqual: deepEqualCyclic,
      identical,
      isArguments,
      isElement,
      isMap,
      isNegZero,
      isSet,
      match
    };
  }
});

// lib/sinon/util/core/times-in-words.js
var require_times_in_words = __commonJS({
  "lib/sinon/util/core/times-in-words.js"(exports, module) {
    "use strict";
    var array = [null, "once", "twice", "thrice"];
    module.exports = function timesInWords(count) {
      return array[count] || `${count || 0} times`;
    };
  }
});

// node_modules/has-symbols/shams.js
var require_shams = __commonJS({
  "node_modules/has-symbols/shams.js"(exports, module) {
    "use strict";
    module.exports = function hasSymbols() {
      if (typeof Symbol !== "function" || typeof Object.getOwnPropertySymbols !== "function") {
        return false;
      }
      if (typeof Symbol.iterator === "symbol") {
        return true;
      }
      var obj = {};
      var sym = /* @__PURE__ */ Symbol("test");
      var symObj = Object(sym);
      if (typeof sym === "string") {
        return false;
      }
      if (Object.prototype.toString.call(sym) !== "[object Symbol]") {
        return false;
      }
      if (Object.prototype.toString.call(symObj) !== "[object Symbol]") {
        return false;
      }
      var symVal = 42;
      obj[sym] = symVal;
      for (sym in obj) {
        return false;
      }
      if (typeof Object.keys === "function" && Object.keys(obj).length !== 0) {
        return false;
      }
      if (typeof Object.getOwnPropertyNames === "function" && Object.getOwnPropertyNames(obj).length !== 0) {
        return false;
      }
      var syms = Object.getOwnPropertySymbols(obj);
      if (syms.length !== 1 || syms[0] !== sym) {
        return false;
      }
      if (!Object.prototype.propertyIsEnumerable.call(obj, sym)) {
        return false;
      }
      if (typeof Object.getOwnPropertyDescriptor === "function") {
        var descriptor = Object.getOwnPropertyDescriptor(obj, sym);
        if (descriptor.value !== symVal || descriptor.enumerable !== true) {
          return false;
        }
      }
      return true;
    };
  }
});

// node_modules/has-tostringtag/shams.js
var require_shams2 = __commonJS({
  "node_modules/has-tostringtag/shams.js"(exports, module) {
    "use strict";
    var hasSymbols = require_shams();
    module.exports = function hasToStringTagShams() {
      return hasSymbols() && !!Symbol.toStringTag;
    };
  }
});

// node_modules/es-errors/index.js
var require_es_errors = __commonJS({
  "node_modules/es-errors/index.js"(exports, module) {
    "use strict";
    module.exports = Error;
  }
});

// node_modules/es-errors/eval.js
var require_eval = __commonJS({
  "node_modules/es-errors/eval.js"(exports, module) {
    "use strict";
    module.exports = EvalError;
  }
});

// node_modules/es-errors/range.js
var require_range = __commonJS({
  "node_modules/es-errors/range.js"(exports, module) {
    "use strict";
    module.exports = RangeError;
  }
});

// node_modules/es-errors/ref.js
var require_ref = __commonJS({
  "node_modules/es-errors/ref.js"(exports, module) {
    "use strict";
    module.exports = ReferenceError;
  }
});

// node_modules/es-errors/syntax.js
var require_syntax = __commonJS({
  "node_modules/es-errors/syntax.js"(exports, module) {
    "use strict";
    module.exports = SyntaxError;
  }
});

// node_modules/es-errors/type.js
var require_type = __commonJS({
  "node_modules/es-errors/type.js"(exports, module) {
    "use strict";
    module.exports = TypeError;
  }
});

// node_modules/es-errors/uri.js
var require_uri = __commonJS({
  "node_modules/es-errors/uri.js"(exports, module) {
    "use strict";
    module.exports = URIError;
  }
});

// node_modules/has-symbols/index.js
var require_has_symbols = __commonJS({
  "node_modules/has-symbols/index.js"(exports, module) {
    "use strict";
    var origSymbol = typeof Symbol !== "undefined" && Symbol;
    var hasSymbolSham = require_shams();
    module.exports = function hasNativeSymbols() {
      if (typeof origSymbol !== "function") {
        return false;
      }
      if (typeof Symbol !== "function") {
        return false;
      }
      if (typeof origSymbol("foo") !== "symbol") {
        return false;
      }
      if (typeof /* @__PURE__ */ Symbol("bar") !== "symbol") {
        return false;
      }
      return hasSymbolSham();
    };
  }
});

// node_modules/has-proto/index.js
var require_has_proto = __commonJS({
  "node_modules/has-proto/index.js"(exports, module) {
    "use strict";
    var test = {
      __proto__: null,
      foo: {}
    };
    var $Object = Object;
    module.exports = function hasProto() {
      return { __proto__: test }.foo === test.foo && !(test instanceof $Object);
    };
  }
});

// node_modules/function-bind/implementation.js
var require_implementation = __commonJS({
  "node_modules/function-bind/implementation.js"(exports, module) {
    "use strict";
    var ERROR_MESSAGE = "Function.prototype.bind called on incompatible ";
    var toStr = Object.prototype.toString;
    var max = Math.max;
    var funcType = "[object Function]";
    var concatty = function concatty2(a, b) {
      var arr = [];
      for (var i = 0; i < a.length; i += 1) {
        arr[i] = a[i];
      }
      for (var j = 0; j < b.length; j += 1) {
        arr[j + a.length] = b[j];
      }
      return arr;
    };
    var slicy = function slicy2(arrLike, offset) {
      var arr = [];
      for (var i = offset || 0, j = 0; i < arrLike.length; i += 1, j += 1) {
        arr[j] = arrLike[i];
      }
      return arr;
    };
    var joiny = function(arr, joiner) {
      var str = "";
      for (var i = 0; i < arr.length; i += 1) {
        str += arr[i];
        if (i + 1 < arr.length) {
          str += joiner;
        }
      }
      return str;
    };
    module.exports = function bind(that) {
      var target = this;
      if (typeof target !== "function" || toStr.apply(target) !== funcType) {
        throw new TypeError(ERROR_MESSAGE + target);
      }
      var args = slicy(arguments, 1);
      var bound;
      var binder = function() {
        if (this instanceof bound) {
          var result = target.apply(
            this,
            concatty(args, arguments)
          );
          if (Object(result) === result) {
            return result;
          }
          return this;
        }
        return target.apply(
          that,
          concatty(args, arguments)
        );
      };
      var boundLength = max(0, target.length - args.length);
      var boundArgs = [];
      for (var i = 0; i < boundLength; i++) {
        boundArgs[i] = "$" + i;
      }
      bound = Function("binder", "return function (" + joiny(boundArgs, ",") + "){ return binder.apply(this,arguments); }")(binder);
      if (target.prototype) {
        var Empty = function Empty2() {
        };
        Empty.prototype = target.prototype;
        bound.prototype = new Empty();
        Empty.prototype = null;
      }
      return bound;
    };
  }
});

// node_modules/function-bind/index.js
var require_function_bind = __commonJS({
  "node_modules/function-bind/index.js"(exports, module) {
    "use strict";
    var implementation = require_implementation();
    module.exports = Function.prototype.bind || implementation;
  }
});

// node_modules/hasown/index.js
var require_hasown = __commonJS({
  "node_modules/hasown/index.js"(exports, module) {
    "use strict";
    var call = Function.prototype.call;
    var $hasOwn = Object.prototype.hasOwnProperty;
    var bind = require_function_bind();
    module.exports = bind.call(call, $hasOwn);
  }
});

// node_modules/get-intrinsic/index.js
var require_get_intrinsic = __commonJS({
  "node_modules/get-intrinsic/index.js"(exports, module) {
    "use strict";
    var undefined2;
    var $Error = require_es_errors();
    var $EvalError = require_eval();
    var $RangeError = require_range();
    var $ReferenceError = require_ref();
    var $SyntaxError = require_syntax();
    var $TypeError = require_type();
    var $URIError = require_uri();
    var $Function = Function;
    var getEvalledConstructor = function(expressionSyntax) {
      try {
        return $Function('"use strict"; return (' + expressionSyntax + ").constructor;")();
      } catch (e) {
      }
    };
    var $gOPD = Object.getOwnPropertyDescriptor;
    if ($gOPD) {
      try {
        $gOPD({}, "");
      } catch (e) {
        $gOPD = null;
      }
    }
    var throwTypeError = function() {
      throw new $TypeError();
    };
    var ThrowTypeError = $gOPD ? (function() {
      try {
        arguments.callee;
        return throwTypeError;
      } catch (calleeThrows) {
        try {
          return $gOPD(arguments, "callee").get;
        } catch (gOPDthrows) {
          return throwTypeError;
        }
      }
    })() : throwTypeError;
    var hasSymbols = require_has_symbols()();
    var hasProto = require_has_proto()();
    var getProto = Object.getPrototypeOf || (hasProto ? function(x) {
      return x.__proto__;
    } : null);
    var needsEval = {};
    var TypedArray = typeof Uint8Array === "undefined" || !getProto ? undefined2 : getProto(Uint8Array);
    var INTRINSICS = {
      __proto__: null,
      "%AggregateError%": typeof AggregateError === "undefined" ? undefined2 : AggregateError,
      "%Array%": Array,
      "%ArrayBuffer%": typeof ArrayBuffer === "undefined" ? undefined2 : ArrayBuffer,
      "%ArrayIteratorPrototype%": hasSymbols && getProto ? getProto([][Symbol.iterator]()) : undefined2,
      "%AsyncFromSyncIteratorPrototype%": undefined2,
      "%AsyncFunction%": needsEval,
      "%AsyncGenerator%": needsEval,
      "%AsyncGeneratorFunction%": needsEval,
      "%AsyncIteratorPrototype%": needsEval,
      "%Atomics%": typeof Atomics === "undefined" ? undefined2 : Atomics,
      "%BigInt%": typeof BigInt === "undefined" ? undefined2 : BigInt,
      "%BigInt64Array%": typeof BigInt64Array === "undefined" ? undefined2 : BigInt64Array,
      "%BigUint64Array%": typeof BigUint64Array === "undefined" ? undefined2 : BigUint64Array,
      "%Boolean%": Boolean,
      "%DataView%": typeof DataView === "undefined" ? undefined2 : DataView,
      "%Date%": Date,
      "%decodeURI%": decodeURI,
      "%decodeURIComponent%": decodeURIComponent,
      "%encodeURI%": encodeURI,
      "%encodeURIComponent%": encodeURIComponent,
      "%Error%": $Error,
      "%eval%": eval,
      // eslint-disable-line no-eval
      "%EvalError%": $EvalError,
      "%Float32Array%": typeof Float32Array === "undefined" ? undefined2 : Float32Array,
      "%Float64Array%": typeof Float64Array === "undefined" ? undefined2 : Float64Array,
      "%FinalizationRegistry%": typeof FinalizationRegistry === "undefined" ? undefined2 : FinalizationRegistry,
      "%Function%": $Function,
      "%GeneratorFunction%": needsEval,
      "%Int8Array%": typeof Int8Array === "undefined" ? undefined2 : Int8Array,
      "%Int16Array%": typeof Int16Array === "undefined" ? undefined2 : Int16Array,
      "%Int32Array%": typeof Int32Array === "undefined" ? undefined2 : Int32Array,
      "%isFinite%": isFinite,
      "%isNaN%": isNaN,
      "%IteratorPrototype%": hasSymbols && getProto ? getProto(getProto([][Symbol.iterator]())) : undefined2,
      "%JSON%": typeof JSON === "object" ? JSON : undefined2,
      "%Map%": typeof Map === "undefined" ? undefined2 : Map,
      "%MapIteratorPrototype%": typeof Map === "undefined" || !hasSymbols || !getProto ? undefined2 : getProto((/* @__PURE__ */ new Map())[Symbol.iterator]()),
      "%Math%": Math,
      "%Number%": Number,
      "%Object%": Object,
      "%parseFloat%": parseFloat,
      "%parseInt%": parseInt,
      "%Promise%": typeof Promise === "undefined" ? undefined2 : Promise,
      "%Proxy%": typeof Proxy === "undefined" ? undefined2 : Proxy,
      "%RangeError%": $RangeError,
      "%ReferenceError%": $ReferenceError,
      "%Reflect%": typeof Reflect === "undefined" ? undefined2 : Reflect,
      "%RegExp%": RegExp,
      "%Set%": typeof Set === "undefined" ? undefined2 : Set,
      "%SetIteratorPrototype%": typeof Set === "undefined" || !hasSymbols || !getProto ? undefined2 : getProto((/* @__PURE__ */ new Set())[Symbol.iterator]()),
      "%SharedArrayBuffer%": typeof SharedArrayBuffer === "undefined" ? undefined2 : SharedArrayBuffer,
      "%String%": String,
      "%StringIteratorPrototype%": hasSymbols && getProto ? getProto(""[Symbol.iterator]()) : undefined2,
      "%Symbol%": hasSymbols ? Symbol : undefined2,
      "%SyntaxError%": $SyntaxError,
      "%ThrowTypeError%": ThrowTypeError,
      "%TypedArray%": TypedArray,
      "%TypeError%": $TypeError,
      "%Uint8Array%": typeof Uint8Array === "undefined" ? undefined2 : Uint8Array,
      "%Uint8ClampedArray%": typeof Uint8ClampedArray === "undefined" ? undefined2 : Uint8ClampedArray,
      "%Uint16Array%": typeof Uint16Array === "undefined" ? undefined2 : Uint16Array,
      "%Uint32Array%": typeof Uint32Array === "undefined" ? undefined2 : Uint32Array,
      "%URIError%": $URIError,
      "%WeakMap%": typeof WeakMap === "undefined" ? undefined2 : WeakMap,
      "%WeakRef%": typeof WeakRef === "undefined" ? undefined2 : WeakRef,
      "%WeakSet%": typeof WeakSet === "undefined" ? undefined2 : WeakSet
    };
    if (getProto) {
      try {
        null.error;
      } catch (e) {
        errorProto = getProto(getProto(e));
        INTRINSICS["%Error.prototype%"] = errorProto;
      }
    }
    var errorProto;
    var doEval = function doEval2(name) {
      var value;
      if (name === "%AsyncFunction%") {
        value = getEvalledConstructor("async function () {}");
      } else if (name === "%GeneratorFunction%") {
        value = getEvalledConstructor("function* () {}");
      } else if (name === "%AsyncGeneratorFunction%") {
        value = getEvalledConstructor("async function* () {}");
      } else if (name === "%AsyncGenerator%") {
        var fn = doEval2("%AsyncGeneratorFunction%");
        if (fn) {
          value = fn.prototype;
        }
      } else if (name === "%AsyncIteratorPrototype%") {
        var gen = doEval2("%AsyncGenerator%");
        if (gen && getProto) {
          value = getProto(gen.prototype);
        }
      }
      INTRINSICS[name] = value;
      return value;
    };
    var LEGACY_ALIASES = {
      __proto__: null,
      "%ArrayBufferPrototype%": ["ArrayBuffer", "prototype"],
      "%ArrayPrototype%": ["Array", "prototype"],
      "%ArrayProto_entries%": ["Array", "prototype", "entries"],
      "%ArrayProto_forEach%": ["Array", "prototype", "forEach"],
      "%ArrayProto_keys%": ["Array", "prototype", "keys"],
      "%ArrayProto_values%": ["Array", "prototype", "values"],
      "%AsyncFunctionPrototype%": ["AsyncFunction", "prototype"],
      "%AsyncGenerator%": ["AsyncGeneratorFunction", "prototype"],
      "%AsyncGeneratorPrototype%": ["AsyncGeneratorFunction", "prototype", "prototype"],
      "%BooleanPrototype%": ["Boolean", "prototype"],
      "%DataViewPrototype%": ["DataView", "prototype"],
      "%DatePrototype%": ["Date", "prototype"],
      "%ErrorPrototype%": ["Error", "prototype"],
      "%EvalErrorPrototype%": ["EvalError", "prototype"],
      "%Float32ArrayPrototype%": ["Float32Array", "prototype"],
      "%Float64ArrayPrototype%": ["Float64Array", "prototype"],
      "%FunctionPrototype%": ["Function", "prototype"],
      "%Generator%": ["GeneratorFunction", "prototype"],
      "%GeneratorPrototype%": ["GeneratorFunction", "prototype", "prototype"],
      "%Int8ArrayPrototype%": ["Int8Array", "prototype"],
      "%Int16ArrayPrototype%": ["Int16Array", "prototype"],
      "%Int32ArrayPrototype%": ["Int32Array", "prototype"],
      "%JSONParse%": ["JSON", "parse"],
      "%JSONStringify%": ["JSON", "stringify"],
      "%MapPrototype%": ["Map", "prototype"],
      "%NumberPrototype%": ["Number", "prototype"],
      "%ObjectPrototype%": ["Object", "prototype"],
      "%ObjProto_toString%": ["Object", "prototype", "toString"],
      "%ObjProto_valueOf%": ["Object", "prototype", "valueOf"],
      "%PromisePrototype%": ["Promise", "prototype"],
      "%PromiseProto_then%": ["Promise", "prototype", "then"],
      "%Promise_all%": ["Promise", "all"],
      "%Promise_reject%": ["Promise", "reject"],
      "%Promise_resolve%": ["Promise", "resolve"],
      "%RangeErrorPrototype%": ["RangeError", "prototype"],
      "%ReferenceErrorPrototype%": ["ReferenceError", "prototype"],
      "%RegExpPrototype%": ["RegExp", "prototype"],
      "%SetPrototype%": ["Set", "prototype"],
      "%SharedArrayBufferPrototype%": ["SharedArrayBuffer", "prototype"],
      "%StringPrototype%": ["String", "prototype"],
      "%SymbolPrototype%": ["Symbol", "prototype"],
      "%SyntaxErrorPrototype%": ["SyntaxError", "prototype"],
      "%TypedArrayPrototype%": ["TypedArray", "prototype"],
      "%TypeErrorPrototype%": ["TypeError", "prototype"],
      "%Uint8ArrayPrototype%": ["Uint8Array", "prototype"],
      "%Uint8ClampedArrayPrototype%": ["Uint8ClampedArray", "prototype"],
      "%Uint16ArrayPrototype%": ["Uint16Array", "prototype"],
      "%Uint32ArrayPrototype%": ["Uint32Array", "prototype"],
      "%URIErrorPrototype%": ["URIError", "prototype"],
      "%WeakMapPrototype%": ["WeakMap", "prototype"],
      "%WeakSetPrototype%": ["WeakSet", "prototype"]
    };
    var bind = require_function_bind();
    var hasOwn = require_hasown();
    var $concat = bind.call(Function.call, Array.prototype.concat);
    var $spliceApply = bind.call(Function.apply, Array.prototype.splice);
    var $replace = bind.call(Function.call, String.prototype.replace);
    var $strSlice = bind.call(Function.call, String.prototype.slice);
    var $exec = bind.call(Function.call, RegExp.prototype.exec);
    var rePropName = /[^%.[\]]+|\[(?:(-?\d+(?:\.\d+)?)|(["'])((?:(?!\2)[^\\]|\\.)*?)\2)\]|(?=(?:\.|\[\])(?:\.|\[\]|%$))/g;
    var reEscapeChar = /\\(\\)?/g;
    var stringToPath = function stringToPath2(string) {
      var first = $strSlice(string, 0, 1);
      var last = $strSlice(string, -1);
      if (first === "%" && last !== "%") {
        throw new $SyntaxError("invalid intrinsic syntax, expected closing `%`");
      } else if (last === "%" && first !== "%") {
        throw new $SyntaxError("invalid intrinsic syntax, expected opening `%`");
      }
      var result = [];
      $replace(string, rePropName, function(match, number, quote, subString) {
        result[result.length] = quote ? $replace(subString, reEscapeChar, "$1") : number || match;
      });
      return result;
    };
    var getBaseIntrinsic = function getBaseIntrinsic2(name, allowMissing) {
      var intrinsicName = name;
      var alias;
      if (hasOwn(LEGACY_ALIASES, intrinsicName)) {
        alias = LEGACY_ALIASES[intrinsicName];
        intrinsicName = "%" + alias[0] + "%";
      }
      if (hasOwn(INTRINSICS, intrinsicName)) {
        var value = INTRINSICS[intrinsicName];
        if (value === needsEval) {
          value = doEval(intrinsicName);
        }
        if (typeof value === "undefined" && !allowMissing) {
          throw new $TypeError("intrinsic " + name + " exists, but is not available. Please file an issue!");
        }
        return {
          alias,
          name: intrinsicName,
          value
        };
      }
      throw new $SyntaxError("intrinsic " + name + " does not exist!");
    };
    module.exports = function GetIntrinsic(name, allowMissing) {
      if (typeof name !== "string" || name.length === 0) {
        throw new $TypeError("intrinsic name must be a non-empty string");
      }
      if (arguments.length > 1 && typeof allowMissing !== "boolean") {
        throw new $TypeError('"allowMissing" argument must be a boolean');
      }
      if ($exec(/^%?[^%]*%?$/, name) === null) {
        throw new $SyntaxError("`%` may not be present anywhere but at the beginning and end of the intrinsic name");
      }
      var parts = stringToPath(name);
      var intrinsicBaseName = parts.length > 0 ? parts[0] : "";
      var intrinsic = getBaseIntrinsic("%" + intrinsicBaseName + "%", allowMissing);
      var intrinsicRealName = intrinsic.name;
      var value = intrinsic.value;
      var skipFurtherCaching = false;
      var alias = intrinsic.alias;
      if (alias) {
        intrinsicBaseName = alias[0];
        $spliceApply(parts, $concat([0, 1], alias));
      }
      for (var i = 1, isOwn = true; i < parts.length; i += 1) {
        var part = parts[i];
        var first = $strSlice(part, 0, 1);
        var last = $strSlice(part, -1);
        if ((first === '"' || first === "'" || first === "`" || (last === '"' || last === "'" || last === "`")) && first !== last) {
          throw new $SyntaxError("property names with quotes must have matching quotes");
        }
        if (part === "constructor" || !isOwn) {
          skipFurtherCaching = true;
        }
        intrinsicBaseName += "." + part;
        intrinsicRealName = "%" + intrinsicBaseName + "%";
        if (hasOwn(INTRINSICS, intrinsicRealName)) {
          value = INTRINSICS[intrinsicRealName];
        } else if (value != null) {
          if (!(part in value)) {
            if (!allowMissing) {
              throw new $TypeError("base intrinsic for " + name + " exists, but the property is not available.");
            }
            return void undefined2;
          }
          if ($gOPD && i + 1 >= parts.length) {
            var desc = $gOPD(value, part);
            isOwn = !!desc;
            if (isOwn && "get" in desc && !("originalValue" in desc.get)) {
              value = desc.get;
            } else {
              value = value[part];
            }
          } else {
            isOwn = hasOwn(value, part);
            value = value[part];
          }
          if (isOwn && !skipFurtherCaching) {
            INTRINSICS[intrinsicRealName] = value;
          }
        }
      }
      return value;
    };
  }
});

// node_modules/es-define-property/index.js
var require_es_define_property = __commonJS({
  "node_modules/es-define-property/index.js"(exports, module) {
    "use strict";
    var GetIntrinsic = require_get_intrinsic();
    var $defineProperty = GetIntrinsic("%Object.defineProperty%", true) || false;
    if ($defineProperty) {
      try {
        $defineProperty({}, "a", { value: 1 });
      } catch (e) {
        $defineProperty = false;
      }
    }
    module.exports = $defineProperty;
  }
});

// node_modules/gopd/index.js
var require_gopd = __commonJS({
  "node_modules/gopd/index.js"(exports, module) {
    "use strict";
    var GetIntrinsic = require_get_intrinsic();
    var $gOPD = GetIntrinsic("%Object.getOwnPropertyDescriptor%", true);
    if ($gOPD) {
      try {
        $gOPD([], "length");
      } catch (e) {
        $gOPD = null;
      }
    }
    module.exports = $gOPD;
  }
});

// node_modules/define-data-property/index.js
var require_define_data_property = __commonJS({
  "node_modules/define-data-property/index.js"(exports, module) {
    "use strict";
    var $defineProperty = require_es_define_property();
    var $SyntaxError = require_syntax();
    var $TypeError = require_type();
    var gopd = require_gopd();
    module.exports = function defineDataProperty(obj, property, value) {
      if (!obj || typeof obj !== "object" && typeof obj !== "function") {
        throw new $TypeError("`obj` must be an object or a function`");
      }
      if (typeof property !== "string" && typeof property !== "symbol") {
        throw new $TypeError("`property` must be a string or a symbol`");
      }
      if (arguments.length > 3 && typeof arguments[3] !== "boolean" && arguments[3] !== null) {
        throw new $TypeError("`nonEnumerable`, if provided, must be a boolean or null");
      }
      if (arguments.length > 4 && typeof arguments[4] !== "boolean" && arguments[4] !== null) {
        throw new $TypeError("`nonWritable`, if provided, must be a boolean or null");
      }
      if (arguments.length > 5 && typeof arguments[5] !== "boolean" && arguments[5] !== null) {
        throw new $TypeError("`nonConfigurable`, if provided, must be a boolean or null");
      }
      if (arguments.length > 6 && typeof arguments[6] !== "boolean") {
        throw new $TypeError("`loose`, if provided, must be a boolean");
      }
      var nonEnumerable = arguments.length > 3 ? arguments[3] : null;
      var nonWritable = arguments.length > 4 ? arguments[4] : null;
      var nonConfigurable = arguments.length > 5 ? arguments[5] : null;
      var loose = arguments.length > 6 ? arguments[6] : false;
      var desc = !!gopd && gopd(obj, property);
      if ($defineProperty) {
        $defineProperty(obj, property, {
          configurable: nonConfigurable === null && desc ? desc.configurable : !nonConfigurable,
          enumerable: nonEnumerable === null && desc ? desc.enumerable : !nonEnumerable,
          value,
          writable: nonWritable === null && desc ? desc.writable : !nonWritable
        });
      } else if (loose || !nonEnumerable && !nonWritable && !nonConfigurable) {
        obj[property] = value;
      } else {
        throw new $SyntaxError("This environment does not support defining a property as non-configurable, non-writable, or non-enumerable.");
      }
    };
  }
});

// node_modules/has-property-descriptors/index.js
var require_has_property_descriptors = __commonJS({
  "node_modules/has-property-descriptors/index.js"(exports, module) {
    "use strict";
    var $defineProperty = require_es_define_property();
    var hasPropertyDescriptors = function hasPropertyDescriptors2() {
      return !!$defineProperty;
    };
    hasPropertyDescriptors.hasArrayLengthDefineBug = function hasArrayLengthDefineBug() {
      if (!$defineProperty) {
        return null;
      }
      try {
        return $defineProperty([], "length", { value: 1 }).length !== 1;
      } catch (e) {
        return true;
      }
    };
    module.exports = hasPropertyDescriptors;
  }
});

// node_modules/set-function-length/index.js
var require_set_function_length = __commonJS({
  "node_modules/set-function-length/index.js"(exports, module) {
    "use strict";
    var GetIntrinsic = require_get_intrinsic();
    var define2 = require_define_data_property();
    var hasDescriptors = require_has_property_descriptors()();
    var gOPD = require_gopd();
    var $TypeError = require_type();
    var $floor = GetIntrinsic("%Math.floor%");
    module.exports = function setFunctionLength(fn, length) {
      if (typeof fn !== "function") {
        throw new $TypeError("`fn` is not a function");
      }
      if (typeof length !== "number" || length < 0 || length > 4294967295 || $floor(length) !== length) {
        throw new $TypeError("`length` must be a positive 32-bit integer");
      }
      var loose = arguments.length > 2 && !!arguments[2];
      var functionLengthIsConfigurable = true;
      var functionLengthIsWritable = true;
      if ("length" in fn && gOPD) {
        var desc = gOPD(fn, "length");
        if (desc && !desc.configurable) {
          functionLengthIsConfigurable = false;
        }
        if (desc && !desc.writable) {
          functionLengthIsWritable = false;
        }
      }
      if (functionLengthIsConfigurable || functionLengthIsWritable || !loose) {
        if (hasDescriptors) {
          define2(
            /** @type {Parameters<define>[0]} */
            fn,
            "length",
            length,
            true,
            true
          );
        } else {
          define2(
            /** @type {Parameters<define>[0]} */
            fn,
            "length",
            length
          );
        }
      }
      return fn;
    };
  }
});

// node_modules/call-bind/index.js
var require_call_bind = __commonJS({
  "node_modules/call-bind/index.js"(exports, module) {
    "use strict";
    var bind = require_function_bind();
    var GetIntrinsic = require_get_intrinsic();
    var setFunctionLength = require_set_function_length();
    var $TypeError = require_type();
    var $apply = GetIntrinsic("%Function.prototype.apply%");
    var $call = GetIntrinsic("%Function.prototype.call%");
    var $reflectApply = GetIntrinsic("%Reflect.apply%", true) || bind.call($call, $apply);
    var $defineProperty = require_es_define_property();
    var $max = GetIntrinsic("%Math.max%");
    module.exports = function callBind(originalFunction) {
      if (typeof originalFunction !== "function") {
        throw new $TypeError("a function is required");
      }
      var func = $reflectApply(bind, $call, arguments);
      return setFunctionLength(
        func,
        1 + $max(0, originalFunction.length - (arguments.length - 1)),
        true
      );
    };
    var applyBind = function applyBind2() {
      return $reflectApply(bind, $apply, arguments);
    };
    if ($defineProperty) {
      $defineProperty(module.exports, "apply", { value: applyBind });
    } else {
      module.exports.apply = applyBind;
    }
  }
});

// node_modules/call-bind/callBound.js
var require_callBound = __commonJS({
  "node_modules/call-bind/callBound.js"(exports, module) {
    "use strict";
    var GetIntrinsic = require_get_intrinsic();
    var callBind = require_call_bind();
    var $indexOf = callBind(GetIntrinsic("String.prototype.indexOf"));
    module.exports = function callBoundIntrinsic(name, allowMissing) {
      var intrinsic = GetIntrinsic(name, !!allowMissing);
      if (typeof intrinsic === "function" && $indexOf(name, ".prototype.") > -1) {
        return callBind(intrinsic);
      }
      return intrinsic;
    };
  }
});

// node_modules/is-arguments/index.js
var require_is_arguments2 = __commonJS({
  "node_modules/is-arguments/index.js"(exports, module) {
    "use strict";
    var hasToStringTag = require_shams2()();
    var callBound = require_callBound();
    var $toString = callBound("Object.prototype.toString");
    var isStandardArguments = function isArguments(value) {
      if (hasToStringTag && value && typeof value === "object" && Symbol.toStringTag in value) {
        return false;
      }
      return $toString(value) === "[object Arguments]";
    };
    var isLegacyArguments = function isArguments(value) {
      if (isStandardArguments(value)) {
        return true;
      }
      return value !== null && typeof value === "object" && typeof value.length === "number" && value.length >= 0 && $toString(value) !== "[object Array]" && $toString(value.callee) === "[object Function]";
    };
    var supportsStandardArguments = (function() {
      return isStandardArguments(arguments);
    })();
    isStandardArguments.isLegacyArguments = isLegacyArguments;
    module.exports = supportsStandardArguments ? isStandardArguments : isLegacyArguments;
  }
});

// node_modules/is-generator-function/index.js
var require_is_generator_function = __commonJS({
  "node_modules/is-generator-function/index.js"(exports, module) {
    "use strict";
    var toStr = Object.prototype.toString;
    var fnToStr = Function.prototype.toString;
    var isFnRegex = /^\s*(?:function)?\*/;
    var hasToStringTag = require_shams2()();
    var getProto = Object.getPrototypeOf;
    var getGeneratorFunc = function() {
      if (!hasToStringTag) {
        return false;
      }
      try {
        return Function("return function*() {}")();
      } catch (e) {
      }
    };
    var GeneratorFunction;
    module.exports = function isGeneratorFunction(fn) {
      if (typeof fn !== "function") {
        return false;
      }
      if (isFnRegex.test(fnToStr.call(fn))) {
        return true;
      }
      if (!hasToStringTag) {
        var str = toStr.call(fn);
        return str === "[object GeneratorFunction]";
      }
      if (!getProto) {
        return false;
      }
      if (typeof GeneratorFunction === "undefined") {
        var generatorFunc = getGeneratorFunc();
        GeneratorFunction = generatorFunc ? getProto(generatorFunc) : false;
      }
      return getProto(fn) === GeneratorFunction;
    };
  }
});

// node_modules/is-callable/index.js
var require_is_callable = __commonJS({
  "node_modules/is-callable/index.js"(exports, module) {
    "use strict";
    var fnToStr = Function.prototype.toString;
    var reflectApply = typeof Reflect === "object" && Reflect !== null && Reflect.apply;
    var badArrayLike;
    var isCallableMarker;
    if (typeof reflectApply === "function" && typeof Object.defineProperty === "function") {
      try {
        badArrayLike = Object.defineProperty({}, "length", {
          get: function() {
            throw isCallableMarker;
          }
        });
        isCallableMarker = {};
        reflectApply(function() {
          throw 42;
        }, null, badArrayLike);
      } catch (_) {
        if (_ !== isCallableMarker) {
          reflectApply = null;
        }
      }
    } else {
      reflectApply = null;
    }
    var constructorRegex = /^\s*class\b/;
    var isES6ClassFn = function isES6ClassFunction(value) {
      try {
        var fnStr = fnToStr.call(value);
        return constructorRegex.test(fnStr);
      } catch (e) {
        return false;
      }
    };
    var tryFunctionObject = function tryFunctionToStr(value) {
      try {
        if (isES6ClassFn(value)) {
          return false;
        }
        fnToStr.call(value);
        return true;
      } catch (e) {
        return false;
      }
    };
    var toStr = Object.prototype.toString;
    var objectClass = "[object Object]";
    var fnClass = "[object Function]";
    var genClass = "[object GeneratorFunction]";
    var ddaClass = "[object HTMLAllCollection]";
    var ddaClass2 = "[object HTML document.all class]";
    var ddaClass3 = "[object HTMLCollection]";
    var hasToStringTag = typeof Symbol === "function" && !!Symbol.toStringTag;
    var isIE68 = !(0 in [,]);
    var isDDA = function isDocumentDotAll() {
      return false;
    };
    if (typeof document === "object") {
      all = document.all;
      if (toStr.call(all) === toStr.call(document.all)) {
        isDDA = function isDocumentDotAll(value) {
          if ((isIE68 || !value) && (typeof value === "undefined" || typeof value === "object")) {
            try {
              var str = toStr.call(value);
              return (str === ddaClass || str === ddaClass2 || str === ddaClass3 || str === objectClass) && value("") == null;
            } catch (e) {
            }
          }
          return false;
        };
      }
    }
    var all;
    module.exports = reflectApply ? function isCallable(value) {
      if (isDDA(value)) {
        return true;
      }
      if (!value) {
        return false;
      }
      if (typeof value !== "function" && typeof value !== "object") {
        return false;
      }
      try {
        reflectApply(value, null, badArrayLike);
      } catch (e) {
        if (e !== isCallableMarker) {
          return false;
        }
      }
      return !isES6ClassFn(value) && tryFunctionObject(value);
    } : function isCallable(value) {
      if (isDDA(value)) {
        return true;
      }
      if (!value) {
        return false;
      }
      if (typeof value !== "function" && typeof value !== "object") {
        return false;
      }
      if (hasToStringTag) {
        return tryFunctionObject(value);
      }
      if (isES6ClassFn(value)) {
        return false;
      }
      var strClass = toStr.call(value);
      if (strClass !== fnClass && strClass !== genClass && !/^\[object HTML/.test(strClass)) {
        return false;
      }
      return tryFunctionObject(value);
    };
  }
});

// node_modules/for-each/index.js
var require_for_each = __commonJS({
  "node_modules/for-each/index.js"(exports, module) {
    "use strict";
    var isCallable = require_is_callable();
    var toStr = Object.prototype.toString;
    var hasOwnProperty = Object.prototype.hasOwnProperty;
    var forEachArray = function forEachArray2(array, iterator, receiver) {
      for (var i = 0, len = array.length; i < len; i++) {
        if (hasOwnProperty.call(array, i)) {
          if (receiver == null) {
            iterator(array[i], i, array);
          } else {
            iterator.call(receiver, array[i], i, array);
          }
        }
      }
    };
    var forEachString = function forEachString2(string, iterator, receiver) {
      for (var i = 0, len = string.length; i < len; i++) {
        if (receiver == null) {
          iterator(string.charAt(i), i, string);
        } else {
          iterator.call(receiver, string.charAt(i), i, string);
        }
      }
    };
    var forEachObject = function forEachObject2(object, iterator, receiver) {
      for (var k in object) {
        if (hasOwnProperty.call(object, k)) {
          if (receiver == null) {
            iterator(object[k], k, object);
          } else {
            iterator.call(receiver, object[k], k, object);
          }
        }
      }
    };
    var forEach = function forEach2(list, iterator, thisArg) {
      if (!isCallable(iterator)) {
        throw new TypeError("iterator must be a function");
      }
      var receiver;
      if (arguments.length >= 3) {
        receiver = thisArg;
      }
      if (toStr.call(list) === "[object Array]") {
        forEachArray(list, iterator, receiver);
      } else if (typeof list === "string") {
        forEachString(list, iterator, receiver);
      } else {
        forEachObject(list, iterator, receiver);
      }
    };
    module.exports = forEach;
  }
});

// node_modules/possible-typed-array-names/index.js
var require_possible_typed_array_names = __commonJS({
  "node_modules/possible-typed-array-names/index.js"(exports, module) {
    "use strict";
    module.exports = [
      "Float32Array",
      "Float64Array",
      "Int8Array",
      "Int16Array",
      "Int32Array",
      "Uint8Array",
      "Uint8ClampedArray",
      "Uint16Array",
      "Uint32Array",
      "BigInt64Array",
      "BigUint64Array"
    ];
  }
});

// node_modules/available-typed-arrays/index.js
var require_available_typed_arrays = __commonJS({
  "node_modules/available-typed-arrays/index.js"(exports, module) {
    "use strict";
    var possibleNames = require_possible_typed_array_names();
    var g = typeof globalThis === "undefined" ? global : globalThis;
    module.exports = function availableTypedArrays() {
      var out = [];
      for (var i = 0; i < possibleNames.length; i++) {
        if (typeof g[possibleNames[i]] === "function") {
          out[out.length] = possibleNames[i];
        }
      }
      return out;
    };
  }
});

// node_modules/which-typed-array/index.js
var require_which_typed_array = __commonJS({
  "node_modules/which-typed-array/index.js"(exports, module) {
    "use strict";
    var forEach = require_for_each();
    var availableTypedArrays = require_available_typed_arrays();
    var callBind = require_call_bind();
    var callBound = require_callBound();
    var gOPD = require_gopd();
    var $toString = callBound("Object.prototype.toString");
    var hasToStringTag = require_shams2()();
    var g = typeof globalThis === "undefined" ? global : globalThis;
    var typedArrays = availableTypedArrays();
    var $slice = callBound("String.prototype.slice");
    var getPrototypeOf = Object.getPrototypeOf;
    var $indexOf = callBound("Array.prototype.indexOf", true) || function indexOf(array, value) {
      for (var i = 0; i < array.length; i += 1) {
        if (array[i] === value) {
          return i;
        }
      }
      return -1;
    };
    var cache = { __proto__: null };
    if (hasToStringTag && gOPD && getPrototypeOf) {
      forEach(typedArrays, function(typedArray) {
        var arr = new g[typedArray]();
        if (Symbol.toStringTag in arr) {
          var proto = getPrototypeOf(arr);
          var descriptor = gOPD(proto, Symbol.toStringTag);
          if (!descriptor) {
            var superProto = getPrototypeOf(proto);
            descriptor = gOPD(superProto, Symbol.toStringTag);
          }
          cache["$" + typedArray] = callBind(descriptor.get);
        }
      });
    } else {
      forEach(typedArrays, function(typedArray) {
        var arr = new g[typedArray]();
        var fn = arr.slice || arr.set;
        if (fn) {
          cache["$" + typedArray] = callBind(fn);
        }
      });
    }
    var tryTypedArrays = function tryAllTypedArrays(value) {
      var found = false;
      forEach(
        // eslint-disable-next-line no-extra-parens
        /** @type {Record<`\$${TypedArrayName}`, Getter>} */
        /** @type {any} */
        cache,
        /** @type {(getter: Getter, name: `\$${import('.').TypedArrayName}`) => void} */
        function(getter, typedArray) {
          if (!found) {
            try {
              if ("$" + getter(value) === typedArray) {
                found = $slice(typedArray, 1);
              }
            } catch (e) {
            }
          }
        }
      );
      return found;
    };
    var trySlices = function tryAllSlices(value) {
      var found = false;
      forEach(
        // eslint-disable-next-line no-extra-parens
        /** @type {Record<`\$${TypedArrayName}`, Getter>} */
        /** @type {any} */
        cache,
        /** @type {(getter: typeof cache, name: `\$${import('.').TypedArrayName}`) => void} */
        function(getter, name) {
          if (!found) {
            try {
              getter(value);
              found = $slice(name, 1);
            } catch (e) {
            }
          }
        }
      );
      return found;
    };
    module.exports = function whichTypedArray(value) {
      if (!value || typeof value !== "object") {
        return false;
      }
      if (!hasToStringTag) {
        var tag = $slice($toString(value), 8, -1);
        if ($indexOf(typedArrays, tag) > -1) {
          return tag;
        }
        if (tag !== "Object") {
          return false;
        }
        return trySlices(value);
      }
      if (!gOPD) {
        return null;
      }
      return tryTypedArrays(value);
    };
  }
});

// node_modules/is-typed-array/index.js
var require_is_typed_array = __commonJS({
  "node_modules/is-typed-array/index.js"(exports, module) {
    "use strict";
    var whichTypedArray = require_which_typed_array();
    module.exports = function isTypedArray(value) {
      return !!whichTypedArray(value);
    };
  }
});

// node_modules/util/support/types.js
var require_types = __commonJS({
  "node_modules/util/support/types.js"(exports) {
    "use strict";
    var isArgumentsObject = require_is_arguments2();
    var isGeneratorFunction = require_is_generator_function();
    var whichTypedArray = require_which_typed_array();
    var isTypedArray = require_is_typed_array();
    function uncurryThis(f) {
      return f.call.bind(f);
    }
    var BigIntSupported = typeof BigInt !== "undefined";
    var SymbolSupported = typeof Symbol !== "undefined";
    var ObjectToString = uncurryThis(Object.prototype.toString);
    var numberValue = uncurryThis(Number.prototype.valueOf);
    var stringValue = uncurryThis(String.prototype.valueOf);
    var booleanValue = uncurryThis(Boolean.prototype.valueOf);
    if (BigIntSupported) {
      bigIntValue = uncurryThis(BigInt.prototype.valueOf);
    }
    var bigIntValue;
    if (SymbolSupported) {
      symbolValue = uncurryThis(Symbol.prototype.valueOf);
    }
    var symbolValue;
    function checkBoxedPrimitive(value, prototypeValueOf) {
      if (typeof value !== "object") {
        return false;
      }
      try {
        prototypeValueOf(value);
        return true;
      } catch (e) {
        return false;
      }
    }
    exports.isArgumentsObject = isArgumentsObject;
    exports.isGeneratorFunction = isGeneratorFunction;
    exports.isTypedArray = isTypedArray;
    function isPromise(input) {
      return typeof Promise !== "undefined" && input instanceof Promise || input !== null && typeof input === "object" && typeof input.then === "function" && typeof input.catch === "function";
    }
    exports.isPromise = isPromise;
    function isArrayBufferView(value) {
      if (typeof ArrayBuffer !== "undefined" && ArrayBuffer.isView) {
        return ArrayBuffer.isView(value);
      }
      return isTypedArray(value) || isDataView(value);
    }
    exports.isArrayBufferView = isArrayBufferView;
    function isUint8Array(value) {
      return whichTypedArray(value) === "Uint8Array";
    }
    exports.isUint8Array = isUint8Array;
    function isUint8ClampedArray(value) {
      return whichTypedArray(value) === "Uint8ClampedArray";
    }
    exports.isUint8ClampedArray = isUint8ClampedArray;
    function isUint16Array(value) {
      return whichTypedArray(value) === "Uint16Array";
    }
    exports.isUint16Array = isUint16Array;
    function isUint32Array(value) {
      return whichTypedArray(value) === "Uint32Array";
    }
    exports.isUint32Array = isUint32Array;
    function isInt8Array(value) {
      return whichTypedArray(value) === "Int8Array";
    }
    exports.isInt8Array = isInt8Array;
    function isInt16Array(value) {
      return whichTypedArray(value) === "Int16Array";
    }
    exports.isInt16Array = isInt16Array;
    function isInt32Array(value) {
      return whichTypedArray(value) === "Int32Array";
    }
    exports.isInt32Array = isInt32Array;
    function isFloat32Array(value) {
      return whichTypedArray(value) === "Float32Array";
    }
    exports.isFloat32Array = isFloat32Array;
    function isFloat64Array(value) {
      return whichTypedArray(value) === "Float64Array";
    }
    exports.isFloat64Array = isFloat64Array;
    function isBigInt64Array(value) {
      return whichTypedArray(value) === "BigInt64Array";
    }
    exports.isBigInt64Array = isBigInt64Array;
    function isBigUint64Array(value) {
      return whichTypedArray(value) === "BigUint64Array";
    }
    exports.isBigUint64Array = isBigUint64Array;
    function isMapToString(value) {
      return ObjectToString(value) === "[object Map]";
    }
    isMapToString.working = typeof Map !== "undefined" && isMapToString(/* @__PURE__ */ new Map());
    function isMap(value) {
      if (typeof Map === "undefined") {
        return false;
      }
      return isMapToString.working ? isMapToString(value) : value instanceof Map;
    }
    exports.isMap = isMap;
    function isSetToString(value) {
      return ObjectToString(value) === "[object Set]";
    }
    isSetToString.working = typeof Set !== "undefined" && isSetToString(/* @__PURE__ */ new Set());
    function isSet(value) {
      if (typeof Set === "undefined") {
        return false;
      }
      return isSetToString.working ? isSetToString(value) : value instanceof Set;
    }
    exports.isSet = isSet;
    function isWeakMapToString(value) {
      return ObjectToString(value) === "[object WeakMap]";
    }
    isWeakMapToString.working = typeof WeakMap !== "undefined" && isWeakMapToString(/* @__PURE__ */ new WeakMap());
    function isWeakMap(value) {
      if (typeof WeakMap === "undefined") {
        return false;
      }
      return isWeakMapToString.working ? isWeakMapToString(value) : value instanceof WeakMap;
    }
    exports.isWeakMap = isWeakMap;
    function isWeakSetToString(value) {
      return ObjectToString(value) === "[object WeakSet]";
    }
    isWeakSetToString.working = typeof WeakSet !== "undefined" && isWeakSetToString(/* @__PURE__ */ new WeakSet());
    function isWeakSet(value) {
      return isWeakSetToString(value);
    }
    exports.isWeakSet = isWeakSet;
    function isArrayBufferToString(value) {
      return ObjectToString(value) === "[object ArrayBuffer]";
    }
    isArrayBufferToString.working = typeof ArrayBuffer !== "undefined" && isArrayBufferToString(new ArrayBuffer());
    function isArrayBuffer(value) {
      if (typeof ArrayBuffer === "undefined") {
        return false;
      }
      return isArrayBufferToString.working ? isArrayBufferToString(value) : value instanceof ArrayBuffer;
    }
    exports.isArrayBuffer = isArrayBuffer;
    function isDataViewToString(value) {
      return ObjectToString(value) === "[object DataView]";
    }
    isDataViewToString.working = typeof ArrayBuffer !== "undefined" && typeof DataView !== "undefined" && isDataViewToString(new DataView(new ArrayBuffer(1), 0, 1));
    function isDataView(value) {
      if (typeof DataView === "undefined") {
        return false;
      }
      return isDataViewToString.working ? isDataViewToString(value) : value instanceof DataView;
    }
    exports.isDataView = isDataView;
    var SharedArrayBufferCopy = typeof SharedArrayBuffer !== "undefined" ? SharedArrayBuffer : void 0;
    function isSharedArrayBufferToString(value) {
      return ObjectToString(value) === "[object SharedArrayBuffer]";
    }
    function isSharedArrayBuffer(value) {
      if (typeof SharedArrayBufferCopy === "undefined") {
        return false;
      }
      if (typeof isSharedArrayBufferToString.working === "undefined") {
        isSharedArrayBufferToString.working = isSharedArrayBufferToString(new SharedArrayBufferCopy());
      }
      return isSharedArrayBufferToString.working ? isSharedArrayBufferToString(value) : value instanceof SharedArrayBufferCopy;
    }
    exports.isSharedArrayBuffer = isSharedArrayBuffer;
    function isAsyncFunction(value) {
      return ObjectToString(value) === "[object AsyncFunction]";
    }
    exports.isAsyncFunction = isAsyncFunction;
    function isMapIterator(value) {
      return ObjectToString(value) === "[object Map Iterator]";
    }
    exports.isMapIterator = isMapIterator;
    function isSetIterator(value) {
      return ObjectToString(value) === "[object Set Iterator]";
    }
    exports.isSetIterator = isSetIterator;
    function isGeneratorObject(value) {
      return ObjectToString(value) === "[object Generator]";
    }
    exports.isGeneratorObject = isGeneratorObject;
    function isWebAssemblyCompiledModule(value) {
      return ObjectToString(value) === "[object WebAssembly.Module]";
    }
    exports.isWebAssemblyCompiledModule = isWebAssemblyCompiledModule;
    function isNumberObject(value) {
      return checkBoxedPrimitive(value, numberValue);
    }
    exports.isNumberObject = isNumberObject;
    function isStringObject(value) {
      return checkBoxedPrimitive(value, stringValue);
    }
    exports.isStringObject = isStringObject;
    function isBooleanObject(value) {
      return checkBoxedPrimitive(value, booleanValue);
    }
    exports.isBooleanObject = isBooleanObject;
    function isBigIntObject(value) {
      return BigIntSupported && checkBoxedPrimitive(value, bigIntValue);
    }
    exports.isBigIntObject = isBigIntObject;
    function isSymbolObject(value) {
      return SymbolSupported && checkBoxedPrimitive(value, symbolValue);
    }
    exports.isSymbolObject = isSymbolObject;
    function isBoxedPrimitive(value) {
      return isNumberObject(value) || isStringObject(value) || isBooleanObject(value) || isBigIntObject(value) || isSymbolObject(value);
    }
    exports.isBoxedPrimitive = isBoxedPrimitive;
    function isAnyArrayBuffer(value) {
      return typeof Uint8Array !== "undefined" && (isArrayBuffer(value) || isSharedArrayBuffer(value));
    }
    exports.isAnyArrayBuffer = isAnyArrayBuffer;
    ["isProxy", "isExternal", "isModuleNamespaceObject"].forEach(function(method) {
      Object.defineProperty(exports, method, {
        enumerable: false,
        value: function() {
          throw new Error(method + " is not supported in userland");
        }
      });
    });
  }
});

// node_modules/util/support/isBufferBrowser.js
var require_isBufferBrowser = __commonJS({
  "node_modules/util/support/isBufferBrowser.js"(exports, module) {
    module.exports = function isBuffer(arg) {
      return arg && typeof arg === "object" && typeof arg.copy === "function" && typeof arg.fill === "function" && typeof arg.readUInt8 === "function";
    };
  }
});

// node_modules/inherits/inherits_browser.js
var require_inherits_browser = __commonJS({
  "node_modules/inherits/inherits_browser.js"(exports, module) {
    if (typeof Object.create === "function") {
      module.exports = function inherits(ctor, superCtor) {
        if (superCtor) {
          ctor.super_ = superCtor;
          ctor.prototype = Object.create(superCtor.prototype, {
            constructor: {
              value: ctor,
              enumerable: false,
              writable: true,
              configurable: true
            }
          });
        }
      };
    } else {
      module.exports = function inherits(ctor, superCtor) {
        if (superCtor) {
          ctor.super_ = superCtor;
          var TempCtor = function() {
          };
          TempCtor.prototype = superCtor.prototype;
          ctor.prototype = new TempCtor();
          ctor.prototype.constructor = ctor;
        }
      };
    }
  }
});

// node_modules/util/util.js
var require_util = __commonJS({
  "node_modules/util/util.js"(exports) {
    var getOwnPropertyDescriptors = Object.getOwnPropertyDescriptors || function getOwnPropertyDescriptors2(obj) {
      var keys = Object.keys(obj);
      var descriptors = {};
      for (var i = 0; i < keys.length; i++) {
        descriptors[keys[i]] = Object.getOwnPropertyDescriptor(obj, keys[i]);
      }
      return descriptors;
    };
    var formatRegExp = /%[sdj%]/g;
    exports.format = function(f) {
      if (!isString(f)) {
        var objects = [];
        for (var i = 0; i < arguments.length; i++) {
          objects.push(inspect(arguments[i]));
        }
        return objects.join(" ");
      }
      var i = 1;
      var args = arguments;
      var len = args.length;
      var str = String(f).replace(formatRegExp, function(x2) {
        if (x2 === "%%") return "%";
        if (i >= len) return x2;
        switch (x2) {
          case "%s":
            return String(args[i++]);
          case "%d":
            return Number(args[i++]);
          case "%j":
            try {
              return JSON.stringify(args[i++]);
            } catch (_) {
              return "[Circular]";
            }
          default:
            return x2;
        }
      });
      for (var x = args[i]; i < len; x = args[++i]) {
        if (isNull(x) || !isObject(x)) {
          str += " " + x;
        } else {
          str += " " + inspect(x);
        }
      }
      return str;
    };
    exports.deprecate = function(fn, msg) {
      if (typeof process !== "undefined" && process.noDeprecation === true) {
        return fn;
      }
      if (typeof process === "undefined") {
        return function() {
          return exports.deprecate(fn, msg).apply(this, arguments);
        };
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
    var debugEnvRegex = /^$/;
    if ("") {
      debugEnv = "";
      debugEnv = debugEnv.replace(/[|\\{}()[\]^$+?.]/g, "\\$&").replace(/\*/g, ".*").replace(/,/g, "$|^").toUpperCase();
      debugEnvRegex = new RegExp("^" + debugEnv + "$", "i");
    }
    var debugEnv;
    exports.debuglog = function(set) {
      set = set.toUpperCase();
      if (!debugs[set]) {
        if (debugEnvRegex.test(set)) {
          var pid = process.pid;
          debugs[set] = function() {
            var msg = exports.format.apply(exports, arguments);
            console.error("%s %d: %s", set, pid, msg);
          };
        } else {
          debugs[set] = function() {
          };
        }
      }
      return debugs[set];
    };
    function inspect(obj, opts) {
      var ctx = {
        seen: [],
        stylize: stylizeNoColor
      };
      if (arguments.length >= 3) ctx.depth = arguments[2];
      if (arguments.length >= 4) ctx.colors = arguments[3];
      if (isBoolean(opts)) {
        ctx.showHidden = opts;
      } else if (opts) {
        exports._extend(ctx, opts);
      }
      if (isUndefined(ctx.showHidden)) ctx.showHidden = false;
      if (isUndefined(ctx.depth)) ctx.depth = 2;
      if (isUndefined(ctx.colors)) ctx.colors = false;
      if (isUndefined(ctx.customInspect)) ctx.customInspect = true;
      if (ctx.colors) ctx.stylize = stylizeWithColor;
      return formatValue(ctx, obj, ctx.depth);
    }
    exports.inspect = inspect;
    inspect.colors = {
      "bold": [1, 22],
      "italic": [3, 23],
      "underline": [4, 24],
      "inverse": [7, 27],
      "white": [37, 39],
      "grey": [90, 39],
      "black": [30, 39],
      "blue": [34, 39],
      "cyan": [36, 39],
      "green": [32, 39],
      "magenta": [35, 39],
      "red": [31, 39],
      "yellow": [33, 39]
    };
    inspect.styles = {
      "special": "cyan",
      "number": "yellow",
      "boolean": "yellow",
      "undefined": "grey",
      "null": "bold",
      "string": "green",
      "date": "magenta",
      // "name": intentionally not styling
      "regexp": "red"
    };
    function stylizeWithColor(str, styleType) {
      var style = inspect.styles[styleType];
      if (style) {
        return "\x1B[" + inspect.colors[style][0] + "m" + str + "\x1B[" + inspect.colors[style][1] + "m";
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
      if (ctx.customInspect && value && isFunction(value.inspect) && // Filter out the util module, it's inspect function is special
      value.inspect !== exports.inspect && // Also filter out any prototype objects using the circular check.
      !(value.constructor && value.constructor.prototype === value)) {
        var ret = value.inspect(recurseTimes, ctx);
        if (!isString(ret)) {
          ret = formatValue(ctx, ret, recurseTimes);
        }
        return ret;
      }
      var primitive = formatPrimitive(ctx, value);
      if (primitive) {
        return primitive;
      }
      var keys = Object.keys(value);
      var visibleKeys = arrayToHash(keys);
      if (ctx.showHidden) {
        keys = Object.getOwnPropertyNames(value);
      }
      if (isError(value) && (keys.indexOf("message") >= 0 || keys.indexOf("description") >= 0)) {
        return formatError(value);
      }
      if (keys.length === 0) {
        if (isFunction(value)) {
          var name = value.name ? ": " + value.name : "";
          return ctx.stylize("[Function" + name + "]", "special");
        }
        if (isRegExp(value)) {
          return ctx.stylize(RegExp.prototype.toString.call(value), "regexp");
        }
        if (isDate(value)) {
          return ctx.stylize(Date.prototype.toString.call(value), "date");
        }
        if (isError(value)) {
          return formatError(value);
        }
      }
      var base = "", array = false, braces = ["{", "}"];
      if (isArray(value)) {
        array = true;
        braces = ["[", "]"];
      }
      if (isFunction(value)) {
        var n = value.name ? ": " + value.name : "";
        base = " [Function" + n + "]";
      }
      if (isRegExp(value)) {
        base = " " + RegExp.prototype.toString.call(value);
      }
      if (isDate(value)) {
        base = " " + Date.prototype.toUTCString.call(value);
      }
      if (isError(value)) {
        base = " " + formatError(value);
      }
      if (keys.length === 0 && (!array || value.length == 0)) {
        return braces[0] + base + braces[1];
      }
      if (recurseTimes < 0) {
        if (isRegExp(value)) {
          return ctx.stylize(RegExp.prototype.toString.call(value), "regexp");
        } else {
          return ctx.stylize("[Object]", "special");
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
        return ctx.stylize("undefined", "undefined");
      if (isString(value)) {
        var simple = "'" + JSON.stringify(value).replace(/^"|"$/g, "").replace(/'/g, "\\'").replace(/\\"/g, '"') + "'";
        return ctx.stylize(simple, "string");
      }
      if (isNumber(value))
        return ctx.stylize("" + value, "number");
      if (isBoolean(value))
        return ctx.stylize("" + value, "boolean");
      if (isNull(value))
        return ctx.stylize("null", "null");
    }
    function formatError(value) {
      return "[" + Error.prototype.toString.call(value) + "]";
    }
    function formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
      var output = [];
      for (var i = 0, l = value.length; i < l; ++i) {
        if (hasOwnProperty(value, String(i))) {
          output.push(formatProperty(
            ctx,
            value,
            recurseTimes,
            visibleKeys,
            String(i),
            true
          ));
        } else {
          output.push("");
        }
      }
      keys.forEach(function(key) {
        if (!key.match(/^\d+$/)) {
          output.push(formatProperty(
            ctx,
            value,
            recurseTimes,
            visibleKeys,
            key,
            true
          ));
        }
      });
      return output;
    }
    function formatProperty(ctx, value, recurseTimes, visibleKeys, key, array) {
      var name, str, desc;
      desc = Object.getOwnPropertyDescriptor(value, key) || { value: value[key] };
      if (desc.get) {
        if (desc.set) {
          str = ctx.stylize("[Getter/Setter]", "special");
        } else {
          str = ctx.stylize("[Getter]", "special");
        }
      } else {
        if (desc.set) {
          str = ctx.stylize("[Setter]", "special");
        }
      }
      if (!hasOwnProperty(visibleKeys, key)) {
        name = "[" + key + "]";
      }
      if (!str) {
        if (ctx.seen.indexOf(desc.value) < 0) {
          if (isNull(recurseTimes)) {
            str = formatValue(ctx, desc.value, null);
          } else {
            str = formatValue(ctx, desc.value, recurseTimes - 1);
          }
          if (str.indexOf("\n") > -1) {
            if (array) {
              str = str.split("\n").map(function(line) {
                return "  " + line;
              }).join("\n").slice(2);
            } else {
              str = "\n" + str.split("\n").map(function(line) {
                return "   " + line;
              }).join("\n");
            }
          }
        } else {
          str = ctx.stylize("[Circular]", "special");
        }
      }
      if (isUndefined(name)) {
        if (array && key.match(/^\d+$/)) {
          return str;
        }
        name = JSON.stringify("" + key);
        if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
          name = name.slice(1, -1);
          name = ctx.stylize(name, "name");
        } else {
          name = name.replace(/'/g, "\\'").replace(/\\"/g, '"').replace(/(^"|"$)/g, "'");
          name = ctx.stylize(name, "string");
        }
      }
      return name + ": " + str;
    }
    function reduceToSingleString(output, base, braces) {
      var numLinesEst = 0;
      var length = output.reduce(function(prev, cur) {
        numLinesEst++;
        if (cur.indexOf("\n") >= 0) numLinesEst++;
        return prev + cur.replace(/\u001b\[\d\d?m/g, "").length + 1;
      }, 0);
      if (length > 60) {
        return braces[0] + (base === "" ? "" : base + "\n ") + " " + output.join(",\n  ") + " " + braces[1];
      }
      return braces[0] + base + " " + output.join(", ") + " " + braces[1];
    }
    exports.types = require_types();
    function isArray(ar) {
      return Array.isArray(ar);
    }
    exports.isArray = isArray;
    function isBoolean(arg) {
      return typeof arg === "boolean";
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
      return typeof arg === "number";
    }
    exports.isNumber = isNumber;
    function isString(arg) {
      return typeof arg === "string";
    }
    exports.isString = isString;
    function isSymbol(arg) {
      return typeof arg === "symbol";
    }
    exports.isSymbol = isSymbol;
    function isUndefined(arg) {
      return arg === void 0;
    }
    exports.isUndefined = isUndefined;
    function isRegExp(re) {
      return isObject(re) && objectToString(re) === "[object RegExp]";
    }
    exports.isRegExp = isRegExp;
    exports.types.isRegExp = isRegExp;
    function isObject(arg) {
      return typeof arg === "object" && arg !== null;
    }
    exports.isObject = isObject;
    function isDate(d) {
      return isObject(d) && objectToString(d) === "[object Date]";
    }
    exports.isDate = isDate;
    exports.types.isDate = isDate;
    function isError(e) {
      return isObject(e) && (objectToString(e) === "[object Error]" || e instanceof Error);
    }
    exports.isError = isError;
    exports.types.isNativeError = isError;
    function isFunction(arg) {
      return typeof arg === "function";
    }
    exports.isFunction = isFunction;
    function isPrimitive(arg) {
      return arg === null || typeof arg === "boolean" || typeof arg === "number" || typeof arg === "string" || typeof arg === "symbol" || // ES6 symbol
      typeof arg === "undefined";
    }
    exports.isPrimitive = isPrimitive;
    exports.isBuffer = require_isBufferBrowser();
    function objectToString(o) {
      return Object.prototype.toString.call(o);
    }
    function pad(n) {
      return n < 10 ? "0" + n.toString(10) : n.toString(10);
    }
    var months = [
      "Jan",
      "Feb",
      "Mar",
      "Apr",
      "May",
      "Jun",
      "Jul",
      "Aug",
      "Sep",
      "Oct",
      "Nov",
      "Dec"
    ];
    function timestamp() {
      var d = /* @__PURE__ */ new Date();
      var time = [
        pad(d.getHours()),
        pad(d.getMinutes()),
        pad(d.getSeconds())
      ].join(":");
      return [d.getDate(), months[d.getMonth()], time].join(" ");
    }
    exports.log = function() {
      console.log("%s - %s", timestamp(), exports.format.apply(exports, arguments));
    };
    exports.inherits = require_inherits_browser();
    exports._extend = function(origin, add) {
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
    var kCustomPromisifiedSymbol = typeof Symbol !== "undefined" ? /* @__PURE__ */ Symbol("util.promisify.custom") : void 0;
    exports.promisify = function promisify(original) {
      if (typeof original !== "function")
        throw new TypeError('The "original" argument must be of type Function');
      if (kCustomPromisifiedSymbol && original[kCustomPromisifiedSymbol]) {
        var fn = original[kCustomPromisifiedSymbol];
        if (typeof fn !== "function") {
          throw new TypeError('The "util.promisify.custom" argument must be of type Function');
        }
        Object.defineProperty(fn, kCustomPromisifiedSymbol, {
          value: fn,
          enumerable: false,
          writable: false,
          configurable: true
        });
        return fn;
      }
      function fn() {
        var promiseResolve, promiseReject;
        var promise = new Promise(function(resolve, reject) {
          promiseResolve = resolve;
          promiseReject = reject;
        });
        var args = [];
        for (var i = 0; i < arguments.length; i++) {
          args.push(arguments[i]);
        }
        args.push(function(err, value) {
          if (err) {
            promiseReject(err);
          } else {
            promiseResolve(value);
          }
        });
        try {
          original.apply(this, args);
        } catch (err) {
          promiseReject(err);
        }
        return promise;
      }
      Object.setPrototypeOf(fn, Object.getPrototypeOf(original));
      if (kCustomPromisifiedSymbol) Object.defineProperty(fn, kCustomPromisifiedSymbol, {
        value: fn,
        enumerable: false,
        writable: false,
        configurable: true
      });
      return Object.defineProperties(
        fn,
        getOwnPropertyDescriptors(original)
      );
    };
    exports.promisify.custom = kCustomPromisifiedSymbol;
    function callbackifyOnRejected(reason, cb) {
      if (!reason) {
        var newReason = new Error("Promise was rejected with a falsy value");
        newReason.reason = reason;
        reason = newReason;
      }
      return cb(reason);
    }
    function callbackify(original) {
      if (typeof original !== "function") {
        throw new TypeError('The "original" argument must be of type Function');
      }
      function callbackified() {
        var args = [];
        for (var i = 0; i < arguments.length; i++) {
          args.push(arguments[i]);
        }
        var maybeCb = args.pop();
        if (typeof maybeCb !== "function") {
          throw new TypeError("The last argument must be of type Function");
        }
        var self2 = this;
        var cb = function() {
          return maybeCb.apply(self2, arguments);
        };
        original.apply(this, args).then(
          function(ret) {
            process.nextTick(cb.bind(null, null, ret));
          },
          function(rej) {
            process.nextTick(callbackifyOnRejected.bind(null, rej, cb));
          }
        );
      }
      Object.setPrototypeOf(callbackified, Object.getPrototypeOf(original));
      Object.defineProperties(
        callbackified,
        getOwnPropertyDescriptors(original)
      );
      return callbackified;
    }
    exports.callbackify = callbackify;
  }
});

// lib/sinon/assert.js
var require_assert = __commonJS({
  "lib/sinon/assert.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var calledInOrder = require_lib().calledInOrder;
    var createMatcher = require_samsam().createMatcher;
    var orderByFirstCall = require_lib().orderByFirstCall;
    var timesInWords = require_times_in_words();
    var inspect = require_util().inspect;
    var stringSlice = require_lib().prototypes.string.slice;
    var globalObject = require_lib().global;
    var arraySlice = arrayProto.slice;
    var concat = arrayProto.concat;
    var forEach = arrayProto.forEach;
    var join = arrayProto.join;
    var splice = arrayProto.splice;
    function applyDefaults(obj, defaults) {
      for (const key of Object.keys(defaults)) {
        const val = obj[key];
        if (val === null || typeof val === "undefined") {
          obj[key] = defaults[key];
        }
      }
    }
    function createAssertObject(opts) {
      const cleanedAssertOptions = opts || {};
      applyDefaults(cleanedAssertOptions, {
        shouldLimitAssertionLogs: false,
        assertionLogLimit: 1e4
      });
      const assert = {
        fail: function fail(message) {
          let msg = message;
          if (cleanedAssertOptions.shouldLimitAssertionLogs) {
            msg = message.substring(
              0,
              cleanedAssertOptions.assertionLogLimit
            );
          }
          const error = new Error(msg);
          error.name = "AssertError";
          throw error;
        },
        pass: function pass() {
          return;
        },
        callOrder: function assertCallOrder() {
          verifyIsStub.apply(null, arguments);
          let expected = "";
          let actual = "";
          if (!calledInOrder(arguments)) {
            try {
              expected = join(arguments, ", ");
              const calls = arraySlice(arguments);
              let i = calls.length;
              while (i) {
                if (!calls[--i].called) {
                  splice(calls, i, 1);
                }
              }
              actual = join(orderByFirstCall(calls), ", ");
            } catch (e) {
            }
            failAssertion(
              this,
              `expected ${expected} to be called in order but were called as ${actual}`
            );
          } else {
            assert.pass("callOrder");
          }
        },
        callCount: function assertCallCount(method, count) {
          verifyIsStub(method);
          let msg;
          if (typeof count !== "number") {
            msg = `expected ${inspect(count)} to be a number but was of type ${typeof count}`;
            failAssertion(this, msg);
          } else if (method.callCount !== count) {
            msg = `expected %n to be called ${timesInWords(count)} but was called %c%C`;
            failAssertion(this, method.printf(msg));
          } else {
            assert.pass("callCount");
          }
        },
        expose: function expose(target, options) {
          if (!target) {
            throw new TypeError("target is null or undefined");
          }
          const o = options || {};
          const prefix = typeof o.prefix === "undefined" && "assert" || o.prefix;
          const includeFail = typeof o.includeFail === "undefined" || Boolean(o.includeFail);
          const instance = this;
          forEach(Object.keys(instance), function(method) {
            if (method !== "expose" && (includeFail || !/^(fail)/.test(method))) {
              target[exposedName(prefix, method)] = instance[method];
            }
          });
          return target;
        },
        match: function match(actual, expectation) {
          const matcher = createMatcher(expectation);
          if (matcher.test(actual)) {
            assert.pass("match");
          } else {
            const formatted = [
              "expected value to match",
              `    expected = ${inspect(expectation)}`,
              `    actual = ${inspect(actual)}`
            ];
            failAssertion(this, join(formatted, "\n"));
          }
        }
      };
      function verifyIsStub() {
        const args = arraySlice(arguments);
        forEach(args, function(method) {
          if (!method) {
            assert.fail("fake is not a spy");
          }
          if (method.proxy && method.proxy.isSinonProxy) {
            verifyIsStub(method.proxy);
          } else {
            if (typeof method !== "function") {
              assert.fail(`${method} is not a function`);
            }
            if (typeof method.getCall !== "function") {
              assert.fail(`${method} is not stubbed`);
            }
          }
        });
      }
      function verifyIsValidAssertion(assertionMethod, assertionArgs) {
        switch (assertionMethod) {
          case "notCalled":
          case "called":
          case "calledOnce":
          case "calledTwice":
          case "calledThrice":
            if (assertionArgs.length !== 0) {
              assert.fail(
                `${assertionMethod} takes 1 argument but was called with ${assertionArgs.length + 1} arguments`
              );
            }
            break;
          default:
            break;
        }
      }
      function failAssertion(object, msg) {
        const obj = object || globalObject;
        const failMethod = obj.fail || assert.fail;
        failMethod.call(obj, msg);
      }
      function mirrorPropAsAssertion(name, method, message) {
        let msg = message;
        let meth = method;
        if (arguments.length === 2) {
          msg = method;
          meth = name;
        }
        assert[name] = function(fake) {
          verifyIsStub(fake);
          const args = arraySlice(arguments, 1);
          let failed = false;
          verifyIsValidAssertion(name, args);
          if (typeof meth === "function") {
            failed = !meth(fake);
          } else {
            failed = typeof fake[meth] === "function" ? !fake[meth].apply(fake, args) : !fake[meth];
          }
          if (failed) {
            failAssertion(
              this,
              (fake.printf || fake.proxy.printf).apply(
                fake,
                concat([msg], args)
              )
            );
          } else {
            assert.pass(name);
          }
        };
      }
      function exposedName(prefix, prop) {
        return !prefix || /^fail/.test(prop) ? prop : prefix + stringSlice(prop, 0, 1).toUpperCase() + stringSlice(prop, 1);
      }
      mirrorPropAsAssertion(
        "called",
        "expected %n to have been called at least once but was never called"
      );
      mirrorPropAsAssertion(
        "notCalled",
        function(spy) {
          return !spy.called;
        },
        "expected %n to not have been called but was called %c%C"
      );
      mirrorPropAsAssertion(
        "calledOnce",
        "expected %n to be called once but was called %c%C"
      );
      mirrorPropAsAssertion(
        "calledTwice",
        "expected %n to be called twice but was called %c%C"
      );
      mirrorPropAsAssertion(
        "calledThrice",
        "expected %n to be called thrice but was called %c%C"
      );
      mirrorPropAsAssertion(
        "calledOn",
        "expected %n to be called with %1 as this but was called with %t"
      );
      mirrorPropAsAssertion(
        "alwaysCalledOn",
        "expected %n to always be called with %1 as this but was called with %t"
      );
      mirrorPropAsAssertion("calledWithNew", "expected %n to be called with new");
      mirrorPropAsAssertion(
        "alwaysCalledWithNew",
        "expected %n to always be called with new"
      );
      mirrorPropAsAssertion(
        "calledWith",
        "expected %n to be called with arguments %D"
      );
      mirrorPropAsAssertion(
        "calledWithMatch",
        "expected %n to be called with match %D"
      );
      mirrorPropAsAssertion(
        "alwaysCalledWith",
        "expected %n to always be called with arguments %D"
      );
      mirrorPropAsAssertion(
        "alwaysCalledWithMatch",
        "expected %n to always be called with match %D"
      );
      mirrorPropAsAssertion(
        "calledWithExactly",
        "expected %n to be called with exact arguments %D"
      );
      mirrorPropAsAssertion(
        "calledOnceWith",
        "expected %n to be called once and with arguments %D"
      );
      mirrorPropAsAssertion(
        "calledOnceWithExactly",
        "expected %n to be called once and with exact arguments %D"
      );
      mirrorPropAsAssertion(
        "calledOnceWithMatch",
        "expected %n to be called once and with match %D"
      );
      mirrorPropAsAssertion(
        "alwaysCalledWithExactly",
        "expected %n to always be called with exact arguments %D"
      );
      mirrorPropAsAssertion(
        "neverCalledWith",
        "expected %n to never be called with arguments %*%C"
      );
      mirrorPropAsAssertion(
        "neverCalledWithMatch",
        "expected %n to never be called with match %*%C"
      );
      mirrorPropAsAssertion("threw", "%n did not throw exception%C");
      mirrorPropAsAssertion("alwaysThrew", "%n did not always throw exception%C");
      return assert;
    }
    module.exports = createAssertObject();
    module.exports.createAssertObject = createAssertObject;
  }
});

// node_modules/@sinonjs/fake-timers/src/fake-timers-src.js
var require_fake_timers_src = __commonJS({
  "node_modules/@sinonjs/fake-timers/src/fake-timers-src.js"(exports, module) {
    "use strict";
    var globalObject = require_lib().global;
    var timersModule;
    var timersPromisesModule;
    if (typeof __require === "function" && typeof module === "object") {
      try {
        timersModule = __require("timers");
      } catch (e) {
      }
      try {
        timersPromisesModule = __require("timers/promises");
      } catch (e) {
      }
    }
    function withGlobal(_global) {
      const maxTimeout = Math.pow(2, 31) - 1;
      const idCounterStart = 1e12;
      const NOOP = function() {
        return void 0;
      };
      const NOOP_ARRAY = function() {
        return [];
      };
      const isPresent = {};
      let timeoutResult, addTimerReturnsObject = false;
      if (_global.setTimeout) {
        isPresent.setTimeout = true;
        timeoutResult = _global.setTimeout(NOOP, 0);
        addTimerReturnsObject = typeof timeoutResult === "object";
      }
      isPresent.clearTimeout = Boolean(_global.clearTimeout);
      isPresent.setInterval = Boolean(_global.setInterval);
      isPresent.clearInterval = Boolean(_global.clearInterval);
      isPresent.hrtime = _global.process && typeof _global.process.hrtime === "function";
      isPresent.hrtimeBigint = isPresent.hrtime && typeof _global.process.hrtime.bigint === "function";
      isPresent.nextTick = _global.process && typeof _global.process.nextTick === "function";
      const utilPromisify = _global.process && require_util().promisify;
      isPresent.performance = _global.performance && typeof _global.performance.now === "function";
      const hasPerformancePrototype = _global.Performance && (typeof _global.Performance).match(/^(function|object)$/);
      const hasPerformanceConstructorPrototype = _global.performance && _global.performance.constructor && _global.performance.constructor.prototype;
      isPresent.queueMicrotask = _global.hasOwnProperty("queueMicrotask");
      isPresent.requestAnimationFrame = _global.requestAnimationFrame && typeof _global.requestAnimationFrame === "function";
      isPresent.cancelAnimationFrame = _global.cancelAnimationFrame && typeof _global.cancelAnimationFrame === "function";
      isPresent.requestIdleCallback = _global.requestIdleCallback && typeof _global.requestIdleCallback === "function";
      isPresent.cancelIdleCallbackPresent = _global.cancelIdleCallback && typeof _global.cancelIdleCallback === "function";
      isPresent.setImmediate = _global.setImmediate && typeof _global.setImmediate === "function";
      isPresent.clearImmediate = _global.clearImmediate && typeof _global.clearImmediate === "function";
      isPresent.Intl = _global.Intl && typeof _global.Intl === "object";
      if (_global.clearTimeout) {
        _global.clearTimeout(timeoutResult);
      }
      const NativeDate = _global.Date;
      const NativeIntl = isPresent.Intl ? Object.defineProperties(
        /* @__PURE__ */ Object.create(null),
        Object.getOwnPropertyDescriptors(_global.Intl)
      ) : void 0;
      let uniqueTimerId = idCounterStart;
      if (NativeDate === void 0) {
        throw new Error(
          "The global scope doesn't have a `Date` object (see https://github.com/sinonjs/sinon/issues/1852#issuecomment-419622780)"
        );
      }
      isPresent.Date = true;
      class FakePerformanceEntry {
        constructor(name, entryType, startTime, duration) {
          this.name = name;
          this.entryType = entryType;
          this.startTime = startTime;
          this.duration = duration;
        }
        toJSON() {
          return JSON.stringify({ ...this });
        }
      }
      function isNumberFinite(num) {
        if (Number.isFinite) {
          return Number.isFinite(num);
        }
        return isFinite(num);
      }
      let isNearInfiniteLimit = false;
      function checkIsNearInfiniteLimit(clock, i) {
        if (clock.loopLimit && i === clock.loopLimit - 1) {
          isNearInfiniteLimit = true;
        }
      }
      function resetIsNearInfiniteLimit() {
        isNearInfiniteLimit = false;
      }
      function parseTime(str) {
        if (!str) {
          return 0;
        }
        const strings = str.split(":");
        const l = strings.length;
        let i = l;
        let ms = 0;
        let parsed;
        if (l > 3 || !/^(\d\d:){0,2}\d\d?$/.test(str)) {
          throw new Error(
            "tick only understands numbers, 'm:s' and 'h:m:s'. Each part must be two digits"
          );
        }
        while (i--) {
          parsed = parseInt(strings[i], 10);
          if (parsed >= 60) {
            throw new Error(`Invalid time ${str}`);
          }
          ms += parsed * Math.pow(60, l - i - 1);
        }
        return ms * 1e3;
      }
      function nanoRemainder(msFloat) {
        const modulo = 1e6;
        const remainder = msFloat * 1e6 % modulo;
        const positiveRemainder = remainder < 0 ? remainder + modulo : remainder;
        return Math.floor(positiveRemainder);
      }
      function getEpoch(epoch) {
        if (!epoch) {
          return 0;
        }
        if (typeof epoch.getTime === "function") {
          return epoch.getTime();
        }
        if (typeof epoch === "number") {
          return epoch;
        }
        throw new TypeError("now should be milliseconds since UNIX epoch");
      }
      function inRange(from, to, timer) {
        return timer && timer.callAt >= from && timer.callAt <= to;
      }
      function getInfiniteLoopError(clock, job) {
        const infiniteLoopError = new Error(
          `Aborting after running ${clock.loopLimit} timers, assuming an infinite loop!`
        );
        if (!job.error) {
          return infiniteLoopError;
        }
        const computedTargetPattern = /target\.*[<|(|[].*?[>|\]|)]\s*/;
        let clockMethodPattern = new RegExp(
          String(Object.keys(clock).join("|"))
        );
        if (addTimerReturnsObject) {
          clockMethodPattern = new RegExp(
            `\\s+at (Object\\.)?(?:${Object.keys(clock).join("|")})\\s+`
          );
        }
        let matchedLineIndex = -1;
        job.error.stack.split("\n").some(function(line, i) {
          const matchedComputedTarget = line.match(computedTargetPattern);
          if (matchedComputedTarget) {
            matchedLineIndex = i;
            return true;
          }
          const matchedClockMethod = line.match(clockMethodPattern);
          if (matchedClockMethod) {
            matchedLineIndex = i;
            return false;
          }
          return matchedLineIndex >= 0;
        });
        const stack = `${infiniteLoopError}
${job.type || "Microtask"} - ${job.func.name || "anonymous"}
${job.error.stack.split("\n").slice(matchedLineIndex + 1).join("\n")}`;
        try {
          Object.defineProperty(infiniteLoopError, "stack", {
            value: stack
          });
        } catch (e) {
        }
        return infiniteLoopError;
      }
      function createDate() {
        class ClockDate extends NativeDate {
          /**
           * @param {number} year
           * @param {number} month
           * @param {number} date
           * @param {number} hour
           * @param {number} minute
           * @param {number} second
           * @param {number} ms
           * @returns void
           */
          // eslint-disable-next-line no-unused-vars
          constructor(year, month, date, hour, minute, second, ms) {
            if (arguments.length === 0) {
              super(ClockDate.clock.now);
            } else {
              super(...arguments);
            }
            Object.defineProperty(this, "constructor", {
              value: NativeDate,
              enumerable: false
            });
          }
          static [Symbol.hasInstance](instance) {
            return instance instanceof NativeDate;
          }
        }
        ClockDate.isFake = true;
        if (NativeDate.now) {
          ClockDate.now = function now() {
            return ClockDate.clock.now;
          };
        }
        if (NativeDate.toSource) {
          ClockDate.toSource = function toSource() {
            return NativeDate.toSource();
          };
        }
        ClockDate.toString = function toString() {
          return NativeDate.toString();
        };
        const ClockDateProxy = new Proxy(ClockDate, {
          // handler for [[Call]] invocations (i.e. not using `new`)
          apply() {
            if (this instanceof ClockDate) {
              throw new TypeError(
                "A Proxy should only capture `new` calls with the `construct` handler. This is not supposed to be possible, so check the logic."
              );
            }
            return new NativeDate(ClockDate.clock.now).toString();
          }
        });
        return ClockDateProxy;
      }
      function createIntl() {
        const ClockIntl = {};
        Object.getOwnPropertyNames(NativeIntl).forEach(
          (property) => ClockIntl[property] = NativeIntl[property]
        );
        ClockIntl.DateTimeFormat = function(...args) {
          const realFormatter = new NativeIntl.DateTimeFormat(...args);
          const formatter = {};
          ["formatRange", "formatRangeToParts", "resolvedOptions"].forEach(
            (method) => {
              formatter[method] = realFormatter[method].bind(realFormatter);
            }
          );
          ["format", "formatToParts"].forEach((method) => {
            formatter[method] = function(date) {
              return realFormatter[method](date || ClockIntl.clock.now);
            };
          });
          return formatter;
        };
        ClockIntl.DateTimeFormat.prototype = Object.create(
          NativeIntl.DateTimeFormat.prototype
        );
        ClockIntl.DateTimeFormat.supportedLocalesOf = NativeIntl.DateTimeFormat.supportedLocalesOf;
        return ClockIntl;
      }
      function enqueueJob(clock, job) {
        if (!clock.jobs) {
          clock.jobs = [];
        }
        clock.jobs.push(job);
      }
      function runJobs(clock) {
        if (!clock.jobs) {
          return;
        }
        for (let i = 0; i < clock.jobs.length; i++) {
          const job = clock.jobs[i];
          job.func.apply(null, job.args);
          checkIsNearInfiniteLimit(clock, i);
          if (clock.loopLimit && i > clock.loopLimit) {
            throw getInfiniteLoopError(clock, job);
          }
        }
        resetIsNearInfiniteLimit();
        clock.jobs = [];
      }
      function addTimer(clock, timer) {
        if (timer.func === void 0) {
          throw new Error("Callback must be provided to timer calls");
        }
        if (addTimerReturnsObject) {
          if (typeof timer.func !== "function") {
            throw new TypeError(
              `[ERR_INVALID_CALLBACK]: Callback must be a function. Received ${timer.func} of type ${typeof timer.func}`
            );
          }
        }
        if (isNearInfiniteLimit) {
          timer.error = new Error();
        }
        timer.type = timer.immediate ? "Immediate" : "Timeout";
        if (timer.hasOwnProperty("delay")) {
          if (typeof timer.delay !== "number") {
            timer.delay = parseInt(timer.delay, 10);
          }
          if (!isNumberFinite(timer.delay)) {
            timer.delay = 0;
          }
          timer.delay = timer.delay > maxTimeout ? 1 : timer.delay;
          timer.delay = Math.max(0, timer.delay);
        }
        if (timer.hasOwnProperty("interval")) {
          timer.type = "Interval";
          timer.interval = timer.interval > maxTimeout ? 1 : timer.interval;
        }
        if (timer.hasOwnProperty("animation")) {
          timer.type = "AnimationFrame";
          timer.animation = true;
        }
        if (timer.hasOwnProperty("idleCallback")) {
          timer.type = "IdleCallback";
          timer.idleCallback = true;
        }
        if (!clock.timers) {
          clock.timers = {};
        }
        timer.id = uniqueTimerId++;
        timer.createdAt = clock.now;
        timer.callAt = clock.now + (parseInt(timer.delay) || (clock.duringTick ? 1 : 0));
        clock.timers[timer.id] = timer;
        if (addTimerReturnsObject) {
          const res = {
            refed: true,
            ref: function() {
              this.refed = true;
              return res;
            },
            unref: function() {
              this.refed = false;
              return res;
            },
            hasRef: function() {
              return this.refed;
            },
            refresh: function() {
              timer.callAt = clock.now + (parseInt(timer.delay) || (clock.duringTick ? 1 : 0));
              clock.timers[timer.id] = timer;
              return res;
            },
            [Symbol.toPrimitive]: function() {
              return timer.id;
            }
          };
          return res;
        }
        return timer.id;
      }
      function compareTimers(a, b) {
        if (a.callAt < b.callAt) {
          return -1;
        }
        if (a.callAt > b.callAt) {
          return 1;
        }
        if (a.immediate && !b.immediate) {
          return -1;
        }
        if (!a.immediate && b.immediate) {
          return 1;
        }
        if (a.createdAt < b.createdAt) {
          return -1;
        }
        if (a.createdAt > b.createdAt) {
          return 1;
        }
        if (a.id < b.id) {
          return -1;
        }
        if (a.id > b.id) {
          return 1;
        }
      }
      function firstTimerInRange(clock, from, to) {
        const timers2 = clock.timers;
        let timer = null;
        let id, isInRange;
        for (id in timers2) {
          if (timers2.hasOwnProperty(id)) {
            isInRange = inRange(from, to, timers2[id]);
            if (isInRange && (!timer || compareTimers(timer, timers2[id]) === 1)) {
              timer = timers2[id];
            }
          }
        }
        return timer;
      }
      function firstTimer(clock) {
        const timers2 = clock.timers;
        let timer = null;
        let id;
        for (id in timers2) {
          if (timers2.hasOwnProperty(id)) {
            if (!timer || compareTimers(timer, timers2[id]) === 1) {
              timer = timers2[id];
            }
          }
        }
        return timer;
      }
      function lastTimer(clock) {
        const timers2 = clock.timers;
        let timer = null;
        let id;
        for (id in timers2) {
          if (timers2.hasOwnProperty(id)) {
            if (!timer || compareTimers(timer, timers2[id]) === -1) {
              timer = timers2[id];
            }
          }
        }
        return timer;
      }
      function callTimer(clock, timer) {
        if (typeof timer.interval === "number") {
          clock.timers[timer.id].callAt += timer.interval;
        } else {
          delete clock.timers[timer.id];
        }
        if (typeof timer.func === "function") {
          timer.func.apply(null, timer.args);
        } else {
          const eval2 = eval;
          (function() {
            eval2(timer.func);
          })();
        }
      }
      function getClearHandler(ttype) {
        if (ttype === "IdleCallback" || ttype === "AnimationFrame") {
          return `cancel${ttype}`;
        }
        return `clear${ttype}`;
      }
      function getScheduleHandler(ttype) {
        if (ttype === "IdleCallback" || ttype === "AnimationFrame") {
          return `request${ttype}`;
        }
        return `set${ttype}`;
      }
      function createWarnOnce() {
        let calls = 0;
        return function(msg) {
          !calls++ && console.warn(msg);
        };
      }
      const warnOnce = createWarnOnce();
      function clearTimer(clock, timerId, ttype) {
        if (!timerId) {
          return;
        }
        if (!clock.timers) {
          clock.timers = {};
        }
        const id = Number(timerId);
        if (Number.isNaN(id) || id < idCounterStart) {
          const handlerName = getClearHandler(ttype);
          if (clock.shouldClearNativeTimers === true) {
            const nativeHandler = clock[`_${handlerName}`];
            return typeof nativeHandler === "function" ? nativeHandler(timerId) : void 0;
          }
          const stackTrace = new Error().stack.split("\n").slice(1).join("\n");
          warnOnce(
            `FakeTimers: ${handlerName} was invoked to clear a native timer instead of one created by this library.
To automatically clean-up native timers, use \`shouldClearNativeTimers\`.
${stackTrace}`
          );
        }
        if (clock.timers.hasOwnProperty(id)) {
          const timer = clock.timers[id];
          if (timer.type === ttype || timer.type === "Timeout" && ttype === "Interval" || timer.type === "Interval" && ttype === "Timeout") {
            delete clock.timers[id];
          } else {
            const clear = getClearHandler(ttype);
            const schedule = getScheduleHandler(timer.type);
            throw new Error(
              `Cannot clear timer: timer created with ${schedule}() but cleared with ${clear}()`
            );
          }
        }
      }
      function uninstall(clock) {
        let method, i, l;
        const installedHrTime = "_hrtime";
        const installedNextTick = "_nextTick";
        for (i = 0, l = clock.methods.length; i < l; i++) {
          method = clock.methods[i];
          if (method === "hrtime" && _global.process) {
            _global.process.hrtime = clock[installedHrTime];
          } else if (method === "nextTick" && _global.process) {
            _global.process.nextTick = clock[installedNextTick];
          } else if (method === "performance") {
            const originalPerfDescriptor = Object.getOwnPropertyDescriptor(
              clock,
              `_${method}`
            );
            if (originalPerfDescriptor && originalPerfDescriptor.get && !originalPerfDescriptor.set) {
              Object.defineProperty(
                _global,
                method,
                originalPerfDescriptor
              );
            } else if (originalPerfDescriptor.configurable) {
              _global[method] = clock[`_${method}`];
            }
          } else {
            if (_global[method] && _global[method].hadOwnProperty) {
              _global[method] = clock[`_${method}`];
            } else {
              try {
                delete _global[method];
              } catch (ignore) {
              }
            }
          }
          if (clock.timersModuleMethods !== void 0) {
            for (let j = 0; j < clock.timersModuleMethods.length; j++) {
              const entry = clock.timersModuleMethods[j];
              timersModule[entry.methodName] = entry.original;
            }
          }
          if (clock.timersPromisesModuleMethods !== void 0) {
            for (let j = 0; j < clock.timersPromisesModuleMethods.length; j++) {
              const entry = clock.timersPromisesModuleMethods[j];
              timersPromisesModule[entry.methodName] = entry.original;
            }
          }
        }
        clock.setTickMode("manual");
        clock.methods = [];
        for (const [listener, signal] of clock.abortListenerMap.entries()) {
          signal.removeEventListener("abort", listener);
          clock.abortListenerMap.delete(listener);
        }
        if (!clock.timers) {
          return [];
        }
        return Object.keys(clock.timers).map(function mapper(key) {
          return clock.timers[key];
        });
      }
      function hijackMethod(target, method, clock) {
        clock[method].hadOwnProperty = Object.prototype.hasOwnProperty.call(
          target,
          method
        );
        clock[`_${method}`] = target[method];
        if (method === "Date") {
          target[method] = clock[method];
        } else if (method === "Intl") {
          target[method] = clock[method];
        } else if (method === "performance") {
          const originalPerfDescriptor = Object.getOwnPropertyDescriptor(
            target,
            method
          );
          if (originalPerfDescriptor && originalPerfDescriptor.get && !originalPerfDescriptor.set) {
            Object.defineProperty(
              clock,
              `_${method}`,
              originalPerfDescriptor
            );
            const perfDescriptor = Object.getOwnPropertyDescriptor(
              clock,
              method
            );
            Object.defineProperty(target, method, perfDescriptor);
          } else {
            target[method] = clock[method];
          }
        } else {
          target[method] = function() {
            return clock[method].apply(clock, arguments);
          };
          Object.defineProperties(
            target[method],
            Object.getOwnPropertyDescriptors(clock[method])
          );
        }
        target[method].clock = clock;
      }
      function doIntervalTick(clock, advanceTimeDelta) {
        clock.tick(advanceTimeDelta);
      }
      const timers = {
        setTimeout: _global.setTimeout,
        clearTimeout: _global.clearTimeout,
        setInterval: _global.setInterval,
        clearInterval: _global.clearInterval,
        Date: _global.Date
      };
      if (isPresent.setImmediate) {
        timers.setImmediate = _global.setImmediate;
      }
      if (isPresent.clearImmediate) {
        timers.clearImmediate = _global.clearImmediate;
      }
      if (isPresent.hrtime) {
        timers.hrtime = _global.process.hrtime;
      }
      if (isPresent.nextTick) {
        timers.nextTick = _global.process.nextTick;
      }
      if (isPresent.performance) {
        timers.performance = _global.performance;
      }
      if (isPresent.requestAnimationFrame) {
        timers.requestAnimationFrame = _global.requestAnimationFrame;
      }
      if (isPresent.queueMicrotask) {
        timers.queueMicrotask = _global.queueMicrotask;
      }
      if (isPresent.cancelAnimationFrame) {
        timers.cancelAnimationFrame = _global.cancelAnimationFrame;
      }
      if (isPresent.requestIdleCallback) {
        timers.requestIdleCallback = _global.requestIdleCallback;
      }
      if (isPresent.cancelIdleCallback) {
        timers.cancelIdleCallback = _global.cancelIdleCallback;
      }
      if (isPresent.Intl) {
        timers.Intl = NativeIntl;
      }
      const originalSetTimeout = _global.setImmediate || _global.setTimeout;
      const originalClearInterval = _global.clearInterval;
      const originalSetInterval = _global.setInterval;
      function createClock(start, loopLimit) {
        start = Math.floor(getEpoch(start));
        loopLimit = loopLimit || 1e3;
        let nanos = 0;
        const adjustedSystemTime = [0, 0];
        const clock = {
          now: start,
          Date: createDate(),
          loopLimit,
          tickMode: { mode: "manual", counter: 0, delta: void 0 }
        };
        clock.Date.clock = clock;
        function getTimeToNextFrame() {
          return 16 - (clock.now - start) % 16;
        }
        function hrtime(prev) {
          const millisSinceStart = clock.now - adjustedSystemTime[0] - start;
          const secsSinceStart = Math.floor(millisSinceStart / 1e3);
          const remainderInNanos = (millisSinceStart - secsSinceStart * 1e3) * 1e6 + nanos - adjustedSystemTime[1];
          if (Array.isArray(prev)) {
            if (prev[1] > 1e9) {
              throw new TypeError(
                "Number of nanoseconds can't exceed a billion"
              );
            }
            const oldSecs = prev[0];
            let nanoDiff = remainderInNanos - prev[1];
            let secDiff = secsSinceStart - oldSecs;
            if (nanoDiff < 0) {
              nanoDiff += 1e9;
              secDiff -= 1;
            }
            return [secDiff, nanoDiff];
          }
          return [secsSinceStart, remainderInNanos];
        }
        function fakePerformanceNow() {
          const hrt = hrtime();
          const millis = hrt[0] * 1e3 + hrt[1] / 1e6;
          return millis;
        }
        if (isPresent.hrtimeBigint) {
          hrtime.bigint = function() {
            const parts = hrtime();
            return BigInt(parts[0]) * BigInt(1e9) + BigInt(parts[1]);
          };
        }
        if (isPresent.Intl) {
          clock.Intl = createIntl();
          clock.Intl.clock = clock;
        }
        clock.setTickMode = function(tickModeConfig) {
          const { mode: newMode, delta: newDelta } = tickModeConfig;
          const { mode: oldMode, delta: oldDelta } = clock.tickMode;
          if (newMode === oldMode && newDelta === oldDelta) {
            return;
          }
          if (oldMode === "interval") {
            originalClearInterval(clock.attachedInterval);
          }
          clock.tickMode = {
            counter: clock.tickMode.counter + 1,
            mode: newMode,
            delta: newDelta
          };
          if (newMode === "nextAsync") {
            advanceUntilModeChanges();
          } else if (newMode === "interval") {
            createIntervalTick(clock, newDelta || 20);
          }
        };
        async function advanceUntilModeChanges() {
          async function newMacrotask() {
            const channel = new MessageChannel();
            await new Promise((resolve) => {
              channel.port1.onmessage = () => {
                resolve();
                channel.port1.close();
              };
              channel.port2.postMessage(void 0);
            });
            channel.port1.close();
            channel.port2.close();
            await new Promise((resolve) => {
              originalSetTimeout(resolve);
            });
          }
          const { counter } = clock.tickMode;
          while (clock.tickMode.counter === counter) {
            await newMacrotask();
            if (clock.tickMode.counter !== counter) {
              return;
            }
            clock.next();
          }
        }
        function pauseAutoTickUntilFinished(promise) {
          if (clock.tickMode.mode !== "nextAsync") {
            return promise;
          }
          clock.setTickMode({ mode: "manual" });
          return promise.finally(() => {
            clock.setTickMode({ mode: "nextAsync" });
          });
        }
        clock.requestIdleCallback = function requestIdleCallback(func, timeout) {
          let timeToNextIdlePeriod = 0;
          if (clock.countTimers() > 0) {
            timeToNextIdlePeriod = 50;
          }
          const result = addTimer(clock, {
            func,
            args: Array.prototype.slice.call(arguments, 2),
            delay: typeof timeout === "undefined" ? timeToNextIdlePeriod : Math.min(timeout, timeToNextIdlePeriod),
            idleCallback: true
          });
          return Number(result);
        };
        clock.cancelIdleCallback = function cancelIdleCallback(timerId) {
          return clearTimer(clock, timerId, "IdleCallback");
        };
        clock.setTimeout = function setTimeout2(func, timeout) {
          return addTimer(clock, {
            func,
            args: Array.prototype.slice.call(arguments, 2),
            delay: timeout
          });
        };
        if (typeof _global.Promise !== "undefined" && utilPromisify) {
          clock.setTimeout[utilPromisify.custom] = function promisifiedSetTimeout(timeout, arg) {
            return new _global.Promise(function setTimeoutExecutor(resolve) {
              addTimer(clock, {
                func: resolve,
                args: [arg],
                delay: timeout
              });
            });
          };
        }
        clock.clearTimeout = function clearTimeout2(timerId) {
          return clearTimer(clock, timerId, "Timeout");
        };
        clock.nextTick = function nextTick(func) {
          return enqueueJob(clock, {
            func,
            args: Array.prototype.slice.call(arguments, 1),
            error: isNearInfiniteLimit ? new Error() : null
          });
        };
        clock.queueMicrotask = function queueMicrotask(func) {
          return clock.nextTick(func);
        };
        clock.setInterval = function setInterval2(func, timeout) {
          timeout = parseInt(timeout, 10);
          return addTimer(clock, {
            func,
            args: Array.prototype.slice.call(arguments, 2),
            delay: timeout,
            interval: timeout
          });
        };
        clock.clearInterval = function clearInterval2(timerId) {
          return clearTimer(clock, timerId, "Interval");
        };
        if (isPresent.setImmediate) {
          clock.setImmediate = function setImmediate(func) {
            return addTimer(clock, {
              func,
              args: Array.prototype.slice.call(arguments, 1),
              immediate: true
            });
          };
          if (typeof _global.Promise !== "undefined" && utilPromisify) {
            clock.setImmediate[utilPromisify.custom] = function promisifiedSetImmediate(arg) {
              return new _global.Promise(
                function setImmediateExecutor(resolve) {
                  addTimer(clock, {
                    func: resolve,
                    args: [arg],
                    immediate: true
                  });
                }
              );
            };
          }
          clock.clearImmediate = function clearImmediate(timerId) {
            return clearTimer(clock, timerId, "Immediate");
          };
        }
        clock.countTimers = function countTimers() {
          return Object.keys(clock.timers || {}).length + (clock.jobs || []).length;
        };
        clock.requestAnimationFrame = function requestAnimationFrame(func) {
          const result = addTimer(clock, {
            func,
            delay: getTimeToNextFrame(),
            get args() {
              return [fakePerformanceNow()];
            },
            animation: true
          });
          return Number(result);
        };
        clock.cancelAnimationFrame = function cancelAnimationFrame(timerId) {
          return clearTimer(clock, timerId, "AnimationFrame");
        };
        clock.runMicrotasks = function runMicrotasks() {
          runJobs(clock);
        };
        function doTick(tickValue, isAsync, resolve, reject) {
          const msFloat = typeof tickValue === "number" ? tickValue : parseTime(tickValue);
          const ms = Math.floor(msFloat);
          const remainder = nanoRemainder(msFloat);
          let nanosTotal = nanos + remainder;
          let tickTo = clock.now + ms;
          if (msFloat < 0) {
            throw new TypeError("Negative ticks are not supported");
          }
          if (nanosTotal >= 1e6) {
            tickTo += 1;
            nanosTotal -= 1e6;
          }
          nanos = nanosTotal;
          let tickFrom = clock.now;
          let previous = clock.now;
          let timer, firstException, oldNow, nextPromiseTick, compensationCheck, postTimerCall;
          clock.duringTick = true;
          oldNow = clock.now;
          runJobs(clock);
          if (oldNow !== clock.now) {
            tickFrom += clock.now - oldNow;
            tickTo += clock.now - oldNow;
          }
          function doTickInner() {
            timer = firstTimerInRange(clock, tickFrom, tickTo);
            while (timer && tickFrom <= tickTo) {
              if (clock.timers[timer.id]) {
                tickFrom = timer.callAt;
                clock.now = timer.callAt;
                oldNow = clock.now;
                try {
                  runJobs(clock);
                  callTimer(clock, timer);
                } catch (e) {
                  firstException = firstException || e;
                }
                if (isAsync) {
                  originalSetTimeout(nextPromiseTick);
                  return;
                }
                compensationCheck();
              }
              postTimerCall();
            }
            oldNow = clock.now;
            runJobs(clock);
            if (oldNow !== clock.now) {
              tickFrom += clock.now - oldNow;
              tickTo += clock.now - oldNow;
            }
            clock.duringTick = false;
            timer = firstTimerInRange(clock, tickFrom, tickTo);
            if (timer) {
              try {
                clock.tick(tickTo - clock.now);
              } catch (e) {
                firstException = firstException || e;
              }
            } else {
              clock.now = tickTo;
              nanos = nanosTotal;
            }
            if (firstException) {
              throw firstException;
            }
            if (isAsync) {
              resolve(clock.now);
            } else {
              return clock.now;
            }
          }
          nextPromiseTick = isAsync && function() {
            try {
              compensationCheck();
              postTimerCall();
              doTickInner();
            } catch (e) {
              reject(e);
            }
          };
          compensationCheck = function() {
            if (oldNow !== clock.now) {
              tickFrom += clock.now - oldNow;
              tickTo += clock.now - oldNow;
              previous += clock.now - oldNow;
            }
          };
          postTimerCall = function() {
            timer = firstTimerInRange(clock, previous, tickTo);
            previous = tickFrom;
          };
          return doTickInner();
        }
        clock.tick = function tick(tickValue) {
          return doTick(tickValue, false);
        };
        if (typeof _global.Promise !== "undefined") {
          clock.tickAsync = function tickAsync(tickValue) {
            return pauseAutoTickUntilFinished(
              new _global.Promise(function(resolve, reject) {
                originalSetTimeout(function() {
                  try {
                    doTick(tickValue, true, resolve, reject);
                  } catch (e) {
                    reject(e);
                  }
                });
              })
            );
          };
        }
        clock.next = function next() {
          runJobs(clock);
          const timer = firstTimer(clock);
          if (!timer) {
            return clock.now;
          }
          clock.duringTick = true;
          try {
            clock.now = timer.callAt;
            callTimer(clock, timer);
            runJobs(clock);
            return clock.now;
          } finally {
            clock.duringTick = false;
          }
        };
        if (typeof _global.Promise !== "undefined") {
          clock.nextAsync = function nextAsync() {
            return pauseAutoTickUntilFinished(
              new _global.Promise(function(resolve, reject) {
                originalSetTimeout(function() {
                  try {
                    const timer = firstTimer(clock);
                    if (!timer) {
                      resolve(clock.now);
                      return;
                    }
                    let err;
                    clock.duringTick = true;
                    clock.now = timer.callAt;
                    try {
                      callTimer(clock, timer);
                    } catch (e) {
                      err = e;
                    }
                    clock.duringTick = false;
                    originalSetTimeout(function() {
                      if (err) {
                        reject(err);
                      } else {
                        resolve(clock.now);
                      }
                    });
                  } catch (e) {
                    reject(e);
                  }
                });
              })
            );
          };
        }
        clock.runAll = function runAll() {
          let numTimers, i;
          runJobs(clock);
          for (i = 0; i < clock.loopLimit; i++) {
            if (!clock.timers) {
              resetIsNearInfiniteLimit();
              return clock.now;
            }
            numTimers = Object.keys(clock.timers).length;
            if (numTimers === 0) {
              resetIsNearInfiniteLimit();
              return clock.now;
            }
            clock.next();
            checkIsNearInfiniteLimit(clock, i);
          }
          const excessJob = firstTimer(clock);
          throw getInfiniteLoopError(clock, excessJob);
        };
        clock.runToFrame = function runToFrame() {
          return clock.tick(getTimeToNextFrame());
        };
        if (typeof _global.Promise !== "undefined") {
          clock.runAllAsync = function runAllAsync() {
            return pauseAutoTickUntilFinished(
              new _global.Promise(function(resolve, reject) {
                let i = 0;
                function doRun() {
                  originalSetTimeout(function() {
                    try {
                      runJobs(clock);
                      let numTimers;
                      if (i < clock.loopLimit) {
                        if (!clock.timers) {
                          resetIsNearInfiniteLimit();
                          resolve(clock.now);
                          return;
                        }
                        numTimers = Object.keys(
                          clock.timers
                        ).length;
                        if (numTimers === 0) {
                          resetIsNearInfiniteLimit();
                          resolve(clock.now);
                          return;
                        }
                        clock.next();
                        i++;
                        doRun();
                        checkIsNearInfiniteLimit(clock, i);
                        return;
                      }
                      const excessJob = firstTimer(clock);
                      reject(
                        getInfiniteLoopError(clock, excessJob)
                      );
                    } catch (e) {
                      reject(e);
                    }
                  });
                }
                doRun();
              })
            );
          };
        }
        clock.runToLast = function runToLast() {
          const timer = lastTimer(clock);
          if (!timer) {
            runJobs(clock);
            return clock.now;
          }
          return clock.tick(timer.callAt - clock.now);
        };
        if (typeof _global.Promise !== "undefined") {
          clock.runToLastAsync = function runToLastAsync() {
            return pauseAutoTickUntilFinished(
              new _global.Promise(function(resolve, reject) {
                originalSetTimeout(function() {
                  try {
                    const timer = lastTimer(clock);
                    if (!timer) {
                      runJobs(clock);
                      resolve(clock.now);
                    }
                    resolve(
                      clock.tickAsync(timer.callAt - clock.now)
                    );
                  } catch (e) {
                    reject(e);
                  }
                });
              })
            );
          };
        }
        clock.reset = function reset() {
          nanos = 0;
          clock.timers = {};
          clock.jobs = [];
          clock.now = start;
        };
        clock.setSystemTime = function setSystemTime(systemTime) {
          const newNow = getEpoch(systemTime);
          const difference = newNow - clock.now;
          let id, timer;
          adjustedSystemTime[0] = adjustedSystemTime[0] + difference;
          adjustedSystemTime[1] = adjustedSystemTime[1] + nanos;
          clock.now = newNow;
          nanos = 0;
          for (id in clock.timers) {
            if (clock.timers.hasOwnProperty(id)) {
              timer = clock.timers[id];
              timer.createdAt += difference;
              timer.callAt += difference;
            }
          }
        };
        clock.jump = function jump(tickValue) {
          const msFloat = typeof tickValue === "number" ? tickValue : parseTime(tickValue);
          const ms = Math.floor(msFloat);
          for (const timer of Object.values(clock.timers)) {
            if (clock.now + ms > timer.callAt) {
              timer.callAt = clock.now + ms;
            }
          }
          clock.tick(ms);
        };
        if (isPresent.performance) {
          clock.performance = /* @__PURE__ */ Object.create(null);
          clock.performance.now = fakePerformanceNow;
        }
        if (isPresent.hrtime) {
          clock.hrtime = hrtime;
        }
        return clock;
      }
      function createIntervalTick(clock, delta) {
        const intervalTick = doIntervalTick.bind(null, clock, delta);
        const intervalId = originalSetInterval(intervalTick, delta);
        clock.attachedInterval = intervalId;
      }
      function install(config) {
        if (arguments.length > 1 || config instanceof Date || Array.isArray(config) || typeof config === "number") {
          throw new TypeError(
            `FakeTimers.install called with ${String(
              config
            )} install requires an object parameter`
          );
        }
        if (_global.Date.isFake === true) {
          throw new TypeError(
            "Can't install fake timers twice on the same global object."
          );
        }
        config = typeof config !== "undefined" ? config : {};
        config.shouldAdvanceTime = config.shouldAdvanceTime || false;
        config.advanceTimeDelta = config.advanceTimeDelta || 20;
        config.shouldClearNativeTimers = config.shouldClearNativeTimers || false;
        if (config.target) {
          throw new TypeError(
            "config.target is no longer supported. Use `withGlobal(target)` instead."
          );
        }
        function handleMissingTimer(timer) {
          if (config.ignoreMissingTimers) {
            return;
          }
          throw new ReferenceError(
            `non-existent timers and/or objects cannot be faked: '${timer}'`
          );
        }
        let i, l;
        const clock = createClock(config.now, config.loopLimit);
        clock.shouldClearNativeTimers = config.shouldClearNativeTimers;
        clock.uninstall = function() {
          return uninstall(clock);
        };
        clock.abortListenerMap = /* @__PURE__ */ new Map();
        clock.methods = config.toFake || [];
        if (clock.methods.length === 0) {
          clock.methods = Object.keys(timers);
        }
        if (config.shouldAdvanceTime === true) {
          clock.setTickMode({
            mode: "interval",
            delta: config.advanceTimeDelta
          });
        }
        if (clock.methods.includes("performance")) {
          const proto = (() => {
            if (hasPerformanceConstructorPrototype) {
              return _global.performance.constructor.prototype;
            }
            if (hasPerformancePrototype) {
              return _global.Performance.prototype;
            }
          })();
          if (proto) {
            Object.getOwnPropertyNames(proto).forEach(function(name) {
              if (name !== "now") {
                clock.performance[name] = name.indexOf("getEntries") === 0 ? NOOP_ARRAY : NOOP;
              }
            });
            clock.performance.mark = (name) => new FakePerformanceEntry(name, "mark", 0, 0);
            clock.performance.measure = (name) => new FakePerformanceEntry(name, "measure", 0, 100);
            clock.performance.timeOrigin = getEpoch(config.now);
          } else if ((config.toFake || []).includes("performance")) {
            return handleMissingTimer("performance");
          }
        }
        if (_global === globalObject && timersModule) {
          clock.timersModuleMethods = [];
        }
        if (_global === globalObject && timersPromisesModule) {
          clock.timersPromisesModuleMethods = [];
        }
        for (i = 0, l = clock.methods.length; i < l; i++) {
          const nameOfMethodToReplace = clock.methods[i];
          if (!isPresent[nameOfMethodToReplace]) {
            handleMissingTimer(nameOfMethodToReplace);
            continue;
          }
          if (nameOfMethodToReplace === "hrtime") {
            if (_global.process && typeof _global.process.hrtime === "function") {
              hijackMethod(_global.process, nameOfMethodToReplace, clock);
            }
          } else if (nameOfMethodToReplace === "nextTick") {
            if (_global.process && typeof _global.process.nextTick === "function") {
              hijackMethod(_global.process, nameOfMethodToReplace, clock);
            }
          } else {
            hijackMethod(_global, nameOfMethodToReplace, clock);
          }
          if (clock.timersModuleMethods !== void 0 && timersModule[nameOfMethodToReplace]) {
            const original = timersModule[nameOfMethodToReplace];
            clock.timersModuleMethods.push({
              methodName: nameOfMethodToReplace,
              original
            });
            timersModule[nameOfMethodToReplace] = _global[nameOfMethodToReplace];
          }
          if (clock.timersPromisesModuleMethods !== void 0) {
            if (nameOfMethodToReplace === "setTimeout") {
              clock.timersPromisesModuleMethods.push({
                methodName: "setTimeout",
                original: timersPromisesModule.setTimeout
              });
              timersPromisesModule.setTimeout = (delay, value, options = {}) => new Promise((resolve, reject) => {
                const abort = () => {
                  options.signal.removeEventListener(
                    "abort",
                    abort
                  );
                  clock.abortListenerMap.delete(abort);
                  clock.clearTimeout(handle);
                  reject(options.signal.reason);
                };
                const handle = clock.setTimeout(() => {
                  if (options.signal) {
                    options.signal.removeEventListener(
                      "abort",
                      abort
                    );
                    clock.abortListenerMap.delete(abort);
                  }
                  resolve(value);
                }, delay);
                if (options.signal) {
                  if (options.signal.aborted) {
                    abort();
                  } else {
                    options.signal.addEventListener(
                      "abort",
                      abort
                    );
                    clock.abortListenerMap.set(
                      abort,
                      options.signal
                    );
                  }
                }
              });
            } else if (nameOfMethodToReplace === "setImmediate") {
              clock.timersPromisesModuleMethods.push({
                methodName: "setImmediate",
                original: timersPromisesModule.setImmediate
              });
              timersPromisesModule.setImmediate = (value, options = {}) => new Promise((resolve, reject) => {
                const abort = () => {
                  options.signal.removeEventListener(
                    "abort",
                    abort
                  );
                  clock.abortListenerMap.delete(abort);
                  clock.clearImmediate(handle);
                  reject(options.signal.reason);
                };
                const handle = clock.setImmediate(() => {
                  if (options.signal) {
                    options.signal.removeEventListener(
                      "abort",
                      abort
                    );
                    clock.abortListenerMap.delete(abort);
                  }
                  resolve(value);
                });
                if (options.signal) {
                  if (options.signal.aborted) {
                    abort();
                  } else {
                    options.signal.addEventListener(
                      "abort",
                      abort
                    );
                    clock.abortListenerMap.set(
                      abort,
                      options.signal
                    );
                  }
                }
              });
            } else if (nameOfMethodToReplace === "setInterval") {
              clock.timersPromisesModuleMethods.push({
                methodName: "setInterval",
                original: timersPromisesModule.setInterval
              });
              timersPromisesModule.setInterval = (delay, value, options = {}) => ({
                [Symbol.asyncIterator]: () => {
                  const createResolvable = () => {
                    let resolve, reject;
                    const promise = new Promise((res, rej) => {
                      resolve = res;
                      reject = rej;
                    });
                    promise.resolve = resolve;
                    promise.reject = reject;
                    return promise;
                  };
                  let done = false;
                  let hasThrown = false;
                  let returnCall;
                  let nextAvailable = 0;
                  const nextQueue = [];
                  const handle = clock.setInterval(() => {
                    if (nextQueue.length > 0) {
                      nextQueue.shift().resolve();
                    } else {
                      nextAvailable++;
                    }
                  }, delay);
                  const abort = () => {
                    options.signal.removeEventListener(
                      "abort",
                      abort
                    );
                    clock.abortListenerMap.delete(abort);
                    clock.clearInterval(handle);
                    done = true;
                    for (const resolvable of nextQueue) {
                      resolvable.resolve();
                    }
                  };
                  if (options.signal) {
                    if (options.signal.aborted) {
                      done = true;
                    } else {
                      options.signal.addEventListener(
                        "abort",
                        abort
                      );
                      clock.abortListenerMap.set(
                        abort,
                        options.signal
                      );
                    }
                  }
                  return {
                    next: async () => {
                      if (options.signal?.aborted && !hasThrown) {
                        hasThrown = true;
                        throw options.signal.reason;
                      }
                      if (done) {
                        return { done: true, value: void 0 };
                      }
                      if (nextAvailable > 0) {
                        nextAvailable--;
                        return { done: false, value };
                      }
                      const resolvable = createResolvable();
                      nextQueue.push(resolvable);
                      await resolvable;
                      if (returnCall && nextQueue.length === 0) {
                        returnCall.resolve();
                      }
                      if (options.signal?.aborted && !hasThrown) {
                        hasThrown = true;
                        throw options.signal.reason;
                      }
                      if (done) {
                        return { done: true, value: void 0 };
                      }
                      return { done: false, value };
                    },
                    return: async () => {
                      if (done) {
                        return { done: true, value: void 0 };
                      }
                      if (nextQueue.length > 0) {
                        returnCall = createResolvable();
                        await returnCall;
                      }
                      clock.clearInterval(handle);
                      done = true;
                      if (options.signal) {
                        options.signal.removeEventListener(
                          "abort",
                          abort
                        );
                        clock.abortListenerMap.delete(abort);
                      }
                      return { done: true, value: void 0 };
                    }
                  };
                }
              });
            }
          }
        }
        return clock;
      }
      return {
        timers,
        createClock,
        install,
        withGlobal
      };
    }
    var defaultImplementation = withGlobal(globalObject);
    exports.timers = defaultImplementation.timers;
    exports.createClock = defaultImplementation.createClock;
    exports.install = defaultImplementation.install;
    exports.withGlobal = withGlobal;
  }
});

// lib/sinon/util/fake-timers.js
var require_fake_timers = __commonJS({
  "lib/sinon/util/fake-timers.js"(exports) {
    "use strict";
    var extend = require_extend();
    var FakeTimers = require_fake_timers_src();
    var globalObject = require_lib().global;
    function createClock(config, globalCtx) {
      let FakeTimersCtx = FakeTimers;
      if (globalCtx !== null && typeof globalCtx === "object") {
        FakeTimersCtx = FakeTimers.withGlobal(globalCtx);
      }
      const clock = FakeTimersCtx.install(config);
      clock.restore = clock.uninstall;
      return clock;
    }
    function addIfDefined(obj, globalPropName) {
      const globalProp = globalObject[globalPropName];
      if (typeof globalProp !== "undefined") {
        obj[globalPropName] = globalProp;
      }
    }
    exports.useFakeTimers = function(dateOrConfig) {
      const hasArguments = typeof dateOrConfig !== "undefined";
      const argumentIsDateLike = (typeof dateOrConfig === "number" || dateOrConfig instanceof Date) && arguments.length === 1;
      const argumentIsObject = dateOrConfig !== null && typeof dateOrConfig === "object" && arguments.length === 1;
      if (!hasArguments) {
        return createClock({
          now: 0
        });
      }
      if (argumentIsDateLike) {
        return createClock({
          now: dateOrConfig
        });
      }
      if (argumentIsObject) {
        const config = extend.nonEnum({}, dateOrConfig);
        const globalCtx = config.global;
        delete config.global;
        return createClock(config, globalCtx);
      }
      throw new TypeError(
        "useFakeTimers expected epoch or config object. See https://github.com/sinonjs/sinon"
      );
    };
    exports.clock = {
      create: function(now) {
        return FakeTimers.createClock(now);
      }
    };
    var timers = {
      setTimeout,
      clearTimeout,
      setInterval,
      clearInterval,
      Date
    };
    addIfDefined(timers, "setImmediate");
    addIfDefined(timers, "clearImmediate");
    exports.timers = timers;
  }
});

// lib/sinon/proxy-call-util.js
var require_proxy_call_util = __commonJS({
  "lib/sinon/proxy-call-util.js"(exports) {
    "use strict";
    var push = require_lib().prototypes.array.push;
    exports.incrementCallCount = function incrementCallCount(proxy) {
      proxy.called = true;
      proxy.callCount += 1;
      proxy.notCalled = false;
      proxy.calledOnce = proxy.callCount === 1;
      proxy.calledTwice = proxy.callCount === 2;
      proxy.calledThrice = proxy.callCount === 3;
    };
    exports.createCallProperties = function createCallProperties(proxy) {
      proxy.firstCall = proxy.getCall(0);
      proxy.secondCall = proxy.getCall(1);
      proxy.thirdCall = proxy.getCall(2);
      proxy.lastCall = proxy.getCall(proxy.callCount - 1);
    };
    exports.delegateToCalls = function delegateToCalls(proxy, method, matchAny, actual, returnsValues, notCalled, totalCallCount) {
      proxy[method] = function() {
        if (!this.called) {
          if (notCalled) {
            return notCalled.apply(this, arguments);
          }
          return false;
        }
        if (totalCallCount !== void 0 && this.callCount !== totalCallCount) {
          return false;
        }
        let currentCall;
        let matches = 0;
        const returnValues = [];
        for (let i = 0, l = this.callCount; i < l; i += 1) {
          currentCall = this.getCall(i);
          const returnValue = currentCall[actual || method].apply(
            currentCall,
            arguments
          );
          push(returnValues, returnValue);
          if (returnValue) {
            matches += 1;
            if (matchAny) {
              return true;
            }
          }
        }
        if (returnsValues) {
          return returnValues;
        }
        return matches === this.callCount;
      };
    };
  }
});

// lib/sinon/proxy-invoke.js
var require_proxy_invoke = __commonJS({
  "lib/sinon/proxy-invoke.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var proxyCallUtil = require_proxy_call_util();
    var push = arrayProto.push;
    var forEach = arrayProto.forEach;
    var concat = arrayProto.concat;
    var ErrorConstructor = Error.prototype.constructor;
    var bind = Function.prototype.bind;
    var callId = 0;
    module.exports = function invoke(func, thisValue, args) {
      const matchings = this.matchingFakes(args);
      const currentCallId = callId++;
      let exception, returnValue;
      proxyCallUtil.incrementCallCount(this);
      push(this.thisValues, thisValue);
      push(this.args, args);
      push(this.callIds, currentCallId);
      forEach(matchings, function(matching) {
        proxyCallUtil.incrementCallCount(matching);
        push(matching.thisValues, thisValue);
        push(matching.args, args);
        push(matching.callIds, currentCallId);
      });
      proxyCallUtil.createCallProperties(this);
      forEach(matchings, proxyCallUtil.createCallProperties);
      try {
        this.invoking = true;
        const thisCall = this.getCall(this.callCount - 1);
        if (thisCall.calledWithNew()) {
          returnValue = new (bind.apply(
            this.func || func,
            concat([thisValue], args)
          ))();
          if (typeof returnValue !== "object" && typeof returnValue !== "function") {
            returnValue = thisValue;
          }
        } else {
          returnValue = (this.func || func).apply(thisValue, args);
        }
      } catch (e) {
        exception = e;
      } finally {
        delete this.invoking;
      }
      push(this.exceptions, exception);
      push(this.returnValues, returnValue);
      forEach(matchings, function(matching) {
        push(matching.exceptions, exception);
        push(matching.returnValues, returnValue);
      });
      const err = new ErrorConstructor();
      try {
        throw err;
      } catch (e) {
      }
      push(this.errorsWithCallStack, err);
      forEach(matchings, function(matching) {
        push(matching.errorsWithCallStack, err);
      });
      proxyCallUtil.createCallProperties(this);
      forEach(matchings, proxyCallUtil.createCallProperties);
      if (exception !== void 0) {
        throw exception;
      }
      return returnValue;
    };
  }
});

// lib/sinon/proxy-call.js
var require_proxy_call = __commonJS({
  "lib/sinon/proxy-call.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var match = require_samsam().createMatcher;
    var deepEqual = require_samsam().deepEqual;
    var functionName = require_lib().functionName;
    var inspect = require_util().inspect;
    var valueToString = require_lib().valueToString;
    var concat = arrayProto.concat;
    var filter = arrayProto.filter;
    var join = arrayProto.join;
    var map = arrayProto.map;
    var reduce = arrayProto.reduce;
    var slice = arrayProto.slice;
    function throwYieldError(proxy, text, args) {
      let msg = functionName(proxy) + text;
      if (args.length) {
        msg += ` Received [${join(slice(args), ", ")}]`;
      }
      throw new Error(msg);
    }
    var callProto = {
      calledOn: function calledOn(thisValue) {
        if (match.isMatcher(thisValue)) {
          return thisValue.test(this.thisValue);
        }
        return this.thisValue === thisValue;
      },
      calledWith: function calledWith() {
        const self2 = this;
        const calledWithArgs = slice(arguments);
        if (calledWithArgs.length > self2.args.length) {
          return false;
        }
        return reduce(
          calledWithArgs,
          function(prev, arg, i) {
            return prev && deepEqual(self2.args[i], arg);
          },
          true
        );
      },
      calledWithMatch: function calledWithMatch() {
        const self2 = this;
        const calledWithMatchArgs = slice(arguments);
        if (calledWithMatchArgs.length > self2.args.length) {
          return false;
        }
        return reduce(
          calledWithMatchArgs,
          function(prev, expectation, i) {
            const actual = self2.args[i];
            return prev && match(expectation).test(actual);
          },
          true
        );
      },
      calledWithExactly: function calledWithExactly() {
        return arguments.length === this.args.length && this.calledWith.apply(this, arguments);
      },
      notCalledWith: function notCalledWith() {
        return !this.calledWith.apply(this, arguments);
      },
      notCalledWithMatch: function notCalledWithMatch() {
        return !this.calledWithMatch.apply(this, arguments);
      },
      returned: function returned(value) {
        return deepEqual(this.returnValue, value);
      },
      threw: function threw(error) {
        if (typeof error === "undefined" || !this.exception) {
          return Boolean(this.exception);
        }
        return this.exception === error || this.exception.name === error;
      },
      calledWithNew: function calledWithNew() {
        return this.proxy.prototype && this.thisValue instanceof this.proxy;
      },
      calledBefore: function(other) {
        return this.callId < other.callId;
      },
      calledAfter: function(other) {
        return this.callId > other.callId;
      },
      calledImmediatelyBefore: function(other) {
        return this.callId === other.callId - 1;
      },
      calledImmediatelyAfter: function(other) {
        return this.callId === other.callId + 1;
      },
      callArg: function(pos) {
        this.ensureArgIsAFunction(pos);
        return this.args[pos]();
      },
      callArgOn: function(pos, thisValue) {
        this.ensureArgIsAFunction(pos);
        return this.args[pos].apply(thisValue);
      },
      callArgWith: function(pos) {
        return this.callArgOnWith.apply(
          this,
          concat([pos, null], slice(arguments, 1))
        );
      },
      callArgOnWith: function(pos, thisValue) {
        this.ensureArgIsAFunction(pos);
        const args = slice(arguments, 2);
        return this.args[pos].apply(thisValue, args);
      },
      throwArg: function(pos) {
        if (pos > this.args.length) {
          throw new TypeError(
            `Not enough arguments: ${pos} required but only ${this.args.length} present`
          );
        }
        throw this.args[pos];
      },
      yield: function() {
        return this.yieldOn.apply(this, concat([null], slice(arguments, 0)));
      },
      yieldOn: function(thisValue) {
        const args = slice(this.args);
        const yieldFn = filter(args, function(arg) {
          return typeof arg === "function";
        })[0];
        if (!yieldFn) {
          throwYieldError(
            this.proxy,
            " cannot yield since no callback was passed.",
            args
          );
        }
        return yieldFn.apply(thisValue, slice(arguments, 1));
      },
      yieldTo: function(prop) {
        return this.yieldToOn.apply(
          this,
          concat([prop, null], slice(arguments, 1))
        );
      },
      yieldToOn: function(prop, thisValue) {
        const args = slice(this.args);
        const yieldArg = filter(args, function(arg) {
          return arg && typeof arg[prop] === "function";
        })[0];
        const yieldFn = yieldArg && yieldArg[prop];
        if (!yieldFn) {
          throwYieldError(
            this.proxy,
            ` cannot yield to '${valueToString(
              prop
            )}' since no callback was passed.`,
            args
          );
        }
        return yieldFn.apply(thisValue, slice(arguments, 2));
      },
      toString: function() {
        if (!this.args) {
          return ":(";
        }
        let callStr = this.proxy ? `${String(this.proxy)}(` : "";
        const formattedArgs = map(this.args, function(arg) {
          return inspect(arg);
        });
        callStr = `${callStr + join(formattedArgs, ", ")})`;
        if (typeof this.returnValue !== "undefined") {
          callStr += ` => ${inspect(this.returnValue)}`;
        }
        if (this.exception) {
          callStr += ` !${this.exception.name}`;
          if (this.exception.message) {
            callStr += `(${this.exception.message})`;
          }
        }
        if (this.stack) {
          callStr += (this.stack.split("\n")[3] || "unknown").replace(
            /^\s*(?:at\s+|@)?/,
            " at "
          );
        }
        return callStr;
      },
      ensureArgIsAFunction: function(pos) {
        if (typeof this.args[pos] !== "function") {
          throw new TypeError(
            `Expected argument at position ${pos} to be a Function, but was ${typeof this.args[pos]}`
          );
        }
      }
    };
    Object.defineProperty(callProto, "stack", {
      enumerable: true,
      configurable: true,
      get: function() {
        return this.errorWithCallStack && this.errorWithCallStack.stack || "";
      }
    });
    callProto.invokeCallback = callProto.yield;
    function createProxyCall(proxy, thisValue, args, returnValue, exception, id, errorWithCallStack) {
      if (typeof id !== "number") {
        throw new TypeError("Call id is not a number");
      }
      let firstArg, lastArg;
      if (args.length > 0) {
        firstArg = args[0];
        lastArg = args[args.length - 1];
      }
      const proxyCall = Object.create(callProto);
      const callback = lastArg && typeof lastArg === "function" ? lastArg : void 0;
      proxyCall.proxy = proxy;
      proxyCall.thisValue = thisValue;
      proxyCall.args = args;
      proxyCall.firstArg = firstArg;
      proxyCall.lastArg = lastArg;
      proxyCall.callback = callback;
      proxyCall.returnValue = returnValue;
      proxyCall.exception = exception;
      proxyCall.callId = id;
      proxyCall.errorWithCallStack = errorWithCallStack;
      return proxyCall;
    }
    createProxyCall.toString = callProto.toString;
    module.exports = createProxyCall;
  }
});

// lib/sinon/default-behaviors.js
var require_default_behaviors = __commonJS({
  "lib/sinon/default-behaviors.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var isPropertyConfigurable = require_is_property_configurable();
    var exportAsyncBehaviors = require_export_async_behaviors();
    var extend = require_extend();
    var slice = arrayProto.slice;
    var useLeftMostCallback = -1;
    var useRightMostCallback = -2;
    function throwsException(fake, error, message) {
      if (typeof error === "function") {
        fake.exceptionCreator = error;
      } else if (typeof error === "string") {
        fake.exceptionCreator = function() {
          const newException = new Error(
            message || `Sinon-provided ${error}`
          );
          newException.name = error;
          return newException;
        };
      } else if (!error) {
        fake.exceptionCreator = function() {
          return new Error("Error");
        };
      } else {
        fake.exception = error;
      }
    }
    var defaultBehaviors = {
      callsFake: function callsFake(fake, fn) {
        fake.fakeFn = fn;
        fake.exception = void 0;
        fake.exceptionCreator = void 0;
        fake.callsThrough = false;
      },
      callsArg: function callsArg(fake, index) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.callArgAt = index;
        fake.callbackArguments = [];
        fake.callbackContext = void 0;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.callsThrough = false;
      },
      callsArgOn: function callsArgOn(fake, index, context) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.callArgAt = index;
        fake.callbackArguments = [];
        fake.callbackContext = context;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.callsThrough = false;
      },
      callsArgWith: function callsArgWith(fake, index) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.callArgAt = index;
        fake.callbackArguments = slice(arguments, 2);
        fake.callbackContext = void 0;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.callsThrough = false;
      },
      callsArgOnWith: function callsArgWith(fake, index, context) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.callArgAt = index;
        fake.callbackArguments = slice(arguments, 3);
        fake.callbackContext = context;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.callsThrough = false;
      },
      yields: function(fake) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 1);
        fake.callbackContext = void 0;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.fakeFn = void 0;
        fake.callsThrough = false;
      },
      yieldsRight: function(fake) {
        fake.callArgAt = useRightMostCallback;
        fake.callbackArguments = slice(arguments, 1);
        fake.callbackContext = void 0;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.callsThrough = false;
        fake.fakeFn = void 0;
      },
      yieldsOn: function(fake, context) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 2);
        fake.callbackContext = context;
        fake.callArgProp = void 0;
        fake.callbackAsync = false;
        fake.callsThrough = false;
        fake.fakeFn = void 0;
      },
      yieldsTo: function(fake, prop) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 2);
        fake.callbackContext = void 0;
        fake.callArgProp = prop;
        fake.callbackAsync = false;
        fake.callsThrough = false;
        fake.fakeFn = void 0;
      },
      yieldsToOn: function(fake, prop, context) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 3);
        fake.callbackContext = context;
        fake.callArgProp = prop;
        fake.callbackAsync = false;
        fake.fakeFn = void 0;
      },
      throws: throwsException,
      throwsException,
      returns: function returns(fake, value) {
        fake.callsThrough = false;
        fake.returnValue = value;
        fake.resolve = false;
        fake.reject = false;
        fake.returnValueDefined = true;
        fake.exception = void 0;
        fake.exceptionCreator = void 0;
        fake.fakeFn = void 0;
      },
      returnsArg: function returnsArg(fake, index) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.callsThrough = false;
        fake.returnArgAt = index;
      },
      throwsArg: function throwsArg(fake, index) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.callsThrough = false;
        fake.throwArgAt = index;
      },
      returnsThis: function returnsThis(fake) {
        fake.returnThis = true;
        fake.callsThrough = false;
      },
      resolves: function resolves(fake, value) {
        fake.returnValue = value;
        fake.resolve = true;
        fake.resolveThis = false;
        fake.reject = false;
        fake.returnValueDefined = true;
        fake.exception = void 0;
        fake.exceptionCreator = void 0;
        fake.fakeFn = void 0;
        fake.callsThrough = false;
      },
      resolvesArg: function resolvesArg(fake, index) {
        if (typeof index !== "number") {
          throw new TypeError("argument index is not number");
        }
        fake.resolveArgAt = index;
        fake.returnValue = void 0;
        fake.resolve = true;
        fake.resolveThis = false;
        fake.reject = false;
        fake.returnValueDefined = false;
        fake.exception = void 0;
        fake.exceptionCreator = void 0;
        fake.fakeFn = void 0;
        fake.callsThrough = false;
      },
      rejects: function rejects(fake, error, message) {
        let reason;
        if (typeof error === "string") {
          reason = new Error(message || "");
          reason.name = error;
        } else if (!error) {
          reason = new Error("Error");
        } else {
          reason = error;
        }
        fake.returnValue = reason;
        fake.resolve = false;
        fake.resolveThis = false;
        fake.reject = true;
        fake.returnValueDefined = true;
        fake.exception = void 0;
        fake.exceptionCreator = void 0;
        fake.fakeFn = void 0;
        fake.callsThrough = false;
        return fake;
      },
      resolvesThis: function resolvesThis(fake) {
        fake.returnValue = void 0;
        fake.resolve = false;
        fake.resolveThis = true;
        fake.reject = false;
        fake.returnValueDefined = false;
        fake.exception = void 0;
        fake.exceptionCreator = void 0;
        fake.fakeFn = void 0;
        fake.callsThrough = false;
      },
      callThrough: function callThrough(fake) {
        fake.callsThrough = true;
      },
      callThroughWithNew: function callThroughWithNew(fake) {
        fake.callsThroughWithNew = true;
      },
      get: function get(fake, getterFunction) {
        const rootStub = fake.stub || fake;
        Object.defineProperty(rootStub.rootObj, rootStub.propName, {
          get: getterFunction,
          configurable: isPropertyConfigurable(
            rootStub.rootObj,
            rootStub.propName
          )
        });
        return fake;
      },
      set: function set(fake, setterFunction) {
        const rootStub = fake.stub || fake;
        Object.defineProperty(
          rootStub.rootObj,
          rootStub.propName,
          // eslint-disable-next-line accessor-pairs
          {
            set: setterFunction,
            configurable: isPropertyConfigurable(
              rootStub.rootObj,
              rootStub.propName
            )
          }
        );
        return fake;
      },
      value: function value(fake, newVal) {
        const rootStub = fake.stub || fake;
        Object.defineProperty(rootStub.rootObj, rootStub.propName, {
          value: newVal,
          enumerable: true,
          writable: true,
          configurable: rootStub.shadowsPropOnPrototype || isPropertyConfigurable(rootStub.rootObj, rootStub.propName)
        });
        return fake;
      }
    };
    var asyncBehaviors = exportAsyncBehaviors(defaultBehaviors);
    module.exports = extend({}, defaultBehaviors, asyncBehaviors);
  }
});

// lib/sinon/util/core/function-to-string.js
var require_function_to_string = __commonJS({
  "lib/sinon/util/core/function-to-string.js"(exports, module) {
    "use strict";
    module.exports = function toString() {
      let i, prop, thisValue;
      if (this.getCall && this.callCount) {
        i = this.callCount;
        while (i--) {
          thisValue = this.getCall(i).thisValue;
          for (prop in thisValue) {
            try {
              if (thisValue[prop] === this) {
                return prop;
              }
            } catch (e) {
            }
          }
        }
      }
      return this.displayName || "sinon fake";
    };
  }
});

// node_modules/supports-color/browser.js
var require_browser = __commonJS({
  "node_modules/supports-color/browser.js"(exports, module) {
    "use strict";
    module.exports = {
      stdout: false,
      stderr: false
    };
  }
});

// lib/sinon/colorizer.js
var require_colorizer = __commonJS({
  "lib/sinon/colorizer.js"(exports, module) {
    "use strict";
    module.exports = class Colorizer {
      constructor(supportsColor = require_browser()) {
        this.supportsColor = supportsColor;
      }
      /**
       * Should be renamed to true #privateField
       * when we can ensure ES2022 support
       *
       * @private
       */
      colorize(str, color) {
        if (this.supportsColor.stdout === false) {
          return str;
        }
        return `\x1B[${color}m${str}\x1B[0m`;
      }
      red(str) {
        return this.colorize(str, 31);
      }
      green(str) {
        return this.colorize(str, 32);
      }
      cyan(str) {
        return this.colorize(str, 96);
      }
      white(str) {
        return this.colorize(str, 39);
      }
      bold(str) {
        return this.colorize(str, 1);
      }
    };
  }
});

// node_modules/diff/libcjs/diff/base.js
var require_base = __commonJS({
  "node_modules/diff/libcjs/diff/base.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    var Diff = (
      /** @class */
      (function() {
        function Diff2() {
        }
        Diff2.prototype.diff = function(oldStr, newStr, options) {
          if (options === void 0) {
            options = {};
          }
          var callback;
          if (typeof options === "function") {
            callback = options;
            options = {};
          } else if ("callback" in options) {
            callback = options.callback;
          }
          var oldString = this.castInput(oldStr, options);
          var newString = this.castInput(newStr, options);
          var oldTokens = this.removeEmpty(this.tokenize(oldString, options));
          var newTokens = this.removeEmpty(this.tokenize(newString, options));
          return this.diffWithOptionsObj(oldTokens, newTokens, options, callback);
        };
        Diff2.prototype.diffWithOptionsObj = function(oldTokens, newTokens, options, callback) {
          var _this = this;
          var _a;
          var done = function(value) {
            value = _this.postProcess(value, options);
            if (callback) {
              setTimeout(function() {
                callback(value);
              }, 0);
              return void 0;
            } else {
              return value;
            }
          };
          var newLen = newTokens.length, oldLen = oldTokens.length;
          var editLength = 1;
          var maxEditLength = newLen + oldLen;
          if (options.maxEditLength != null) {
            maxEditLength = Math.min(maxEditLength, options.maxEditLength);
          }
          var maxExecutionTime = (_a = options.timeout) !== null && _a !== void 0 ? _a : Infinity;
          var abortAfterTimestamp = Date.now() + maxExecutionTime;
          var bestPath = [{ oldPos: -1, lastComponent: void 0 }];
          var newPos = this.extractCommon(bestPath[0], newTokens, oldTokens, 0, options);
          if (bestPath[0].oldPos + 1 >= oldLen && newPos + 1 >= newLen) {
            return done(this.buildValues(bestPath[0].lastComponent, newTokens, oldTokens));
          }
          var minDiagonalToConsider = -Infinity, maxDiagonalToConsider = Infinity;
          var execEditLength = function() {
            for (var diagonalPath = Math.max(minDiagonalToConsider, -editLength); diagonalPath <= Math.min(maxDiagonalToConsider, editLength); diagonalPath += 2) {
              var basePath = void 0;
              var removePath = bestPath[diagonalPath - 1], addPath = bestPath[diagonalPath + 1];
              if (removePath) {
                bestPath[diagonalPath - 1] = void 0;
              }
              var canAdd = false;
              if (addPath) {
                var addPathNewPos = addPath.oldPos - diagonalPath;
                canAdd = addPath && 0 <= addPathNewPos && addPathNewPos < newLen;
              }
              var canRemove = removePath && removePath.oldPos + 1 < oldLen;
              if (!canAdd && !canRemove) {
                bestPath[diagonalPath] = void 0;
                continue;
              }
              if (!canRemove || canAdd && removePath.oldPos < addPath.oldPos) {
                basePath = _this.addToPath(addPath, true, false, 0, options);
              } else {
                basePath = _this.addToPath(removePath, false, true, 1, options);
              }
              newPos = _this.extractCommon(basePath, newTokens, oldTokens, diagonalPath, options);
              if (basePath.oldPos + 1 >= oldLen && newPos + 1 >= newLen) {
                return done(_this.buildValues(basePath.lastComponent, newTokens, oldTokens)) || true;
              } else {
                bestPath[diagonalPath] = basePath;
                if (basePath.oldPos + 1 >= oldLen) {
                  maxDiagonalToConsider = Math.min(maxDiagonalToConsider, diagonalPath - 1);
                }
                if (newPos + 1 >= newLen) {
                  minDiagonalToConsider = Math.max(minDiagonalToConsider, diagonalPath + 1);
                }
              }
            }
            editLength++;
          };
          if (callback) {
            (function exec() {
              setTimeout(function() {
                if (editLength > maxEditLength || Date.now() > abortAfterTimestamp) {
                  return callback(void 0);
                }
                if (!execEditLength()) {
                  exec();
                }
              }, 0);
            })();
          } else {
            while (editLength <= maxEditLength && Date.now() <= abortAfterTimestamp) {
              var ret = execEditLength();
              if (ret) {
                return ret;
              }
            }
          }
        };
        Diff2.prototype.addToPath = function(path, added, removed, oldPosInc, options) {
          var last = path.lastComponent;
          if (last && !options.oneChangePerToken && last.added === added && last.removed === removed) {
            return {
              oldPos: path.oldPos + oldPosInc,
              lastComponent: { count: last.count + 1, added, removed, previousComponent: last.previousComponent }
            };
          } else {
            return {
              oldPos: path.oldPos + oldPosInc,
              lastComponent: { count: 1, added, removed, previousComponent: last }
            };
          }
        };
        Diff2.prototype.extractCommon = function(basePath, newTokens, oldTokens, diagonalPath, options) {
          var newLen = newTokens.length, oldLen = oldTokens.length;
          var oldPos = basePath.oldPos, newPos = oldPos - diagonalPath, commonCount = 0;
          while (newPos + 1 < newLen && oldPos + 1 < oldLen && this.equals(oldTokens[oldPos + 1], newTokens[newPos + 1], options)) {
            newPos++;
            oldPos++;
            commonCount++;
            if (options.oneChangePerToken) {
              basePath.lastComponent = { count: 1, previousComponent: basePath.lastComponent, added: false, removed: false };
            }
          }
          if (commonCount && !options.oneChangePerToken) {
            basePath.lastComponent = { count: commonCount, previousComponent: basePath.lastComponent, added: false, removed: false };
          }
          basePath.oldPos = oldPos;
          return newPos;
        };
        Diff2.prototype.equals = function(left, right, options) {
          if (options.comparator) {
            return options.comparator(left, right);
          } else {
            return left === right || !!options.ignoreCase && left.toLowerCase() === right.toLowerCase();
          }
        };
        Diff2.prototype.removeEmpty = function(array) {
          var ret = [];
          for (var i = 0; i < array.length; i++) {
            if (array[i]) {
              ret.push(array[i]);
            }
          }
          return ret;
        };
        Diff2.prototype.castInput = function(value, options) {
          return value;
        };
        Diff2.prototype.tokenize = function(value, options) {
          return Array.from(value);
        };
        Diff2.prototype.join = function(chars) {
          return chars.join("");
        };
        Diff2.prototype.postProcess = function(changeObjects, options) {
          return changeObjects;
        };
        Object.defineProperty(Diff2.prototype, "useLongestToken", {
          get: function() {
            return false;
          },
          enumerable: false,
          configurable: true
        });
        Diff2.prototype.buildValues = function(lastComponent, newTokens, oldTokens) {
          var components = [];
          var nextComponent;
          while (lastComponent) {
            components.push(lastComponent);
            nextComponent = lastComponent.previousComponent;
            delete lastComponent.previousComponent;
            lastComponent = nextComponent;
          }
          components.reverse();
          var componentLen = components.length;
          var componentPos = 0, newPos = 0, oldPos = 0;
          for (; componentPos < componentLen; componentPos++) {
            var component = components[componentPos];
            if (!component.removed) {
              if (!component.added && this.useLongestToken) {
                var value = newTokens.slice(newPos, newPos + component.count);
                value = value.map(function(value2, i) {
                  var oldValue = oldTokens[oldPos + i];
                  return oldValue.length > value2.length ? oldValue : value2;
                });
                component.value = this.join(value);
              } else {
                component.value = this.join(newTokens.slice(newPos, newPos + component.count));
              }
              newPos += component.count;
              if (!component.added) {
                oldPos += component.count;
              }
            } else {
              component.value = this.join(oldTokens.slice(oldPos, oldPos + component.count));
              oldPos += component.count;
            }
          }
          return components;
        };
        return Diff2;
      })()
    );
    exports.default = Diff;
  }
});

// node_modules/diff/libcjs/diff/character.js
var require_character = __commonJS({
  "node_modules/diff/libcjs/diff/character.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.characterDiff = void 0;
    exports.diffChars = diffChars;
    var base_js_1 = require_base();
    var CharacterDiff = (
      /** @class */
      (function(_super) {
        __extends(CharacterDiff2, _super);
        function CharacterDiff2() {
          return _super !== null && _super.apply(this, arguments) || this;
        }
        return CharacterDiff2;
      })(base_js_1.default)
    );
    exports.characterDiff = new CharacterDiff();
    function diffChars(oldStr, newStr, options) {
      return exports.characterDiff.diff(oldStr, newStr, options);
    }
  }
});

// node_modules/diff/libcjs/util/string.js
var require_string2 = __commonJS({
  "node_modules/diff/libcjs/util/string.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.longestCommonPrefix = longestCommonPrefix;
    exports.longestCommonSuffix = longestCommonSuffix;
    exports.replacePrefix = replacePrefix;
    exports.replaceSuffix = replaceSuffix;
    exports.removePrefix = removePrefix;
    exports.removeSuffix = removeSuffix;
    exports.maximumOverlap = maximumOverlap;
    exports.hasOnlyWinLineEndings = hasOnlyWinLineEndings;
    exports.hasOnlyUnixLineEndings = hasOnlyUnixLineEndings;
    exports.trailingWs = trailingWs;
    exports.leadingWs = leadingWs;
    function longestCommonPrefix(str1, str2) {
      var i;
      for (i = 0; i < str1.length && i < str2.length; i++) {
        if (str1[i] != str2[i]) {
          return str1.slice(0, i);
        }
      }
      return str1.slice(0, i);
    }
    function longestCommonSuffix(str1, str2) {
      var i;
      if (!str1 || !str2 || str1[str1.length - 1] != str2[str2.length - 1]) {
        return "";
      }
      for (i = 0; i < str1.length && i < str2.length; i++) {
        if (str1[str1.length - (i + 1)] != str2[str2.length - (i + 1)]) {
          return str1.slice(-i);
        }
      }
      return str1.slice(-i);
    }
    function replacePrefix(string, oldPrefix, newPrefix) {
      if (string.slice(0, oldPrefix.length) != oldPrefix) {
        throw Error("string ".concat(JSON.stringify(string), " doesn't start with prefix ").concat(JSON.stringify(oldPrefix), "; this is a bug"));
      }
      return newPrefix + string.slice(oldPrefix.length);
    }
    function replaceSuffix(string, oldSuffix, newSuffix) {
      if (!oldSuffix) {
        return string + newSuffix;
      }
      if (string.slice(-oldSuffix.length) != oldSuffix) {
        throw Error("string ".concat(JSON.stringify(string), " doesn't end with suffix ").concat(JSON.stringify(oldSuffix), "; this is a bug"));
      }
      return string.slice(0, -oldSuffix.length) + newSuffix;
    }
    function removePrefix(string, oldPrefix) {
      return replacePrefix(string, oldPrefix, "");
    }
    function removeSuffix(string, oldSuffix) {
      return replaceSuffix(string, oldSuffix, "");
    }
    function maximumOverlap(string1, string2) {
      return string2.slice(0, overlapCount(string1, string2));
    }
    function overlapCount(a, b) {
      var startA = 0;
      if (a.length > b.length) {
        startA = a.length - b.length;
      }
      var endB = b.length;
      if (a.length < b.length) {
        endB = a.length;
      }
      var map = Array(endB);
      var k = 0;
      map[0] = 0;
      for (var j = 1; j < endB; j++) {
        if (b[j] == b[k]) {
          map[j] = map[k];
        } else {
          map[j] = k;
        }
        while (k > 0 && b[j] != b[k]) {
          k = map[k];
        }
        if (b[j] == b[k]) {
          k++;
        }
      }
      k = 0;
      for (var i = startA; i < a.length; i++) {
        while (k > 0 && a[i] != b[k]) {
          k = map[k];
        }
        if (a[i] == b[k]) {
          k++;
        }
      }
      return k;
    }
    function hasOnlyWinLineEndings(string) {
      return string.includes("\r\n") && !string.startsWith("\n") && !string.match(/[^\r]\n/);
    }
    function hasOnlyUnixLineEndings(string) {
      return !string.includes("\r\n") && string.includes("\n");
    }
    function trailingWs(string) {
      var i;
      for (i = string.length - 1; i >= 0; i--) {
        if (!string[i].match(/\s/)) {
          break;
        }
      }
      return string.substring(i + 1);
    }
    function leadingWs(string) {
      var match = string.match(/^\s*/);
      return match ? match[0] : "";
    }
  }
});

// node_modules/diff/libcjs/diff/word.js
var require_word = __commonJS({
  "node_modules/diff/libcjs/diff/word.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.wordsWithSpaceDiff = exports.wordDiff = void 0;
    exports.diffWords = diffWords;
    exports.diffWordsWithSpace = diffWordsWithSpace;
    var base_js_1 = require_base();
    var string_js_1 = require_string2();
    var extendedWordChars = "a-zA-Z0-9_\\u{C0}-\\u{FF}\\u{D8}-\\u{F6}\\u{F8}-\\u{2C6}\\u{2C8}-\\u{2D7}\\u{2DE}-\\u{2FF}\\u{1E00}-\\u{1EFF}";
    var tokenizeIncludingWhitespace = new RegExp("[".concat(extendedWordChars, "]+|\\s+|[^").concat(extendedWordChars, "]"), "ug");
    var WordDiff = (
      /** @class */
      (function(_super) {
        __extends(WordDiff2, _super);
        function WordDiff2() {
          return _super !== null && _super.apply(this, arguments) || this;
        }
        WordDiff2.prototype.equals = function(left, right, options) {
          if (options.ignoreCase) {
            left = left.toLowerCase();
            right = right.toLowerCase();
          }
          return left.trim() === right.trim();
        };
        WordDiff2.prototype.tokenize = function(value, options) {
          if (options === void 0) {
            options = {};
          }
          var parts;
          if (options.intlSegmenter) {
            var segmenter = options.intlSegmenter;
            if (segmenter.resolvedOptions().granularity != "word") {
              throw new Error('The segmenter passed must have a granularity of "word"');
            }
            parts = Array.from(segmenter.segment(value), function(segment) {
              return segment.segment;
            });
          } else {
            parts = value.match(tokenizeIncludingWhitespace) || [];
          }
          var tokens = [];
          var prevPart = null;
          parts.forEach(function(part) {
            if (/\s/.test(part)) {
              if (prevPart == null) {
                tokens.push(part);
              } else {
                tokens.push(tokens.pop() + part);
              }
            } else if (prevPart != null && /\s/.test(prevPart)) {
              if (tokens[tokens.length - 1] == prevPart) {
                tokens.push(tokens.pop() + part);
              } else {
                tokens.push(prevPart + part);
              }
            } else {
              tokens.push(part);
            }
            prevPart = part;
          });
          return tokens;
        };
        WordDiff2.prototype.join = function(tokens) {
          return tokens.map(function(token, i) {
            if (i == 0) {
              return token;
            } else {
              return token.replace(/^\s+/, "");
            }
          }).join("");
        };
        WordDiff2.prototype.postProcess = function(changes, options) {
          if (!changes || options.oneChangePerToken) {
            return changes;
          }
          var lastKeep = null;
          var insertion = null;
          var deletion = null;
          changes.forEach(function(change) {
            if (change.added) {
              insertion = change;
            } else if (change.removed) {
              deletion = change;
            } else {
              if (insertion || deletion) {
                dedupeWhitespaceInChangeObjects(lastKeep, deletion, insertion, change);
              }
              lastKeep = change;
              insertion = null;
              deletion = null;
            }
          });
          if (insertion || deletion) {
            dedupeWhitespaceInChangeObjects(lastKeep, deletion, insertion, null);
          }
          return changes;
        };
        return WordDiff2;
      })(base_js_1.default)
    );
    exports.wordDiff = new WordDiff();
    function diffWords(oldStr, newStr, options) {
      if ((options === null || options === void 0 ? void 0 : options.ignoreWhitespace) != null && !options.ignoreWhitespace) {
        return diffWordsWithSpace(oldStr, newStr, options);
      }
      return exports.wordDiff.diff(oldStr, newStr, options);
    }
    function dedupeWhitespaceInChangeObjects(startKeep, deletion, insertion, endKeep) {
      if (deletion && insertion) {
        var oldWsPrefix = (0, string_js_1.leadingWs)(deletion.value);
        var oldWsSuffix = (0, string_js_1.trailingWs)(deletion.value);
        var newWsPrefix = (0, string_js_1.leadingWs)(insertion.value);
        var newWsSuffix = (0, string_js_1.trailingWs)(insertion.value);
        if (startKeep) {
          var commonWsPrefix = (0, string_js_1.longestCommonPrefix)(oldWsPrefix, newWsPrefix);
          startKeep.value = (0, string_js_1.replaceSuffix)(startKeep.value, newWsPrefix, commonWsPrefix);
          deletion.value = (0, string_js_1.removePrefix)(deletion.value, commonWsPrefix);
          insertion.value = (0, string_js_1.removePrefix)(insertion.value, commonWsPrefix);
        }
        if (endKeep) {
          var commonWsSuffix = (0, string_js_1.longestCommonSuffix)(oldWsSuffix, newWsSuffix);
          endKeep.value = (0, string_js_1.replacePrefix)(endKeep.value, newWsSuffix, commonWsSuffix);
          deletion.value = (0, string_js_1.removeSuffix)(deletion.value, commonWsSuffix);
          insertion.value = (0, string_js_1.removeSuffix)(insertion.value, commonWsSuffix);
        }
      } else if (insertion) {
        if (startKeep) {
          var ws = (0, string_js_1.leadingWs)(insertion.value);
          insertion.value = insertion.value.substring(ws.length);
        }
        if (endKeep) {
          var ws = (0, string_js_1.leadingWs)(endKeep.value);
          endKeep.value = endKeep.value.substring(ws.length);
        }
      } else if (startKeep && endKeep) {
        var newWsFull = (0, string_js_1.leadingWs)(endKeep.value), delWsStart = (0, string_js_1.leadingWs)(deletion.value), delWsEnd = (0, string_js_1.trailingWs)(deletion.value);
        var newWsStart = (0, string_js_1.longestCommonPrefix)(newWsFull, delWsStart);
        deletion.value = (0, string_js_1.removePrefix)(deletion.value, newWsStart);
        var newWsEnd = (0, string_js_1.longestCommonSuffix)((0, string_js_1.removePrefix)(newWsFull, newWsStart), delWsEnd);
        deletion.value = (0, string_js_1.removeSuffix)(deletion.value, newWsEnd);
        endKeep.value = (0, string_js_1.replacePrefix)(endKeep.value, newWsFull, newWsEnd);
        startKeep.value = (0, string_js_1.replaceSuffix)(startKeep.value, newWsFull, newWsFull.slice(0, newWsFull.length - newWsEnd.length));
      } else if (endKeep) {
        var endKeepWsPrefix = (0, string_js_1.leadingWs)(endKeep.value);
        var deletionWsSuffix = (0, string_js_1.trailingWs)(deletion.value);
        var overlap = (0, string_js_1.maximumOverlap)(deletionWsSuffix, endKeepWsPrefix);
        deletion.value = (0, string_js_1.removeSuffix)(deletion.value, overlap);
      } else if (startKeep) {
        var startKeepWsSuffix = (0, string_js_1.trailingWs)(startKeep.value);
        var deletionWsPrefix = (0, string_js_1.leadingWs)(deletion.value);
        var overlap = (0, string_js_1.maximumOverlap)(startKeepWsSuffix, deletionWsPrefix);
        deletion.value = (0, string_js_1.removePrefix)(deletion.value, overlap);
      }
    }
    var WordsWithSpaceDiff = (
      /** @class */
      (function(_super) {
        __extends(WordsWithSpaceDiff2, _super);
        function WordsWithSpaceDiff2() {
          return _super !== null && _super.apply(this, arguments) || this;
        }
        WordsWithSpaceDiff2.prototype.tokenize = function(value) {
          var regex = new RegExp("(\\r?\\n)|[".concat(extendedWordChars, "]+|[^\\S\\n\\r]+|[^").concat(extendedWordChars, "]"), "ug");
          return value.match(regex) || [];
        };
        return WordsWithSpaceDiff2;
      })(base_js_1.default)
    );
    exports.wordsWithSpaceDiff = new WordsWithSpaceDiff();
    function diffWordsWithSpace(oldStr, newStr, options) {
      return exports.wordsWithSpaceDiff.diff(oldStr, newStr, options);
    }
  }
});

// node_modules/diff/libcjs/util/params.js
var require_params = __commonJS({
  "node_modules/diff/libcjs/util/params.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.generateOptions = generateOptions;
    function generateOptions(options, defaults) {
      if (typeof options === "function") {
        defaults.callback = options;
      } else if (options) {
        for (var name in options) {
          if (Object.prototype.hasOwnProperty.call(options, name)) {
            defaults[name] = options[name];
          }
        }
      }
      return defaults;
    }
  }
});

// node_modules/diff/libcjs/diff/line.js
var require_line = __commonJS({
  "node_modules/diff/libcjs/diff/line.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.lineDiff = void 0;
    exports.diffLines = diffLines;
    exports.diffTrimmedLines = diffTrimmedLines;
    exports.tokenize = tokenize;
    var base_js_1 = require_base();
    var params_js_1 = require_params();
    var LineDiff = (
      /** @class */
      (function(_super) {
        __extends(LineDiff2, _super);
        function LineDiff2() {
          var _this = _super !== null && _super.apply(this, arguments) || this;
          _this.tokenize = tokenize;
          return _this;
        }
        LineDiff2.prototype.equals = function(left, right, options) {
          if (options.ignoreWhitespace) {
            if (!options.newlineIsToken || !left.includes("\n")) {
              left = left.trim();
            }
            if (!options.newlineIsToken || !right.includes("\n")) {
              right = right.trim();
            }
          } else if (options.ignoreNewlineAtEof && !options.newlineIsToken) {
            if (left.endsWith("\n")) {
              left = left.slice(0, -1);
            }
            if (right.endsWith("\n")) {
              right = right.slice(0, -1);
            }
          }
          return _super.prototype.equals.call(this, left, right, options);
        };
        return LineDiff2;
      })(base_js_1.default)
    );
    exports.lineDiff = new LineDiff();
    function diffLines(oldStr, newStr, options) {
      return exports.lineDiff.diff(oldStr, newStr, options);
    }
    function diffTrimmedLines(oldStr, newStr, options) {
      options = (0, params_js_1.generateOptions)(options, { ignoreWhitespace: true });
      return exports.lineDiff.diff(oldStr, newStr, options);
    }
    function tokenize(value, options) {
      if (options.stripTrailingCr) {
        value = value.replace(/\r\n/g, "\n");
      }
      var retLines = [], linesAndNewlines = value.split(/(\n|\r\n)/);
      if (!linesAndNewlines[linesAndNewlines.length - 1]) {
        linesAndNewlines.pop();
      }
      for (var i = 0; i < linesAndNewlines.length; i++) {
        var line = linesAndNewlines[i];
        if (i % 2 && !options.newlineIsToken) {
          retLines[retLines.length - 1] += line;
        } else {
          retLines.push(line);
        }
      }
      return retLines;
    }
  }
});

// node_modules/diff/libcjs/diff/sentence.js
var require_sentence = __commonJS({
  "node_modules/diff/libcjs/diff/sentence.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.sentenceDiff = void 0;
    exports.diffSentences = diffSentences;
    var base_js_1 = require_base();
    function isSentenceEndPunct(char) {
      return char == "." || char == "!" || char == "?";
    }
    var SentenceDiff = (
      /** @class */
      (function(_super) {
        __extends(SentenceDiff2, _super);
        function SentenceDiff2() {
          return _super !== null && _super.apply(this, arguments) || this;
        }
        SentenceDiff2.prototype.tokenize = function(value) {
          var _a;
          var result = [];
          var tokenStartI = 0;
          for (var i = 0; i < value.length; i++) {
            if (i == value.length - 1) {
              result.push(value.slice(tokenStartI));
              break;
            }
            if (isSentenceEndPunct(value[i]) && value[i + 1].match(/\s/)) {
              result.push(value.slice(tokenStartI, i + 1));
              i = tokenStartI = i + 1;
              while ((_a = value[i + 1]) === null || _a === void 0 ? void 0 : _a.match(/\s/)) {
                i++;
              }
              result.push(value.slice(tokenStartI, i + 1));
              tokenStartI = i + 1;
            }
          }
          return result;
        };
        return SentenceDiff2;
      })(base_js_1.default)
    );
    exports.sentenceDiff = new SentenceDiff();
    function diffSentences(oldStr, newStr, options) {
      return exports.sentenceDiff.diff(oldStr, newStr, options);
    }
  }
});

// node_modules/diff/libcjs/diff/css.js
var require_css = __commonJS({
  "node_modules/diff/libcjs/diff/css.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.cssDiff = void 0;
    exports.diffCss = diffCss;
    var base_js_1 = require_base();
    var CssDiff = (
      /** @class */
      (function(_super) {
        __extends(CssDiff2, _super);
        function CssDiff2() {
          return _super !== null && _super.apply(this, arguments) || this;
        }
        CssDiff2.prototype.tokenize = function(value) {
          return value.split(/([{}:;,]|\s+)/);
        };
        return CssDiff2;
      })(base_js_1.default)
    );
    exports.cssDiff = new CssDiff();
    function diffCss(oldStr, newStr, options) {
      return exports.cssDiff.diff(oldStr, newStr, options);
    }
  }
});

// node_modules/diff/libcjs/diff/json.js
var require_json = __commonJS({
  "node_modules/diff/libcjs/diff/json.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.jsonDiff = void 0;
    exports.diffJson = diffJson;
    exports.canonicalize = canonicalize;
    var base_js_1 = require_base();
    var line_js_1 = require_line();
    var JsonDiff = (
      /** @class */
      (function(_super) {
        __extends(JsonDiff2, _super);
        function JsonDiff2() {
          var _this = _super !== null && _super.apply(this, arguments) || this;
          _this.tokenize = line_js_1.tokenize;
          return _this;
        }
        Object.defineProperty(JsonDiff2.prototype, "useLongestToken", {
          get: function() {
            return true;
          },
          enumerable: false,
          configurable: true
        });
        JsonDiff2.prototype.castInput = function(value, options) {
          var undefinedReplacement = options.undefinedReplacement, _a = options.stringifyReplacer, stringifyReplacer = _a === void 0 ? function(k, v) {
            return typeof v === "undefined" ? undefinedReplacement : v;
          } : _a;
          return typeof value === "string" ? value : JSON.stringify(canonicalize(value, null, null, stringifyReplacer), null, "  ");
        };
        JsonDiff2.prototype.equals = function(left, right, options) {
          return _super.prototype.equals.call(this, left.replace(/,([\r\n])/g, "$1"), right.replace(/,([\r\n])/g, "$1"), options);
        };
        return JsonDiff2;
      })(base_js_1.default)
    );
    exports.jsonDiff = new JsonDiff();
    function diffJson(oldStr, newStr, options) {
      return exports.jsonDiff.diff(oldStr, newStr, options);
    }
    function canonicalize(obj, stack, replacementStack, replacer, key) {
      stack = stack || [];
      replacementStack = replacementStack || [];
      if (replacer) {
        obj = replacer(key === void 0 ? "" : key, obj);
      }
      var i;
      for (i = 0; i < stack.length; i += 1) {
        if (stack[i] === obj) {
          return replacementStack[i];
        }
      }
      var canonicalizedObj;
      if ("[object Array]" === Object.prototype.toString.call(obj)) {
        stack.push(obj);
        canonicalizedObj = new Array(obj.length);
        replacementStack.push(canonicalizedObj);
        for (i = 0; i < obj.length; i += 1) {
          canonicalizedObj[i] = canonicalize(obj[i], stack, replacementStack, replacer, String(i));
        }
        stack.pop();
        replacementStack.pop();
        return canonicalizedObj;
      }
      if (obj && obj.toJSON) {
        obj = obj.toJSON();
      }
      if (typeof obj === "object" && obj !== null) {
        stack.push(obj);
        canonicalizedObj = {};
        replacementStack.push(canonicalizedObj);
        var sortedKeys = [];
        var key_1;
        for (key_1 in obj) {
          if (Object.prototype.hasOwnProperty.call(obj, key_1)) {
            sortedKeys.push(key_1);
          }
        }
        sortedKeys.sort();
        for (i = 0; i < sortedKeys.length; i += 1) {
          key_1 = sortedKeys[i];
          canonicalizedObj[key_1] = canonicalize(obj[key_1], stack, replacementStack, replacer, key_1);
        }
        stack.pop();
        replacementStack.pop();
      } else {
        canonicalizedObj = obj;
      }
      return canonicalizedObj;
    }
  }
});

// node_modules/diff/libcjs/diff/array.js
var require_array2 = __commonJS({
  "node_modules/diff/libcjs/diff/array.js"(exports) {
    "use strict";
    var __extends = exports && exports.__extends || /* @__PURE__ */ (function() {
      var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf || { __proto__: [] } instanceof Array && function(d2, b2) {
          d2.__proto__ = b2;
        } || function(d2, b2) {
          for (var p in b2) if (Object.prototype.hasOwnProperty.call(b2, p)) d2[p] = b2[p];
        };
        return extendStatics(d, b);
      };
      return function(d, b) {
        if (typeof b !== "function" && b !== null)
          throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
        extendStatics(d, b);
        function __() {
          this.constructor = d;
        }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
      };
    })();
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.arrayDiff = void 0;
    exports.diffArrays = diffArrays;
    var base_js_1 = require_base();
    var ArrayDiff = (
      /** @class */
      (function(_super) {
        __extends(ArrayDiff2, _super);
        function ArrayDiff2() {
          return _super !== null && _super.apply(this, arguments) || this;
        }
        ArrayDiff2.prototype.tokenize = function(value) {
          return value.slice();
        };
        ArrayDiff2.prototype.join = function(value) {
          return value;
        };
        ArrayDiff2.prototype.removeEmpty = function(value) {
          return value;
        };
        return ArrayDiff2;
      })(base_js_1.default)
    );
    exports.arrayDiff = new ArrayDiff();
    function diffArrays(oldArr, newArr, options) {
      return exports.arrayDiff.diff(oldArr, newArr, options);
    }
  }
});

// node_modules/diff/libcjs/patch/line-endings.js
var require_line_endings = __commonJS({
  "node_modules/diff/libcjs/patch/line-endings.js"(exports) {
    "use strict";
    var __assign = exports && exports.__assign || function() {
      __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
          s = arguments[i];
          for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
            t[p] = s[p];
        }
        return t;
      };
      return __assign.apply(this, arguments);
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.unixToWin = unixToWin;
    exports.winToUnix = winToUnix;
    exports.isUnix = isUnix;
    exports.isWin = isWin;
    function unixToWin(patch) {
      if (Array.isArray(patch)) {
        return patch.map(function(p) {
          return unixToWin(p);
        });
      }
      return __assign(__assign({}, patch), { hunks: patch.hunks.map(function(hunk) {
        return __assign(__assign({}, hunk), { lines: hunk.lines.map(function(line, i) {
          var _a;
          return line.startsWith("\\") || line.endsWith("\r") || ((_a = hunk.lines[i + 1]) === null || _a === void 0 ? void 0 : _a.startsWith("\\")) ? line : line + "\r";
        }) });
      }) });
    }
    function winToUnix(patch) {
      if (Array.isArray(patch)) {
        return patch.map(function(p) {
          return winToUnix(p);
        });
      }
      return __assign(__assign({}, patch), { hunks: patch.hunks.map(function(hunk) {
        return __assign(__assign({}, hunk), { lines: hunk.lines.map(function(line) {
          return line.endsWith("\r") ? line.substring(0, line.length - 1) : line;
        }) });
      }) });
    }
    function isUnix(patch) {
      if (!Array.isArray(patch)) {
        patch = [patch];
      }
      return !patch.some(function(index) {
        return index.hunks.some(function(hunk) {
          return hunk.lines.some(function(line) {
            return !line.startsWith("\\") && line.endsWith("\r");
          });
        });
      });
    }
    function isWin(patch) {
      if (!Array.isArray(patch)) {
        patch = [patch];
      }
      return patch.some(function(index) {
        return index.hunks.some(function(hunk) {
          return hunk.lines.some(function(line) {
            return line.endsWith("\r");
          });
        });
      }) && patch.every(function(index) {
        return index.hunks.every(function(hunk) {
          return hunk.lines.every(function(line, i) {
            var _a;
            return line.startsWith("\\") || line.endsWith("\r") || ((_a = hunk.lines[i + 1]) === null || _a === void 0 ? void 0 : _a.startsWith("\\"));
          });
        });
      });
    }
  }
});

// node_modules/diff/libcjs/patch/parse.js
var require_parse = __commonJS({
  "node_modules/diff/libcjs/patch/parse.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.parsePatch = parsePatch;
    function parsePatch(uniDiff) {
      var diffstr = uniDiff.split(/\n/), list = [];
      var i = 0;
      function parseIndex() {
        var index = {};
        list.push(index);
        while (i < diffstr.length) {
          var line = diffstr[i];
          if (/^(---|\+\+\+|@@)\s/.test(line)) {
            break;
          }
          var header = /^(?:Index:|diff(?: -r \w+)+)\s+(.+?)\s*$/.exec(line);
          if (header) {
            index.index = header[1];
          }
          i++;
        }
        parseFileHeader(index);
        parseFileHeader(index);
        index.hunks = [];
        while (i < diffstr.length) {
          var line = diffstr[i];
          if (/^(Index:\s|diff\s|---\s|\+\+\+\s|===================================================================)/.test(line)) {
            break;
          } else if (/^@@/.test(line)) {
            index.hunks.push(parseHunk());
          } else if (line) {
            throw new Error("Unknown line " + (i + 1) + " " + JSON.stringify(line));
          } else {
            i++;
          }
        }
      }
      function parseFileHeader(index) {
        var fileHeader = /^(---|\+\+\+)\s+(.*)\r?$/.exec(diffstr[i]);
        if (fileHeader) {
          var data = fileHeader[2].split("	", 2), header = (data[1] || "").trim();
          var fileName = data[0].replace(/\\\\/g, "\\");
          if (/^".*"$/.test(fileName)) {
            fileName = fileName.substr(1, fileName.length - 2);
          }
          if (fileHeader[1] === "---") {
            index.oldFileName = fileName;
            index.oldHeader = header;
          } else {
            index.newFileName = fileName;
            index.newHeader = header;
          }
          i++;
        }
      }
      function parseHunk() {
        var _a;
        var chunkHeaderIndex = i, chunkHeaderLine = diffstr[i++], chunkHeader = chunkHeaderLine.split(/@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@/);
        var hunk = {
          oldStart: +chunkHeader[1],
          oldLines: typeof chunkHeader[2] === "undefined" ? 1 : +chunkHeader[2],
          newStart: +chunkHeader[3],
          newLines: typeof chunkHeader[4] === "undefined" ? 1 : +chunkHeader[4],
          lines: []
        };
        if (hunk.oldLines === 0) {
          hunk.oldStart += 1;
        }
        if (hunk.newLines === 0) {
          hunk.newStart += 1;
        }
        var addCount = 0, removeCount = 0;
        for (; i < diffstr.length && (removeCount < hunk.oldLines || addCount < hunk.newLines || ((_a = diffstr[i]) === null || _a === void 0 ? void 0 : _a.startsWith("\\"))); i++) {
          var operation = diffstr[i].length == 0 && i != diffstr.length - 1 ? " " : diffstr[i][0];
          if (operation === "+" || operation === "-" || operation === " " || operation === "\\") {
            hunk.lines.push(diffstr[i]);
            if (operation === "+") {
              addCount++;
            } else if (operation === "-") {
              removeCount++;
            } else if (operation === " ") {
              addCount++;
              removeCount++;
            }
          } else {
            throw new Error("Hunk at line ".concat(chunkHeaderIndex + 1, " contained invalid line ").concat(diffstr[i]));
          }
        }
        if (!addCount && hunk.newLines === 1) {
          hunk.newLines = 0;
        }
        if (!removeCount && hunk.oldLines === 1) {
          hunk.oldLines = 0;
        }
        if (addCount !== hunk.newLines) {
          throw new Error("Added line count did not match for hunk at line " + (chunkHeaderIndex + 1));
        }
        if (removeCount !== hunk.oldLines) {
          throw new Error("Removed line count did not match for hunk at line " + (chunkHeaderIndex + 1));
        }
        return hunk;
      }
      while (i < diffstr.length) {
        parseIndex();
      }
      return list;
    }
  }
});

// node_modules/diff/libcjs/util/distance-iterator.js
var require_distance_iterator = __commonJS({
  "node_modules/diff/libcjs/util/distance-iterator.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.default = default_1;
    function default_1(start, minLine, maxLine) {
      var wantForward = true, backwardExhausted = false, forwardExhausted = false, localOffset = 1;
      return function iterator() {
        if (wantForward && !forwardExhausted) {
          if (backwardExhausted) {
            localOffset++;
          } else {
            wantForward = false;
          }
          if (start + localOffset <= maxLine) {
            return start + localOffset;
          }
          forwardExhausted = true;
        }
        if (!backwardExhausted) {
          if (!forwardExhausted) {
            wantForward = true;
          }
          if (minLine <= start - localOffset) {
            return start - localOffset++;
          }
          backwardExhausted = true;
          return iterator();
        }
        return void 0;
      };
    }
  }
});

// node_modules/diff/libcjs/patch/apply.js
var require_apply = __commonJS({
  "node_modules/diff/libcjs/patch/apply.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.applyPatch = applyPatch;
    exports.applyPatches = applyPatches;
    var string_js_1 = require_string2();
    var line_endings_js_1 = require_line_endings();
    var parse_js_1 = require_parse();
    var distance_iterator_js_1 = require_distance_iterator();
    function applyPatch(source, patch, options) {
      if (options === void 0) {
        options = {};
      }
      var patches;
      if (typeof patch === "string") {
        patches = (0, parse_js_1.parsePatch)(patch);
      } else if (Array.isArray(patch)) {
        patches = patch;
      } else {
        patches = [patch];
      }
      if (patches.length > 1) {
        throw new Error("applyPatch only works with a single input.");
      }
      return applyStructuredPatch(source, patches[0], options);
    }
    function applyStructuredPatch(source, patch, options) {
      if (options === void 0) {
        options = {};
      }
      if (options.autoConvertLineEndings || options.autoConvertLineEndings == null) {
        if ((0, string_js_1.hasOnlyWinLineEndings)(source) && (0, line_endings_js_1.isUnix)(patch)) {
          patch = (0, line_endings_js_1.unixToWin)(patch);
        } else if ((0, string_js_1.hasOnlyUnixLineEndings)(source) && (0, line_endings_js_1.isWin)(patch)) {
          patch = (0, line_endings_js_1.winToUnix)(patch);
        }
      }
      var lines = source.split("\n"), hunks = patch.hunks, compareLine = options.compareLine || (function(lineNumber, line2, operation, patchContent) {
        return line2 === patchContent;
      }), fuzzFactor = options.fuzzFactor || 0;
      var minLine = 0;
      if (fuzzFactor < 0 || !Number.isInteger(fuzzFactor)) {
        throw new Error("fuzzFactor must be a non-negative integer");
      }
      if (!hunks.length) {
        return source;
      }
      var prevLine = "", removeEOFNL = false, addEOFNL = false;
      for (var i = 0; i < hunks[hunks.length - 1].lines.length; i++) {
        var line = hunks[hunks.length - 1].lines[i];
        if (line[0] == "\\") {
          if (prevLine[0] == "+") {
            removeEOFNL = true;
          } else if (prevLine[0] == "-") {
            addEOFNL = true;
          }
        }
        prevLine = line;
      }
      if (removeEOFNL) {
        if (addEOFNL) {
          if (!fuzzFactor && lines[lines.length - 1] == "") {
            return false;
          }
        } else if (lines[lines.length - 1] == "") {
          lines.pop();
        } else if (!fuzzFactor) {
          return false;
        }
      } else if (addEOFNL) {
        if (lines[lines.length - 1] != "") {
          lines.push("");
        } else if (!fuzzFactor) {
          return false;
        }
      }
      function applyHunk(hunkLines, toPos2, maxErrors2, hunkLinesI, lastContextLineMatched, patchedLines, patchedLinesLength) {
        if (hunkLinesI === void 0) {
          hunkLinesI = 0;
        }
        if (lastContextLineMatched === void 0) {
          lastContextLineMatched = true;
        }
        if (patchedLines === void 0) {
          patchedLines = [];
        }
        if (patchedLinesLength === void 0) {
          patchedLinesLength = 0;
        }
        var nConsecutiveOldContextLines = 0;
        var nextContextLineMustMatch = false;
        for (; hunkLinesI < hunkLines.length; hunkLinesI++) {
          var hunkLine = hunkLines[hunkLinesI], operation = hunkLine.length > 0 ? hunkLine[0] : " ", content = hunkLine.length > 0 ? hunkLine.substr(1) : hunkLine;
          if (operation === "-") {
            if (compareLine(toPos2 + 1, lines[toPos2], operation, content)) {
              toPos2++;
              nConsecutiveOldContextLines = 0;
            } else {
              if (!maxErrors2 || lines[toPos2] == null) {
                return null;
              }
              patchedLines[patchedLinesLength] = lines[toPos2];
              return applyHunk(hunkLines, toPos2 + 1, maxErrors2 - 1, hunkLinesI, false, patchedLines, patchedLinesLength + 1);
            }
          }
          if (operation === "+") {
            if (!lastContextLineMatched) {
              return null;
            }
            patchedLines[patchedLinesLength] = content;
            patchedLinesLength++;
            nConsecutiveOldContextLines = 0;
            nextContextLineMustMatch = true;
          }
          if (operation === " ") {
            nConsecutiveOldContextLines++;
            patchedLines[patchedLinesLength] = lines[toPos2];
            if (compareLine(toPos2 + 1, lines[toPos2], operation, content)) {
              patchedLinesLength++;
              lastContextLineMatched = true;
              nextContextLineMustMatch = false;
              toPos2++;
            } else {
              if (nextContextLineMustMatch || !maxErrors2) {
                return null;
              }
              return lines[toPos2] && (applyHunk(hunkLines, toPos2 + 1, maxErrors2 - 1, hunkLinesI + 1, false, patchedLines, patchedLinesLength + 1) || applyHunk(hunkLines, toPos2 + 1, maxErrors2 - 1, hunkLinesI, false, patchedLines, patchedLinesLength + 1)) || applyHunk(hunkLines, toPos2, maxErrors2 - 1, hunkLinesI + 1, false, patchedLines, patchedLinesLength);
            }
          }
        }
        patchedLinesLength -= nConsecutiveOldContextLines;
        toPos2 -= nConsecutiveOldContextLines;
        patchedLines.length = patchedLinesLength;
        return {
          patchedLines,
          oldLineLastI: toPos2 - 1
        };
      }
      var resultLines = [];
      var prevHunkOffset = 0;
      for (var i = 0; i < hunks.length; i++) {
        var hunk = hunks[i];
        var hunkResult = void 0;
        var maxLine = lines.length - hunk.oldLines + fuzzFactor;
        var toPos = void 0;
        for (var maxErrors = 0; maxErrors <= fuzzFactor; maxErrors++) {
          toPos = hunk.oldStart + prevHunkOffset - 1;
          var iterator = (0, distance_iterator_js_1.default)(toPos, minLine, maxLine);
          for (; toPos !== void 0; toPos = iterator()) {
            hunkResult = applyHunk(hunk.lines, toPos, maxErrors);
            if (hunkResult) {
              break;
            }
          }
          if (hunkResult) {
            break;
          }
        }
        if (!hunkResult) {
          return false;
        }
        for (var i_1 = minLine; i_1 < toPos; i_1++) {
          resultLines.push(lines[i_1]);
        }
        for (var i_2 = 0; i_2 < hunkResult.patchedLines.length; i_2++) {
          var line = hunkResult.patchedLines[i_2];
          resultLines.push(line);
        }
        minLine = hunkResult.oldLineLastI + 1;
        prevHunkOffset = toPos + 1 - hunk.oldStart;
      }
      for (var i = minLine; i < lines.length; i++) {
        resultLines.push(lines[i]);
      }
      return resultLines.join("\n");
    }
    function applyPatches(uniDiff, options) {
      var spDiff = typeof uniDiff === "string" ? (0, parse_js_1.parsePatch)(uniDiff) : uniDiff;
      var currentIndex = 0;
      function processIndex() {
        var index = spDiff[currentIndex++];
        if (!index) {
          return options.complete();
        }
        options.loadFile(index, function(err, data) {
          if (err) {
            return options.complete(err);
          }
          var updatedContent = applyPatch(data, index, options);
          options.patched(index, updatedContent, function(err2) {
            if (err2) {
              return options.complete(err2);
            }
            processIndex();
          });
        });
      }
      processIndex();
    }
  }
});

// node_modules/diff/libcjs/patch/reverse.js
var require_reverse = __commonJS({
  "node_modules/diff/libcjs/patch/reverse.js"(exports) {
    "use strict";
    var __assign = exports && exports.__assign || function() {
      __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
          s = arguments[i];
          for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
            t[p] = s[p];
        }
        return t;
      };
      return __assign.apply(this, arguments);
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.reversePatch = reversePatch;
    function reversePatch(structuredPatch) {
      if (Array.isArray(structuredPatch)) {
        return structuredPatch.map(function(patch) {
          return reversePatch(patch);
        }).reverse();
      }
      return __assign(__assign({}, structuredPatch), { oldFileName: structuredPatch.newFileName, oldHeader: structuredPatch.newHeader, newFileName: structuredPatch.oldFileName, newHeader: structuredPatch.oldHeader, hunks: structuredPatch.hunks.map(function(hunk) {
        return {
          oldLines: hunk.newLines,
          oldStart: hunk.newStart,
          newLines: hunk.oldLines,
          newStart: hunk.oldStart,
          lines: hunk.lines.map(function(l) {
            if (l.startsWith("-")) {
              return "+".concat(l.slice(1));
            }
            if (l.startsWith("+")) {
              return "-".concat(l.slice(1));
            }
            return l;
          })
        };
      }) });
    }
  }
});

// node_modules/diff/libcjs/patch/create.js
var require_create = __commonJS({
  "node_modules/diff/libcjs/patch/create.js"(exports) {
    "use strict";
    var __assign = exports && exports.__assign || function() {
      __assign = Object.assign || function(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
          s = arguments[i];
          for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p))
            t[p] = s[p];
        }
        return t;
      };
      return __assign.apply(this, arguments);
    };
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.structuredPatch = structuredPatch;
    exports.formatPatch = formatPatch;
    exports.createTwoFilesPatch = createTwoFilesPatch;
    exports.createPatch = createPatch;
    var line_js_1 = require_line();
    function structuredPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, options) {
      var optionsObj;
      if (!options) {
        optionsObj = {};
      } else if (typeof options === "function") {
        optionsObj = { callback: options };
      } else {
        optionsObj = options;
      }
      if (typeof optionsObj.context === "undefined") {
        optionsObj.context = 4;
      }
      var context = optionsObj.context;
      if (optionsObj.newlineIsToken) {
        throw new Error("newlineIsToken may not be used with patch-generation functions, only with diffing functions");
      }
      if (!optionsObj.callback) {
        return diffLinesResultToPatch((0, line_js_1.diffLines)(oldStr, newStr, optionsObj));
      } else {
        var callback_1 = optionsObj.callback;
        (0, line_js_1.diffLines)(oldStr, newStr, __assign(__assign({}, optionsObj), { callback: function(diff) {
          var patch = diffLinesResultToPatch(diff);
          callback_1(patch);
        } }));
      }
      function diffLinesResultToPatch(diff) {
        if (!diff) {
          return;
        }
        diff.push({ value: "", lines: [] });
        function contextLines(lines2) {
          return lines2.map(function(entry) {
            return " " + entry;
          });
        }
        var hunks = [];
        var oldRangeStart = 0, newRangeStart = 0, curRange = [], oldLine = 1, newLine = 1;
        for (var i = 0; i < diff.length; i++) {
          var current = diff[i], lines = current.lines || splitLines(current.value);
          current.lines = lines;
          if (current.added || current.removed) {
            if (!oldRangeStart) {
              var prev = diff[i - 1];
              oldRangeStart = oldLine;
              newRangeStart = newLine;
              if (prev) {
                curRange = context > 0 ? contextLines(prev.lines.slice(-context)) : [];
                oldRangeStart -= curRange.length;
                newRangeStart -= curRange.length;
              }
            }
            for (var _i = 0, lines_1 = lines; _i < lines_1.length; _i++) {
              var line = lines_1[_i];
              curRange.push((current.added ? "+" : "-") + line);
            }
            if (current.added) {
              newLine += lines.length;
            } else {
              oldLine += lines.length;
            }
          } else {
            if (oldRangeStart) {
              if (lines.length <= context * 2 && i < diff.length - 2) {
                for (var _a = 0, _b = contextLines(lines); _a < _b.length; _a++) {
                  var line = _b[_a];
                  curRange.push(line);
                }
              } else {
                var contextSize = Math.min(lines.length, context);
                for (var _c = 0, _d = contextLines(lines.slice(0, contextSize)); _c < _d.length; _c++) {
                  var line = _d[_c];
                  curRange.push(line);
                }
                var hunk = {
                  oldStart: oldRangeStart,
                  oldLines: oldLine - oldRangeStart + contextSize,
                  newStart: newRangeStart,
                  newLines: newLine - newRangeStart + contextSize,
                  lines: curRange
                };
                hunks.push(hunk);
                oldRangeStart = 0;
                newRangeStart = 0;
                curRange = [];
              }
            }
            oldLine += lines.length;
            newLine += lines.length;
          }
        }
        for (var _e = 0, hunks_1 = hunks; _e < hunks_1.length; _e++) {
          var hunk = hunks_1[_e];
          for (var i = 0; i < hunk.lines.length; i++) {
            if (hunk.lines[i].endsWith("\n")) {
              hunk.lines[i] = hunk.lines[i].slice(0, -1);
            } else {
              hunk.lines.splice(i + 1, 0, "\\ No newline at end of file");
              i++;
            }
          }
        }
        return {
          oldFileName,
          newFileName,
          oldHeader,
          newHeader,
          hunks
        };
      }
    }
    function formatPatch(patch) {
      if (Array.isArray(patch)) {
        return patch.map(formatPatch).join("\n");
      }
      var ret = [];
      if (patch.oldFileName == patch.newFileName) {
        ret.push("Index: " + patch.oldFileName);
      }
      ret.push("===================================================================");
      ret.push("--- " + patch.oldFileName + (typeof patch.oldHeader === "undefined" ? "" : "	" + patch.oldHeader));
      ret.push("+++ " + patch.newFileName + (typeof patch.newHeader === "undefined" ? "" : "	" + patch.newHeader));
      for (var i = 0; i < patch.hunks.length; i++) {
        var hunk = patch.hunks[i];
        if (hunk.oldLines === 0) {
          hunk.oldStart -= 1;
        }
        if (hunk.newLines === 0) {
          hunk.newStart -= 1;
        }
        ret.push("@@ -" + hunk.oldStart + "," + hunk.oldLines + " +" + hunk.newStart + "," + hunk.newLines + " @@");
        for (var _i = 0, _a = hunk.lines; _i < _a.length; _i++) {
          var line = _a[_i];
          ret.push(line);
        }
      }
      return ret.join("\n") + "\n";
    }
    function createTwoFilesPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, options) {
      if (typeof options === "function") {
        options = { callback: options };
      }
      if (!(options === null || options === void 0 ? void 0 : options.callback)) {
        var patchObj = structuredPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, options);
        if (!patchObj) {
          return;
        }
        return formatPatch(patchObj);
      } else {
        var callback_2 = options.callback;
        structuredPatch(oldFileName, newFileName, oldStr, newStr, oldHeader, newHeader, __assign(__assign({}, options), { callback: function(patchObj2) {
          if (!patchObj2) {
            callback_2(void 0);
          } else {
            callback_2(formatPatch(patchObj2));
          }
        } }));
      }
    }
    function createPatch(fileName, oldStr, newStr, oldHeader, newHeader, options) {
      return createTwoFilesPatch(fileName, fileName, oldStr, newStr, oldHeader, newHeader, options);
    }
    function splitLines(text) {
      var hasTrailingNl = text.endsWith("\n");
      var result = text.split("\n").map(function(line) {
        return line + "\n";
      });
      if (hasTrailingNl) {
        result.pop();
      } else {
        result.push(result.pop().slice(0, -1));
      }
      return result;
    }
  }
});

// node_modules/diff/libcjs/convert/dmp.js
var require_dmp = __commonJS({
  "node_modules/diff/libcjs/convert/dmp.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.convertChangesToDMP = convertChangesToDMP;
    function convertChangesToDMP(changes) {
      var ret = [];
      var change, operation;
      for (var i = 0; i < changes.length; i++) {
        change = changes[i];
        if (change.added) {
          operation = 1;
        } else if (change.removed) {
          operation = -1;
        } else {
          operation = 0;
        }
        ret.push([operation, change.value]);
      }
      return ret;
    }
  }
});

// node_modules/diff/libcjs/convert/xml.js
var require_xml = __commonJS({
  "node_modules/diff/libcjs/convert/xml.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.convertChangesToXML = convertChangesToXML;
    function convertChangesToXML(changes) {
      var ret = [];
      for (var i = 0; i < changes.length; i++) {
        var change = changes[i];
        if (change.added) {
          ret.push("<ins>");
        } else if (change.removed) {
          ret.push("<del>");
        }
        ret.push(escapeHTML(change.value));
        if (change.added) {
          ret.push("</ins>");
        } else if (change.removed) {
          ret.push("</del>");
        }
      }
      return ret.join("");
    }
    function escapeHTML(s) {
      var n = s;
      n = n.replace(/&/g, "&amp;");
      n = n.replace(/</g, "&lt;");
      n = n.replace(/>/g, "&gt;");
      n = n.replace(/"/g, "&quot;");
      return n;
    }
  }
});

// node_modules/diff/libcjs/index.js
var require_libcjs = __commonJS({
  "node_modules/diff/libcjs/index.js"(exports) {
    "use strict";
    Object.defineProperty(exports, "__esModule", { value: true });
    exports.canonicalize = exports.convertChangesToXML = exports.convertChangesToDMP = exports.reversePatch = exports.parsePatch = exports.applyPatches = exports.applyPatch = exports.formatPatch = exports.createPatch = exports.createTwoFilesPatch = exports.structuredPatch = exports.arrayDiff = exports.diffArrays = exports.jsonDiff = exports.diffJson = exports.cssDiff = exports.diffCss = exports.sentenceDiff = exports.diffSentences = exports.diffTrimmedLines = exports.lineDiff = exports.diffLines = exports.wordsWithSpaceDiff = exports.diffWordsWithSpace = exports.wordDiff = exports.diffWords = exports.characterDiff = exports.diffChars = exports.Diff = void 0;
    var base_js_1 = require_base();
    exports.Diff = base_js_1.default;
    var character_js_1 = require_character();
    Object.defineProperty(exports, "diffChars", { enumerable: true, get: function() {
      return character_js_1.diffChars;
    } });
    Object.defineProperty(exports, "characterDiff", { enumerable: true, get: function() {
      return character_js_1.characterDiff;
    } });
    var word_js_1 = require_word();
    Object.defineProperty(exports, "diffWords", { enumerable: true, get: function() {
      return word_js_1.diffWords;
    } });
    Object.defineProperty(exports, "diffWordsWithSpace", { enumerable: true, get: function() {
      return word_js_1.diffWordsWithSpace;
    } });
    Object.defineProperty(exports, "wordDiff", { enumerable: true, get: function() {
      return word_js_1.wordDiff;
    } });
    Object.defineProperty(exports, "wordsWithSpaceDiff", { enumerable: true, get: function() {
      return word_js_1.wordsWithSpaceDiff;
    } });
    var line_js_1 = require_line();
    Object.defineProperty(exports, "diffLines", { enumerable: true, get: function() {
      return line_js_1.diffLines;
    } });
    Object.defineProperty(exports, "diffTrimmedLines", { enumerable: true, get: function() {
      return line_js_1.diffTrimmedLines;
    } });
    Object.defineProperty(exports, "lineDiff", { enumerable: true, get: function() {
      return line_js_1.lineDiff;
    } });
    var sentence_js_1 = require_sentence();
    Object.defineProperty(exports, "diffSentences", { enumerable: true, get: function() {
      return sentence_js_1.diffSentences;
    } });
    Object.defineProperty(exports, "sentenceDiff", { enumerable: true, get: function() {
      return sentence_js_1.sentenceDiff;
    } });
    var css_js_1 = require_css();
    Object.defineProperty(exports, "diffCss", { enumerable: true, get: function() {
      return css_js_1.diffCss;
    } });
    Object.defineProperty(exports, "cssDiff", { enumerable: true, get: function() {
      return css_js_1.cssDiff;
    } });
    var json_js_1 = require_json();
    Object.defineProperty(exports, "diffJson", { enumerable: true, get: function() {
      return json_js_1.diffJson;
    } });
    Object.defineProperty(exports, "canonicalize", { enumerable: true, get: function() {
      return json_js_1.canonicalize;
    } });
    Object.defineProperty(exports, "jsonDiff", { enumerable: true, get: function() {
      return json_js_1.jsonDiff;
    } });
    var array_js_1 = require_array2();
    Object.defineProperty(exports, "diffArrays", { enumerable: true, get: function() {
      return array_js_1.diffArrays;
    } });
    Object.defineProperty(exports, "arrayDiff", { enumerable: true, get: function() {
      return array_js_1.arrayDiff;
    } });
    var apply_js_1 = require_apply();
    Object.defineProperty(exports, "applyPatch", { enumerable: true, get: function() {
      return apply_js_1.applyPatch;
    } });
    Object.defineProperty(exports, "applyPatches", { enumerable: true, get: function() {
      return apply_js_1.applyPatches;
    } });
    var parse_js_1 = require_parse();
    Object.defineProperty(exports, "parsePatch", { enumerable: true, get: function() {
      return parse_js_1.parsePatch;
    } });
    var reverse_js_1 = require_reverse();
    Object.defineProperty(exports, "reversePatch", { enumerable: true, get: function() {
      return reverse_js_1.reversePatch;
    } });
    var create_js_1 = require_create();
    Object.defineProperty(exports, "structuredPatch", { enumerable: true, get: function() {
      return create_js_1.structuredPatch;
    } });
    Object.defineProperty(exports, "createTwoFilesPatch", { enumerable: true, get: function() {
      return create_js_1.createTwoFilesPatch;
    } });
    Object.defineProperty(exports, "createPatch", { enumerable: true, get: function() {
      return create_js_1.createPatch;
    } });
    Object.defineProperty(exports, "formatPatch", { enumerable: true, get: function() {
      return create_js_1.formatPatch;
    } });
    var dmp_js_1 = require_dmp();
    Object.defineProperty(exports, "convertChangesToDMP", { enumerable: true, get: function() {
      return dmp_js_1.convertChangesToDMP;
    } });
    var xml_js_1 = require_xml();
    Object.defineProperty(exports, "convertChangesToXML", { enumerable: true, get: function() {
      return xml_js_1.convertChangesToXML;
    } });
  }
});

// lib/sinon/spy-formatters.js
var require_spy_formatters = __commonJS({
  "lib/sinon/spy-formatters.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var Colorizer = require_colorizer();
    var colororizer = new Colorizer();
    var match = require_samsam().createMatcher;
    var timesInWords = require_times_in_words();
    var inspect = require_util().inspect;
    var jsDiff = require_libcjs();
    var join = arrayProto.join;
    var map = arrayProto.map;
    var push = arrayProto.push;
    var slice = arrayProto.slice;
    function colorSinonMatchText(matcher, calledArg, calledArgMessage) {
      let calledArgumentMessage = calledArgMessage;
      let matcherMessage = matcher.message;
      if (!matcher.test(calledArg)) {
        matcherMessage = colororizer.red(matcher.message);
        if (calledArgumentMessage) {
          calledArgumentMessage = colororizer.green(calledArgumentMessage);
        }
      }
      return `${calledArgumentMessage} ${matcherMessage}`;
    }
    function colorDiffText(diff) {
      const objects = map(diff, function(part) {
        let text = part.value;
        if (part.added) {
          text = colororizer.green(text);
        } else if (part.removed) {
          text = colororizer.red(text);
        }
        if (diff.length === 2) {
          text += " ";
        }
        return text;
      });
      return join(objects, "");
    }
    function quoteStringValue(value) {
      if (typeof value === "string") {
        return JSON.stringify(value);
      }
      return value;
    }
    module.exports = {
      c: function(spyInstance) {
        return timesInWords(spyInstance.callCount);
      },
      n: function(spyInstance) {
        return spyInstance.toString();
      },
      D: function(spyInstance, args) {
        let message = "";
        for (let i = 0, l = spyInstance.callCount; i < l; ++i) {
          if (l > 1) {
            message += `
Call ${i + 1}:`;
          }
          const calledArgs = spyInstance.getCall(i).args;
          const expectedArgs = slice(args);
          for (let j = 0; j < calledArgs.length || j < expectedArgs.length; ++j) {
            let calledArg = calledArgs[j];
            let expectedArg = expectedArgs[j];
            if (calledArg) {
              calledArg = quoteStringValue(calledArg);
            }
            if (expectedArg) {
              expectedArg = quoteStringValue(expectedArg);
            }
            message += "\n";
            const calledArgMessage = j < calledArgs.length ? inspect(calledArg) : "";
            if (match.isMatcher(expectedArg)) {
              message += colorSinonMatchText(
                expectedArg,
                calledArg,
                calledArgMessage
              );
            } else {
              const expectedArgMessage = j < expectedArgs.length ? inspect(expectedArg) : "";
              const diff = jsDiff.diffJson(
                calledArgMessage,
                expectedArgMessage
              );
              message += colorDiffText(diff);
            }
          }
        }
        return message;
      },
      C: function(spyInstance) {
        const calls = [];
        for (let i = 0, l = spyInstance.callCount; i < l; ++i) {
          let stringifiedCall = `    ${spyInstance.getCall(i).toString()}`;
          if (/\n/.test(calls[i - 1])) {
            stringifiedCall = `
${stringifiedCall}`;
          }
          push(calls, stringifiedCall);
        }
        return calls.length > 0 ? `
${join(calls, "\n")}` : "";
      },
      t: function(spyInstance) {
        const objects = [];
        for (let i = 0, l = spyInstance.callCount; i < l; ++i) {
          push(objects, inspect(spyInstance.thisValues[i]));
        }
        return join(objects, ", ");
      },
      "*": function(spyInstance, args) {
        return join(
          map(args, function(arg) {
            return inspect(arg);
          }),
          ", "
        );
      }
    };
  }
});

// lib/sinon/proxy.js
var require_proxy = __commonJS({
  "lib/sinon/proxy.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var extend = require_extend();
    var functionToString = require_function_to_string();
    var proxyCall = require_proxy_call();
    var proxyCallUtil = require_proxy_call_util();
    var proxyInvoke = require_proxy_invoke();
    var inspect = require_util().inspect;
    var push = arrayProto.push;
    var forEach = arrayProto.forEach;
    var slice = arrayProto.slice;
    var emptyFakes = Object.freeze([]);
    var proxyApi = {
      toString: functionToString,
      named: function named(name) {
        this.displayName = name;
        const nameDescriptor = Object.getOwnPropertyDescriptor(this, "name");
        if (nameDescriptor && nameDescriptor.configurable) {
          nameDescriptor.value = name;
          Object.defineProperty(this, "name", nameDescriptor);
        }
        return this;
      },
      invoke: proxyInvoke,
      /*
       * Hook for derived implementation to return fake instances matching the
       * given arguments.
       */
      matchingFakes: function() {
        return emptyFakes;
      },
      getCall: function getCall(index) {
        let i = index;
        if (i < 0) {
          i += this.callCount;
        }
        if (i < 0 || i >= this.callCount) {
          return null;
        }
        return proxyCall(
          this,
          this.thisValues[i],
          this.args[i],
          this.returnValues[i],
          this.exceptions[i],
          this.callIds[i],
          this.errorsWithCallStack[i]
        );
      },
      getCalls: function() {
        const calls = [];
        let i;
        for (i = 0; i < this.callCount; i++) {
          push(calls, this.getCall(i));
        }
        return calls;
      },
      calledBefore: function calledBefore(proxy) {
        if (!this.called) {
          return false;
        }
        if (!proxy.called) {
          return true;
        }
        return this.callIds[0] < proxy.callIds[proxy.callIds.length - 1];
      },
      calledAfter: function calledAfter(proxy) {
        if (!this.called || !proxy.called) {
          return false;
        }
        return this.callIds[this.callCount - 1] > proxy.callIds[0];
      },
      calledImmediatelyBefore: function calledImmediatelyBefore(proxy) {
        if (!this.called || !proxy.called) {
          return false;
        }
        return this.callIds[this.callCount - 1] === proxy.callIds[proxy.callCount - 1] - 1;
      },
      calledImmediatelyAfter: function calledImmediatelyAfter(proxy) {
        if (!this.called || !proxy.called) {
          return false;
        }
        return this.callIds[this.callCount - 1] === proxy.callIds[proxy.callCount - 1] + 1;
      },
      formatters: require_spy_formatters(),
      printf: function(format) {
        const spyInstance = this;
        const args = slice(arguments, 1);
        let formatter;
        return (format || "").replace(/%(.)/g, function(match, specifier) {
          formatter = proxyApi.formatters[specifier];
          if (typeof formatter === "function") {
            return String(formatter(spyInstance, args));
          } else if (!isNaN(parseInt(specifier, 10))) {
            return inspect(args[specifier - 1]);
          }
          return `%${specifier}`;
        });
      },
      resetHistory: function() {
        if (this.invoking) {
          const err = new Error(
            "Cannot reset Sinon function while invoking it. Move the call to .resetHistory outside of the callback."
          );
          err.name = "InvalidResetException";
          throw err;
        }
        this.called = false;
        this.notCalled = true;
        this.calledOnce = false;
        this.calledTwice = false;
        this.calledThrice = false;
        this.callCount = 0;
        this.firstCall = null;
        this.secondCall = null;
        this.thirdCall = null;
        this.lastCall = null;
        this.args = [];
        this.firstArg = null;
        this.lastArg = null;
        this.returnValues = [];
        this.thisValues = [];
        this.exceptions = [];
        this.callIds = [];
        this.errorsWithCallStack = [];
        if (this.fakes) {
          forEach(this.fakes, function(fake) {
            fake.resetHistory();
          });
        }
        return this;
      }
    };
    var delegateToCalls = proxyCallUtil.delegateToCalls;
    delegateToCalls(proxyApi, "calledOn", true);
    delegateToCalls(proxyApi, "alwaysCalledOn", false, "calledOn");
    delegateToCalls(proxyApi, "calledWith", true);
    delegateToCalls(
      proxyApi,
      "calledOnceWith",
      true,
      "calledWith",
      false,
      void 0,
      1
    );
    delegateToCalls(proxyApi, "calledWithMatch", true);
    delegateToCalls(proxyApi, "alwaysCalledWith", false, "calledWith");
    delegateToCalls(proxyApi, "alwaysCalledWithMatch", false, "calledWithMatch");
    delegateToCalls(proxyApi, "calledWithExactly", true);
    delegateToCalls(
      proxyApi,
      "calledOnceWithExactly",
      true,
      "calledWithExactly",
      false,
      void 0,
      1
    );
    delegateToCalls(
      proxyApi,
      "calledOnceWithMatch",
      true,
      "calledWithMatch",
      false,
      void 0,
      1
    );
    delegateToCalls(
      proxyApi,
      "alwaysCalledWithExactly",
      false,
      "calledWithExactly"
    );
    delegateToCalls(
      proxyApi,
      "neverCalledWith",
      false,
      "notCalledWith",
      false,
      function() {
        return true;
      }
    );
    delegateToCalls(
      proxyApi,
      "neverCalledWithMatch",
      false,
      "notCalledWithMatch",
      false,
      function() {
        return true;
      }
    );
    delegateToCalls(proxyApi, "threw", true);
    delegateToCalls(proxyApi, "alwaysThrew", false, "threw");
    delegateToCalls(proxyApi, "returned", true);
    delegateToCalls(proxyApi, "alwaysReturned", false, "returned");
    delegateToCalls(proxyApi, "calledWithNew", true);
    delegateToCalls(proxyApi, "alwaysCalledWithNew", false, "calledWithNew");
    function createProxy(func, originalFunc) {
      const proxy = wrapFunction(func, originalFunc);
      extend(proxy, func);
      proxy.prototype = func.prototype;
      extend.nonEnum(proxy, proxyApi);
      return proxy;
    }
    function wrapFunction(func, originalFunc) {
      const arity = originalFunc.length;
      let p;
      switch (arity) {
        /*eslint-disable no-unused-vars, max-len*/
        case 0:
          p = function proxy() {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 1:
          p = function proxy(a) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 2:
          p = function proxy(a, b) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 3:
          p = function proxy(a, b, c) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 4:
          p = function proxy(a, b, c, d) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 5:
          p = function proxy(a, b, c, d, e) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 6:
          p = function proxy(a, b, c, d, e, f) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 7:
          p = function proxy(a, b, c, d, e, f, g) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 8:
          p = function proxy(a, b, c, d, e, f, g, h) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 9:
          p = function proxy(a, b, c, d, e, f, g, h, i) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 10:
          p = function proxy(a, b, c, d, e, f, g, h, i, j) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 11:
          p = function proxy(a, b, c, d, e, f, g, h, i, j, k) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        case 12:
          p = function proxy(a, b, c, d, e, f, g, h, i, j, k, l) {
            return p.invoke(func, this, slice(arguments));
          };
          break;
        default:
          p = function proxy() {
            return p.invoke(func, this, slice(arguments));
          };
          break;
      }
      const nameDescriptor = Object.getOwnPropertyDescriptor(
        originalFunc,
        "name"
      );
      if (nameDescriptor && nameDescriptor.configurable) {
        Object.defineProperty(p, "name", nameDescriptor);
      }
      extend.nonEnum(p, {
        isSinonProxy: true,
        called: false,
        notCalled: true,
        calledOnce: false,
        calledTwice: false,
        calledThrice: false,
        callCount: 0,
        firstCall: null,
        firstArg: null,
        secondCall: null,
        thirdCall: null,
        lastCall: null,
        lastArg: null,
        args: [],
        returnValues: [],
        thisValues: [],
        exceptions: [],
        callIds: [],
        errorsWithCallStack: []
      });
      return p;
    }
    module.exports = createProxy;
  }
});

// lib/sinon/util/core/is-non-existent-property.js
var require_is_non_existent_property = __commonJS({
  "lib/sinon/util/core/is-non-existent-property.js"(exports, module) {
    "use strict";
    function isNonExistentProperty(object, property) {
      return Boolean(
        object && typeof property !== "undefined" && !(property in object)
      );
    }
    module.exports = isNonExistentProperty;
  }
});

// lib/sinon/util/core/is-es-module.js
var require_is_es_module = __commonJS({
  "lib/sinon/util/core/is-es-module.js"(exports, module) {
    "use strict";
    module.exports = function(object) {
      return object && typeof Symbol !== "undefined" && object[Symbol.toStringTag] === "Module" && Object.isSealed(object);
    };
  }
});

// lib/sinon/util/core/walk-object.js
var require_walk_object = __commonJS({
  "lib/sinon/util/core/walk-object.js"(exports, module) {
    "use strict";
    var functionName = require_lib().functionName;
    var getPropertyDescriptor = require_get_property_descriptor();
    var walk = require_walk();
    function walkObject(mutator, object, filter) {
      let called = false;
      const name = functionName(mutator);
      if (!object) {
        throw new Error(
          `Trying to ${name} object but received ${String(object)}`
        );
      }
      walk(object, function(prop, propOwner) {
        if (propOwner !== Object.prototype && prop !== "constructor" && typeof getPropertyDescriptor(propOwner, prop).value === "function") {
          if (filter) {
            if (filter(object, prop)) {
              called = true;
              mutator(object, prop);
            }
          } else {
            called = true;
            mutator(object, prop);
          }
        }
      });
      if (!called) {
        throw new Error(
          `Found no methods on object to which we could apply mutations`
        );
      }
      return object;
    }
    module.exports = walkObject;
  }
});

// lib/sinon/util/core/sinon-type.js
var require_sinon_type = __commonJS({
  "lib/sinon/util/core/sinon-type.js"(exports, module) {
    "use strict";
    var sinonTypeSymbolProperty = /* @__PURE__ */ Symbol("SinonType");
    module.exports = {
      /**
       * Set the type of a Sinon object to make it possible to identify it later at runtime
       *
       * @param {object|Function} object  object/function to set the type on
       * @param {string} type the named type of the object/function
       */
      set(object, type) {
        Object.defineProperty(object, sinonTypeSymbolProperty, {
          value: type,
          configurable: false,
          enumerable: false
        });
      },
      get(object) {
        return object && object[sinonTypeSymbolProperty];
      }
    };
  }
});

// lib/sinon/util/core/wrap-method.js
var require_wrap_method = __commonJS({
  "lib/sinon/util/core/wrap-method.js"(exports, module) {
    "use strict";
    var noop = () => {
    };
    var getPropertyDescriptor = require_get_property_descriptor();
    var extend = require_extend();
    var sinonType = require_sinon_type();
    var hasOwnProperty = require_lib().prototypes.object.hasOwnProperty;
    var valueToString = require_lib().valueToString;
    var push = require_lib().prototypes.array.push;
    function isFunction(obj) {
      return typeof obj === "function" || Boolean(obj && obj.constructor && obj.call && obj.apply);
    }
    function mirrorProperties(target, source) {
      for (const prop in source) {
        if (!hasOwnProperty(target, prop)) {
          target[prop] = source[prop];
        }
      }
    }
    function getAccessor(object, property, method) {
      const accessors = ["get", "set"];
      const descriptor = getPropertyDescriptor(object, property);
      for (let i = 0; i < accessors.length; i++) {
        if (descriptor[accessors[i]] && descriptor[accessors[i]].name === method.name) {
          return accessors[i];
        }
      }
      return null;
    }
    var hasES5Support = "keys" in Object;
    module.exports = function wrapMethod(object, property, method) {
      if (!object) {
        throw new TypeError("Should wrap property of object");
      }
      if (typeof method !== "function" && typeof method !== "object") {
        throw new TypeError(
          "Method wrapper should be a function or a property descriptor"
        );
      }
      function checkWrappedMethod(wrappedMethod2) {
        let error2;
        if (!isFunction(wrappedMethod2)) {
          error2 = new TypeError(
            `Attempted to wrap ${typeof wrappedMethod2} property ${valueToString(
              property
            )} as function`
          );
        } else if (wrappedMethod2.restore && wrappedMethod2.restore.sinon) {
          error2 = new TypeError(
            `Attempted to wrap ${valueToString(
              property
            )} which is already wrapped`
          );
        } else if (wrappedMethod2.calledBefore) {
          const verb = wrappedMethod2.returns ? "stubbed" : "spied on";
          error2 = new TypeError(
            `Attempted to wrap ${valueToString(
              property
            )} which is already ${verb}`
          );
        }
        if (error2) {
          if (wrappedMethod2 && wrappedMethod2.stackTraceError) {
            error2.stack += `
--------------
${wrappedMethod2.stackTraceError.stack}`;
          }
          throw error2;
        }
      }
      let error, wrappedMethod, i, wrappedMethodDesc, target, accessor;
      const wrappedMethods = [];
      function simplePropertyAssignment() {
        wrappedMethod = object[property];
        checkWrappedMethod(wrappedMethod);
        object[property] = method;
        method.displayName = property;
      }
      const owned = object.hasOwnProperty ? object.hasOwnProperty(property) : hasOwnProperty(object, property);
      if (hasES5Support) {
        const methodDesc = typeof method === "function" ? { value: method } : method;
        wrappedMethodDesc = getPropertyDescriptor(object, property);
        if (!wrappedMethodDesc) {
          error = new TypeError(
            `Attempted to wrap ${typeof wrappedMethod} property ${property} as function`
          );
        } else if (wrappedMethodDesc.restore && wrappedMethodDesc.restore.sinon) {
          error = new TypeError(
            `Attempted to wrap ${property} which is already wrapped`
          );
        }
        if (error) {
          if (wrappedMethodDesc && wrappedMethodDesc.stackTraceError) {
            error.stack += `
--------------
${wrappedMethodDesc.stackTraceError.stack}`;
          }
          throw error;
        }
        const types = Object.keys(methodDesc);
        for (i = 0; i < types.length; i++) {
          wrappedMethod = wrappedMethodDesc[types[i]];
          checkWrappedMethod(wrappedMethod);
          push(wrappedMethods, wrappedMethod);
        }
        mirrorProperties(methodDesc, wrappedMethodDesc);
        for (i = 0; i < types.length; i++) {
          mirrorProperties(methodDesc[types[i]], wrappedMethodDesc[types[i]]);
        }
        if (!owned) {
          methodDesc.configurable = true;
        }
        Object.defineProperty(object, property, methodDesc);
        if (typeof method === "function" && object[property] !== method) {
          delete object[property];
          simplePropertyAssignment();
        }
      } else {
        simplePropertyAssignment();
      }
      extendObjectWithWrappedMethods();
      function extendObjectWithWrappedMethods() {
        for (i = 0; i < wrappedMethods.length; i++) {
          accessor = getAccessor(object, property, wrappedMethods[i]);
          target = accessor ? method[accessor] : method;
          extend.nonEnum(target, {
            displayName: property,
            wrappedMethod: wrappedMethods[i],
            // Set up an Error object for a stack trace which can be used later to find what line of
            // code the original method was created on.
            stackTraceError: new Error("Stack Trace for original"),
            restore
          });
          target.restore.sinon = true;
          if (!hasES5Support) {
            mirrorProperties(target, wrappedMethod);
          }
        }
      }
      function restore() {
        accessor = getAccessor(object, property, this.wrappedMethod);
        let descriptor;
        if (accessor) {
          if (!owned) {
            try {
              delete object[property][accessor];
            } catch (e) {
            }
          } else if (hasES5Support) {
            descriptor = getPropertyDescriptor(object, property);
            descriptor[accessor] = wrappedMethodDesc[accessor];
            Object.defineProperty(object, property, descriptor);
          }
          if (hasES5Support) {
            descriptor = getPropertyDescriptor(object, property);
            if (descriptor && descriptor.value === target) {
              object[property][accessor] = this.wrappedMethod;
            }
          } else {
            if (object[property][accessor] === target) {
              object[property][accessor] = this.wrappedMethod;
            }
          }
        } else {
          if (!owned) {
            try {
              delete object[property];
            } catch (e) {
            }
          } else if (hasES5Support) {
            Object.defineProperty(object, property, wrappedMethodDesc);
          }
          if (hasES5Support) {
            descriptor = getPropertyDescriptor(object, property);
            if (descriptor && descriptor.value === target) {
              object[property] = this.wrappedMethod;
            }
          } else {
            if (object[property] === target) {
              object[property] = this.wrappedMethod;
            }
          }
        }
        if (sinonType.get(object) === "stub-instance") {
          object[property] = noop;
        }
      }
      return method;
    };
  }
});

// lib/sinon/spy.js
var require_spy = __commonJS({
  "lib/sinon/spy.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var createProxy = require_proxy();
    var extend = require_extend();
    var functionName = require_lib().functionName;
    var getPropertyDescriptor = require_get_property_descriptor();
    var deepEqual = require_samsam().deepEqual;
    var isEsModule = require_is_es_module();
    var proxyCallUtil = require_proxy_call_util();
    var walkObject = require_walk_object();
    var wrapMethod = require_wrap_method();
    var valueToString = require_lib().valueToString;
    var forEach = arrayProto.forEach;
    var pop = arrayProto.pop;
    var push = arrayProto.push;
    var slice = arrayProto.slice;
    var filter = Array.prototype.filter;
    var uuid = 0;
    function matches(fake, args, strict) {
      const margs = fake.matchingArguments;
      if (margs.length <= args.length && deepEqual(slice(args, 0, margs.length), margs)) {
        return !strict || margs.length === args.length;
      }
      return false;
    }
    var spyApi = {
      withArgs: function() {
        const args = slice(arguments);
        const matching = pop(this.matchingFakes(args, true));
        if (matching) {
          return matching;
        }
        const original = this;
        const fake = this.instantiateFake();
        fake.matchingArguments = args;
        fake.parent = this;
        push(this.fakes, fake);
        fake.withArgs = function() {
          return original.withArgs.apply(original, arguments);
        };
        forEach(original.args, function(arg, i) {
          if (!matches(fake, arg)) {
            return;
          }
          proxyCallUtil.incrementCallCount(fake);
          push(fake.thisValues, original.thisValues[i]);
          push(fake.args, arg);
          push(fake.returnValues, original.returnValues[i]);
          push(fake.exceptions, original.exceptions[i]);
          push(fake.callIds, original.callIds[i]);
        });
        proxyCallUtil.createCallProperties(fake);
        return fake;
      },
      // Override proxy default implementation
      matchingFakes: function(args, strict) {
        return filter.call(this.fakes, function(fake) {
          return matches(fake, args, strict);
        });
      }
    };
    var delegateToCalls = proxyCallUtil.delegateToCalls;
    delegateToCalls(spyApi, "callArg", false, "callArgWith", true, function() {
      throw new Error(
        `${this.toString()} cannot call arg since it was not yet invoked.`
      );
    });
    spyApi.callArgWith = spyApi.callArg;
    delegateToCalls(spyApi, "callArgOn", false, "callArgOnWith", true, function() {
      throw new Error(
        `${this.toString()} cannot call arg since it was not yet invoked.`
      );
    });
    spyApi.callArgOnWith = spyApi.callArgOn;
    delegateToCalls(spyApi, "throwArg", false, "throwArg", false, function() {
      throw new Error(
        `${this.toString()} cannot throw arg since it was not yet invoked.`
      );
    });
    delegateToCalls(spyApi, "yield", false, "yield", true, function() {
      throw new Error(
        `${this.toString()} cannot yield since it was not yet invoked.`
      );
    });
    spyApi.invokeCallback = spyApi.yield;
    delegateToCalls(spyApi, "yieldOn", false, "yieldOn", true, function() {
      throw new Error(
        `${this.toString()} cannot yield since it was not yet invoked.`
      );
    });
    delegateToCalls(spyApi, "yieldTo", false, "yieldTo", true, function(property) {
      throw new Error(
        `${this.toString()} cannot yield to '${valueToString(
          property
        )}' since it was not yet invoked.`
      );
    });
    delegateToCalls(
      spyApi,
      "yieldToOn",
      false,
      "yieldToOn",
      true,
      function(property) {
        throw new Error(
          `${this.toString()} cannot yield to '${valueToString(
            property
          )}' since it was not yet invoked.`
        );
      }
    );
    function createSpy(func) {
      let name;
      let funk = func;
      if (typeof funk !== "function") {
        funk = function() {
          return;
        };
      } else {
        name = functionName(funk);
      }
      const proxy = createProxy(funk, funk);
      extend.nonEnum(proxy, spyApi);
      extend.nonEnum(proxy, {
        displayName: name || "spy",
        fakes: [],
        instantiateFake: createSpy,
        id: `spy#${uuid++}`
      });
      return proxy;
    }
    function spy(object, property, types) {
      if (isEsModule(object)) {
        throw new TypeError("ES Modules cannot be spied");
      }
      if (!property && typeof object === "function") {
        return createSpy(object);
      }
      if (!property && typeof object === "object") {
        return walkObject(spy, object);
      }
      if (!object && !property) {
        return createSpy(function() {
          return;
        });
      }
      if (!types) {
        return wrapMethod(object, property, createSpy(object[property]));
      }
      const descriptor = {};
      const methodDesc = getPropertyDescriptor(object, property);
      forEach(types, function(type) {
        descriptor[type] = createSpy(methodDesc[type]);
      });
      return wrapMethod(object, property, descriptor);
    }
    extend(spy, spyApi);
    module.exports = spy;
  }
});

// lib/sinon/throw-on-falsy-object.js
var require_throw_on_falsy_object = __commonJS({
  "lib/sinon/throw-on-falsy-object.js"(exports, module) {
    "use strict";
    var valueToString = require_lib().valueToString;
    function throwOnFalsyObject(object, property) {
      if (property && !object) {
        const type = object === null ? "null" : "undefined";
        throw new Error(
          `Trying to stub property '${valueToString(property)}' of ${type}`
        );
      }
    }
    module.exports = throwOnFalsyObject;
  }
});

// lib/sinon/stub.js
var require_stub = __commonJS({
  "lib/sinon/stub.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var behavior = require_behavior();
    var behaviors = require_default_behaviors();
    var createProxy = require_proxy();
    var functionName = require_lib().functionName;
    var hasOwnProperty = require_lib().prototypes.object.hasOwnProperty;
    var isNonExistentProperty = require_is_non_existent_property();
    var spy = require_spy();
    var extend = require_extend();
    var getPropertyDescriptor = require_get_property_descriptor();
    var isEsModule = require_is_es_module();
    var sinonType = require_sinon_type();
    var wrapMethod = require_wrap_method();
    var throwOnFalsyObject = require_throw_on_falsy_object();
    var valueToString = require_lib().valueToString;
    var walkObject = require_walk_object();
    var forEach = arrayProto.forEach;
    var pop = arrayProto.pop;
    var slice = arrayProto.slice;
    var sort = arrayProto.sort;
    var uuid = 0;
    function createStub(originalFunc) {
      let proxy;
      function functionStub() {
        const args = slice(arguments);
        const matchings = proxy.matchingFakes(args);
        const fnStub = pop(
          sort(matchings, function(a, b) {
            return a.matchingArguments.length - b.matchingArguments.length;
          })
        ) || proxy;
        return getCurrentBehavior(fnStub).invoke(this, arguments);
      }
      proxy = createProxy(functionStub, originalFunc || functionStub);
      extend.nonEnum(proxy, spy);
      extend.nonEnum(proxy, stub);
      const name = originalFunc ? functionName(originalFunc) : null;
      extend.nonEnum(proxy, {
        fakes: [],
        instantiateFake: createStub,
        displayName: name || "stub",
        defaultBehavior: null,
        behaviors: [],
        id: `stub#${uuid++}`
      });
      sinonType.set(proxy, "stub");
      return proxy;
    }
    function stub(object, property) {
      if (arguments.length > 2) {
        throw new TypeError(
          "stub(obj, 'meth', fn) has been removed, see documentation"
        );
      }
      if (isEsModule(object)) {
        throw new TypeError("ES Modules cannot be stubbed");
      }
      throwOnFalsyObject.apply(null, arguments);
      if (isNonExistentProperty(object, property)) {
        throw new TypeError(
          `Cannot stub non-existent property ${valueToString(property)}`
        );
      }
      const actualDescriptor = getPropertyDescriptor(object, property);
      assertValidPropertyDescriptor(actualDescriptor, property);
      const isObjectOrFunction = typeof object === "object" || typeof object === "function";
      const isStubbingEntireObject = typeof property === "undefined" && isObjectOrFunction;
      const isCreatingNewStub = !object && typeof property === "undefined";
      const isStubbingNonFuncProperty = isObjectOrFunction && typeof property !== "undefined" && (typeof actualDescriptor === "undefined" || typeof actualDescriptor.value !== "function");
      if (isStubbingEntireObject) {
        return walkObject(stub, object);
      }
      if (isCreatingNewStub) {
        return createStub();
      }
      const func = typeof actualDescriptor.value === "function" ? actualDescriptor.value : null;
      const s = createStub(func);
      extend.nonEnum(s, {
        rootObj: object,
        propName: property,
        shadowsPropOnPrototype: !actualDescriptor.isOwn,
        restore: function restore() {
          if (actualDescriptor !== void 0 && actualDescriptor.isOwn) {
            Object.defineProperty(object, property, actualDescriptor);
            return;
          }
          delete object[property];
        }
      });
      return isStubbingNonFuncProperty ? s : wrapMethod(object, property, s);
    }
    function assertValidPropertyDescriptor(descriptor, property) {
      if (!descriptor || !property) {
        return;
      }
      if (descriptor.isOwn && !descriptor.configurable && !descriptor.writable) {
        throw new TypeError(
          `The descriptor for property \`${property}\` is non-configurable and non-writable. Sinon cannot stub properties that are immutable. See https://sinonjs.org/faq#property-descriptor-errors for help fixing this issue.`
        );
      }
      if ((descriptor.get || descriptor.set) && !descriptor.configurable) {
        throw new TypeError(
          `Descriptor for accessor property ${property} is non-configurable`
        );
      }
      if (isDataDescriptor(descriptor) && !descriptor.writable) {
        throw new TypeError(
          `Descriptor for data property ${property} is non-writable`
        );
      }
    }
    function isDataDescriptor(descriptor) {
      return !descriptor.value && !descriptor.writable && !descriptor.set && !descriptor.get;
    }
    function getParentBehaviour(stubInstance) {
      return stubInstance.parent && getCurrentBehavior(stubInstance.parent);
    }
    function getDefaultBehavior(stubInstance) {
      return stubInstance.defaultBehavior || getParentBehaviour(stubInstance) || behavior.create(stubInstance);
    }
    function getCurrentBehavior(stubInstance) {
      const currentBehavior = stubInstance.behaviors[stubInstance.callCount - 1];
      return currentBehavior && currentBehavior.isPresent() ? currentBehavior : getDefaultBehavior(stubInstance);
    }
    var proto = {
      resetBehavior: function() {
        this.defaultBehavior = null;
        this.behaviors = [];
        delete this.returnValue;
        delete this.returnArgAt;
        delete this.throwArgAt;
        delete this.resolveArgAt;
        delete this.fakeFn;
        this.returnThis = false;
        this.resolveThis = false;
        forEach(this.fakes, function(fake) {
          fake.resetBehavior();
        });
      },
      reset: function() {
        this.resetHistory();
        this.resetBehavior();
      },
      onCall: function onCall(index) {
        if (!this.behaviors[index]) {
          this.behaviors[index] = behavior.create(this);
        }
        return this.behaviors[index];
      },
      onFirstCall: function onFirstCall() {
        return this.onCall(0);
      },
      onSecondCall: function onSecondCall() {
        return this.onCall(1);
      },
      onThirdCall: function onThirdCall() {
        return this.onCall(2);
      },
      withArgs: function withArgs() {
        const fake = spy.withArgs.apply(this, arguments);
        if (this.defaultBehavior && this.defaultBehavior.promiseLibrary) {
          fake.defaultBehavior = fake.defaultBehavior || behavior.create(fake);
          fake.defaultBehavior.promiseLibrary = this.defaultBehavior.promiseLibrary;
        }
        return fake;
      }
    };
    forEach(Object.keys(behavior), function(method) {
      if (hasOwnProperty(behavior, method) && !hasOwnProperty(proto, method) && method !== "create" && method !== "invoke") {
        proto[method] = behavior.createBehavior(method);
      }
    });
    forEach(Object.keys(behaviors), function(method) {
      if (hasOwnProperty(behaviors, method) && !hasOwnProperty(proto, method)) {
        behavior.addBehavior(stub, method, behaviors[method]);
      }
    });
    extend(stub, proto);
    module.exports = stub;
  }
});

// lib/sinon/mock-expectation.js
var require_mock_expectation = __commonJS({
  "lib/sinon/mock-expectation.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var proxyInvoke = require_proxy_invoke();
    var proxyCallToString = require_proxy_call().toString;
    var timesInWords = require_times_in_words();
    var extend = require_extend();
    var match = require_samsam().createMatcher;
    var stub = require_stub();
    var assert = require_assert();
    var deepEqual = require_samsam().deepEqual;
    var inspect = require_util().inspect;
    var valueToString = require_lib().valueToString;
    var every = arrayProto.every;
    var forEach = arrayProto.forEach;
    var push = arrayProto.push;
    var slice = arrayProto.slice;
    function callCountInWords(callCount) {
      if (callCount === 0) {
        return "never called";
      }
      return `called ${timesInWords(callCount)}`;
    }
    function expectedCallCountInWords(expectation) {
      const min = expectation.minCalls;
      const max = expectation.maxCalls;
      if (typeof min === "number" && typeof max === "number") {
        let str = timesInWords(min);
        if (min !== max) {
          str = `at least ${str} and at most ${timesInWords(max)}`;
        }
        return str;
      }
      if (typeof min === "number") {
        return `at least ${timesInWords(min)}`;
      }
      return `at most ${timesInWords(max)}`;
    }
    function receivedMinCalls(expectation) {
      const hasMinLimit = typeof expectation.minCalls === "number";
      return !hasMinLimit || expectation.callCount >= expectation.minCalls;
    }
    function receivedMaxCalls(expectation) {
      if (typeof expectation.maxCalls !== "number") {
        return false;
      }
      return expectation.callCount === expectation.maxCalls;
    }
    function verifyMatcher(possibleMatcher, arg) {
      const isMatcher = match.isMatcher(possibleMatcher);
      return isMatcher && possibleMatcher.test(arg) || true;
    }
    var mockExpectation = {
      minCalls: 1,
      maxCalls: 1,
      create: function create(methodName) {
        const expectation = extend.nonEnum(stub(), mockExpectation);
        delete expectation.create;
        expectation.method = methodName;
        return expectation;
      },
      invoke: function invoke(func, thisValue, args) {
        this.verifyCallAllowed(thisValue, args);
        return proxyInvoke.apply(this, arguments);
      },
      atLeast: function atLeast(num) {
        if (typeof num !== "number") {
          throw new TypeError(`'${valueToString(num)}' is not number`);
        }
        if (!this.limitsSet) {
          this.maxCalls = null;
          this.limitsSet = true;
        }
        this.minCalls = num;
        return this;
      },
      atMost: function atMost(num) {
        if (typeof num !== "number") {
          throw new TypeError(`'${valueToString(num)}' is not number`);
        }
        if (!this.limitsSet) {
          this.minCalls = null;
          this.limitsSet = true;
        }
        this.maxCalls = num;
        return this;
      },
      never: function never() {
        return this.exactly(0);
      },
      once: function once() {
        return this.exactly(1);
      },
      twice: function twice() {
        return this.exactly(2);
      },
      thrice: function thrice() {
        return this.exactly(3);
      },
      exactly: function exactly(num) {
        if (typeof num !== "number") {
          throw new TypeError(`'${valueToString(num)}' is not a number`);
        }
        this.atLeast(num);
        return this.atMost(num);
      },
      met: function met() {
        return !this.failed && receivedMinCalls(this);
      },
      verifyCallAllowed: function verifyCallAllowed(thisValue, args) {
        const expectedArguments = this.expectedArguments;
        if (receivedMaxCalls(this)) {
          this.failed = true;
          mockExpectation.fail(
            `${this.method} already called ${timesInWords(this.maxCalls)}`
          );
        }
        if ("expectedThis" in this && this.expectedThis !== thisValue) {
          mockExpectation.fail(
            `${this.method} called with ${valueToString(
              thisValue
            )} as thisValue, expected ${valueToString(this.expectedThis)}`
          );
        }
        if (!("expectedArguments" in this)) {
          return;
        }
        if (!args) {
          mockExpectation.fail(
            `${this.method} received no arguments, expected ${inspect(
              expectedArguments
            )}`
          );
        }
        if (args.length < expectedArguments.length) {
          mockExpectation.fail(
            `${this.method} received too few arguments (${inspect(
              args
            )}), expected ${inspect(expectedArguments)}`
          );
        }
        if (this.expectsExactArgCount && args.length !== expectedArguments.length) {
          mockExpectation.fail(
            `${this.method} received too many arguments (${inspect(
              args
            )}), expected ${inspect(expectedArguments)}`
          );
        }
        forEach(
          expectedArguments,
          function(expectedArgument, i) {
            if (!verifyMatcher(expectedArgument, args[i])) {
              mockExpectation.fail(
                `${this.method} received wrong arguments ${inspect(
                  args
                )}, didn't match ${String(expectedArguments)}`
              );
            }
            if (!deepEqual(args[i], expectedArgument)) {
              mockExpectation.fail(
                `${this.method} received wrong arguments ${inspect(
                  args
                )}, expected ${inspect(expectedArguments)}`
              );
            }
          },
          this
        );
      },
      allowsCall: function allowsCall(thisValue, args) {
        const expectedArguments = this.expectedArguments;
        if (this.met() && receivedMaxCalls(this)) {
          return false;
        }
        if ("expectedThis" in this && this.expectedThis !== thisValue) {
          return false;
        }
        if (!("expectedArguments" in this)) {
          return true;
        }
        const _args = args || [];
        if (_args.length < expectedArguments.length) {
          return false;
        }
        if (this.expectsExactArgCount && _args.length !== expectedArguments.length) {
          return false;
        }
        return every(expectedArguments, function(expectedArgument, i) {
          if (!verifyMatcher(expectedArgument, _args[i])) {
            return false;
          }
          if (!deepEqual(_args[i], expectedArgument)) {
            return false;
          }
          return true;
        });
      },
      withArgs: function withArgs() {
        this.expectedArguments = slice(arguments);
        return this;
      },
      withExactArgs: function withExactArgs() {
        this.withArgs.apply(this, arguments);
        this.expectsExactArgCount = true;
        return this;
      },
      on: function on(thisValue) {
        this.expectedThis = thisValue;
        return this;
      },
      toString: function() {
        const args = slice(this.expectedArguments || []);
        if (!this.expectsExactArgCount) {
          push(args, "[...]");
        }
        const callStr = proxyCallToString.call({
          proxy: this.method || "anonymous mock expectation",
          args
        });
        const message = `${callStr.replace(
          ", [...",
          "[, ..."
        )} ${expectedCallCountInWords(this)}`;
        if (this.met()) {
          return `Expectation met: ${message}`;
        }
        return `Expected ${message} (${callCountInWords(this.callCount)})`;
      },
      verify: function verify() {
        if (!this.met()) {
          mockExpectation.fail(String(this));
        } else {
          mockExpectation.pass(String(this));
        }
        return true;
      },
      pass: function pass(message) {
        assert.pass(message);
      },
      fail: function fail(message) {
        const exception = new Error(message);
        exception.name = "ExpectationError";
        throw exception;
      }
    };
    module.exports = mockExpectation;
  }
});

// lib/sinon/mock.js
var require_mock = __commonJS({
  "lib/sinon/mock.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var mockExpectation = require_mock_expectation();
    var proxyCallToString = require_proxy_call().toString;
    var extend = require_extend();
    var deepEqual = require_samsam().deepEqual;
    var wrapMethod = require_wrap_method();
    var concat = arrayProto.concat;
    var filter = arrayProto.filter;
    var forEach = arrayProto.forEach;
    var every = arrayProto.every;
    var join = arrayProto.join;
    var push = arrayProto.push;
    var slice = arrayProto.slice;
    var unshift = arrayProto.unshift;
    function mock(object) {
      if (!object || typeof object === "string") {
        return mockExpectation.create(object ? object : "Anonymous mock");
      }
      return mock.create(object);
    }
    function each(collection, callback) {
      const col = collection || [];
      forEach(col, callback);
    }
    function arrayEquals(arr1, arr2, compareLength) {
      if (compareLength && arr1.length !== arr2.length) {
        return false;
      }
      return every(arr1, function(element, i) {
        return deepEqual(arr2[i], element);
      });
    }
    extend(mock, {
      create: function create(object) {
        if (!object) {
          throw new TypeError("object is null");
        }
        const mockObject = extend.nonEnum({}, mock, { object });
        delete mockObject.create;
        return mockObject;
      },
      expects: function expects(method) {
        if (!method) {
          throw new TypeError("method is falsy");
        }
        if (!this.expectations) {
          this.expectations = {};
          this.proxies = [];
          this.failures = [];
        }
        if (!this.expectations[method]) {
          this.expectations[method] = [];
          const mockObject = this;
          wrapMethod(this.object, method, function() {
            return mockObject.invokeMethod(method, this, arguments);
          });
          push(this.proxies, method);
        }
        const expectation = mockExpectation.create(method);
        expectation.wrappedMethod = this.object[method].wrappedMethod;
        push(this.expectations[method], expectation);
        return expectation;
      },
      restore: function restore() {
        const object = this.object;
        each(this.proxies, function(proxy) {
          if (typeof object[proxy].restore === "function") {
            object[proxy].restore();
          }
        });
      },
      verify: function verify() {
        const expectations = this.expectations || {};
        const messages = this.failures ? slice(this.failures) : [];
        const met = [];
        each(this.proxies, function(proxy) {
          each(expectations[proxy], function(expectation) {
            if (!expectation.met()) {
              push(messages, String(expectation));
            } else {
              push(met, String(expectation));
            }
          });
        });
        this.restore();
        if (messages.length > 0) {
          mockExpectation.fail(join(concat(messages, met), "\n"));
        } else if (met.length > 0) {
          mockExpectation.pass(join(concat(messages, met), "\n"));
        }
        return true;
      },
      invokeMethod: function invokeMethod(method, thisValue, args) {
        const expectations = this.expectations && this.expectations[method] ? this.expectations[method] : [];
        const currentArgs = args || [];
        let available;
        const expectationsWithMatchingArgs = filter(
          expectations,
          function(expectation) {
            const expectedArgs = expectation.expectedArguments || [];
            return arrayEquals(
              expectedArgs,
              currentArgs,
              expectation.expectsExactArgCount
            );
          }
        );
        const expectationsToApply = filter(
          expectationsWithMatchingArgs,
          function(expectation) {
            return !expectation.met() && expectation.allowsCall(thisValue, args);
          }
        );
        if (expectationsToApply.length > 0) {
          return expectationsToApply[0].apply(thisValue, args);
        }
        const messages = [];
        let exhausted = 0;
        forEach(expectationsWithMatchingArgs, function(expectation) {
          if (expectation.allowsCall(thisValue, args)) {
            available = available || expectation;
          } else {
            exhausted += 1;
          }
        });
        if (available && exhausted === 0) {
          return available.apply(thisValue, args);
        }
        forEach(expectations, function(expectation) {
          push(messages, `    ${String(expectation)}`);
        });
        unshift(
          messages,
          `Unexpected call: ${proxyCallToString.call({
            proxy: method,
            args
          })}`
        );
        const err = new Error();
        if (!err.stack) {
          try {
            throw err;
          } catch (e) {
          }
        }
        push(
          this.failures,
          `Unexpected call: ${proxyCallToString.call({
            proxy: method,
            args,
            stack: err.stack
          })}`
        );
        mockExpectation.fail(join(messages, "\n"));
      }
    });
    module.exports = mock;
  }
});

// lib/sinon/create-stub-instance.js
var require_create_stub_instance = __commonJS({
  "lib/sinon/create-stub-instance.js"(exports, module) {
    "use strict";
    var stub = require_stub();
    var sinonType = require_sinon_type();
    var forEach = require_lib().prototypes.array.forEach;
    function isStub(value) {
      return sinonType.get(value) === "stub";
    }
    module.exports = function createStubInstance(constructor, overrides) {
      if (typeof constructor !== "function") {
        throw new TypeError("The constructor should be a function.");
      }
      const stubInstance = Object.create(constructor.prototype);
      sinonType.set(stubInstance, "stub-instance");
      const stubbedObject = stub(stubInstance);
      forEach(Object.keys(overrides || {}), function(propertyName) {
        if (propertyName in stubbedObject) {
          const value = overrides[propertyName];
          if (isStub(value)) {
            stubbedObject[propertyName] = value;
          } else {
            stubbedObject[propertyName].returns(value);
          }
        } else {
          throw new Error(
            `Cannot stub ${propertyName}. Property does not exist!`
          );
        }
      });
      return stubbedObject;
    };
  }
});

// lib/sinon/fake.js
var require_fake = __commonJS({
  "lib/sinon/fake.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var createProxy = require_proxy();
    var nextTick = require_next_tick();
    var slice = arrayProto.slice;
    module.exports = fake;
    function fake(f) {
      if (arguments.length > 0 && typeof f !== "function") {
        throw new TypeError("Expected f argument to be a Function");
      }
      return wrapFunc(f);
    }
    fake.returns = function returns(value) {
      function f() {
        return value;
      }
      return wrapFunc(f);
    };
    fake.throws = function throws(value) {
      function f() {
        throw getError(value);
      }
      return wrapFunc(f);
    };
    fake.resolves = function resolves(value) {
      function f() {
        return Promise.resolve(value);
      }
      return wrapFunc(f);
    };
    fake.rejects = function rejects(value) {
      function f() {
        return Promise.reject(getError(value));
      }
      return wrapFunc(f);
    };
    fake.yields = function yields() {
      const values = slice(arguments);
      function f() {
        const callback = arguments[arguments.length - 1];
        if (typeof callback !== "function") {
          throw new TypeError("Expected last argument to be a function");
        }
        callback.apply(null, values);
      }
      return wrapFunc(f);
    };
    fake.yieldsAsync = function yieldsAsync() {
      const values = slice(arguments);
      function f() {
        const callback = arguments[arguments.length - 1];
        if (typeof callback !== "function") {
          throw new TypeError("Expected last argument to be a function");
        }
        nextTick(function() {
          callback.apply(null, values);
        });
      }
      return wrapFunc(f);
    };
    var uuid = 0;
    function wrapFunc(f) {
      const fakeInstance = function() {
        let firstArg, lastArg;
        if (arguments.length > 0) {
          firstArg = arguments[0];
          lastArg = arguments[arguments.length - 1];
        }
        const callback = lastArg && typeof lastArg === "function" ? lastArg : void 0;
        proxy.firstArg = firstArg;
        proxy.lastArg = lastArg;
        proxy.callback = callback;
        return f && f.apply(this, arguments);
      };
      const proxy = createProxy(fakeInstance, f || fakeInstance);
      proxy.displayName = "fake";
      proxy.id = `fake#${uuid++}`;
      return proxy;
    }
    function getError(value) {
      return value instanceof Error ? value : new Error(value);
    }
  }
});

// lib/sinon/sandbox.js
var require_sandbox = __commonJS({
  "lib/sinon/sandbox.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var logger = require_lib().deprecated;
    var collectOwnMethods = require_collect_own_methods();
    var getPropertyDescriptor = require_get_property_descriptor();
    var isPropertyConfigurable = require_is_property_configurable();
    var match = require_samsam().createMatcher;
    var sinonAssert = require_assert();
    var sinonClock = require_fake_timers();
    var sinonMock = require_mock();
    var sinonSpy = require_spy();
    var sinonStub = require_stub();
    var sinonCreateStubInstance = require_create_stub_instance();
    var sinonFake = require_fake();
    var valueToString = require_lib().valueToString;
    var DEFAULT_LEAK_THRESHOLD = 1e4;
    var filter = arrayProto.filter;
    var forEach = arrayProto.forEach;
    var push = arrayProto.push;
    var reverse = arrayProto.reverse;
    function applyOnEach(fakes, method) {
      const matchingFakes = filter(fakes, function(fake) {
        return typeof fake[method] === "function";
      });
      forEach(matchingFakes, function(fake) {
        fake[method]();
      });
    }
    function throwOnAccessors(descriptor) {
      if (typeof descriptor.get === "function") {
        throw new Error("Use sandbox.replaceGetter for replacing getters");
      }
      if (typeof descriptor.set === "function") {
        throw new Error("Use sandbox.replaceSetter for replacing setters");
      }
    }
    function verifySameType(object, property, replacement) {
      if (typeof object[property] !== typeof replacement) {
        throw new TypeError(
          `Cannot replace ${typeof object[property]} with ${typeof replacement}`
        );
      }
    }
    function checkForValidArguments(descriptor, property, replacement) {
      if (typeof descriptor === "undefined") {
        throw new TypeError(
          `Cannot replace non-existent property ${valueToString(
            property
          )}. Perhaps you meant sandbox.define()?`
        );
      }
      if (typeof replacement === "undefined") {
        throw new TypeError("Expected replacement argument to be defined");
      }
    }
    function Sandbox(opts = {}) {
      const sandbox = this;
      const assertOptions = opts.assertOptions || {};
      let fakeRestorers = [];
      let collection = [];
      let loggedLeakWarning = false;
      sandbox.leakThreshold = DEFAULT_LEAK_THRESHOLD;
      function addToCollection(object) {
        if (push(collection, object) > sandbox.leakThreshold && !loggedLeakWarning) {
          logger.printWarning(
            "Potential memory leak detected; be sure to call restore() to clean up your sandbox. To suppress this warning, modify the leakThreshold property of your sandbox."
          );
          loggedLeakWarning = true;
        }
      }
      sandbox.assert = sinonAssert.createAssertObject(assertOptions);
      sandbox.getFakes = function getFakes() {
        return collection;
      };
      sandbox.createStubInstance = function createStubInstance() {
        const stubbed = sinonCreateStubInstance.apply(null, arguments);
        const ownMethods = collectOwnMethods(stubbed);
        forEach(ownMethods, function(method) {
          addToCollection(method);
        });
        return stubbed;
      };
      sandbox.inject = function inject(obj) {
        obj.spy = function() {
          return sandbox.spy.apply(null, arguments);
        };
        obj.stub = function() {
          return sandbox.stub.apply(null, arguments);
        };
        obj.mock = function() {
          return sandbox.mock.apply(null, arguments);
        };
        obj.createStubInstance = function() {
          return sandbox.createStubInstance.apply(sandbox, arguments);
        };
        obj.fake = function() {
          return sandbox.fake.apply(null, arguments);
        };
        obj.define = function() {
          return sandbox.define.apply(null, arguments);
        };
        obj.replace = function() {
          return sandbox.replace.apply(null, arguments);
        };
        obj.replaceSetter = function() {
          return sandbox.replaceSetter.apply(null, arguments);
        };
        obj.replaceGetter = function() {
          return sandbox.replaceGetter.apply(null, arguments);
        };
        if (sandbox.clock) {
          obj.clock = sandbox.clock;
        }
        obj.match = match;
        return obj;
      };
      sandbox.mock = function mock() {
        const m = sinonMock.apply(null, arguments);
        addToCollection(m);
        return m;
      };
      sandbox.reset = function reset() {
        applyOnEach(collection, "reset");
        applyOnEach(collection, "resetHistory");
      };
      sandbox.resetBehavior = function resetBehavior() {
        applyOnEach(collection, "resetBehavior");
      };
      sandbox.resetHistory = function resetHistory() {
        function privateResetHistory(f) {
          const method = f.resetHistory || f.reset;
          if (method) {
            method.call(f);
          }
        }
        forEach(collection, privateResetHistory);
      };
      sandbox.restore = function restore() {
        if (arguments.length) {
          throw new Error(
            "sandbox.restore() does not take any parameters. Perhaps you meant stub.restore()"
          );
        }
        reverse(collection);
        applyOnEach(collection, "restore");
        collection = [];
        forEach(fakeRestorers, function(restorer) {
          restorer();
        });
        fakeRestorers = [];
        sandbox.restoreContext();
      };
      sandbox.restoreContext = function restoreContext() {
        if (!sandbox.injectedKeys) {
          return;
        }
        forEach(sandbox.injectedKeys, function(injectedKey) {
          delete sandbox.injectInto[injectedKey];
        });
        sandbox.injectedKeys.length = 0;
      };
      function getFakeRestorer(object, property, forceAssignment = false) {
        const descriptor = getPropertyDescriptor(object, property);
        const value = forceAssignment && object[property];
        function restorer() {
          if (forceAssignment) {
            object[property] = value;
          } else if (descriptor?.isOwn) {
            Object.defineProperty(object, property, descriptor);
          } else {
            delete object[property];
          }
        }
        restorer.object = object;
        restorer.property = property;
        return restorer;
      }
      function verifyNotReplaced(object, property) {
        forEach(fakeRestorers, function(fakeRestorer) {
          if (fakeRestorer.object === object && fakeRestorer.property === property) {
            throw new TypeError(
              `Attempted to replace ${property} which is already replaced`
            );
          }
        });
      }
      sandbox.replace = function replace(object, property, replacement) {
        const descriptor = getPropertyDescriptor(object, property);
        checkForValidArguments(descriptor, property, replacement);
        throwOnAccessors(descriptor);
        verifySameType(object, property, replacement);
        verifyNotReplaced(object, property);
        push(fakeRestorers, getFakeRestorer(object, property));
        object[property] = replacement;
        return replacement;
      };
      sandbox.replace.usingAccessor = function replaceUsingAccessor(object, property, replacement) {
        const descriptor = getPropertyDescriptor(object, property);
        checkForValidArguments(descriptor, property, replacement);
        verifySameType(object, property, replacement);
        verifyNotReplaced(object, property);
        push(fakeRestorers, getFakeRestorer(object, property, true));
        object[property] = replacement;
        return replacement;
      };
      sandbox.define = function define2(object, property, value) {
        const descriptor = getPropertyDescriptor(object, property);
        if (descriptor) {
          throw new TypeError(
            `Cannot define the already existing property ${valueToString(
              property
            )}. Perhaps you meant sandbox.replace()?`
          );
        }
        if (typeof value === "undefined") {
          throw new TypeError("Expected value argument to be defined");
        }
        verifyNotReplaced(object, property);
        push(fakeRestorers, getFakeRestorer(object, property));
        object[property] = value;
        return value;
      };
      sandbox.replaceGetter = function replaceGetter(object, property, replacement) {
        const descriptor = getPropertyDescriptor(object, property);
        if (typeof descriptor === "undefined") {
          throw new TypeError(
            `Cannot replace non-existent property ${valueToString(
              property
            )}`
          );
        }
        if (typeof replacement !== "function") {
          throw new TypeError(
            "Expected replacement argument to be a function"
          );
        }
        if (typeof descriptor.get !== "function") {
          throw new Error("`object.property` is not a getter");
        }
        verifyNotReplaced(object, property);
        push(fakeRestorers, getFakeRestorer(object, property));
        Object.defineProperty(object, property, {
          get: replacement,
          configurable: isPropertyConfigurable(object, property)
        });
        return replacement;
      };
      sandbox.replaceSetter = function replaceSetter(object, property, replacement) {
        const descriptor = getPropertyDescriptor(object, property);
        if (typeof descriptor === "undefined") {
          throw new TypeError(
            `Cannot replace non-existent property ${valueToString(
              property
            )}`
          );
        }
        if (typeof replacement !== "function") {
          throw new TypeError(
            "Expected replacement argument to be a function"
          );
        }
        if (typeof descriptor.set !== "function") {
          throw new Error("`object.property` is not a setter");
        }
        verifyNotReplaced(object, property);
        push(fakeRestorers, getFakeRestorer(object, property));
        Object.defineProperty(object, property, {
          set: replacement,
          configurable: isPropertyConfigurable(object, property)
        });
        return replacement;
      };
      function commonPostInitSetup(args, spy) {
        const [object, property, types] = args;
        const isSpyingOnEntireObject = typeof property === "undefined" && (typeof object === "object" || typeof object === "function");
        if (isSpyingOnEntireObject) {
          const ownMethods = collectOwnMethods(spy);
          forEach(ownMethods, function(method) {
            addToCollection(method);
          });
        } else if (Array.isArray(types)) {
          for (const accessorType of types) {
            addToCollection(spy[accessorType]);
          }
        } else {
          addToCollection(spy);
        }
        return spy;
      }
      sandbox.spy = function spy() {
        const createdSpy = sinonSpy.apply(sinonSpy, arguments);
        return commonPostInitSetup(arguments, createdSpy);
      };
      sandbox.stub = function stub() {
        const createdStub = sinonStub.apply(sinonStub, arguments);
        return commonPostInitSetup(arguments, createdStub);
      };
      sandbox.fake = function fake(f) {
        const s = sinonFake.apply(sinonFake, arguments);
        addToCollection(s);
        return s;
      };
      forEach(Object.keys(sinonFake), function(key) {
        const fakeBehavior = sinonFake[key];
        if (typeof fakeBehavior === "function") {
          sandbox.fake[key] = function() {
            const s = fakeBehavior.apply(fakeBehavior, arguments);
            addToCollection(s);
            return s;
          };
        }
      });
      sandbox.useFakeTimers = function useFakeTimers(args) {
        const clock = sinonClock.useFakeTimers.call(null, args);
        sandbox.clock = clock;
        addToCollection(clock);
        return clock;
      };
      sandbox.verify = function verify() {
        applyOnEach(collection, "verify");
      };
      sandbox.verifyAndRestore = function verifyAndRestore() {
        let exception;
        try {
          sandbox.verify();
        } catch (e) {
          exception = e;
        }
        sandbox.restore();
        if (exception) {
          throw exception;
        }
      };
    }
    Sandbox.prototype.match = match;
    module.exports = Sandbox;
  }
});

// lib/sinon/create-sandbox.js
var require_create_sandbox = __commonJS({
  "lib/sinon/create-sandbox.js"(exports, module) {
    "use strict";
    var arrayProto = require_lib().prototypes.array;
    var Sandbox = require_sandbox();
    var forEach = arrayProto.forEach;
    var push = arrayProto.push;
    function prepareSandboxFromConfig(config) {
      const sandbox = new Sandbox({ assertOptions: config.assertOptions });
      if (config.useFakeTimers) {
        if (typeof config.useFakeTimers === "object") {
          sandbox.useFakeTimers(config.useFakeTimers);
        } else {
          sandbox.useFakeTimers();
        }
      }
      return sandbox;
    }
    function exposeValue(sandbox, config, key, value) {
      if (!value) {
        return;
      }
      if (config.injectInto && !(key in config.injectInto)) {
        config.injectInto[key] = value;
        push(sandbox.injectedKeys, key);
      } else {
        push(sandbox.args, value);
      }
    }
    function createSandbox(config) {
      if (!config) {
        return new Sandbox();
      }
      const configuredSandbox = prepareSandboxFromConfig(config);
      configuredSandbox.args = configuredSandbox.args || [];
      configuredSandbox.injectedKeys = [];
      configuredSandbox.injectInto = config.injectInto;
      const exposed = configuredSandbox.inject({});
      if (config.properties) {
        forEach(config.properties, function(prop) {
          const value = exposed[prop] || prop === "sandbox" && configuredSandbox;
          exposeValue(configuredSandbox, config, prop, value);
        });
      } else {
        exposeValue(configuredSandbox, config, "sandbox");
      }
      return configuredSandbox;
    }
    module.exports = createSandbox;
  }
});

// lib/sinon/util/core/is-restorable.js
var require_is_restorable = __commonJS({
  "lib/sinon/util/core/is-restorable.js"(exports, module) {
    "use strict";
    function isRestorable(obj) {
      return typeof obj === "function" && typeof obj.restore === "function" && obj.restore.sinon;
    }
    module.exports = isRestorable;
  }
});

// lib/sinon/promise.js
var require_promise = __commonJS({
  "lib/sinon/promise.js"(exports, module) {
    "use strict";
    var fake = require_fake();
    var isRestorable = require_is_restorable();
    var STATUS_PENDING = "pending";
    var STATUS_RESOLVED = "resolved";
    var STATUS_REJECTED = "rejected";
    function getFakeExecutor(executor) {
      if (isRestorable(executor)) {
        return executor;
      }
      if (executor) {
        return fake(executor);
      }
      return fake();
    }
    function promise(executor) {
      const fakeExecutor = getFakeExecutor(executor);
      const sinonPromise = new Promise(fakeExecutor);
      sinonPromise.status = STATUS_PENDING;
      sinonPromise.then(function(value) {
        sinonPromise.status = STATUS_RESOLVED;
        sinonPromise.resolvedValue = value;
      }).catch(function(reason) {
        sinonPromise.status = STATUS_REJECTED;
        sinonPromise.rejectedValue = reason;
      });
      function finalize(status, value, callback) {
        if (sinonPromise.status !== STATUS_PENDING) {
          throw new Error(`Promise already ${sinonPromise.status}`);
        }
        sinonPromise.status = status;
        callback(value);
      }
      sinonPromise.resolve = function(value) {
        finalize(STATUS_RESOLVED, value, fakeExecutor.firstCall.args[0]);
        return sinonPromise;
      };
      sinonPromise.reject = function(reason) {
        finalize(STATUS_REJECTED, reason, fakeExecutor.firstCall.args[1]);
        return new Promise(function(resolve) {
          sinonPromise.catch(() => resolve());
        });
      };
      return sinonPromise;
    }
    module.exports = promise;
  }
});

// lib/sinon/restore-object.js
var require_restore_object = __commonJS({
  "lib/sinon/restore-object.js"(exports, module) {
    "use strict";
    var walkObject = require_walk_object();
    function filter(object, property) {
      return object[property].restore && object[property].restore.sinon;
    }
    function restore(object, property) {
      object[property].restore();
    }
    function restoreObject(object) {
      return walkObject(restore, object, filter);
    }
    module.exports = restoreObject;
  }
});

// lib/create-sinon-api.js
var require_create_sinon_api = __commonJS({
  "lib/create-sinon-api.js"(exports, module) {
    "use strict";
    var behavior = require_behavior();
    var createSandbox = require_create_sandbox();
    var extend = require_extend();
    var fakeTimers = require_fake_timers();
    var Sandbox = require_sandbox();
    var stub = require_stub();
    var promise = require_promise();
    module.exports = function createApi() {
      const apiMethods = {
        createSandbox,
        match: require_samsam().createMatcher,
        restoreObject: require_restore_object(),
        expectation: require_mock_expectation(),
        // fake timers
        timers: fakeTimers.timers,
        addBehavior: function(name, fn) {
          behavior.addBehavior(stub, name, fn);
        },
        // fake promise
        promise
      };
      const sandbox = new Sandbox();
      return extend(sandbox, apiMethods);
    };
  }
});

// lib/sinon.js
var require_sinon = __commonJS({
  "lib/sinon.js"(exports, module) {
    "use strict";
    var createApi = require_create_sinon_api();
    module.exports = createApi();
  }
});

// lib/sinon-esm.js
var require_sinon_esm = __commonJS({
  "lib/sinon-esm.js"(exports, module) {
    var sinon = require_sinon();
    module.exports = sinon;
  }
});
export default require_sinon_esm();

const _leakThreshold = require_sinon().leakThreshold;
export { _leakThreshold as leakThreshold };
const _assert = require_sinon().assert;
export { _assert as assert };
const _getFakes = require_sinon().getFakes;
export { _getFakes as getFakes };
const _createStubInstance = require_sinon().createStubInstance;
export { _createStubInstance as createStubInstance };
const _inject = require_sinon().inject;
export { _inject as inject };
const _mock = require_sinon().mock;
export { _mock as mock };
const _reset = require_sinon().reset;
export { _reset as reset };
const _resetBehavior = require_sinon().resetBehavior;
export { _resetBehavior as resetBehavior };
const _resetHistory = require_sinon().resetHistory;
export { _resetHistory as resetHistory };
const _restore = require_sinon().restore;
export { _restore as restore };
const _restoreContext = require_sinon().restoreContext;
export { _restoreContext as restoreContext };
const _replace = require_sinon().replace;
export { _replace as replace };
const _define = require_sinon().define;
export { _define as define };
const _replaceGetter = require_sinon().replaceGetter;
export { _replaceGetter as replaceGetter };
const _replaceSetter = require_sinon().replaceSetter;
export { _replaceSetter as replaceSetter };
const _spy = require_sinon().spy;
export { _spy as spy };
const _stub = require_sinon().stub;
export { _stub as stub };
const _fake = require_sinon().fake;
export { _fake as fake };
const _useFakeTimers = require_sinon().useFakeTimers;
export { _useFakeTimers as useFakeTimers };
const _verify = require_sinon().verify;
export { _verify as verify };
const _verifyAndRestore = require_sinon().verifyAndRestore;
export { _verifyAndRestore as verifyAndRestore };
const _createSandbox = require_sinon().createSandbox;
export { _createSandbox as createSandbox };
const _match = require_sinon().match;
export { _match as match };
const _restoreObject = require_sinon().restoreObject;
export { _restoreObject as restoreObject };
const _expectation = require_sinon().expectation;
export { _expectation as expectation };
const _timers = require_sinon().timers;
export { _timers as timers };
const _addBehavior = require_sinon().addBehavior;
export { _addBehavior as addBehavior };
const _promise = require_sinon().promise;
export { _promise as promise };