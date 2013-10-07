"no use strict";

if (typeof window != "undefined" && window.document)
    throw "atempt to load ace worker into main window instead of webWorker";

var console = {
    log: function() {
        var msgs = Array.prototype.slice.call(arguments, 0);
        postMessage({type: "log", data: msgs});
    },
    error: function() {
        var msgs = Array.prototype.slice.call(arguments, 0);
        postMessage({type: "log", data: msgs});
    }
};
var window = {
    console: console
};

var normalizeModule = function(parentId, moduleName) {
    // normalize plugin requires
    if (moduleName.indexOf("!") !== -1) {
        var chunks = moduleName.split("!");
        return normalizeModule(parentId, chunks[0]) + "!" + normalizeModule(parentId, chunks[1]);
    }
    // normalize relative requires
    if (moduleName.charAt(0) == ".") {
        var base = parentId.split("/").slice(0, -1).join("/");
        moduleName = base + "/" + moduleName;
        
        while(moduleName.indexOf(".") !== -1 && previous != moduleName) {
            var previous = moduleName;
            moduleName = moduleName.replace(/\/\.\//, "/").replace(/[^\/]+\/\.\.\//, "");
        }
    }
    
    return moduleName;
};

var require = function(parentId, id) {
    if (!id.charAt)
        throw new Error("worker.js require() accepts only (parentId, id) as arguments");

    id = normalizeModule(parentId, id);

    var module = require.modules[id];
    if (module) {
        if (!module.initialized) {
            module.initialized = true;
            module.exports = module.factory().exports;
        }
        return module.exports;
    }
    
    var chunks = id.split("/");
    chunks[0] = require.tlns[chunks[0]] || chunks[0];
    var path = chunks.join("/") + ".js";
    
    require.id = id;
    importScripts(path);
    return require(parentId, id);    
};

require.modules = {};
require.tlns = {};

var define = function(id, deps, factory) {
    if (arguments.length == 2) {
        factory = deps;
        if (typeof id != "string") {
            deps = id;
            id = require.id;
        }
    } else if (arguments.length == 1) {
        factory = id;
        id = require.id;
    }

    if (id.indexOf("text!") === 0) 
        return;
    
    var req = function(deps, factory) {
        return require(id, deps, factory);
    };

    require.modules[id] = {
        factory: function() {
            var module = {
                exports: {}
            };
            var returnExports = factory(req, module.exports, module);
            if (returnExports)
                module.exports = returnExports;
            return module;
        }
    };
};

function initBaseUrls(topLevelNamespaces) {
    require.tlns = topLevelNamespaces;
}

function initSender() {

    var EventEmitter = require(null, "ace/lib/event_emitter").EventEmitter;
    var oop = require(null, "ace/lib/oop");
    
    var Sender = function() {};
    
    (function() {
        
        oop.implement(this, EventEmitter);
                
        this.callback = function(data, callbackId) {
            postMessage({
                type: "call",
                id: callbackId,
                data: data
            });
        };
    
        this.emit = function(name, data) {
            postMessage({
                type: "event",
                name: name,
                data: data
            });
        };
        
    }).call(Sender.prototype);
    
    return new Sender();
}

var main;
var sender;

onmessage = function(e) {
    var msg = e.data;
    if (msg.command) {
        if (main[msg.command])
            main[msg.command].apply(main, msg.args);
        else
            throw new Error("Unknown command:" + msg.command);
    }
    else if (msg.init) {        
        initBaseUrls(msg.tlns);
        require(null, "ace/lib/fixoldbrowsers");
        sender = initSender();
        var clazz = require(null, msg.module)[msg.classname];
        main = new clazz(sender);
    } 
    else if (msg.event && sender) {
        sender._emit(msg.event, msg.data);
    }
};
// vim:set ts=4 sts=4 sw=4 st:

define('ace/lib/fixoldbrowsers', ['require', 'exports', 'module' , 'ace/lib/regexp', 'ace/lib/es5-shim'], function(require, exports, module) {


require("./regexp");
require("./es5-shim");

});
 
define('ace/lib/regexp', ['require', 'exports', 'module' ], function(require, exports, module) {

    var real = {
            exec: RegExp.prototype.exec,
            test: RegExp.prototype.test,
            match: String.prototype.match,
            replace: String.prototype.replace,
            split: String.prototype.split
        },
        compliantExecNpcg = real.exec.call(/()??/, "")[1] === undefined, // check `exec` handling of nonparticipating capturing groups
        compliantLastIndexIncrement = function () {
            var x = /^/g;
            real.test.call(x, "");
            return !x.lastIndex;
        }();

    if (compliantLastIndexIncrement && compliantExecNpcg)
        return;
    RegExp.prototype.exec = function (str) {
        var match = real.exec.apply(this, arguments),
            name, r2;
        if ( typeof(str) == 'string' && match) {
            if (!compliantExecNpcg && match.length > 1 && indexOf(match, "") > -1) {
                r2 = RegExp(this.source, real.replace.call(getNativeFlags(this), "g", ""));
                real.replace.call(str.slice(match.index), r2, function () {
                    for (var i = 1; i < arguments.length - 2; i++) {
                        if (arguments[i] === undefined)
                            match[i] = undefined;
                    }
                });
            }
            if (this._xregexp && this._xregexp.captureNames) {
                for (var i = 1; i < match.length; i++) {
                    name = this._xregexp.captureNames[i - 1];
                    if (name)
                       match[name] = match[i];
                }
            }
            if (!compliantLastIndexIncrement && this.global && !match[0].length && (this.lastIndex > match.index))
                this.lastIndex--;
        }
        return match;
    };
    if (!compliantLastIndexIncrement) {
        RegExp.prototype.test = function (str) {
            var match = real.exec.call(this, str);
            if (match && this.global && !match[0].length && (this.lastIndex > match.index))
                this.lastIndex--;
            return !!match;
        };
    }

    function getNativeFlags (regex) {
        return (regex.global     ? "g" : "") +
               (regex.ignoreCase ? "i" : "") +
               (regex.multiline  ? "m" : "") +
               (regex.extended   ? "x" : "") + // Proposed for ES4; included in AS3
               (regex.sticky     ? "y" : "");
    }

    function indexOf (array, item, from) {
        if (Array.prototype.indexOf) // Use the native array method if available
            return array.indexOf(item, from);
        for (var i = from || 0; i < array.length; i++) {
            if (array[i] === item)
                return i;
        }
        return -1;
    }

});

define('ace/lib/es5-shim', ['require', 'exports', 'module' ], function(require, exports, module) {

function Empty() {}

if (!Function.prototype.bind) {
    Function.prototype.bind = function bind(that) { // .length is 1
        var target = this;
        if (typeof target != "function") {
            throw new TypeError("Function.prototype.bind called on incompatible " + target);
        }
        var args = slice.call(arguments, 1); // for normal call
        var bound = function () {

            if (this instanceof bound) {

                var result = target.apply(
                    this,
                    args.concat(slice.call(arguments))
                );
                if (Object(result) === result) {
                    return result;
                }
                return this;

            } else {
                return target.apply(
                    that,
                    args.concat(slice.call(arguments))
                );

            }

        };
        if(target.prototype) {
            Empty.prototype = target.prototype;
            bound.prototype = new Empty();
            Empty.prototype = null;
        }
        return bound;
    };
}
var call = Function.prototype.call;
var prototypeOfArray = Array.prototype;
var prototypeOfObject = Object.prototype;
var slice = prototypeOfArray.slice;
var _toString = call.bind(prototypeOfObject.toString);
var owns = call.bind(prototypeOfObject.hasOwnProperty);
var defineGetter;
var defineSetter;
var lookupGetter;
var lookupSetter;
var supportsAccessors;
if ((supportsAccessors = owns(prototypeOfObject, "__defineGetter__"))) {
    defineGetter = call.bind(prototypeOfObject.__defineGetter__);
    defineSetter = call.bind(prototypeOfObject.__defineSetter__);
    lookupGetter = call.bind(prototypeOfObject.__lookupGetter__);
    lookupSetter = call.bind(prototypeOfObject.__lookupSetter__);
}
if ([1,2].splice(0).length != 2) {
    if(function() { // test IE < 9 to splice bug - see issue #138
        function makeArray(l) {
            var a = new Array(l+2);
            a[0] = a[1] = 0;
            return a;
        }
        var array = [], lengthBefore;
        
        array.splice.apply(array, makeArray(20));
        array.splice.apply(array, makeArray(26));

        lengthBefore = array.length; //46
        array.splice(5, 0, "XXX"); // add one element

        lengthBefore + 1 == array.length

        if (lengthBefore + 1 == array.length) {
            return true;// has right splice implementation without bugs
        }
    }()) {//IE 6/7
        var array_splice = Array.prototype.splice;
        Array.prototype.splice = function(start, deleteCount) {
            if (!arguments.length) {
                return [];
            } else {
                return array_splice.apply(this, [
                    start === void 0 ? 0 : start,
                    deleteCount === void 0 ? (this.length - start) : deleteCount
                ].concat(slice.call(arguments, 2)))
            }
        };
    } else {//IE8
        Array.prototype.splice = function(pos, removeCount){
            var length = this.length;
            if (pos > 0) {
                if (pos > length)
                    pos = length;
            } else if (pos == void 0) {
                pos = 0;
            } else if (pos < 0) {
                pos = Math.max(length + pos, 0);
            }

            if (!(pos+removeCount < length))
                removeCount = length - pos;

            var removed = this.slice(pos, pos+removeCount);
            var insert = slice.call(arguments, 2);
            var add = insert.length;            
            if (pos === length) {
                if (add) {
                    this.push.apply(this, insert);
                }
            } else {
                var remove = Math.min(removeCount, length - pos);
                var tailOldPos = pos + remove;
                var tailNewPos = tailOldPos + add - remove;
                var tailCount = length - tailOldPos;
                var lengthAfterRemove = length - remove;

                if (tailNewPos < tailOldPos) { // case A
                    for (var i = 0; i < tailCount; ++i) {
                        this[tailNewPos+i] = this[tailOldPos+i];
                    }
                } else if (tailNewPos > tailOldPos) { // case B
                    for (i = tailCount; i--; ) {
                        this[tailNewPos+i] = this[tailOldPos+i];
                    }
                } // else, add == remove (nothing to do)

                if (add && pos === lengthAfterRemove) {
                    this.length = lengthAfterRemove; // truncate array
                    this.push.apply(this, insert);
                } else {
                    this.length = lengthAfterRemove + add; // reserves space
                    for (i = 0; i < add; ++i) {
                        this[pos+i] = insert[i];
                    }
                }
            }
            return removed;
        };
    }
}
if (!Array.isArray) {
    Array.isArray = function isArray(obj) {
        return _toString(obj) == "[object Array]";
    };
}
var boxedString = Object("a"),
    splitString = boxedString[0] != "a" || !(0 in boxedString);

if (!Array.prototype.forEach) {
    Array.prototype.forEach = function forEach(fun /*, thisp*/) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                object,
            thisp = arguments[1],
            i = -1,
            length = self.length >>> 0;
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(); // TODO message
        }

        while (++i < length) {
            if (i in self) {
                fun.call(thisp, self[i], i, object);
            }
        }
    };
}
if (!Array.prototype.map) {
    Array.prototype.map = function map(fun /*, thisp*/) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                object,
            length = self.length >>> 0,
            result = Array(length),
            thisp = arguments[1];
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(fun + " is not a function");
        }

        for (var i = 0; i < length; i++) {
            if (i in self)
                result[i] = fun.call(thisp, self[i], i, object);
        }
        return result;
    };
}
if (!Array.prototype.filter) {
    Array.prototype.filter = function filter(fun /*, thisp */) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                    object,
            length = self.length >>> 0,
            result = [],
            value,
            thisp = arguments[1];
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(fun + " is not a function");
        }

        for (var i = 0; i < length; i++) {
            if (i in self) {
                value = self[i];
                if (fun.call(thisp, value, i, object)) {
                    result.push(value);
                }
            }
        }
        return result;
    };
}
if (!Array.prototype.every) {
    Array.prototype.every = function every(fun /*, thisp */) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                object,
            length = self.length >>> 0,
            thisp = arguments[1];
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(fun + " is not a function");
        }

        for (var i = 0; i < length; i++) {
            if (i in self && !fun.call(thisp, self[i], i, object)) {
                return false;
            }
        }
        return true;
    };
}
if (!Array.prototype.some) {
    Array.prototype.some = function some(fun /*, thisp */) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                object,
            length = self.length >>> 0,
            thisp = arguments[1];
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(fun + " is not a function");
        }

        for (var i = 0; i < length; i++) {
            if (i in self && fun.call(thisp, self[i], i, object)) {
                return true;
            }
        }
        return false;
    };
}
if (!Array.prototype.reduce) {
    Array.prototype.reduce = function reduce(fun /*, initial*/) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                object,
            length = self.length >>> 0;
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(fun + " is not a function");
        }
        if (!length && arguments.length == 1) {
            throw new TypeError("reduce of empty array with no initial value");
        }

        var i = 0;
        var result;
        if (arguments.length >= 2) {
            result = arguments[1];
        } else {
            do {
                if (i in self) {
                    result = self[i++];
                    break;
                }
                if (++i >= length) {
                    throw new TypeError("reduce of empty array with no initial value");
                }
            } while (true);
        }

        for (; i < length; i++) {
            if (i in self) {
                result = fun.call(void 0, result, self[i], i, object);
            }
        }

        return result;
    };
}
if (!Array.prototype.reduceRight) {
    Array.prototype.reduceRight = function reduceRight(fun /*, initial*/) {
        var object = toObject(this),
            self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                object,
            length = self.length >>> 0;
        if (_toString(fun) != "[object Function]") {
            throw new TypeError(fun + " is not a function");
        }
        if (!length && arguments.length == 1) {
            throw new TypeError("reduceRight of empty array with no initial value");
        }

        var result, i = length - 1;
        if (arguments.length >= 2) {
            result = arguments[1];
        } else {
            do {
                if (i in self) {
                    result = self[i--];
                    break;
                }
                if (--i < 0) {
                    throw new TypeError("reduceRight of empty array with no initial value");
                }
            } while (true);
        }

        do {
            if (i in this) {
                result = fun.call(void 0, result, self[i], i, object);
            }
        } while (i--);

        return result;
    };
}
if (!Array.prototype.indexOf || ([0, 1].indexOf(1, 2) != -1)) {
    Array.prototype.indexOf = function indexOf(sought /*, fromIndex */ ) {
        var self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                toObject(this),
            length = self.length >>> 0;

        if (!length) {
            return -1;
        }

        var i = 0;
        if (arguments.length > 1) {
            i = toInteger(arguments[1]);
        }
        i = i >= 0 ? i : Math.max(0, length + i);
        for (; i < length; i++) {
            if (i in self && self[i] === sought) {
                return i;
            }
        }
        return -1;
    };
}
if (!Array.prototype.lastIndexOf || ([0, 1].lastIndexOf(0, -3) != -1)) {
    Array.prototype.lastIndexOf = function lastIndexOf(sought /*, fromIndex */) {
        var self = splitString && _toString(this) == "[object String]" ?
                this.split("") :
                toObject(this),
            length = self.length >>> 0;

        if (!length) {
            return -1;
        }
        var i = length - 1;
        if (arguments.length > 1) {
            i = Math.min(i, toInteger(arguments[1]));
        }
        i = i >= 0 ? i : length - Math.abs(i);
        for (; i >= 0; i--) {
            if (i in self && sought === self[i]) {
                return i;
            }
        }
        return -1;
    };
}
if (!Object.getPrototypeOf) {
    Object.getPrototypeOf = function getPrototypeOf(object) {
        return object.__proto__ || (
            object.constructor ?
            object.constructor.prototype :
            prototypeOfObject
        );
    };
}
if (!Object.getOwnPropertyDescriptor) {
    var ERR_NON_OBJECT = "Object.getOwnPropertyDescriptor called on a " +
                         "non-object: ";
    Object.getOwnPropertyDescriptor = function getOwnPropertyDescriptor(object, property) {
        if ((typeof object != "object" && typeof object != "function") || object === null)
            throw new TypeError(ERR_NON_OBJECT + object);
        if (!owns(object, property))
            return;

        var descriptor, getter, setter;
        descriptor =  { enumerable: true, configurable: true };
        if (supportsAccessors) {
            var prototype = object.__proto__;
            object.__proto__ = prototypeOfObject;

            var getter = lookupGetter(object, property);
            var setter = lookupSetter(object, property);
            object.__proto__ = prototype;

            if (getter || setter) {
                if (getter) descriptor.get = getter;
                if (setter) descriptor.set = setter;
                return descriptor;
            }
        }
        descriptor.value = object[property];
        return descriptor;
    };
}
if (!Object.getOwnPropertyNames) {
    Object.getOwnPropertyNames = function getOwnPropertyNames(object) {
        return Object.keys(object);
    };
}
if (!Object.create) {
    var createEmpty;
    if (Object.prototype.__proto__ === null) {
        createEmpty = function () {
            return { "__proto__": null };
        };
    } else {
        createEmpty = function () {
            var empty = {};
            for (var i in empty)
                empty[i] = null;
            empty.constructor =
            empty.hasOwnProperty =
            empty.propertyIsEnumerable =
            empty.isPrototypeOf =
            empty.toLocaleString =
            empty.toString =
            empty.valueOf =
            empty.__proto__ = null;
            return empty;
        }
    }

    Object.create = function create(prototype, properties) {
        var object;
        if (prototype === null) {
            object = createEmpty();
        } else {
            if (typeof prototype != "object")
                throw new TypeError("typeof prototype["+(typeof prototype)+"] != 'object'");
            var Type = function () {};
            Type.prototype = prototype;
            object = new Type();
            object.__proto__ = prototype;
        }
        if (properties !== void 0)
            Object.defineProperties(object, properties);
        return object;
    };
}

function doesDefinePropertyWork(object) {
    try {
        Object.defineProperty(object, "sentinel", {});
        return "sentinel" in object;
    } catch (exception) {
    }
}
if (Object.defineProperty) {
    var definePropertyWorksOnObject = doesDefinePropertyWork({});
    var definePropertyWorksOnDom = typeof document == "undefined" ||
        doesDefinePropertyWork(document.createElement("div"));
    if (!definePropertyWorksOnObject || !definePropertyWorksOnDom) {
        var definePropertyFallback = Object.defineProperty;
    }
}

if (!Object.defineProperty || definePropertyFallback) {
    var ERR_NON_OBJECT_DESCRIPTOR = "Property description must be an object: ";
    var ERR_NON_OBJECT_TARGET = "Object.defineProperty called on non-object: "
    var ERR_ACCESSORS_NOT_SUPPORTED = "getters & setters can not be defined " +
                                      "on this javascript engine";

    Object.defineProperty = function defineProperty(object, property, descriptor) {
        if ((typeof object != "object" && typeof object != "function") || object === null)
            throw new TypeError(ERR_NON_OBJECT_TARGET + object);
        if ((typeof descriptor != "object" && typeof descriptor != "function") || descriptor === null)
            throw new TypeError(ERR_NON_OBJECT_DESCRIPTOR + descriptor);
        if (definePropertyFallback) {
            try {
                return definePropertyFallback.call(Object, object, property, descriptor);
            } catch (exception) {
            }
        }
        if (owns(descriptor, "value")) {

            if (supportsAccessors && (lookupGetter(object, property) ||
                                      lookupSetter(object, property)))
            {
                var prototype = object.__proto__;
                object.__proto__ = prototypeOfObject;
                delete object[property];
                object[property] = descriptor.value;
                object.__proto__ = prototype;
            } else {
                object[property] = descriptor.value;
            }
        } else {
            if (!supportsAccessors)
                throw new TypeError(ERR_ACCESSORS_NOT_SUPPORTED);
            if (owns(descriptor, "get"))
                defineGetter(object, property, descriptor.get);
            if (owns(descriptor, "set"))
                defineSetter(object, property, descriptor.set);
        }

        return object;
    };
}
if (!Object.defineProperties) {
    Object.defineProperties = function defineProperties(object, properties) {
        for (var property in properties) {
            if (owns(properties, property))
                Object.defineProperty(object, property, properties[property]);
        }
        return object;
    };
}
if (!Object.seal) {
    Object.seal = function seal(object) {
        return object;
    };
}
if (!Object.freeze) {
    Object.freeze = function freeze(object) {
        return object;
    };
}
try {
    Object.freeze(function () {});
} catch (exception) {
    Object.freeze = (function freeze(freezeObject) {
        return function freeze(object) {
            if (typeof object == "function") {
                return object;
            } else {
                return freezeObject(object);
            }
        };
    })(Object.freeze);
}
if (!Object.preventExtensions) {
    Object.preventExtensions = function preventExtensions(object) {
        return object;
    };
}
if (!Object.isSealed) {
    Object.isSealed = function isSealed(object) {
        return false;
    };
}
if (!Object.isFrozen) {
    Object.isFrozen = function isFrozen(object) {
        return false;
    };
}
if (!Object.isExtensible) {
    Object.isExtensible = function isExtensible(object) {
        if (Object(object) === object) {
            throw new TypeError(); // TODO message
        }
        var name = '';
        while (owns(object, name)) {
            name += '?';
        }
        object[name] = true;
        var returnValue = owns(object, name);
        delete object[name];
        return returnValue;
    };
}
if (!Object.keys) {
    var hasDontEnumBug = true,
        dontEnums = [
            "toString",
            "toLocaleString",
            "valueOf",
            "hasOwnProperty",
            "isPrototypeOf",
            "propertyIsEnumerable",
            "constructor"
        ],
        dontEnumsLength = dontEnums.length;

    for (var key in {"toString": null}) {
        hasDontEnumBug = false;
    }

    Object.keys = function keys(object) {

        if (
            (typeof object != "object" && typeof object != "function") ||
            object === null
        ) {
            throw new TypeError("Object.keys called on a non-object");
        }

        var keys = [];
        for (var name in object) {
            if (owns(object, name)) {
                keys.push(name);
            }
        }

        if (hasDontEnumBug) {
            for (var i = 0, ii = dontEnumsLength; i < ii; i++) {
                var dontEnum = dontEnums[i];
                if (owns(object, dontEnum)) {
                    keys.push(dontEnum);
                }
            }
        }
        return keys;
    };

}
if (!Date.now) {
    Date.now = function now() {
        return new Date().getTime();
    };
}
if("0".split(void 0, 0).length) {
    var string_split = String.prototype.split;
    String.prototype.split = function(separator, limit) {
        if(separator === void 0 && limit === 0)return [];
        return string_split.apply(this, arguments);
    }
}
if("".substr && "0b".substr(-1) !== "b") {
    var string_substr = String.prototype.substr;
    String.prototype.substr = function(start, length) {
        return string_substr.call(
            this,
            start < 0 ? (start = this.length + start) < 0 ? 0 : start : start,
            length
        );
    }
}
var ws = "\x09\x0A\x0B\x0C\x0D\x20\xA0\u1680\u180E\u2000\u2001\u2002\u2003" +
    "\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u202F\u205F\u3000\u2028" +
    "\u2029\uFEFF";
if (!String.prototype.trim || ws.trim()) {
    ws = "[" + ws + "]";
    var trimBeginRegexp = new RegExp("^" + ws + ws + "*"),
        trimEndRegexp = new RegExp(ws + ws + "*$");
    String.prototype.trim = function trim() {
        if (this === undefined || this === null) {
            throw new TypeError("can't convert "+this+" to object");
        }
        return String(this)
            .replace(trimBeginRegexp, "")
            .replace(trimEndRegexp, "");
    };
}

function toInteger(n) {
    n = +n;
    if (n !== n) { // isNaN
        n = 0;
    } else if (n !== 0 && n !== (1/0) && n !== -(1/0)) {
        n = (n > 0 || -1) * Math.floor(Math.abs(n));
    }
    return n;
}

function isPrimitive(input) {
    var type = typeof input;
    return (
        input === null ||
        type === "undefined" ||
        type === "boolean" ||
        type === "number" ||
        type === "string"
    );
}

function toPrimitive(input) {
    var val, valueOf, toString;
    if (isPrimitive(input)) {
        return input;
    }
    valueOf = input.valueOf;
    if (typeof valueOf === "function") {
        val = valueOf.call(input);
        if (isPrimitive(val)) {
            return val;
        }
    }
    toString = input.toString;
    if (typeof toString === "function") {
        val = toString.call(input);
        if (isPrimitive(val)) {
            return val;
        }
    }
    throw new TypeError();
}
var toObject = function (o) {
    if (o == null) { // this matches both null and undefined
        throw new TypeError("can't convert "+o+" to object");
    }
    return Object(o);
};

});

define('ace/lib/event_emitter', ['require', 'exports', 'module' ], function(require, exports, module) {


var EventEmitter = {};

EventEmitter._emit =
EventEmitter._dispatchEvent = function(eventName, e) {
    this._eventRegistry = this._eventRegistry || {};
    this._defaultHandlers = this._defaultHandlers || {};

    var listeners = this._eventRegistry[eventName] || [];
    var defaultHandler = this._defaultHandlers[eventName];
    if (!listeners.length && !defaultHandler)
        return;

    if (typeof e != "object" || !e)
        e = {};

    if (!e.type)
        e.type = eventName;
    
    if (!e.stopPropagation) {
        e.stopPropagation = function() {
            this.propagationStopped = true;
        };
    }
    
    if (!e.preventDefault) {
        e.preventDefault = function() {
            this.defaultPrevented = true;
        };
    }

    for (var i=0; i<listeners.length; i++) {
        listeners[i](e);
        if (e.propagationStopped)
            break;
    }
    
    if (defaultHandler && !e.defaultPrevented)
        return defaultHandler(e);
};

EventEmitter.setDefaultHandler = function(eventName, callback) {
    this._defaultHandlers = this._defaultHandlers || {};
    
    if (this._defaultHandlers[eventName])
        throw new Error("The default handler for '" + eventName + "' is already set");
        
    this._defaultHandlers[eventName] = callback;
};

EventEmitter.on =
EventEmitter.addEventListener = function(eventName, callback) {
    this._eventRegistry = this._eventRegistry || {};

    var listeners = this._eventRegistry[eventName];
    if (!listeners)
        listeners = this._eventRegistry[eventName] = [];

    if (listeners.indexOf(callback) == -1)
        listeners.push(callback);
};

EventEmitter.removeListener =
EventEmitter.removeEventListener = function(eventName, callback) {
    this._eventRegistry = this._eventRegistry || {};

    var listeners = this._eventRegistry[eventName];
    if (!listeners)
        return;

    var index = listeners.indexOf(callback);
    if (index !== -1)
        listeners.splice(index, 1);
};

EventEmitter.removeAllListeners = function(eventName) {
    if (this._eventRegistry) this._eventRegistry[eventName] = [];
};

exports.EventEmitter = EventEmitter;

});

define('ace/lib/oop', ['require', 'exports', 'module' ], function(require, exports, module) {


exports.inherits = (function() {
    var tempCtor = function() {};
    return function(ctor, superCtor) {
        tempCtor.prototype = superCtor.prototype;
        ctor.super_ = superCtor.prototype;
        ctor.prototype = new tempCtor();
        ctor.prototype.constructor = ctor;
    };
}());

exports.mixin = function(obj, mixin) {
    for (var key in mixin) {
        obj[key] = mixin[key];
    }
};

exports.implement = function(proto, mixin) {
    exports.mixin(proto, mixin);
};

});

define('ace/mode/javascript_worker', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/worker/mirror', 'ace/mode/javascript/jshint'], function(require, exports, module) {


var oop = require("../lib/oop");
var Mirror = require("../worker/mirror").Mirror;
var lint = require("./javascript/jshint").JSHINT;

function startRegex(arr) {
    return RegExp("^(" + arr.join("|") + ")");
}

var disabledWarningsRe = startRegex([
    "Bad for in variable '(.+)'.",
    'Missing "use strict"'
]);
var errorsRe = startRegex([
    "Unexpected",
    "Expected ",
    "Confusing (plus|minus)",
    "\\{a\\} unterminated regular expression",
    "Unclosed ",
    "Unmatched ",
    "Unbegun comment",
    "Bad invocation",
    "Missing space after",
    "Missing operator at"
]);
var infoRe = startRegex([
    "Expected an assignment",
    "Bad escapement of EOL",
    "Unexpected comma",
    "Unexpected space",
    "Missing radix parameter.",
    "A leading decimal point can",
    "\\['{a}'\\] is better written in dot notation.",
    "'{a}' used out of scope"
]);

var JavaScriptWorker = exports.JavaScriptWorker = function(sender) {
    Mirror.call(this, sender);
    this.setTimeout(500);
    this.setOptions();
};

oop.inherits(JavaScriptWorker, Mirror);

(function() {
    this.setOptions = function(options) {
        this.options = options || {
            es5: true,
            esnext: true,
            devel: true,
            browser: true,
            node: true,
            laxcomma: true,
            laxbreak: true,
            lastsemic: true,
            onevar: false,
            passfail: false,
            maxerr: 100,
            expr: true,
            multistr: true,
            globalstrict: true
        };
        this.doc.getValue() && this.deferredUpdate.schedule(100);
    };

    this.changeOptions = function(newOptions) {
        oop.mixin(this.options, newOptions);
        this.doc.getValue() && this.deferredUpdate.schedule(100);
    };

    this.isValidJS = function(str) {
        try {
            eval("throw 0;" + str);
        } catch(e) {
            if (e === 0)
                return true;
        }
        return false
    };

    this.onUpdate = function() {
        var value = this.doc.getValue();
        value = value.replace(/^#!.*\n/, "\n");
        if (!value) {
            this.sender.emit("jslint", []);
            return;
        }
        var errors = [];
        var maxErrorLevel = this.isValidJS(value) ? "warning" : "error";
        lint(value, this.options);
        var results = lint.errors;

        var errorAdded = false
        for (var i = 0; i < results.length; i++) {
            var error = results[i];
            if (!error)
                continue;
            var raw = error.raw;
            var type = "warning";

            if (raw == "Missing semicolon.") {
                var str = error.evidence.substr(error.character);
                str = str.charAt(str.search(/\S/));
                if (maxErrorLevel == "error" && str && /[\w\d{(['"]/.test(str)) {
                    error.reason = 'Missing ";" before statement';
                    type = "error";
                } else {
                    type = "info";
                }
            }
            else if (disabledWarningsRe.test(raw)) {
                continue;
            }
            else if (infoRe.test(raw)) {
                type = "info"
            }
            else if (errorsRe.test(raw)) {
                errorAdded  = true;
                type = maxErrorLevel;
            }
            else if (raw == "'{a}' is not defined.") {
                type = "warning";
            }
            else if (raw == "'{a}' is defined but never used.") {
                type = "info";
            }

            errors.push({
                row: error.line-1,
                column: error.character-1,
                text: error.reason,
                type: type,
                raw: raw
            });

            if (errorAdded) {
            }
        }

        this.sender.emit("jslint", errors);
    };

}).call(JavaScriptWorker.prototype);

});
define('ace/worker/mirror', ['require', 'exports', 'module' , 'ace/document', 'ace/lib/lang'], function(require, exports, module) {


var Document = require("../document").Document;
var lang = require("../lib/lang");
    
var Mirror = exports.Mirror = function(sender) {
    this.sender = sender;
    var doc = this.doc = new Document("");
    
    var deferredUpdate = this.deferredUpdate = lang.deferredCall(this.onUpdate.bind(this));
    
    var _self = this;
    sender.on("change", function(e) {
        doc.applyDeltas([e.data]);        
        deferredUpdate.schedule(_self.$timeout);
    });
};

(function() {
    
    this.$timeout = 500;
    
    this.setTimeout = function(timeout) {
        this.$timeout = timeout;
    };
    
    this.setValue = function(value) {
        this.doc.setValue(value);
        this.deferredUpdate.schedule(this.$timeout);
    };
    
    this.getValue = function(callbackId) {
        this.sender.callback(this.doc.getValue(), callbackId);
    };
    
    this.onUpdate = function() {
    };
    
}).call(Mirror.prototype);

});

define('ace/document', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/lib/event_emitter', 'ace/range', 'ace/anchor'], function(require, exports, module) {


var oop = require("./lib/oop");
var EventEmitter = require("./lib/event_emitter").EventEmitter;
var Range = require("./range").Range;
var Anchor = require("./anchor").Anchor;

var Document = function(text) {
    this.$lines = [];
    if (text.length == 0) {
        this.$lines = [""];
    } else if (Array.isArray(text)) {
        this.insertLines(0, text);
    } else {
        this.insert({row: 0, column:0}, text);
    }
};

(function() {

    oop.implement(this, EventEmitter);
    this.setValue = function(text) {
        var len = this.getLength();
        this.remove(new Range(0, 0, len, this.getLine(len-1).length));
        this.insert({row: 0, column:0}, text);
    };
    this.getValue = function() {
        return this.getAllLines().join(this.getNewLineCharacter());
    };
    this.createAnchor = function(row, column) {
        return new Anchor(this, row, column);
    };
    if ("aaa".split(/a/).length == 0)
        this.$split = function(text) {
            return text.replace(/\r\n|\r/g, "\n").split("\n");
        }
    else
        this.$split = function(text) {
            return text.split(/\r\n|\r|\n/);
        };


 
    this.$detectNewLine = function(text) {
        var match = text.match(/^.*?(\r\n|\r|\n)/m);
        if (match) {
            this.$autoNewLine = match[1];
        } else {
            this.$autoNewLine = "\n";
        }
    };
    this.getNewLineCharacter = function() {
        switch (this.$newLineMode) {
          case "windows":
            return "\r\n";

          case "unix":
            return "\n";

          default:
            return this.$autoNewLine;
        }
    };

    this.$autoNewLine = "\n";
    this.$newLineMode = "auto";
    this.setNewLineMode = function(newLineMode) {
        if (this.$newLineMode === newLineMode)
            return;

        this.$newLineMode = newLineMode;
    };
    this.getNewLineMode = function() {
        return this.$newLineMode;
    };
    this.isNewLine = function(text) {
        return (text == "\r\n" || text == "\r" || text == "\n");
    };
    this.getLine = function(row) {
        return this.$lines[row] || "";
    };
    this.getLines = function(firstRow, lastRow) {
        return this.$lines.slice(firstRow, lastRow + 1);
    };
    this.getAllLines = function() {
        return this.getLines(0, this.getLength());
    };
    this.getLength = function() {
        return this.$lines.length;
    };
    this.getTextRange = function(range) {
        if (range.start.row == range.end.row) {
            return this.$lines[range.start.row].substring(range.start.column,
                                                         range.end.column);
        }
        else {
            var lines = this.getLines(range.start.row+1, range.end.row-1);
            lines.unshift((this.$lines[range.start.row] || "").substring(range.start.column));
            lines.push((this.$lines[range.end.row] || "").substring(0, range.end.column));
            return lines.join(this.getNewLineCharacter());
        }
    };

    this.$clipPosition = function(position) {
        var length = this.getLength();
        if (position.row >= length) {
            position.row = Math.max(0, length - 1);
            position.column = this.getLine(length-1).length;
        }
        return position;
    };
    this.insert = function(position, text) {
        if (!text || text.length === 0)
            return position;

        position = this.$clipPosition(position);
        if (this.getLength() <= 1)
            this.$detectNewLine(text);

        var lines = this.$split(text);
        var firstLine = lines.splice(0, 1)[0];
        var lastLine = lines.length == 0 ? null : lines.splice(lines.length - 1, 1)[0];

        position = this.insertInLine(position, firstLine);
        if (lastLine !== null) {
            position = this.insertNewLine(position); // terminate first line
            position = this.insertLines(position.row, lines);
            position = this.insertInLine(position, lastLine || "");
        }
        return position;
    };
    this.insertLines = function(row, lines) {
        if (lines.length == 0)
            return {row: row, column: 0};
        if (lines.length > 0xFFFF) {
            var end = this.insertLines(row, lines.slice(0xFFFF));
            lines = lines.slice(0, 0xFFFF);
        }

        var args = [row, 0];
        args.push.apply(args, lines);
        this.$lines.splice.apply(this.$lines, args);

        var range = new Range(row, 0, row + lines.length, 0);
        var delta = {
            action: "insertLines",
            range: range,
            lines: lines
        };
        this._emit("change", { data: delta });
        return end || range.end;
    };
    this.insertNewLine = function(position) {
        position = this.$clipPosition(position);
        var line = this.$lines[position.row] || "";

        this.$lines[position.row] = line.substring(0, position.column);
        this.$lines.splice(position.row + 1, 0, line.substring(position.column, line.length));

        var end = {
            row : position.row + 1,
            column : 0
        };

        var delta = {
            action: "insertText",
            range: Range.fromPoints(position, end),
            text: this.getNewLineCharacter()
        };
        this._emit("change", { data: delta });

        return end;
    };
    this.insertInLine = function(position, text) {
        if (text.length == 0)
            return position;

        var line = this.$lines[position.row] || "";

        this.$lines[position.row] = line.substring(0, position.column) + text
                + line.substring(position.column);

        var end = {
            row : position.row,
            column : position.column + text.length
        };

        var delta = {
            action: "insertText",
            range: Range.fromPoints(position, end),
            text: text
        };
        this._emit("change", { data: delta });

        return end;
    };
    this.remove = function(range) {
        range.start = this.$clipPosition(range.start);
        range.end = this.$clipPosition(range.end);

        if (range.isEmpty())
            return range.start;

        var firstRow = range.start.row;
        var lastRow = range.end.row;

        if (range.isMultiLine()) {
            var firstFullRow = range.start.column == 0 ? firstRow : firstRow + 1;
            var lastFullRow = lastRow - 1;

            if (range.end.column > 0)
                this.removeInLine(lastRow, 0, range.end.column);

            if (lastFullRow >= firstFullRow)
                this.removeLines(firstFullRow, lastFullRow);

            if (firstFullRow != firstRow) {
                this.removeInLine(firstRow, range.start.column, this.getLine(firstRow).length);
                this.removeNewLine(range.start.row);
            }
        }
        else {
            this.removeInLine(firstRow, range.start.column, range.end.column);
        }
        return range.start;
    };
    this.removeInLine = function(row, startColumn, endColumn) {
        if (startColumn == endColumn)
            return;

        var range = new Range(row, startColumn, row, endColumn);
        var line = this.getLine(row);
        var removed = line.substring(startColumn, endColumn);
        var newLine = line.substring(0, startColumn) + line.substring(endColumn, line.length);
        this.$lines.splice(row, 1, newLine);

        var delta = {
            action: "removeText",
            range: range,
            text: removed
        };
        this._emit("change", { data: delta });
        return range.start;
    };
    this.removeLines = function(firstRow, lastRow) {
        var range = new Range(firstRow, 0, lastRow + 1, 0);
        var removed = this.$lines.splice(firstRow, lastRow - firstRow + 1);

        var delta = {
            action: "removeLines",
            range: range,
            nl: this.getNewLineCharacter(),
            lines: removed
        };
        this._emit("change", { data: delta });
        return removed;
    };
    this.removeNewLine = function(row) {
        var firstLine = this.getLine(row);
        var secondLine = this.getLine(row+1);

        var range = new Range(row, firstLine.length, row+1, 0);
        var line = firstLine + secondLine;

        this.$lines.splice(row, 2, line);

        var delta = {
            action: "removeText",
            range: range,
            text: this.getNewLineCharacter()
        };
        this._emit("change", { data: delta });
    };
    this.replace = function(range, text) {
        if (text.length == 0 && range.isEmpty())
            return range.start;
        if (text == this.getTextRange(range))
            return range.end;

        this.remove(range);
        if (text) {
            var end = this.insert(range.start, text);
        }
        else {
            end = range.start;
        }

        return end;
    };
    this.applyDeltas = function(deltas) {
        for (var i=0; i<deltas.length; i++) {
            var delta = deltas[i];
            var range = Range.fromPoints(delta.range.start, delta.range.end);

            if (delta.action == "insertLines")
                this.insertLines(range.start.row, delta.lines);
            else if (delta.action == "insertText")
                this.insert(range.start, delta.text);
            else if (delta.action == "removeLines")
                this.removeLines(range.start.row, range.end.row - 1);
            else if (delta.action == "removeText")
                this.remove(range);
        }
    };
    this.revertDeltas = function(deltas) {
        for (var i=deltas.length-1; i>=0; i--) {
            var delta = deltas[i];

            var range = Range.fromPoints(delta.range.start, delta.range.end);

            if (delta.action == "insertLines")
                this.removeLines(range.start.row, range.end.row - 1);
            else if (delta.action == "insertText")
                this.remove(range);
            else if (delta.action == "removeLines")
                this.insertLines(range.start.row, delta.lines);
            else if (delta.action == "removeText")
                this.insert(range.start, delta.text);
        }
    };
    this.indexToPosition = function(index, startRow) {
        var lines = this.$lines || this.getAllLines();
        var newlineLength = this.getNewLineCharacter().length;
        for (var i = startRow || 0, l = lines.length; i < l; i++) {
            index -= lines[i].length + newlineLength;
            if (index < 0)
                return {row: i, column: index + lines[i].length + newlineLength};
        }
        return {row: l-1, column: lines[l-1].length};
    };
    this.positionToIndex = function(pos, startRow) {
        var lines = this.$lines || this.getAllLines();
        var newlineLength = this.getNewLineCharacter().length;
        var index = 0;
        var row = Math.min(pos.row, lines.length);
        for (var i = startRow || 0; i < row; ++i)
            index += lines[i].length;

        return index + newlineLength * i + pos.column;
    };

}).call(Document.prototype);

exports.Document = Document;
});

define('ace/range', ['require', 'exports', 'module' ], function(require, exports, module) {
var Range = function(startRow, startColumn, endRow, endColumn) {
    this.start = {
        row: startRow,
        column: startColumn
    };

    this.end = {
        row: endRow,
        column: endColumn
    };
};

(function() { 
    this.isEqual = function(range) {
        return this.start.row == range.start.row &&
            this.end.row == range.end.row &&
            this.start.column == range.start.column &&
            this.end.column == range.end.column
    }; 
    this.toString = function() {
        return ("Range: [" + this.start.row + "/" + this.start.column +
            "] -> [" + this.end.row + "/" + this.end.column + "]");
    }; 

    this.contains = function(row, column) {
        return this.compare(row, column) == 0;
    }; 
    this.compareRange = function(range) {
        var cmp,
            end = range.end,
            start = range.start;

        cmp = this.compare(end.row, end.column);
        if (cmp == 1) {
            cmp = this.compare(start.row, start.column);
            if (cmp == 1) {
                return 2;
            } else if (cmp == 0) {
                return 1;
            } else {
                return 0;
            }
        } else if (cmp == -1) {
            return -2;
        } else {
            cmp = this.compare(start.row, start.column);
            if (cmp == -1) {
                return -1;
            } else if (cmp == 1) {
                return 42;
            } else {
                return 0;
            }
        }
    }; 
    this.comparePoint = function(p) {
        return this.compare(p.row, p.column);
    }; 
    this.containsRange = function(range) {
        return this.comparePoint(range.start) == 0 && this.comparePoint(range.end) == 0;
    };
    this.intersects = function(range) {
        var cmp = this.compareRange(range);
        return (cmp == -1 || cmp == 0 || cmp == 1);
    };
    this.isEnd = function(row, column) {
        return this.end.row == row && this.end.column == column;
    }; 
    this.isStart = function(row, column) {
        return this.start.row == row && this.start.column == column;
    }; 
    this.setStart = function(row, column) {
        if (typeof row == "object") {
            this.start.column = row.column;
            this.start.row = row.row;
        } else {
            this.start.row = row;
            this.start.column = column;
        }
    }; 
    this.setEnd = function(row, column) {
        if (typeof row == "object") {
            this.end.column = row.column;
            this.end.row = row.row;
        } else {
            this.end.row = row;
            this.end.column = column;
        }
    }; 
    this.inside = function(row, column) {
        if (this.compare(row, column) == 0) {
            if (this.isEnd(row, column) || this.isStart(row, column)) {
                return false;
            } else {
                return true;
            }
        }
        return false;
    }; 
    this.insideStart = function(row, column) {
        if (this.compare(row, column) == 0) {
            if (this.isEnd(row, column)) {
                return false;
            } else {
                return true;
            }
        }
        return false;
    }; 
    this.insideEnd = function(row, column) {
        if (this.compare(row, column) == 0) {
            if (this.isStart(row, column)) {
                return false;
            } else {
                return true;
            }
        }
        return false;
    };
    this.compare = function(row, column) {
        if (!this.isMultiLine()) {
            if (row === this.start.row) {
                return column < this.start.column ? -1 : (column > this.end.column ? 1 : 0);
            };
        }

        if (row < this.start.row)
            return -1;

        if (row > this.end.row)
            return 1;

        if (this.start.row === row)
            return column >= this.start.column ? 0 : -1;

        if (this.end.row === row)
            return column <= this.end.column ? 0 : 1;

        return 0;
    };
    this.compareStart = function(row, column) {
        if (this.start.row == row && this.start.column == column) {
            return -1;
        } else {
            return this.compare(row, column);
        }
    };
    this.compareEnd = function(row, column) {
        if (this.end.row == row && this.end.column == column) {
            return 1;
        } else {
            return this.compare(row, column);
        }
    };
    this.compareInside = function(row, column) {
        if (this.end.row == row && this.end.column == column) {
            return 1;
        } else if (this.start.row == row && this.start.column == column) {
            return -1;
        } else {
            return this.compare(row, column);
        }
    };
    this.clipRows = function(firstRow, lastRow) {
        if (this.end.row > lastRow) {
            var end = {
                row: lastRow+1,
                column: 0
            };
        }

        if (this.start.row > lastRow) {
            var start = {
                row: lastRow+1,
                column: 0
            };
        }

        if (this.start.row < firstRow) {
            var start = {
                row: firstRow,
                column: 0
            };
        }

        if (this.end.row < firstRow) {
            var end = {
                row: firstRow,
                column: 0
            };
        }
        return Range.fromPoints(start || this.start, end || this.end);
    };
    this.extend = function(row, column) {
        var cmp = this.compare(row, column);

        if (cmp == 0)
            return this;
        else if (cmp == -1)
            var start = {row: row, column: column};
        else
            var end = {row: row, column: column};

        return Range.fromPoints(start || this.start, end || this.end);
    };

    this.isEmpty = function() {
        return (this.start.row == this.end.row && this.start.column == this.end.column);
    };
    this.isMultiLine = function() {
        return (this.start.row !== this.end.row);
    };
    this.clone = function() {
        return Range.fromPoints(this.start, this.end);
    };
    this.collapseRows = function() {
        if (this.end.column == 0)
            return new Range(this.start.row, 0, Math.max(this.start.row, this.end.row-1), 0)
        else
            return new Range(this.start.row, 0, this.end.row, 0)
    };
    this.toScreenRange = function(session) {
        var screenPosStart =
            session.documentToScreenPosition(this.start);
        var screenPosEnd =
            session.documentToScreenPosition(this.end);

        return new Range(
            screenPosStart.row, screenPosStart.column,
            screenPosEnd.row, screenPosEnd.column
        );
    };

}).call(Range.prototype);
Range.fromPoints = function(start, end) {
    return new Range(start.row, start.column, end.row, end.column);
};

exports.Range = Range;
});

define('ace/anchor', ['require', 'exports', 'module' , 'ace/lib/oop', 'ace/lib/event_emitter'], function(require, exports, module) {


var oop = require("./lib/oop");
var EventEmitter = require("./lib/event_emitter").EventEmitter;

var Anchor = exports.Anchor = function(doc, row, column) {
    this.document = doc;
    
    if (typeof column == "undefined")
        this.setPosition(row.row, row.column);
    else
        this.setPosition(row, column);

    this.$onChange = this.onChange.bind(this);
    doc.on("change", this.$onChange);
};

(function() {

    oop.implement(this, EventEmitter);

    this.getPosition = function() {
        return this.$clipPositionToDocument(this.row, this.column);
    };
        
    this.getDocument = function() {
        return this.document;
    };

    this.onChange = function(e) {
        var delta = e.data;
        var range = delta.range;
            
        if (range.start.row == range.end.row && range.start.row != this.row)
            return;
            
        if (range.start.row > this.row)
            return;
            
        if (range.start.row == this.row && range.start.column > this.column)
            return;
    
        var row = this.row;
        var column = this.column;
        
        if (delta.action === "insertText") {
            if (range.start.row === row && range.start.column <= column) {
                if (range.start.row === range.end.row) {
                    column += range.end.column - range.start.column;
                }
                else {
                    column -= range.start.column;
                    row += range.end.row - range.start.row;
                }
            }
            else if (range.start.row !== range.end.row && range.start.row < row) {
                row += range.end.row - range.start.row;
            }
        } else if (delta.action === "insertLines") {
            if (range.start.row <= row) {
                row += range.end.row - range.start.row;
            }
        }
        else if (delta.action == "removeText") {
            if (range.start.row == row && range.start.column < column) {
                if (range.end.column >= column)
                    column = range.start.column;
                else
                    column = Math.max(0, column - (range.end.column - range.start.column));
                
            } else if (range.start.row !== range.end.row && range.start.row < row) {
                if (range.end.row == row) {
                    column = Math.max(0, column - range.end.column) + range.start.column;
                }
                row -= (range.end.row - range.start.row);
            }
            else if (range.end.row == row) {
                row -= range.end.row - range.start.row;
                column = Math.max(0, column - range.end.column) + range.start.column;
            }
        } else if (delta.action == "removeLines") {
            if (range.start.row <= row) {
                if (range.end.row <= row)
                    row -= range.end.row - range.start.row;
                else {
                    row = range.start.row;
                    column = 0;
                }
            }
        }

        this.setPosition(row, column, true);
    };

    this.setPosition = function(row, column, noClip) {
        var pos;
        if (noClip) {
            pos = {
                row: row,
                column: column
            };
        }
        else {
            pos = this.$clipPositionToDocument(row, column);
        }
        
        if (this.row == pos.row && this.column == pos.column)
            return;
            
        var old = {
            row: this.row,
            column: this.column
        };
        
        this.row = pos.row;
        this.column = pos.column;
        this._emit("change", {
            old: old,
            value: pos
        });
    };

    this.detach = function() {
        this.document.removeEventListener("change", this.$onChange);
    };
    this.$clipPositionToDocument = function(row, column) {
        var pos = {};
    
        if (row >= this.document.getLength()) {
            pos.row = Math.max(0, this.document.getLength() - 1);
            pos.column = this.document.getLine(pos.row).length;
        }
        else if (row < 0) {
            pos.row = 0;
            pos.column = 0;
        }
        else {
            pos.row = row;
            pos.column = Math.min(this.document.getLine(pos.row).length, Math.max(0, column));
        }
        
        if (column < 0)
            pos.column = 0;
            
        return pos;
    };
    
}).call(Anchor.prototype);

});

define('ace/lib/lang', ['require', 'exports', 'module' ], function(require, exports, module) {


exports.stringReverse = function(string) {
    return string.split("").reverse().join("");
};

exports.stringRepeat = function (string, count) {
    var result = '';
    while (count > 0) {
        if (count & 1)
            result += string;

        if (count >>= 1)
            string += string;
    }
    return result;
};

var trimBeginRegexp = /^\s\s*/;
var trimEndRegexp = /\s\s*$/;

exports.stringTrimLeft = function (string) {
    return string.replace(trimBeginRegexp, '');
};

exports.stringTrimRight = function (string) {
    return string.replace(trimEndRegexp, '');
};

exports.copyObject = function(obj) {
    var copy = {};
    for (var key in obj) {
        copy[key] = obj[key];
    }
    return copy;
};

exports.copyArray = function(array){
    var copy = [];
    for (var i=0, l=array.length; i<l; i++) {
        if (array[i] && typeof array[i] == "object")
            copy[i] = this.copyObject( array[i] );
        else 
            copy[i] = array[i];
    }
    return copy;
};

exports.deepCopy = function (obj) {
    if (typeof obj != "object") {
        return obj;
    }
    
    var copy = obj.constructor();
    for (var key in obj) {
        if (typeof obj[key] == "object") {
            copy[key] = this.deepCopy(obj[key]);
        } else {
            copy[key] = obj[key];
        }
    }
    return copy;
};

exports.arrayToMap = function(arr) {
    var map = {};
    for (var i=0; i<arr.length; i++) {
        map[arr[i]] = 1;
    }
    return map;

};

exports.createMap = function(props) {
    var map = Object.create(null);
    for (var i in props) {
        map[i] = props[i];
    }
    return map;
};
exports.arrayRemove = function(array, value) {
  for (var i = 0; i <= array.length; i++) {
    if (value === array[i]) {
      array.splice(i, 1);
    }
  }
};

exports.escapeRegExp = function(str) {
    return str.replace(/([.*+?^${}()|[\]\/\\])/g, '\\$1');
};

exports.escapeHTML = function(str) {
    return str.replace(/&/g, "&#38;").replace(/"/g, "&#34;").replace(/'/g, "&#39;").replace(/</g, "&#60;");
};

exports.getMatchOffsets = function(string, regExp) {
    var matches = [];

    string.replace(regExp, function(str) {
        matches.push({
            offset: arguments[arguments.length-2],
            length: str.length
        });
    });

    return matches;
};
exports.deferredCall = function(fcn) {

    var timer = null;
    var callback = function() {
        timer = null;
        fcn();
    };

    var deferred = function(timeout) {
        deferred.cancel();
        timer = setTimeout(callback, timeout || 0);
        return deferred;
    };

    deferred.schedule = deferred;

    deferred.call = function() {
        this.cancel();
        fcn();
        return deferred;
    };

    deferred.cancel = function() {
        clearTimeout(timer);
        timer = null;
        return deferred;
    };

    return deferred;
};


exports.delayedCall = function(fcn, defaultTimeout) {
    var timer = null;
    var callback = function() {
        timer = null;
        fcn();
    };

    var _self = function(timeout) {
        timer && clearTimeout(timer);
        timer = setTimeout(callback, timeout || defaultTimeout);
    };

    _self.delay = _self;
    _self.schedule = function(timeout) {
        if (timer == null)
            timer = setTimeout(callback, timeout || 0);
    };

    _self.call = function() {
        this.cancel();
        fcn();
    };

    _self.cancel = function() {
        timer && clearTimeout(timer);
        timer = null;
    };

    _self.isPending = function() {
        return timer;
    };

    return _self;
};
});
define('ace/mode/javascript/jshint', ['require', 'exports', 'module' ], function(require, exports, module) {

var JSHINT = (function () {
	

	var anonname,		// The guessed name for anonymous functions.

		bang = {
			"<"  : true,
			"<=" : true,
			"==" : true,
			"===": true,
			"!==": true,
			"!=" : true,
			">"  : true,
			">=" : true,
			"+"  : true,
			"-"  : true,
			"*"  : true,
			"/"  : true,
			"%"  : true
		},
		boolOptions = {
			asi			: true, // if automatic semicolon insertion should be tolerated
			bitwise		: true, // if bitwise operators should not be allowed
			boss		: true, // if advanced usage of assignments should be allowed
			browser		: true, // if the standard browser globals should be predefined
			camelcase	: true, // if identifiers should be required in camel case
			couch		: true, // if CouchDB globals should be predefined
			curly		: true, // if curly braces around all blocks should be required
			debug		: true, // if debugger statements should be allowed
			devel		: true, // if logging globals should be predefined (console,
			dojo		: true, // if Dojo Toolkit globals should be predefined
			eqeqeq		: true, // if === should be required
			eqnull		: true, // if == null comparisons should be tolerated
			es5			: true, // if ES5 syntax should be allowed
			esnext		: true, // if es.next specific syntax should be allowed
			evil		: true, // if eval should be allowed
			expr		: true, // if ExpressionStatement should be allowed as Programs
			forin		: true, // if for in statements must filter
			funcscope	: true, // if only function scope should be used for scope tests
			globalstrict: true, // if global  should be allowed (also
			immed		: true, // if immediate invocations must be wrapped in parens
			iterator	: true, // if the `__iterator__` property should be allowed
			jquery		: true, // if jQuery globals should be predefined
			lastsemic	: true, // if semicolons may be ommitted for the trailing
			latedef		: true, // if the use before definition should not be tolerated
			laxbreak	: true, // if line breaks should not be checked
			laxcomma	: true, // if line breaks should not be checked around commas
			loopfunc	: true, // if functions should be allowed to be defined within
			mootools	: true, // if MooTools globals should be predefined
			multistr	: true, // allow multiline strings
			newcap		: true, // if constructor names must be capitalized
			noarg		: true, // if arguments.caller and arguments.callee should be
			node		: true, // if the Node.js environment globals should be
			noempty		: true, // if empty blocks should be disallowed
			nonew		: true, // if using `new` for side-effects should be disallowed
			nonstandard : true, // if non-standard (but widely adopted) globals should
			nomen		: true, // if names should be checked
			onevar		: true, // if only one var statement per function should be
			onecase		: true, // if one case switch statements should be allowed
			passfail	: true, // if the scan should stop on first error
			plusplus	: true, // if increment/decrement should not be allowed
			proto		: true, // if the `__proto__` property should be allowed
			prototypejs : true, // if Prototype and Scriptaculous globals should be
			regexdash	: true, // if unescaped first/last dash (-) inside brackets
			regexp		: true, // if the . should not be allowed in regexp literals
			rhino		: true, // if the Rhino environment globals should be predefined
			undef		: true, // if variables should be declared before used
			unused		: true, // if variables should be always used
			scripturl	: true, // if script-targeted URLs should be tolerated
			shadow		: true, // if variable shadowing should be tolerated
			smarttabs	: true, // if smarttabs should be tolerated
			strict		: true, // require the  pragma
			sub			: true, // if all forms of subscript notation are tolerated
			supernew	: true, // if `new function () { ... };` and `new Object;`
			trailing	: true, // if trailing whitespace rules apply
			validthis	: true, // if 'this' inside a non-constructor function is valid.
			withstmt	: true, // if with statements should be allowed
			white		: true, // if strict whitespace rules apply
			worker		: true, // if Web Worker script symbols should be allowed
			wsh			: true, // if the Windows Scripting Host environment globals
			yui			: true	// YUI variables should be predefined
		},
		valOptions = {
			maxlen		 : false,
			indent		 : false,
			maxerr		 : false,
			predef		 : false,
			quotmark	 : false, //'single'|'double'|true
			scope		 : false,
			maxstatements: false, // {int} max statements per function
			maxdepth	 : false, // {int} max nested block depth per function
			maxparams	 : false, // {int} max params per function
			maxcomplexity: false  // {int} max cyclomatic complexity per function
		},
		invertedOptions = {
			bitwise		: true,
			forin		: true,
			newcap		: true,
			nomen		: true,
			plusplus	: true,
			regexp		: true,
			undef		: true,
			white		: true,
			eqeqeq		: true,
			onevar		: true
		},
		renamedOptions = {
			eqeq		: "eqeqeq",
			vars		: "onevar",
			windows		: "wsh"
		},
		browser = {
			ArrayBuffer				 :	false,
			ArrayBufferView			 :	false,
			Audio					 :	false,
			Blob					 :	false,
			addEventListener		 :	false,
			applicationCache		 :	false,
			atob					 :	false,
			blur					 :	false,
			btoa					 :	false,
			clearInterval			 :	false,
			clearTimeout			 :	false,
			close					 :	false,
			closed					 :	false,
			DataView				 :	false,
			DOMParser				 :	false,
			defaultStatus			 :	false,
			document				 :	false,
			event					 :	false,
			FileReader				 :	false,
			Float32Array			 :	false,
			Float64Array			 :	false,
			FormData				 :	false,
			focus					 :	false,
			frames					 :	false,
			getComputedStyle		 :	false,
			HTMLElement				 :	false,
			HTMLAnchorElement		 :	false,
			HTMLBaseElement			 :	false,
			HTMLBlockquoteElement	 :	false,
			HTMLBodyElement			 :	false,
			HTMLBRElement			 :	false,
			HTMLButtonElement		 :	false,
			HTMLCanvasElement		 :	false,
			HTMLDirectoryElement	 :	false,
			HTMLDivElement			 :	false,
			HTMLDListElement		 :	false,
			HTMLFieldSetElement		 :	false,
			HTMLFontElement			 :	false,
			HTMLFormElement			 :	false,
			HTMLFrameElement		 :	false,
			HTMLFrameSetElement		 :	false,
			HTMLHeadElement			 :	false,
			HTMLHeadingElement		 :	false,
			HTMLHRElement			 :	false,
			HTMLHtmlElement			 :	false,
			HTMLIFrameElement		 :	false,
			HTMLImageElement		 :	false,
			HTMLInputElement		 :	false,
			HTMLIsIndexElement		 :	false,
			HTMLLabelElement		 :	false,
			HTMLLayerElement		 :	false,
			HTMLLegendElement		 :	false,
			HTMLLIElement			 :	false,
			HTMLLinkElement			 :	false,
			HTMLMapElement			 :	false,
			HTMLMenuElement			 :	false,
			HTMLMetaElement			 :	false,
			HTMLModElement			 :	false,
			HTMLObjectElement		 :	false,
			HTMLOListElement		 :	false,
			HTMLOptGroupElement		 :	false,
			HTMLOptionElement		 :	false,
			HTMLParagraphElement	 :	false,
			HTMLParamElement		 :	false,
			HTMLPreElement			 :	false,
			HTMLQuoteElement		 :	false,
			HTMLScriptElement		 :	false,
			HTMLSelectElement		 :	false,
			HTMLStyleElement		 :	false,
			HTMLTableCaptionElement  :	false,
			HTMLTableCellElement	 :	false,
			HTMLTableColElement		 :	false,
			HTMLTableElement		 :	false,
			HTMLTableRowElement		 :	false,
			HTMLTableSectionElement  :	false,
			HTMLTextAreaElement		 :	false,
			HTMLTitleElement		 :	false,
			HTMLUListElement		 :	false,
			HTMLVideoElement		 :	false,
			history					 :	false,
			Int16Array				 :	false,
			Int32Array				 :	false,
			Int8Array				 :	false,
			Image					 :	false,
			length					 :	false,
			localStorage			 :	false,
			location				 :	false,
			MessageChannel			 :	false,
			MessageEvent			 :	false,
			MessagePort				 :	false,
			moveBy					 :	false,
			moveTo					 :	false,
			MutationObserver		 :	false,
			name					 :	false,
			Node					 :	false,
			NodeFilter				 :	false,
			navigator				 :	false,
			onbeforeunload			 :	true,
			onblur					 :	true,
			onerror					 :	true,
			onfocus					 :	true,
			onload					 :	true,
			onresize				 :	true,
			onunload				 :	true,
			open					 :	false,
			openDatabase			 :	false,
			opener					 :	false,
			Option					 :	false,
			parent					 :	false,
			print					 :	false,
			removeEventListener		 :	false,
			resizeBy				 :	false,
			resizeTo				 :	false,
			screen					 :	false,
			scroll					 :	false,
			scrollBy				 :	false,
			scrollTo				 :	false,
			sessionStorage			 :	false,
			setInterval				 :	false,
			setTimeout				 :	false,
			SharedWorker			 :	false,
			status					 :	false,
			top						 :	false,
			Uint16Array				 :	false,
			Uint32Array				 :	false,
			Uint8Array				 :	false,
			WebSocket				 :	false,
			window					 :	false,
			Worker					 :	false,
			XMLHttpRequest			 :	false,
			XMLSerializer			 :	false,
			XPathEvaluator			 :	false,
			XPathException			 :	false,
			XPathExpression			 :	false,
			XPathNamespace			 :	false,
			XPathNSResolver			 :	false,
			XPathResult				 :	false
		},

		couch = {
			"require" : false,
			respond   : false,
			getRow	  : false,
			emit	  : false,
			send	  : false,
			start	  : false,
			sum		  : false,
			log		  : false,
			exports   : false,
			module	  : false,
			provides  : false
		},

		declared, // Globals that were declared using /*global ... */ syntax.

		devel = {
			alert	: false,
			confirm : false,
			console : false,
			Debug	: false,
			opera	: false,
			prompt	: false
		},

		dojo = {
			dojo	  : false,
			dijit	  : false,
			dojox	  : false,
			define	  : false,
			"require" : false
		},

		funct,			// The current function

		functionicity = [
			"closure", "exception", "global", "label",
			"outer", "unused", "var"
		],

		functions,		// All of the functions

		global,			// The global scope
		implied,		// Implied globals
		inblock,
		indent,
		jsonmode,

		jquery = {
			"$"    : false,
			jQuery : false
		},

		lines,
		lookahead,
		member,
		membersOnly,

		mootools = {
			"$"				: false,
			"$$"			: false,
			Asset			: false,
			Browser			: false,
			Chain			: false,
			Class			: false,
			Color			: false,
			Cookie			: false,
			Core			: false,
			Document		: false,
			DomReady		: false,
			DOMEvent		: false,
			DOMReady		: false,
			Drag			: false,
			Element			: false,
			Elements		: false,
			Event			: false,
			Events			: false,
			Fx				: false,
			Group			: false,
			Hash			: false,
			HtmlTable		: false,
			Iframe			: false,
			IframeShim		: false,
			InputValidator	: false,
			instanceOf		: false,
			Keyboard		: false,
			Locale			: false,
			Mask			: false,
			MooTools		: false,
			Native			: false,
			Options			: false,
			OverText		: false,
			Request			: false,
			Scroller		: false,
			Slick			: false,
			Slider			: false,
			Sortables		: false,
			Spinner			: false,
			Swiff			: false,
			Tips			: false,
			Type			: false,
			typeOf			: false,
			URI				: false,
			Window			: false
		},

		nexttoken,

		node = {
			__filename	  : false,
			__dirname	  : false,
			Buffer		  : false,
			console		  : false,
			exports		  : true,  // In Node it is ok to exports = module.exports = foo();
			GLOBAL		  : false,
			global		  : false,
			module		  : false,
			process		  : false,
			require		  : false,
			setTimeout	  : false,
			clearTimeout  : false,
			setInterval   : false,
			clearInterval : false
		},

		noreach,
		option,
		predefined,		// Global variables defined by option
		prereg,
		prevtoken,

		prototypejs = {
			"$"				  : false,
			"$$"			  : false,
			"$A"			  : false,
			"$F"			  : false,
			"$H"			  : false,
			"$R"			  : false,
			"$break"		  : false,
			"$continue"		  : false,
			"$w"			  : false,
			Abstract		  : false,
			Ajax			  : false,
			Class			  : false,
			Enumerable		  : false,
			Element			  : false,
			Event			  : false,
			Field			  : false,
			Form			  : false,
			Hash			  : false,
			Insertion		  : false,
			ObjectRange		  : false,
			PeriodicalExecuter: false,
			Position		  : false,
			Prototype		  : false,
			Selector		  : false,
			Template		  : false,
			Toggle			  : false,
			Try				  : false,
			Autocompleter	  : false,
			Builder			  : false,
			Control			  : false,
			Draggable		  : false,
			Draggables		  : false,
			Droppables		  : false,
			Effect			  : false,
			Sortable		  : false,
			SortableObserver  : false,
			Sound			  : false,
			Scriptaculous	  : false
		},

		quotmark,

		rhino = {
			defineClass  : false,
			deserialize  : false,
			gc			 : false,
			help		 : false,
			importPackage: false,
			"java"		 : false,
			load		 : false,
			loadClass	 : false,
			print		 : false,
			quit		 : false,
			readFile	 : false,
			readUrl		 : false,
			runCommand	 : false,
			seal		 : false,
			serialize	 : false,
			spawn		 : false,
			sync		 : false,
			toint32		 : false,
			version		 : false
		},

		scope,		// The current scope
		stack,
		standard = {
			Array				: false,
			Boolean				: false,
			Date				: false,
			decodeURI			: false,
			decodeURIComponent	: false,
			encodeURI			: false,
			encodeURIComponent	: false,
			Error				: false,
			"eval"				: false,
			EvalError			: false,
			Function			: false,
			hasOwnProperty		: false,
			isFinite			: false,
			isNaN				: false,
			JSON				: false,
			Map					: false,
			Math				: false,
			NaN					: false,
			Number				: false,
			Object				: false,
			parseInt			: false,
			parseFloat			: false,
			RangeError			: false,
			ReferenceError		: false,
			RegExp				: false,
			Set					: false,
			String				: false,
			SyntaxError			: false,
			TypeError			: false,
			URIError			: false,
			WeakMap				: false
		},
		nonstandard = {
			escape				: false,
			unescape			: false
		},

		directive,
		syntax = {},
		tab,
		token,
		unuseds,
		urls,
		useESNextSyntax,
		warnings,

		worker = {
			importScripts		: true,
			postMessage			: true,
			self				: true
		},

		wsh = {
			ActiveXObject			  : true,
			Enumerator				  : true,
			GetObject				  : true,
			ScriptEngine			  : true,
			ScriptEngineBuildVersion  : true,
			ScriptEngineMajorVersion  : true,
			ScriptEngineMinorVersion  : true,
			VBArray					  : true,
			WSH						  : true,
			WScript					  : true,
			XDomainRequest			  : true
		},

		yui = {
			YUI				: false,
			Y				: false,
			YUI_config		: false
		};
	var ax, cx, tx, nx, nxg, lx, ix, jx, ft;
	(function () {
		ax = /@cc|<\/?|script|\]\s*\]|<\s*!|&lt/i;
		cx = /[\u0000-\u001f\u007f-\u009f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/;
		tx = /^\s*([(){}\[.,:;'"~\?\]#@]|==?=?|\/=(?!(\S*\/[gim]?))|\/(\*(jshint|jslint|members?|global)?|\/)?|\*[\/=]?|\+(?:=|\++)?|-(?:=|-+)?|%=?|&[&=]?|\|[|=]?|>>?>?=?|<([\/=!]|\!(\[|--)?|<=?)?|\^=?|\!=?=?|[a-zA-Z_$][a-zA-Z0-9_$]*|[0-9]+([xX][0-9a-fA-F]+|\.[0-9]*)?([eE][+\-]?[0-9]+)?)/;
		nx = /[\u0000-\u001f&<"\/\\\u007f-\u009f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/;
		nxg = /[\u0000-\u001f&<"\/\\\u007f-\u009f\u00ad\u0600-\u0604\u070f\u17b4\u17b5\u200c-\u200f\u2028-\u202f\u2060-\u206f\ufeff\ufff0-\uffff]/g;
		lx = /\*\//;
		ix = /^([a-zA-Z_$][a-zA-Z0-9_$]*)$/;
		jx = /^(?:javascript|jscript|ecmascript|vbscript|mocha|livescript)\s*:/i;
		ft = /^\s*\/\*\s*falls\sthrough\s*\*\/\s*$/;
	}());

	function F() {}		// Used by Object.create

	function is_own(object, name) {
		return Object.prototype.hasOwnProperty.call(object, name);
	}

	function checkOption(name, t) {
		if (valOptions[name] === undefined && boolOptions[name] === undefined) {
			warning("Bad option: '" + name + "'.", t);
		}
	}

	function isString(obj) {
		return Object.prototype.toString.call(obj) === "[object String]";
	}

	if (typeof Array.isArray !== "function") {
		Array.isArray = function (o) {
			return Object.prototype.toString.apply(o) === "[object Array]";
		};
	}

	if (!Array.prototype.forEach) {
		Array.prototype.forEach = function (fn, scope) {
			var len = this.length;

			for (var i = 0; i < len; i++) {
				fn.call(scope || this, this[i], i, this);
			}
		};
	}

	if (!Array.prototype.indexOf) {
		Array.prototype.indexOf = function (searchElement /*, fromIndex */ ) {
			if (this === null || this === undefined) {
				throw new TypeError();
			}

			var t = new Object(this);
			var len = t.length >>> 0;

			if (len === 0) {
				return -1;
			}

			var n = 0;
			if (arguments.length > 0) {
				n = Number(arguments[1]);
				if (n != n) { // shortcut for verifying if it's NaN
					n = 0;
				} else if (n !== 0 && n != Infinity && n != -Infinity) {
					n = (n > 0 || -1) * Math.floor(Math.abs(n));
				}
			}

			if (n >= len) {
				return -1;
			}

			var k = n >= 0 ? n : Math.max(len - Math.abs(n), 0);
			for (; k < len; k++) {
				if (k in t && t[k] === searchElement) {
					return k;
				}
			}

			return -1;
		};
	}

	if (typeof Object.create !== "function") {
		Object.create = function (o) {
			F.prototype = o;
			return new F();
		};
	}

	if (typeof Object.keys !== "function") {
		Object.keys = function (o) {
			var a = [], k;
			for (k in o) {
				if (is_own(o, k)) {
					a.push(k);
				}
			}
			return a;
		};
	}

	function isAlpha(str) {
		return (str >= "a" && str <= "z\uffff") ||
			(str >= "A" && str <= "Z\uffff");
	}

	function isDigit(str) {
		return (str >= "0" && str <= "9");
	}

	function isIdentifier(token, value) {
		if (!token)
			return false;

		if (!token.identifier || token.value !== value)
			return false;

		return true;
	}

	function supplant(str, data) {
		return str.replace(/\{([^{}]*)\}/g, function (a, b) {
			var r = data[b];
			return typeof r === "string" || typeof r === "number" ? r : a;
		});
	}

	function combine(t, o) {
		var n;
		for (n in o) {
			if (is_own(o, n) && !is_own(JSHINT.blacklist, n)) {
				t[n] = o[n];
			}
		}
	}

	function updatePredefined() {
		Object.keys(JSHINT.blacklist).forEach(function (key) {
			delete predefined[key];
		});
	}

	function assume() {
		if (option.couch) {
			combine(predefined, couch);
		}

		if (option.rhino) {
			combine(predefined, rhino);
		}

		if (option.prototypejs) {
			combine(predefined, prototypejs);
		}

		if (option.node) {
			combine(predefined, node);
			option.globalstrict = true;
		}

		if (option.devel) {
			combine(predefined, devel);
		}

		if (option.dojo) {
			combine(predefined, dojo);
		}

		if (option.browser) {
			combine(predefined, browser);
		}

		if (option.nonstandard) {
			combine(predefined, nonstandard);
		}

		if (option.jquery) {
			combine(predefined, jquery);
		}

		if (option.mootools) {
			combine(predefined, mootools);
		}

		if (option.worker) {
			combine(predefined, worker);
		}

		if (option.wsh) {
			combine(predefined, wsh);
		}

		if (option.esnext) {
			useESNextSyntax();
		}

		if (option.globalstrict && option.strict !== false) {
			option.strict = true;
		}

		if (option.yui) {
			combine(predefined, yui);
		}
	}
	function quit(message, line, chr) {
		var percentage = Math.floor((line / lines.length) * 100);

		throw {
			name: "JSHintError",
			line: line,
			character: chr,
			message: message + " (" + percentage + "% scanned).",
			raw: message
		};
	}

	function isundef(scope, m, t, a) {
		return JSHINT.undefs.push([scope, m, t, a]);
	}

	function warning(m, t, a, b, c, d) {
		var ch, l, w;
		t = t || nexttoken;
		if (t.id === "(end)") {  // `~
			t = token;
		}
		l = t.line || 0;
		ch = t.from || 0;
		w = {
			id: "(error)",
			raw: m,
			evidence: lines[l - 1] || "",
			line: l,
			character: ch,
			scope: JSHINT.scope,
			a: a,
			b: b,
			c: c,
			d: d
		};
		w.reason = supplant(m, w);
		JSHINT.errors.push(w);
		if (option.passfail) {
			quit("Stopping. ", l, ch);
		}
		warnings += 1;
		if (warnings >= option.maxerr) {
			quit("Too many errors.", l, ch);
		}
		return w;
	}

	function warningAt(m, l, ch, a, b, c, d) {
		return warning(m, {
			line: l,
			from: ch
		}, a, b, c, d);
	}

	function error(m, t, a, b, c, d) {
		warning(m, t, a, b, c, d);
	}

	function errorAt(m, l, ch, a, b, c, d) {
		return error(m, {
			line: l,
			from: ch
		}, a, b, c, d);
	}
	function addInternalSrc(elem, src) {
		var i;
		i = {
			id: "(internal)",
			elem: elem,
			value: src
		};
		JSHINT.internals.push(i);
		return i;
	}

	var lex = (function lex() {
		var character, from, line, s;

		function nextLine() {
			var at,
				match,
				tw; // trailing whitespace check

			if (line >= lines.length)
				return false;

			character = 1;
			s = lines[line];
			line += 1;
			if (option.smarttabs) {
				match = s.match(/(\/\/)? \t/);
				at = match && !match[1] ? 0 : -1;
			} else {
				at = s.search(/ \t|\t [^\*]/);
			}

			if (at >= 0)
				warningAt("Mixed spaces and tabs.", line, at + 1);

			s = s.replace(/\t/g, tab);
			at = s.search(cx);

			if (at >= 0)
				warningAt("Unsafe character.", line, at);

			if (option.maxlen && option.maxlen < s.length)
				warningAt("Line too long.", line, s.length);
			tw = option.trailing && s.match(/^(.*?)\s+$/);
			if (tw && !/^\s+$/.test(s)) {
				warningAt("Trailing whitespace.", line, tw[1].length + 1);
			}
			return true;
		}

		function it(type, value) {
			var i, t;

			function checkName(name) {
				if (!option.proto && name === "__proto__") {
					warningAt("The '{a}' property is deprecated.", line, from, name);
					return;
				}

				if (!option.iterator && name === "__iterator__") {
					warningAt("'{a}' is only available in JavaScript 1.7.", line, from, name);
					return;
				}

				var hasDangling = /^(_+.*|.*_+)$/.test(name);

				if (option.nomen && hasDangling && name !== "_") {
					if (option.node && token.id !== "." && /^(__dirname|__filename)$/.test(name))
						return;

					warningAt("Unexpected {a} in '{b}'.", line, from, "dangling '_'", name);
					return;
				}

				if (option.camelcase) {
					if (name.replace(/^_+/, "").indexOf("_") > -1 && !name.match(/^[A-Z0-9_]*$/)) {
						warningAt("Identifier '{a}' is not in camel case.", line, from, value);
					}
				}
			}

			if (type === "(color)" || type === "(range)") {
				t = {type: type};
			} else if (type === "(punctuator)" ||
					(type === "(identifier)" && is_own(syntax, value))) {
				t = syntax[value] || syntax["(error)"];
			} else {
				t = syntax[type];
			}

			t = Object.create(t);

			if (type === "(string)" || type === "(range)") {
				if (!option.scripturl && jx.test(value)) {
					warningAt("Script URL.", line, from);
				}
			}

			if (type === "(identifier)") {
				t.identifier = true;
				checkName(value);
			}

			t.value = value;
			t.line = line;
			t.character = character;
			t.from = from;
			i = t.id;
			if (i !== "(endline)") {
				prereg = i &&
					(("(,=:[!&|?{};".indexOf(i.charAt(i.length - 1)) >= 0) ||
					i === "return" ||
					i === "case");
			}
			return t;
		}
		return {
			init: function (source) {
				if (typeof source === "string") {
					lines = source
						.replace(/\r\n/g, "\n")
						.replace(/\r/g, "\n")
						.split("\n");
				} else {
					lines = source;
				}
				if (lines[0] && lines[0].substr(0, 2) === "#!")
					lines[0] = "";

				line = 0;
				nextLine();
				from = 1;
			},

			range: function (begin, end) {
				var c, value = "";
				from = character;
				if (s.charAt(0) !== begin) {
					errorAt("Expected '{a}' and instead saw '{b}'.",
							line, character, begin, s.charAt(0));
				}
				for (;;) {
					s = s.slice(1);
					character += 1;
					c = s.charAt(0);
					switch (c) {
					case "":
						errorAt("Missing '{a}'.", line, character, c);
						break;
					case end:
						s = s.slice(1);
						character += 1;
						return it("(range)", value);
					case "\\":
						warningAt("Unexpected '{a}'.", line, character, c);
					}
					value += c;
				}

			},
			token: function () {
				var b, c, captures, d, depth, high, i, l, low, q, t, isLiteral, isInRange, n;

				function match(x) {
					var r = x.exec(s), r1;

					if (r) {
						l = r[0].length;
						r1 = r[1];
						c = r1.charAt(0);
						s = s.substr(l);
						from = character + l - r1.length;
						character += l;
						return r1;
					}
				}

				function string(x) {
					var c, j, r = "", allowNewLine = false;

					if (jsonmode && x !== "\"") {
						warningAt("Strings must use doublequote.",
								line, character);
					}

					if (option.quotmark) {
						if (option.quotmark === "single" && x !== "'") {
							warningAt("Strings must use singlequote.",
									line, character);
						} else if (option.quotmark === "double" && x !== "\"") {
							warningAt("Strings must use doublequote.",
									line, character);
						} else if (option.quotmark === true) {
							quotmark = quotmark || x;
							if (quotmark !== x) {
								warningAt("Mixed double and single quotes.",
										line, character);
							}
						}
					}

					function esc(n) {
						var i = parseInt(s.substr(j + 1, n), 16);
						j += n;
						if (i >= 32 && i <= 126 &&
								i !== 34 && i !== 92 && i !== 39) {
							warningAt("Unnecessary escapement.", line, character);
						}
						character += n;
						c = String.fromCharCode(i);
					}

					j = 0;

unclosedString:
					for (;;) {
						while (j >= s.length) {
							j = 0;

							var cl = line, cf = from;
							if (!nextLine()) {
								errorAt("Unclosed string.", cl, cf);
								break unclosedString;
							}

							if (allowNewLine) {
								allowNewLine = false;
							} else {
								warningAt("Unclosed string.", cl, cf);
							}
						}

						c = s.charAt(j);
						if (c === x) {
							character += 1;
							s = s.substr(j + 1);
							return it("(string)", r, x);
						}

						if (c < " ") {
							if (c === "\n" || c === "\r") {
								break;
							}
							warningAt("Control character in string: {a}.",
									line, character + j, s.slice(0, j));
						} else if (c === "\\") {
							j += 1;
							character += 1;
							c = s.charAt(j);
							n = s.charAt(j + 1);
							switch (c) {
							case "\\":
							case "\"":
							case "/":
								break;
							case "\'":
								if (jsonmode) {
									warningAt("Avoid \\'.", line, character);
								}
								break;
							case "b":
								c = "\b";
								break;
							case "f":
								c = "\f";
								break;
							case "n":
								c = "\n";
								break;
							case "r":
								c = "\r";
								break;
							case "t":
								c = "\t";
								break;
							case "0":
								c = "\0";
								if (n >= 0 && n <= 7 && directive["use strict"]) {
									warningAt(
									"Octal literals are not allowed in strict mode.",
									line, character);
								}
								break;
							case "u":
								esc(4);
								break;
							case "v":
								if (jsonmode) {
									warningAt("Avoid \\v.", line, character);
								}
								c = "\v";
								break;
							case "x":
								if (jsonmode) {
									warningAt("Avoid \\x-.", line, character);
								}
								esc(2);
								break;
							case "":
								allowNewLine = true;
								if (option.multistr) {
									if (jsonmode) {
										warningAt("Avoid EOL escapement.", line, character);
									}
									c = "";
									character -= 1;
									break;
								}
								warningAt("Bad escapement of EOL. Use option multistr if needed.",
									line, character);
								break;
							case "!":
								if (s.charAt(j - 2) === "<")
									break;
							default:
								warningAt("Bad escapement.", line, character);
							}
						}
						r += c;
						character += 1;
						j += 1;
					}
				}

				for (;;) {
					if (!s) {
						return it(nextLine() ? "(endline)" : "(end)", "");
					}

					t = match(tx);

					if (!t) {
						t = "";
						c = "";
						while (s && s < "!") {
							s = s.substr(1);
						}
						if (s) {
							errorAt("Unexpected '{a}'.", line, character, s.substr(0, 1));
							s = "";
						}
					} else {

						if (isAlpha(c) || c === "_" || c === "$") {
							return it("(identifier)", t);
						}

						if (isDigit(c)) {
							if (!isFinite(Number(t))) {
								warningAt("Bad number '{a}'.",
									line, character, t);
							}
							if (isAlpha(s.substr(0, 1))) {
								warningAt("Missing space after '{a}'.",
										line, character, t);
							}
							if (c === "0") {
								d = t.substr(1, 1);
								if (isDigit(d)) {
									if (token.id !== ".") {
										warningAt("Don't use extra leading zeros '{a}'.",
											line, character, t);
									}
								} else if (jsonmode && (d === "x" || d === "X")) {
									warningAt("Avoid 0x-. '{a}'.",
											line, character, t);
								}
							}
							if (t.substr(t.length - 1) === ".") {
								warningAt(
"A trailing decimal point can be confused with a dot '{a}'.", line, character, t);
							}
							return it("(number)", t);
						}
						switch (t) {

						case "\"":
						case "'":
							return string(t);

						case "//":
							s = "";
							token.comment = true;
							break;

						case "/*":
							for (;;) {
								i = s.search(lx);
								if (i >= 0) {
									break;
								}
								if (!nextLine()) {
									errorAt("Unclosed comment.", line, character);
								}
							}
							s = s.substr(i + 2);
							token.comment = true;
							break;

						case "/*members":
						case "/*member":
						case "/*jshint":
						case "/*jslint":
						case "/*global":
						case "*/":
							return {
								value: t,
								type: "special",
								line: line,
								character: character,
								from: from
							};

						case "":
							break;
						case "/":
							if (s.charAt(0) === "=") {
								errorAt("A regular expression literal can be confused with '/='.",
									line, from);
							}

							if (prereg) {
								depth = 0;
								captures = 0;
								l = 0;
								for (;;) {
									b = true;
									c = s.charAt(l);
									l += 1;
									switch (c) {
									case "":
										errorAt("Unclosed regular expression.", line, from);
										return quit("Stopping.", line, from);
									case "/":
										if (depth > 0) {
											warningAt("{a} unterminated regular expression " +
												"group(s).", line, from + l, depth);
										}
										c = s.substr(0, l - 1);
										q = {
											g: true,
											i: true,
											m: true
										};
										while (q[s.charAt(l)] === true) {
											q[s.charAt(l)] = false;
											l += 1;
										}
										character += l;
										s = s.substr(l);
										q = s.charAt(0);
										if (q === "/" || q === "*") {
											errorAt("Confusing regular expression.",
													line, from);
										}
										return it("(regexp)", c);
									case "\\":
										c = s.charAt(l);
										if (c < " ") {
											warningAt(
"Unexpected control character in regular expression.", line, from + l);
										} else if (c === "<") {
											warningAt(
"Unexpected escaped character '{a}' in regular expression.", line, from + l, c);
										}
										l += 1;
										break;
									case "(":
										depth += 1;
										b = false;
										if (s.charAt(l) === "?") {
											l += 1;
											switch (s.charAt(l)) {
											case ":":
											case "=":
											case "!":
												l += 1;
												break;
											default:
												warningAt(
"Expected '{a}' and instead saw '{b}'.", line, from + l, ":", s.charAt(l));
											}
										} else {
											captures += 1;
										}
										break;
									case "|":
										b = false;
										break;
									case ")":
										if (depth === 0) {
											warningAt("Unescaped '{a}'.",
													line, from + l, ")");
										} else {
											depth -= 1;
										}
										break;
									case " ":
										q = 1;
										while (s.charAt(l) === " ") {
											l += 1;
											q += 1;
										}
										if (q > 1) {
											warningAt(
"Spaces are hard to count. Use {{a}}.", line, from + l, q);
										}
										break;
									case "[":
										c = s.charAt(l);
										if (c === "^") {
											l += 1;
											if (s.charAt(l) === "]") {
												errorAt("Unescaped '{a}'.",
													line, from + l, "^");
											}
										}
										if (c === "]") {
											warningAt("Empty class.", line,
													from + l - 1);
										}
										isLiteral = false;
										isInRange = false;
klass:
										do {
											c = s.charAt(l);
											l += 1;
											switch (c) {
											case "[":
											case "^":
												warningAt("Unescaped '{a}'.",
														line, from + l, c);
												if (isInRange) {
													isInRange = false;
												} else {
													isLiteral = true;
												}
												break;
											case "-":
												if (isLiteral && !isInRange) {
													isLiteral = false;
													isInRange = true;
												} else if (isInRange) {
													isInRange = false;
												} else if (s.charAt(l) === "]") {
													isInRange = true;
												} else {
													if (option.regexdash !== (l === 2 || (l === 3 &&
														s.charAt(1) === "^"))) {
														warningAt("Unescaped '{a}'.",
															line, from + l - 1, "-");
													}
													isLiteral = true;
												}
												break;
											case "]":
												if (isInRange && !option.regexdash) {
													warningAt("Unescaped '{a}'.",
															line, from + l - 1, "-");
												}
												break klass;
											case "\\":
												c = s.charAt(l);
												if (c < " ") {
													warningAt(
"Unexpected control character in regular expression.", line, from + l);
												} else if (c === "<") {
													warningAt(
"Unexpected escaped character '{a}' in regular expression.", line, from + l, c);
												}
												l += 1;
												if (/[wsd]/i.test(c)) {
													if (isInRange) {
														warningAt("Unescaped '{a}'.",
															line, from + l, "-");
														isInRange = false;
													}
													isLiteral = false;
												} else if (isInRange) {
													isInRange = false;
												} else {
													isLiteral = true;
												}
												break;
											case "/":
												warningAt("Unescaped '{a}'.",
														line, from + l - 1, "/");

												if (isInRange) {
													isInRange = false;
												} else {
													isLiteral = true;
												}
												break;
											case "<":
												if (isInRange) {
													isInRange = false;
												} else {
													isLiteral = true;
												}
												break;
											default:
												if (isInRange) {
													isInRange = false;
												} else {
													isLiteral = true;
												}
											}
										} while (c);
										break;
									case ".":
										if (option.regexp) {
											warningAt("Insecure '{a}'.", line,
													from + l, c);
										}
										break;
									case "]":
									case "?":
									case "{":
									case "}":
									case "+":
									case "*":
										warningAt("Unescaped '{a}'.", line,
												from + l, c);
									}
									if (b) {
										switch (s.charAt(l)) {
										case "?":
										case "+":
										case "*":
											l += 1;
											if (s.charAt(l) === "?") {
												l += 1;
											}
											break;
										case "{":
											l += 1;
											c = s.charAt(l);
											if (c < "0" || c > "9") {
												warningAt(
"Expected a number and instead saw '{a}'.", line, from + l, c);
												break; // No reason to continue checking numbers.
											}
											l += 1;
											low = +c;
											for (;;) {
												c = s.charAt(l);
												if (c < "0" || c > "9") {
													break;
												}
												l += 1;
												low = +c + (low * 10);
											}
											high = low;
											if (c === ",") {
												l += 1;
												high = Infinity;
												c = s.charAt(l);
												if (c >= "0" && c <= "9") {
													l += 1;
													high = +c;
													for (;;) {
														c = s.charAt(l);
														if (c < "0" || c > "9") {
															break;
														}
														l += 1;
														high = +c + (high * 10);
													}
												}
											}
											if (s.charAt(l) !== "}") {
												warningAt(
"Expected '{a}' and instead saw '{b}'.", line, from + l, "}", c);
											} else {
												l += 1;
											}
											if (s.charAt(l) === "?") {
												l += 1;
											}
											if (low > high) {
												warningAt(
"'{a}' should not be greater than '{b}'.", line, from + l, low, high);
											}
										}
									}
								}
								c = s.substr(0, l - 1);
								character += l;
								s = s.substr(l);
								return it("(regexp)", c);
							}
							return it("(punctuator)", t);

						case "#":
							return it("(punctuator)", t);
						default:
							return it("(punctuator)", t);
						}
					}
				}
			}
		};
	}());


	function addlabel(t, type, token) {
		if (t === "hasOwnProperty") {
			warning("'hasOwnProperty' is a really bad name.");
		}
		if (type === "exception") {
			if (is_own(funct["(context)"], t)) {
				if (funct[t] !== true && !option.node) {
					warning("Value of '{a}' may be overwritten in IE.", nexttoken, t);
				}
			}
		}

		if (is_own(funct, t) && !funct["(global)"]) {
			if (funct[t] === true) {
				if (option.latedef)
					warning("'{a}' was used before it was defined.", nexttoken, t);
			} else {
				if (!option.shadow && type !== "exception") {
					warning("'{a}' is already defined.", nexttoken, t);
				}
			}
		}

		funct[t] = type;

		if (token) {
			funct["(tokens)"][t] = token;
		}

		if (funct["(global)"]) {
			global[t] = funct;
			if (is_own(implied, t)) {
				if (option.latedef)
					warning("'{a}' was used before it was defined.", nexttoken, t);
				delete implied[t];
			}
		} else {
			scope[t] = funct;
		}
	}


	function doOption() {
		var nt = nexttoken;
		var o  = nt.value;
		var quotmarkValue = option.quotmark;
		var predef = {};
		var b, obj, filter, t, tn, v, minus;

		switch (o) {
		case "*/":
			error("Unbegun comment.");
			break;
		case "/*members":
		case "/*member":
			o = "/*members";
			if (!membersOnly) {
				membersOnly = {};
			}
			obj = membersOnly;
			option.quotmark = false;
			break;
		case "/*jshint":
		case "/*jslint":
			obj = option;
			filter = boolOptions;
			break;
		case "/*global":
			obj = predef;
			break;
		default:
			error("What?");
		}

		t = lex.token();

loop:
		for (;;) {
			minus = false;
			for (;;) {
				if (t.type === "special" && t.value === "*/") {
					break loop;
				}
				if (t.id !== "(endline)" && t.id !== ",") {
					break;
				}
				t = lex.token();
			}

			if (o === "/*global" && t.value === "-") {
				minus = true;
				t = lex.token();
			}

			if (t.type !== "(string)" && t.type !== "(identifier)" && o !== "/*members") {
				error("Bad option.", t);
			}

			v = lex.token();
			if (v.id === ":") {
				v = lex.token();

				if (obj === membersOnly) {
					error("Expected '{a}' and instead saw '{b}'.", t, "*/", ":");
				}

				if (o === "/*jshint") {
					checkOption(t.value, t);
				}

				var numericVals = [
					"maxstatements",
					"maxparams",
					"maxdepth",
					"maxcomplexity",
					"maxerr",
					"maxlen",
					"indent"
				];

				if (numericVals.indexOf(t.value) > -1 && (o === "/*jshint" || o === "/*jslint")) {
					b = +v.value;

					if (typeof b !== "number" || !isFinite(b) || b <= 0 || Math.floor(b) !== b) {
						error("Expected a small integer and instead saw '{a}'.", v, v.value);
					}

					if (t.value === "indent")
						obj.white = true;

					obj[t.value] = b;
				} else if (t.value === "validthis") {
					if (funct["(global)"]) {
						error("Option 'validthis' can't be used in a global scope.");
					} else {
						if (v.value === "true" || v.value === "false")
							obj[t.value] = v.value === "true";
						else
							error("Bad option value.", v);
					}
				} else if (t.value === "quotmark" && (o === "/*jshint")) {
					switch (v.value) {
					case "true":
						obj.quotmark = true;
						break;
					case "false":
						obj.quotmark = false;
						break;
					case "double":
					case "single":
						obj.quotmark = v.value;
						break;
					default:
						error("Bad option value.", v);
					}
				} else if (v.value === "true" || v.value === "false") {
					if (o === "/*jslint") {
						tn = renamedOptions[t.value] || t.value;
						obj[tn] = v.value === "true";
						if (invertedOptions[tn] !== undefined) {
							obj[tn] = !obj[tn];
						}
					} else {
						obj[t.value] = v.value === "true";
					}

					if (t.value === "newcap")
						obj["(explicitNewcap)"] = true;
				} else {
					error("Bad option value.", v);
				}
				t = lex.token();
			} else {
				if (o === "/*jshint" || o === "/*jslint") {
					error("Missing option value.", t);
				}

				obj[t.value] = false;

				if (o === "/*global" && minus === true) {
					JSHINT.blacklist[t.value] = t.value;
					updatePredefined();
				}

				t = v;
			}
		}

		if (o === "/*members") {
			option.quotmark = quotmarkValue;
		}

		combine(predefined, predef);

		for (var key in predef) {
			if (is_own(predef, key)) {
				declared[key] = nt;
			}
		}

		if (filter) {
			assume();
		}
	}

	function peek(p) {
		var i = p || 0, j = 0, t;

		while (j <= i) {
			t = lookahead[j];
			if (!t) {
				t = lookahead[j] = lex.token();
			}
			j += 1;
		}
		return t;
	}

	function advance(id, t) {
		switch (token.id) {
		case "(number)":
			if (nexttoken.id === ".") {
				warning("A dot following a number can be confused with a decimal point.", token);
			}
			break;
		case "-":
			if (nexttoken.id === "-" || nexttoken.id === "--") {
				warning("Confusing minusses.");
			}
			break;
		case "+":
			if (nexttoken.id === "+" || nexttoken.id === "++") {
				warning("Confusing plusses.");
			}
			break;
		}

		if (token.type === "(string)" || token.identifier) {
			anonname = token.value;
		}

		if (id && nexttoken.id !== id) {
			if (t) {
				if (nexttoken.id === "(end)") {
					warning("Unmatched '{a}'.", t, t.id);
				} else {
					warning("Expected '{a}' to match '{b}' from line {c} and instead saw '{d}'.",
							nexttoken, id, t.id, t.line, nexttoken.value);
				}
			} else if (nexttoken.type !== "(identifier)" ||
							nexttoken.value !== id) {
				warning("Expected '{a}' and instead saw '{b}'.",
						nexttoken, id, nexttoken.value);
			}
		}

		prevtoken = token;
		token = nexttoken;
		for (;;) {
			nexttoken = lookahead.shift() || lex.token();
			if (nexttoken.id === "(end)" || nexttoken.id === "(error)") {
				return;
			}
			if (nexttoken.type === "special") {
				doOption();
			} else {
				if (nexttoken.id !== "(endline)") {
					break;
				}
			}
		}
	}

	function expression(rbp, initial) {
		var left, isArray = false, isObject = false;

		if (nexttoken.id === "(end)")
			error("Unexpected early end of program.", token);

		advance();
		if (initial) {
			anonname = "anonymous";
			funct["(verb)"] = token.value;
		}
		if (initial === true && token.fud) {
			left = token.fud();
		} else {
			if (token.nud) {
				left = token.nud();
			} else {
				if (nexttoken.type === "(number)" && token.id === ".") {
					warning("A leading decimal point can be confused with a dot: '.{a}'.",
							token, nexttoken.value);
					advance();
					return token;
				} else {
					error("Expected an identifier and instead saw '{a}'.",
							token, token.id);
				}
			}
			while (rbp < nexttoken.lbp) {
				isArray = token.value === "Array";
				isObject = token.value === "Object";
				if (left && (left.value || (left.first && left.first.value))) {
					if (left.value !== "new" ||
					  (left.first && left.first.value && left.first.value === ".")) {
						isArray = false;
						if (left.value !== token.value) {
							isObject = false;
						}
					}
				}

				advance();
				if (isArray && token.id === "(" && nexttoken.id === ")")
					warning("Use the array literal notation [].", token);
				if (isObject && token.id === "(" && nexttoken.id === ")")
					warning("Use the object literal notation {}.", token);
				if (token.led) {
					left = token.led(left);
				} else {
					error("Expected an operator and instead saw '{a}'.",
						token, token.id);
				}
			}
		}
		return left;
	}

	function adjacent(left, right) {
		left = left || token;
		right = right || nexttoken;
		if (option.white) {
			if (left.character !== right.from && left.line === right.line) {
				left.from += (left.character - left.from);
				warning("Unexpected space after '{a}'.", left, left.value);
			}
		}
	}

	function nobreak(left, right) {
		left = left || token;
		right = right || nexttoken;
		if (option.white && (left.character !== right.from || left.line !== right.line)) {
			warning("Unexpected space before '{a}'.", right, right.value);
		}
	}

	function nospace(left, right) {
		left = left || token;
		right = right || nexttoken;
		if (option.white && !left.comment) {
			if (left.line === right.line) {
				adjacent(left, right);
			}
		}
	}

	function nonadjacent(left, right) {
		if (option.white) {
			left = left || token;
			right = right || nexttoken;
			if (left.value === ";" && right.value === ";") {
				return;
			}
			if (left.line === right.line && left.character === right.from) {
				left.from += (left.character - left.from);
				warning("Missing space after '{a}'.",
						left, left.value);
			}
		}
	}

	function nobreaknonadjacent(left, right) {
		left = left || token;
		right = right || nexttoken;
		if (!option.laxbreak && left.line !== right.line) {
			warning("Bad line breaking before '{a}'.", right, right.id);
		} else if (option.white) {
			left = left || token;
			right = right || nexttoken;
			if (left.character === right.from) {
				left.from += (left.character - left.from);
				warning("Missing space after '{a}'.",
						left, left.value);
			}
		}
	}

	function indentation(bias) {
		var i;
		if (option.white && nexttoken.id !== "(end)") {
			i = indent + (bias || 0);
			if (nexttoken.from !== i) {
				warning(
"Expected '{a}' to have an indentation at {b} instead at {c}.",
						nexttoken, nexttoken.value, i, nexttoken.from);
			}
		}
	}

	function nolinebreak(t) {
		t = t || token;
		if (t.line !== nexttoken.line) {
			warning("Line breaking error '{a}'.", t, t.value);
		}
	}


	function comma() {
		if (token.line !== nexttoken.line) {
			if (!option.laxcomma) {
				if (comma.first) {
					warning("Comma warnings can be turned off with 'laxcomma'");
					comma.first = false;
				}
				warning("Bad line breaking before '{a}'.", token, nexttoken.id);
			}
		} else if (!token.comment && token.character !== nexttoken.from && option.white) {
			token.from += (token.character - token.from);
			warning("Unexpected space after '{a}'.", token, token.value);
		}
		advance(",");
		nonadjacent(token, nexttoken);
	}

	function symbol(s, p) {
		var x = syntax[s];
		if (!x || typeof x !== "object") {
			syntax[s] = x = {
				id: s,
				lbp: p,
				value: s
			};
		}
		return x;
	}


	function delim(s) {
		return symbol(s, 0);
	}


	function stmt(s, f) {
		var x = delim(s);
		x.identifier = x.reserved = true;
		x.fud = f;
		return x;
	}


	function blockstmt(s, f) {
		var x = stmt(s, f);
		x.block = true;
		return x;
	}


	function reserveName(x) {
		var c = x.id.charAt(0);
		if ((c >= "a" && c <= "z") || (c >= "A" && c <= "Z")) {
			x.identifier = x.reserved = true;
		}
		return x;
	}


	function prefix(s, f) {
		var x = symbol(s, 150);
		reserveName(x);
		x.nud = (typeof f === "function") ? f : function () {
			this.right = expression(150);
			this.arity = "unary";
			if (this.id === "++" || this.id === "--") {
				if (option.plusplus) {
					warning("Unexpected use of '{a}'.", this, this.id);
				} else if ((!this.right.identifier || this.right.reserved) &&
						this.right.id !== "." && this.right.id !== "[") {
					warning("Bad operand.", this);
				}
			}
			return this;
		};
		return x;
	}


	function type(s, f) {
		var x = delim(s);
		x.type = s;
		x.nud = f;
		return x;
	}


	function reserve(s, f) {
		var x = type(s, f);
		x.identifier = x.reserved = true;
		return x;
	}


	function reservevar(s, v) {
		return reserve(s, function () {
			if (typeof v === "function") {
				v(this);
			}
			return this;
		});
	}


	function infix(s, f, p, w) {
		var x = symbol(s, p);
		reserveName(x);
		x.led = function (left) {
			if (!w) {
				nobreaknonadjacent(prevtoken, token);
				nonadjacent(token, nexttoken);
			}
			if (s === "in" && left.id === "!") {
				warning("Confusing use of '{a}'.", left, "!");
			}
			if (typeof f === "function") {
				return f(left, this);
			} else {
				this.left = left;
				this.right = expression(p);
				return this;
			}
		};
		return x;
	}


	function relation(s, f) {
		var x = symbol(s, 100);
		x.led = function (left) {
			nobreaknonadjacent(prevtoken, token);
			nonadjacent(token, nexttoken);
			var right = expression(100);

			if (isIdentifier(left, "NaN") || isIdentifier(right, "NaN")) {
				warning("Use the isNaN function to compare with NaN.", this);
			} else if (f) {
				f.apply(this, [left, right]);
			}
			if (left.id === "!") {
				warning("Confusing use of '{a}'.", left, "!");
			}
			if (right.id === "!") {
				warning("Confusing use of '{a}'.", right, "!");
			}
			this.left = left;
			this.right = right;
			return this;
		};
		return x;
	}


	function isPoorRelation(node) {
		return node &&
			  ((node.type === "(number)" && +node.value === 0) ||
			   (node.type === "(string)" && node.value === "") ||
			   (node.type === "null" && !option.eqnull) ||
				node.type === "true" ||
				node.type === "false" ||
				node.type === "undefined");
	}


	function assignop(s) {
		symbol(s, 20).exps = true;

		return infix(s, function (left, that) {
			that.left = left;

			if (predefined[left.value] === false &&
					scope[left.value]["(global)"] === true) {
				warning("Read only.", left);
			} else if (left["function"]) {
				warning("'{a}' is a function.", left, left.value);
			}

			if (left) {
				if (option.esnext && funct[left.value] === "const") {
					warning("Attempting to override '{a}' which is a constant", left, left.value);
				}

				if (left.id === "." || left.id === "[") {
					if (!left.left || left.left.value === "arguments") {
						warning("Bad assignment.", that);
					}
					that.right = expression(19);
					return that;
				} else if (left.identifier && !left.reserved) {
					if (funct[left.value] === "exception") {
						warning("Do not assign to the exception parameter.", left);
					}
					that.right = expression(19);
					return that;
				}

				if (left === syntax["function"]) {
					warning(
"Expected an identifier in an assignment and instead saw a function invocation.",
								token);
				}
			}

			error("Bad assignment.", that);
		}, 20);
	}


	function bitwise(s, f, p) {
		var x = symbol(s, p);
		reserveName(x);
		x.led = (typeof f === "function") ? f : function (left) {
			if (option.bitwise) {
				warning("Unexpected use of '{a}'.", this, this.id);
			}
			this.left = left;
			this.right = expression(p);
			return this;
		};
		return x;
	}


	function bitwiseassignop(s) {
		symbol(s, 20).exps = true;
		return infix(s, function (left, that) {
			if (option.bitwise) {
				warning("Unexpected use of '{a}'.", that, that.id);
			}
			nonadjacent(prevtoken, token);
			nonadjacent(token, nexttoken);
			if (left) {
				if (left.id === "." || left.id === "[" ||
						(left.identifier && !left.reserved)) {
					expression(19);
					return that;
				}
				if (left === syntax["function"]) {
					warning(
"Expected an identifier in an assignment, and instead saw a function invocation.",
								token);
				}
				return that;
			}
			error("Bad assignment.", that);
		}, 20);
	}


	function suffix(s) {
		var x = symbol(s, 150);
		x.led = function (left) {
			if (option.plusplus) {
				warning("Unexpected use of '{a}'.", this, this.id);
			} else if ((!left.identifier || left.reserved) &&
					left.id !== "." && left.id !== "[") {
				warning("Bad operand.", this);
			}
			this.left = left;
			return this;
		};
		return x;
	}
	function optionalidentifier(fnparam) {
		if (nexttoken.identifier) {
			advance();
			if (token.reserved && !option.es5) {
				if (!fnparam || token.value !== "undefined") {
					warning("Expected an identifier and instead saw '{a}' (a reserved word).",
							token, token.id);
				}
			}
			return token.value;
		}
	}
	function identifier(fnparam) {
		var i = optionalidentifier(fnparam);
		if (i) {
			return i;
		}
		if (token.id === "function" && nexttoken.id === "(") {
			warning("Missing name in function declaration.");
		} else {
			error("Expected an identifier and instead saw '{a}'.",
					nexttoken, nexttoken.value);
		}
	}


	function reachable(s) {
		var i = 0, t;
		if (nexttoken.id !== ";" || noreach) {
			return;
		}
		for (;;) {
			t = peek(i);
			if (t.reach) {
				return;
			}
			if (t.id !== "(endline)") {
				if (t.id === "function") {
					if (!option.latedef) {
						break;
					}
					warning(
"Inner functions should be listed at the top of the outer function.", t);
					break;
				}
				warning("Unreachable '{a}' after '{b}'.", t, t.value, s);
				break;
			}
			i += 1;
		}
	}


	function statement(noindent) {
		var i = indent, r, s = scope, t = nexttoken;

		if (t.id === ";") {
			advance(";");
			return;
		}

		if (t.identifier && !t.reserved && peek().id === ":") {
			advance();
			advance(":");
			scope = Object.create(s);
			addlabel(t.value, "label");

			if (!nexttoken.labelled && nexttoken.value !== "{") {
				warning("Label '{a}' on {b} statement.", nexttoken, t.value, nexttoken.value);
			}

			if (jx.test(t.value + ":")) {
				warning("Label '{a}' looks like a javascript url.", t, t.value);
			}

			nexttoken.label = t.value;
			t = nexttoken;
		}

		if (t.id === "{") {
			block(true, true);
			return;
		}

		if (!noindent) {
			indentation();
		}
		r = expression(0, true);

		if (!t.block) {
			if (!option.expr && (!r || !r.exps)) {
				warning("Expected an assignment or function call and instead saw an expression.",
					token);
			} else if (option.nonew && r.id === "(" && r.left.id === "new") {
				warning("Do not use 'new' for side effects.", t);
			}

			if (nexttoken.id === ",") {
				return comma();
			}

			if (nexttoken.id !== ";") {
				if (!option.asi) {
					if (!option.lastsemic || nexttoken.id !== "}" ||
							nexttoken.line !== token.line) {
						warningAt("Missing semicolon.", token.line, token.character);
					}
				}
			} else {
				adjacent(token, nexttoken);
				advance(";");
				nonadjacent(token, nexttoken);
			}
		}

		indent = i;
		scope = s;
		return r;
	}


	function statements(startLine) {
		var a = [], p;

		while (!nexttoken.reach && nexttoken.id !== "(end)") {
			if (nexttoken.id === ";") {
				p = peek();
				if (!p || p.id !== "(") {
					warning("Unnecessary semicolon.");
				}
				advance(";");
			} else {
				a.push(statement(startLine === nexttoken.line));
			}
		}
		return a;
	}
	function directives() {
		var i, p, pn;

		for (;;) {
			if (nexttoken.id === "(string)") {
				p = peek(0);
				if (p.id === "(endline)") {
					i = 1;
					do {
						pn = peek(i);
						i = i + 1;
					} while (pn.id === "(endline)");

					if (pn.id !== ";") {
						if (pn.id !== "(string)" && pn.id !== "(number)" &&
							pn.id !== "(regexp)" && pn.identifier !== true &&
							pn.id !== "}") {
							break;
						}
						warning("Missing semicolon.", nexttoken);
					} else {
						p = pn;
					}
				} else if (p.id === "}") {
					warning("Missing semicolon.", p);
				} else if (p.id !== ";") {
					break;
				}

				indentation();
				advance();
				if (directive[token.value]) {
					warning("Unnecessary directive \"{a}\".", token, token.value);
				}

				if (token.value === "use strict") {
					if (!option["(explicitNewcap)"])
						option.newcap = true;
					option.undef = true;
				}
				directive[token.value] = true;

				if (p.id === ";") {
					advance(";");
				}
				continue;
			}
			break;
		}
	}
	function block(ordinary, stmt, isfunc) {
		var a,
			b = inblock,
			old_indent = indent,
			m,
			s = scope,
			t,
			line,
			d;

		inblock = ordinary;

		if (!ordinary || !option.funcscope)
			scope = Object.create(scope);

		nonadjacent(token, nexttoken);
		t = nexttoken;

		var metrics = funct["(metrics)"];
		metrics.nestedBlockDepth += 1;
		metrics.verifyMaxNestedBlockDepthPerFunction();

		if (nexttoken.id === "{") {
			advance("{");
			line = token.line;
			if (nexttoken.id !== "}") {
				indent += option.indent;
				while (!ordinary && nexttoken.from > indent) {
					indent += option.indent;
				}

				if (isfunc) {
					m = {};
					for (d in directive) {
						if (is_own(directive, d)) {
							m[d] = directive[d];
						}
					}
					directives();

					if (option.strict && funct["(context)"]["(global)"]) {
						if (!m["use strict"] && !directive["use strict"]) {
							warning("Missing \"use strict\" statement.");
						}
					}
				}

				a = statements(line);

				metrics.statementCount += a.length;

				if (isfunc) {
					directive = m;
				}

				indent -= option.indent;
				if (line !== nexttoken.line) {
					indentation();
				}
			} else if (line !== nexttoken.line) {
				indentation();
			}
			advance("}", t);
			indent = old_indent;
		} else if (!ordinary) {
			error("Expected '{a}' and instead saw '{b}'.",
				  nexttoken, "{", nexttoken.value);
		} else {
			if (!stmt || option.curly)
				warning("Expected '{a}' and instead saw '{b}'.",
						nexttoken, "{", nexttoken.value);

			noreach = true;
			indent += option.indent;
			a = [statement(nexttoken.line === token.line)];
			indent -= option.indent;
			noreach = false;
		}
		funct["(verb)"] = null;
		if (!ordinary || !option.funcscope) scope = s;
		inblock = b;
		if (ordinary && option.noempty && (!a || a.length === 0)) {
			warning("Empty block.");
		}
		metrics.nestedBlockDepth -= 1;
		return a;
	}


	function countMember(m) {
		if (membersOnly && typeof membersOnly[m] !== "boolean") {
			warning("Unexpected /*member '{a}'.", token, m);
		}
		if (typeof member[m] === "number") {
			member[m] += 1;
		} else {
			member[m] = 1;
		}
	}


	function note_implied(token) {
		var name = token.value, line = token.line, a = implied[name];
		if (typeof a === "function") {
			a = false;
		}

		if (!a) {
			a = [line];
			implied[name] = a;
		} else if (a[a.length - 1] !== line) {
			a.push(line);
		}
	}

	type("(number)", function () {
		return this;
	});

	type("(string)", function () {
		return this;
	});

	syntax["(identifier)"] = {
		type: "(identifier)",
		lbp: 0,
		identifier: true,
		nud: function () {
			var v = this.value,
				s = scope[v],
				f;

			if (typeof s === "function") {
				s = undefined;
			} else if (typeof s === "boolean") {
				f = funct;
				funct = functions[0];
				addlabel(v, "var");
				s = funct;
				funct = f;
			}
			if (funct === s) {
				switch (funct[v]) {
				case "unused":
					funct[v] = "var";
					break;
				case "unction":
					funct[v] = "function";
					this["function"] = true;
					break;
				case "function":
					this["function"] = true;
					break;
				case "label":
					warning("'{a}' is a statement label.", token, v);
					break;
				}
			} else if (funct["(global)"]) {

				if (option.undef && typeof predefined[v] !== "boolean") {
					if (!(anonname === "typeof" || anonname === "delete") ||
						(nexttoken && (nexttoken.value === "." || nexttoken.value === "["))) {

						isundef(funct, "'{a}' is not defined.", token, v);
					}
				}

				note_implied(token);
			} else {

				switch (funct[v]) {
				case "closure":
				case "function":
				case "var":
				case "unused":
					warning("'{a}' used out of scope.", token, v);
					break;
				case "label":
					warning("'{a}' is a statement label.", token, v);
					break;
				case "outer":
				case "global":
					break;
				default:
					if (s === true) {
						funct[v] = true;
					} else if (s === null) {
						warning("'{a}' is not allowed.", token, v);
						note_implied(token);
					} else if (typeof s !== "object") {
						if (option.undef) {
							if (!(anonname === "typeof" || anonname === "delete") ||
								(nexttoken &&
									(nexttoken.value === "." || nexttoken.value === "["))) {

								isundef(funct, "'{a}' is not defined.", token, v);
							}
						}
						funct[v] = true;
						note_implied(token);
					} else {
						switch (s[v]) {
						case "function":
						case "unction":
							this["function"] = true;
							s[v] = "closure";
							funct[v] = s["(global)"] ? "global" : "outer";
							break;
						case "var":
						case "unused":
							s[v] = "closure";
							funct[v] = s["(global)"] ? "global" : "outer";
							break;
						case "closure":
							funct[v] = s["(global)"] ? "global" : "outer";
							break;
						case "label":
							warning("'{a}' is a statement label.", token, v);
						}
					}
				}
			}
			return this;
		},
		led: function () {
			error("Expected an operator and instead saw '{a}'.",
				nexttoken, nexttoken.value);
		}
	};

	type("(regexp)", function () {
		return this;
	});

	delim("(endline)");
	delim("(begin)");
	delim("(end)").reach = true;
	delim("</").reach = true;
	delim("<!");
	delim("<!--");
	delim("-->");
	delim("(error)").reach = true;
	delim("}").reach = true;
	delim(")");
	delim("]");
	delim("\"").reach = true;
	delim("'").reach = true;
	delim(";");
	delim(":").reach = true;
	delim(",");
	delim("#");
	delim("@");
	reserve("else");
	reserve("case").reach = true;
	reserve("catch");
	reserve("default").reach = true;
	reserve("finally");
	reservevar("arguments", function (x) {
		if (directive["use strict"] && funct["(global)"]) {
			warning("Strict violation.", x);
		}
	});
	reservevar("eval");
	reservevar("false");
	reservevar("Infinity");
	reservevar("null");
	reservevar("this", function (x) {
		if (directive["use strict"] && !option.validthis && ((funct["(statement)"] &&
				funct["(name)"].charAt(0) > "Z") || funct["(global)"])) {
			warning("Possible strict violation.", x);
		}
	});
	reservevar("true");
	reservevar("undefined");
	assignop("=", "assign", 20);
	assignop("+=", "assignadd", 20);
	assignop("-=", "assignsub", 20);
	assignop("*=", "assignmult", 20);
	assignop("/=", "assigndiv", 20).nud = function () {
		error("A regular expression literal can be confused with '/='.");
	};
	assignop("%=", "assignmod", 20);
	bitwiseassignop("&=", "assignbitand", 20);
	bitwiseassignop("|=", "assignbitor", 20);
	bitwiseassignop("^=", "assignbitxor", 20);
	bitwiseassignop("<<=", "assignshiftleft", 20);
	bitwiseassignop(">>=", "assignshiftright", 20);
	bitwiseassignop(">>>=", "assignshiftrightunsigned", 20);
	infix("?", function (left, that) {
		that.left = left;
		that.right = expression(10);
		advance(":");
		that["else"] = expression(10);
		return that;
	}, 30);

	infix("||", "or", 40);
	infix("&&", "and", 50);
	bitwise("|", "bitor", 70);
	bitwise("^", "bitxor", 80);
	bitwise("&", "bitand", 90);
	relation("==", function (left, right) {
		var eqnull = option.eqnull && (left.value === "null" || right.value === "null");

		if (!eqnull && option.eqeqeq)
			warning("Expected '{a}' and instead saw '{b}'.", this, "===", "==");
		else if (isPoorRelation(left))
			warning("Use '{a}' to compare with '{b}'.", this, "===", left.value);
		else if (isPoorRelation(right))
			warning("Use '{a}' to compare with '{b}'.", this, "===", right.value);

		return this;
	});
	relation("===");
	relation("!=", function (left, right) {
		var eqnull = option.eqnull &&
				(left.value === "null" || right.value === "null");

		if (!eqnull && option.eqeqeq) {
			warning("Expected '{a}' and instead saw '{b}'.",
					this, "!==", "!=");
		} else if (isPoorRelation(left)) {
			warning("Use '{a}' to compare with '{b}'.",
					this, "!==", left.value);
		} else if (isPoorRelation(right)) {
			warning("Use '{a}' to compare with '{b}'.",
					this, "!==", right.value);
		}
		return this;
	});
	relation("!==");
	relation("<");
	relation(">");
	relation("<=");
	relation(">=");
	bitwise("<<", "shiftleft", 120);
	bitwise(">>", "shiftright", 120);
	bitwise(">>>", "shiftrightunsigned", 120);
	infix("in", "in", 120);
	infix("instanceof", "instanceof", 120);
	infix("+", function (left, that) {
		var right = expression(130);
		if (left && right && left.id === "(string)" && right.id === "(string)") {
			left.value += right.value;
			left.character = right.character;
			if (!option.scripturl && jx.test(left.value)) {
				warning("JavaScript URL.", left);
			}
			return left;
		}
		that.left = left;
		that.right = right;
		return that;
	}, 130);
	prefix("+", "num");
	prefix("+++", function () {
		warning("Confusing pluses.");
		this.right = expression(150);
		this.arity = "unary";
		return this;
	});
	infix("+++", function (left) {
		warning("Confusing pluses.");
		this.left = left;
		this.right = expression(130);
		return this;
	}, 130);
	infix("-", "sub", 130);
	prefix("-", "neg");
	prefix("---", function () {
		warning("Confusing minuses.");
		this.right = expression(150);
		this.arity = "unary";
		return this;
	});
	infix("---", function (left) {
		warning("Confusing minuses.");
		this.left = left;
		this.right = expression(130);
		return this;
	}, 130);
	infix("*", "mult", 140);
	infix("/", "div", 140);
	infix("%", "mod", 140);

	suffix("++", "postinc");
	prefix("++", "preinc");
	syntax["++"].exps = true;

	suffix("--", "postdec");
	prefix("--", "predec");
	syntax["--"].exps = true;
	prefix("delete", function () {
		var p = expression(0);
		if (!p || (p.id !== "." && p.id !== "[")) {
			warning("Variables should not be deleted.");
		}
		this.first = p;
		return this;
	}).exps = true;

	prefix("~", function () {
		if (option.bitwise) {
			warning("Unexpected '{a}'.", this, "~");
		}
		expression(150);
		return this;
	});

	prefix("!", function () {
		this.right = expression(150);
		this.arity = "unary";
		if (bang[this.right.id] === true) {
			warning("Confusing use of '{a}'.", this, "!");
		}
		return this;
	});
	prefix("typeof", "typeof");
	prefix("new", function () {
		var c = expression(155), i;
		if (c && c.id !== "function") {
			if (c.identifier) {
				c["new"] = true;
				switch (c.value) {
				case "Number":
				case "String":
				case "Boolean":
				case "Math":
				case "JSON":
					warning("Do not use {a} as a constructor.", prevtoken, c.value);
					break;
				case "Function":
					if (!option.evil) {
						warning("The Function constructor is eval.");
					}
					break;
				case "Date":
				case "RegExp":
					break;
				default:
					if (c.id !== "function") {
						i = c.value.substr(0, 1);
						if (option.newcap && (i < "A" || i > "Z") && !is_own(global, c.value)) {
							warning("A constructor name should start with an uppercase letter.",
								token);
						}
					}
				}
			} else {
				if (c.id !== "." && c.id !== "[" && c.id !== "(") {
					warning("Bad constructor.", token);
				}
			}
		} else {
			if (!option.supernew)
				warning("Weird construction. Delete 'new'.", this);
		}
		adjacent(token, nexttoken);
		if (nexttoken.id !== "(" && !option.supernew) {
			warning("Missing '()' invoking a constructor.",
				token, token.value);
		}
		this.first = c;
		return this;
	});
	syntax["new"].exps = true;

	prefix("void").exps = true;

	infix(".", function (left, that) {
		adjacent(prevtoken, token);
		nobreak();
		var m = identifier();
		if (typeof m === "string") {
			countMember(m);
		}
		that.left = left;
		that.right = m;
		if (left && left.value === "arguments" && (m === "callee" || m === "caller")) {
			if (option.noarg)
				warning("Avoid arguments.{a}.", left, m);
			else if (directive["use strict"])
				error("Strict violation.");
		} else if (!option.evil && left && left.value === "document" &&
				(m === "write" || m === "writeln")) {
			warning("document.write can be a form of eval.", left);
		}
		if (!option.evil && (m === "eval" || m === "execScript")) {
			warning("eval is evil.");
		}
		return that;
	}, 160, true);

	infix("(", function (left, that) {
		if (prevtoken.id !== "}" && prevtoken.id !== ")") {
			nobreak(prevtoken, token);
		}
		nospace();
		if (option.immed && !left.immed && left.id === "function") {
			warning("Wrap an immediate function invocation in parentheses " +
				"to assist the reader in understanding that the expression " +
				"is the result of a function, and not the function itself.");
		}
		var n = 0,
			p = [];
		if (left) {
			if (left.type === "(identifier)") {
				if (left.value.match(/^[A-Z]([A-Z0-9_$]*[a-z][A-Za-z0-9_$]*)?$/)) {
					if ("Number String Boolean Date Object".indexOf(left.value) === -1) {
						if (left.value === "Math") {
							warning("Math is not a function.", left);
						} else if (option.newcap) {
							warning("Missing 'new' prefix when invoking a constructor.", left);
						}
					}
				}
			}
		}
		if (nexttoken.id !== ")") {
			for (;;) {
				p[p.length] = expression(10);
				n += 1;
				if (nexttoken.id !== ",") {
					break;
				}
				comma();
			}
		}
		advance(")");
		nospace(prevtoken, token);
		if (typeof left === "object") {
			if (left.value === "parseInt" && n === 1) {
				warning("Missing radix parameter.", token);
			}
			if (!option.evil) {
				if (left.value === "eval" || left.value === "Function" ||
						left.value === "execScript") {
					warning("eval is evil.", left);

					if (p[0] && [0].id === "(string)") {
						addInternalSrc(left, p[0].value);
					}
				} else if (p[0] && p[0].id === "(string)" &&
					   (left.value === "setTimeout" ||
						left.value === "setInterval")) {
					warning(
	"Implied eval is evil. Pass a function instead of a string.", left);
					addInternalSrc(left, p[0].value);
				} else if (p[0] && p[0].id === "(string)" &&
					   left.value === "." &&
					   left.left.value === "window" &&
					   (left.right === "setTimeout" ||
						left.right === "setInterval")) {
					warning(
	"Implied eval is evil. Pass a function instead of a string.", left);
					addInternalSrc(left, p[0].value);
				}
			}
			if (!left.identifier && left.id !== "." && left.id !== "[" &&
					left.id !== "(" && left.id !== "&&" && left.id !== "||" &&
					left.id !== "?") {
				warning("Bad invocation.", left);
			}
		}
		that.left = left;
		return that;
	}, 155, true).exps = true;

	prefix("(", function () {
		nospace();
		if (nexttoken.id === "function") {
			nexttoken.immed = true;
		}
		var v = expression(0);
		advance(")", this);
		nospace(prevtoken, token);
		if (option.immed && v.id === "function") {
			if (nexttoken.id !== "(" &&
			  (nexttoken.id !== "." || (peek().value !== "call" && peek().value !== "apply"))) {
				warning(
"Do not wrap function literals in parens unless they are to be immediately invoked.",
						this);
			}
		}

		return v;
	});

	infix("[", function (left, that) {
		nobreak(prevtoken, token);
		nospace();
		var e = expression(0), s;
		if (e && e.type === "(string)") {
			if (!option.evil && (e.value === "eval" || e.value === "execScript")) {
				warning("eval is evil.", that);
			}
			countMember(e.value);
			if (!option.sub && ix.test(e.value)) {
				s = syntax[e.value];
				if (!s || !s.reserved) {
					warning("['{a}'] is better written in dot notation.",
							prevtoken, e.value);
				}
			}
		}
		advance("]", that);
		nospace(prevtoken, token);
		that.left = left;
		that.right = e;
		return that;
	}, 160, true);

	prefix("[", function () {
		var b = token.line !== nexttoken.line;
		this.first = [];
		if (b) {
			indent += option.indent;
			if (nexttoken.from === indent + option.indent) {
				indent += option.indent;
			}
		}
		while (nexttoken.id !== "(end)") {
			while (nexttoken.id === ",") {
				if (!option.es5)
					warning("Extra comma.");
				advance(",");
			}
			if (nexttoken.id === "]") {
				break;
			}
			if (b && token.line !== nexttoken.line) {
				indentation();
			}
			this.first.push(expression(10));
			if (nexttoken.id === ",") {
				comma();
				if (nexttoken.id === "]" && !option.es5) {
					warning("Extra comma.", token);
					break;
				}
			} else {
				break;
			}
		}
		if (b) {
			indent -= option.indent;
			indentation();
		}
		advance("]", this);
		return this;
	}, 160);


	function property_name() {
		var id = optionalidentifier(true);
		if (!id) {
			if (nexttoken.id === "(string)") {
				id = nexttoken.value;
				advance();
			} else if (nexttoken.id === "(number)") {
				id = nexttoken.value.toString();
				advance();
			}
		}
		return id;
	}


	function functionparams() {
		var next   = nexttoken;
		var params = [];
		var ident;

		advance("(");
		nospace();

		if (nexttoken.id === ")") {
			advance(")");
			return;
		}

		for (;;) {
			ident = identifier(true);
			params.push(ident);
			addlabel(ident, "unused", token);
			if (nexttoken.id === ",") {
				comma();
			} else {
				advance(")", next);
				nospace(prevtoken, token);
				return params;
			}
		}
	}


	function doFunction(name, statement) {
		var f;
		var oldOption = option;
		var oldScope  = scope;

		option = Object.create(option);
		scope  = Object.create(scope);

		funct = {
			"(name)"	 : name || "\"" + anonname + "\"",
			"(line)"	 : nexttoken.line,
			"(character)": nexttoken.character,
			"(context)"  : funct,
			"(breakage)" : 0,
			"(loopage)"  : 0,
			"(metrics)"  : createMetrics(nexttoken),
			"(scope)"	 : scope,
			"(statement)": statement,
			"(tokens)"	 : {}
		};

		f = funct;
		token.funct = funct;

		functions.push(funct);

		if (name) {
			addlabel(name, "function");
		}

		funct["(params)"] = functionparams();
		funct["(metrics)"].verifyMaxParametersPerFunction(funct["(params)"]);

		block(false, false, true);

		funct["(metrics)"].verifyMaxStatementsPerFunction();
		funct["(metrics)"].verifyMaxComplexityPerFunction();

		scope = oldScope;
		option = oldOption;
		funct["(last)"] = token.line;
		funct["(lastcharacter)"] = token.character;
		funct = funct["(context)"];

		return f;
	}

	function createMetrics(functionStartToken) {
		return {
			statementCount: 0,
			nestedBlockDepth: -1,
			ComplexityCount: 1,
			verifyMaxStatementsPerFunction: function () {
				if (option.maxstatements &&
					this.statementCount > option.maxstatements) {
					var message = "Too many statements per function (" + this.statementCount + ").";
					warning(message, functionStartToken);
				}
			},

			verifyMaxParametersPerFunction: function (params) {
				params = params || [];

				if (option.maxparams && params.length > option.maxparams) {
					var message = "Too many parameters per function (" + params.length + ").";
					warning(message, functionStartToken);
				}
			},

			verifyMaxNestedBlockDepthPerFunction: function () {
				if (option.maxdepth &&
					this.nestedBlockDepth > 0 &&
					this.nestedBlockDepth === option.maxdepth + 1) {
					var message = "Blocks are nested too deeply (" + this.nestedBlockDepth + ").";
					warning(message);
				}
			},

			verifyMaxComplexityPerFunction: function () {
				var max = option.maxcomplexity;
				var cc = this.ComplexityCount;
				if (max && cc > max) {
					var message = "Cyclomatic complexity is too high per function (" + cc + ").";
					warning(message, functionStartToken);
				}
			}
		};
	}

	function increaseComplexityCount() {
		funct["(metrics)"].ComplexityCount += 1;
	}


	(function (x) {
		x.nud = function () {
			var b, f, i, p, t;
			var props = {}; // All properties, including accessors

			function saveProperty(name, token) {
				if (props[name] && is_own(props, name))
					warning("Duplicate member '{a}'.", nexttoken, i);
				else
					props[name] = {};

				props[name].basic = true;
				props[name].basicToken = token;
			}

			function saveSetter(name, token) {
				if (props[name] && is_own(props, name)) {
					if (props[name].basic || props[name].setter)
						warning("Duplicate member '{a}'.", nexttoken, i);
				} else {
					props[name] = {};
				}

				props[name].setter = true;
				props[name].setterToken = token;
			}

			function saveGetter(name) {
				if (props[name] && is_own(props, name)) {
					if (props[name].basic || props[name].getter)
						warning("Duplicate member '{a}'.", nexttoken, i);
				} else {
					props[name] = {};
				}

				props[name].getter = true;
				props[name].getterToken = token;
			}

			b = token.line !== nexttoken.line;
			if (b) {
				indent += option.indent;
				if (nexttoken.from === indent + option.indent) {
					indent += option.indent;
				}
			}
			for (;;) {
				if (nexttoken.id === "}") {
					break;
				}
				if (b) {
					indentation();
				}
				if (nexttoken.value === "get" && peek().id !== ":") {
					advance("get");
					if (!option.es5) {
						error("get/set are ES5 features.");
					}
					i = property_name();
					if (!i) {
						error("Missing property name.");
					}
					saveGetter(i);
					t = nexttoken;
					adjacent(token, nexttoken);
					f = doFunction();
					p = f["(params)"];
					if (p) {
						warning("Unexpected parameter '{a}' in get {b} function.", t, p[0], i);
					}
					adjacent(token, nexttoken);
				} else if (nexttoken.value === "set" && peek().id !== ":") {
					advance("set");
					if (!option.es5) {
						error("get/set are ES5 features.");
					}
					i = property_name();
					if (!i) {
						error("Missing property name.");
					}
					saveSetter(i, nexttoken);
					t = nexttoken;
					adjacent(token, nexttoken);
					f = doFunction();
					p = f["(params)"];
					if (!p || p.length !== 1) {
						warning("Expected a single parameter in set {a} function.", t, i);
					}
				} else {
					i = property_name();
					saveProperty(i, nexttoken);
					if (typeof i !== "string") {
						break;
					}
					advance(":");
					nonadjacent(token, nexttoken);
					expression(10);
				}

				countMember(i);
				if (nexttoken.id === ",") {
					comma();
					if (nexttoken.id === ",") {
						warning("Extra comma.", token);
					} else if (nexttoken.id === "}" && !option.es5) {
						warning("Extra comma.", token);
					}
				} else {
					break;
				}
			}
			if (b) {
				indent -= option.indent;
				indentation();
			}
			advance("}", this);
			if (option.es5) {
				for (var name in props) {
					if (is_own(props, name) && props[name].setter && !props[name].getter) {
						warning("Setter is defined without getter.", props[name].setterToken);
					}
				}
			}
			return this;
		};
		x.fud = function () {
			error("Expected to see a statement and instead saw a block.", token);
		};
	}(delim("{")));

	useESNextSyntax = function () {
		var conststatement = stmt("const", function (prefix) {
			var id, name, value;

			this.first = [];
			for (;;) {
				nonadjacent(token, nexttoken);
				id = identifier();
				if (funct[id] === "const") {
					warning("const '" + id + "' has already been declared");
				}
				if (funct["(global)"] && predefined[id] === false) {
					warning("Redefinition of '{a}'.", token, id);
				}
				addlabel(id, "const");
				if (prefix) {
					break;
				}
				name = token;
				this.first.push(token);

				if (nexttoken.id !== "=") {
					warning("const " +
					  "'{a}' is initialized to 'undefined'.", token, id);
				}

				if (nexttoken.id === "=") {
					nonadjacent(token, nexttoken);
					advance("=");
					nonadjacent(token, nexttoken);
					if (nexttoken.id === "undefined") {
						warning("It is not necessary to initialize " +
						  "'{a}' to 'undefined'.", token, id);
					}
					if (peek(0).id === "=" && nexttoken.identifier) {
						error("Constant {a} was not declared correctly.",
								nexttoken, nexttoken.value);
					}
					value = expression(0);
					name.first = value;
				}

				if (nexttoken.id !== ",") {
					break;
				}
				comma();
			}
			return this;
		});
		conststatement.exps = true;
	};

	var varstatement = stmt("var", function (prefix) {
		var id, name, value;

		if (funct["(onevar)"] && option.onevar) {
			warning("Too many var statements.");
		} else if (!funct["(global)"]) {
			funct["(onevar)"] = true;
		}

		this.first = [];

		for (;;) {
			nonadjacent(token, nexttoken);
			id = identifier();

			if (option.esnext && funct[id] === "const") {
				warning("const '" + id + "' has already been declared");
			}

			if (funct["(global)"] && predefined[id] === false) {
				warning("Redefinition of '{a}'.", token, id);
			}

			addlabel(id, "unused", token);

			if (prefix) {
				break;
			}

			name = token;
			this.first.push(token);

			if (nexttoken.id === "=") {
				nonadjacent(token, nexttoken);
				advance("=");
				nonadjacent(token, nexttoken);
				if (nexttoken.id === "undefined") {
					warning("It is not necessary to initialize '{a}' to 'undefined'.", token, id);
				}
				if (peek(0).id === "=" && nexttoken.identifier) {
					error("Variable {a} was not declared correctly.",
							nexttoken, nexttoken.value);
				}
				value = expression(0);
				name.first = value;
			}
			if (nexttoken.id !== ",") {
				break;
			}
			comma();
		}
		return this;
	});
	varstatement.exps = true;

	blockstmt("function", function () {
		if (inblock) {
			warning("Function declarations should not be placed in blocks. " +
				"Use a function expression or move the statement to the top of " +
				"the outer function.", token);

		}
		var i = identifier();
		if (option.esnext && funct[i] === "const") {
			warning("const '" + i + "' has already been declared");
		}
		adjacent(token, nexttoken);
		addlabel(i, "unction", token);

		doFunction(i, { statement: true });
		if (nexttoken.id === "(" && nexttoken.line === token.line) {
			error(
"Function declarations are not invocable. Wrap the whole function invocation in parens.");
		}
		return this;
	});

	prefix("function", function () {
		var i = optionalidentifier();
		if (i) {
			adjacent(token, nexttoken);
		} else {
			nonadjacent(token, nexttoken);
		}
		doFunction(i);
		if (!option.loopfunc && funct["(loopage)"]) {
			warning("Don't make functions within a loop.");
		}
		return this;
	});

	blockstmt("if", function () {
		var t = nexttoken;
		increaseComplexityCount();
		advance("(");
		nonadjacent(this, t);
		nospace();
		expression(20);
		if (nexttoken.id === "=") {
			if (!option.boss)
				warning("Assignment in conditional expression");
			advance("=");
			expression(20);
		}
		advance(")", t);
		nospace(prevtoken, token);
		block(true, true);
		if (nexttoken.id === "else") {
			nonadjacent(token, nexttoken);
			advance("else");
			if (nexttoken.id === "if" || nexttoken.id === "switch") {
				statement(true);
			} else {
				block(true, true);
			}
		}
		return this;
	});

	blockstmt("try", function () {
		var b;

		function doCatch() {
			var oldScope = scope;
			var e;

			advance("catch");
			nonadjacent(token, nexttoken);
			advance("(");

			scope = Object.create(oldScope);

			e = nexttoken.value;
			if (nexttoken.type !== "(identifier)") {
				e = null;
				warning("Expected an identifier and instead saw '{a}'.", nexttoken, e);
			}

			advance();
			advance(")");

			funct = {
				"(name)"	 : "(catch)",
				"(line)"	 : nexttoken.line,
				"(character)": nexttoken.character,
				"(context)"  : funct,
				"(breakage)" : funct["(breakage)"],
				"(loopage)"  : funct["(loopage)"],
				"(scope)"	 : scope,
				"(statement)": false,
				"(metrics)"  : createMetrics(nexttoken),
				"(catch)"	 : true,
				"(tokens)"	 : {}
			};

			if (e) {
				addlabel(e, "exception");
			}

			token.funct = funct;
			functions.push(funct);

			block(false);

			scope = oldScope;

			funct["(last)"] = token.line;
			funct["(lastcharacter)"] = token.character;
			funct = funct["(context)"];
		}

		block(false);

		if (nexttoken.id === "catch") {
			increaseComplexityCount();
			doCatch();
			b = true;
		}

		if (nexttoken.id === "finally") {
			advance("finally");
			block(false);
			return;
		} else if (!b) {
			error("Expected '{a}' and instead saw '{b}'.",
					nexttoken, "catch", nexttoken.value);
		}

		return this;
	});

	blockstmt("while", function () {
		var t = nexttoken;
		funct["(breakage)"] += 1;
		funct["(loopage)"] += 1;
		increaseComplexityCount();
		advance("(");
		nonadjacent(this, t);
		nospace();
		expression(20);
		if (nexttoken.id === "=") {
			if (!option.boss)
				warning("Assignment in conditional expression");
			advance("=");
			expression(20);
		}
		advance(")", t);
		nospace(prevtoken, token);
		block(true, true);
		funct["(breakage)"] -= 1;
		funct["(loopage)"] -= 1;
		return this;
	}).labelled = true;

	blockstmt("with", function () {
		var t = nexttoken;
		if (directive["use strict"]) {
			error("'with' is not allowed in strict mode.", token);
		} else if (!option.withstmt) {
			warning("Don't use 'with'.", token);
		}

		advance("(");
		nonadjacent(this, t);
		nospace();
		expression(0);
		advance(")", t);
		nospace(prevtoken, token);
		block(true, true);

		return this;
	});

	blockstmt("switch", function () {
		var t = nexttoken,
			g = false;
		funct["(breakage)"] += 1;
		advance("(");
		nonadjacent(this, t);
		nospace();
		this.condition = expression(20);
		advance(")", t);
		nospace(prevtoken, token);
		nonadjacent(token, nexttoken);
		t = nexttoken;
		advance("{");
		nonadjacent(token, nexttoken);
		indent += option.indent;
		this.cases = [];
		for (;;) {
			switch (nexttoken.id) {
			case "case":
				switch (funct["(verb)"]) {
				case "break":
				case "case":
				case "continue":
				case "return":
				case "switch":
				case "throw":
					break;
				default:
					if (!ft.test(lines[nexttoken.line - 2])) {
						warning(
							"Expected a 'break' statement before 'case'.",
							token);
					}
				}
				indentation(-option.indent);
				advance("case");
				this.cases.push(expression(20));
				increaseComplexityCount();
				g = true;
				advance(":");
				funct["(verb)"] = "case";
				break;
			case "default":
				switch (funct["(verb)"]) {
				case "break":
				case "continue":
				case "return":
				case "throw":
					break;
				default:
					if (!ft.test(lines[nexttoken.line - 2])) {
						warning(
							"Expected a 'break' statement before 'default'.",
							token);
					}
				}
				indentation(-option.indent);
				advance("default");
				g = true;
				advance(":");
				break;
			case "}":
				indent -= option.indent;
				indentation();
				advance("}", t);
				if (this.cases.length === 1 || this.condition.id === "true" ||
						this.condition.id === "false") {
					if (!option.onecase)
						warning("This 'switch' should be an 'if'.", this);
				}
				funct["(breakage)"] -= 1;
				funct["(verb)"] = undefined;
				return;
			case "(end)":
				error("Missing '{a}'.", nexttoken, "}");
				return;
			default:
				if (g) {
					switch (token.id) {
					case ",":
						error("Each value should have its own case label.");
						return;
					case ":":
						g = false;
						statements();
						break;
					default:
						error("Missing ':' on a case clause.", token);
						return;
					}
				} else {
					if (token.id === ":") {
						advance(":");
						error("Unexpected '{a}'.", token, ":");
						statements();
					} else {
						error("Expected '{a}' and instead saw '{b}'.",
							nexttoken, "case", nexttoken.value);
						return;
					}
				}
			}
		}
	}).labelled = true;

	stmt("debugger", function () {
		if (!option.debug) {
			warning("All 'debugger' statements should be removed.");
		}
		return this;
	}).exps = true;

	(function () {
		var x = stmt("do", function () {
			funct["(breakage)"] += 1;
			funct["(loopage)"] += 1;
			increaseComplexityCount();

			this.first = block(true);
			advance("while");
			var t = nexttoken;
			nonadjacent(token, t);
			advance("(");
			nospace();
			expression(20);
			if (nexttoken.id === "=") {
				if (!option.boss)
					warning("Assignment in conditional expression");
				advance("=");
				expression(20);
			}
			advance(")", t);
			nospace(prevtoken, token);
			funct["(breakage)"] -= 1;
			funct["(loopage)"] -= 1;
			return this;
		});
		x.labelled = true;
		x.exps = true;
	}());

	blockstmt("for", function () {
		var s, t = nexttoken;
		funct["(breakage)"] += 1;
		funct["(loopage)"] += 1;
		increaseComplexityCount();
		advance("(");
		nonadjacent(this, t);
		nospace();
		if (peek(nexttoken.id === "var" ? 1 : 0).id === "in") {
			if (nexttoken.id === "var") {
				advance("var");
				varstatement.fud.call(varstatement, true);
			} else {
				switch (funct[nexttoken.value]) {
				case "unused":
					funct[nexttoken.value] = "var";
					break;
				case "var":
					break;
				default:
					warning("Bad for in variable '{a}'.",
							nexttoken, nexttoken.value);
				}
				advance();
			}
			advance("in");
			expression(20);
			advance(")", t);
			s = block(true, true);
			if (option.forin && s && (s.length > 1 || typeof s[0] !== "object" ||
					s[0].value !== "if")) {
				warning("The body of a for in should be wrapped in an if statement to filter " +
						"unwanted properties from the prototype.", this);
			}
			funct["(breakage)"] -= 1;
			funct["(loopage)"] -= 1;
			return this;
		} else {
			if (nexttoken.id !== ";") {
				if (nexttoken.id === "var") {
					advance("var");
					varstatement.fud.call(varstatement);
				} else {
					for (;;) {
						expression(0, "for");
						if (nexttoken.id !== ",") {
							break;
						}
						comma();
					}
				}
			}
			nolinebreak(token);
			advance(";");
			if (nexttoken.id !== ";") {
				expression(20);
				if (nexttoken.id === "=") {
					if (!option.boss)
						warning("Assignment in conditional expression");
					advance("=");
					expression(20);
				}
			}
			nolinebreak(token);
			advance(";");
			if (nexttoken.id === ";") {
				error("Expected '{a}' and instead saw '{b}'.",
						nexttoken, ")", ";");
			}
			if (nexttoken.id !== ")") {
				for (;;) {
					expression(0, "for");
					if (nexttoken.id !== ",") {
						break;
					}
					comma();
				}
			}
			advance(")", t);
			nospace(prevtoken, token);
			block(true, true);
			funct["(breakage)"] -= 1;
			funct["(loopage)"] -= 1;
			return this;
		}
	}).labelled = true;


	stmt("break", function () {
		var v = nexttoken.value;

		if (funct["(breakage)"] === 0)
			warning("Unexpected '{a}'.", nexttoken, this.value);

		if (!option.asi)
			nolinebreak(this);

		if (nexttoken.id !== ";") {
			if (token.line === nexttoken.line) {
				if (funct[v] !== "label") {
					warning("'{a}' is not a statement label.", nexttoken, v);
				} else if (scope[v] !== funct) {
					warning("'{a}' is out of scope.", nexttoken, v);
				}
				this.first = nexttoken;
				advance();
			}
		}
		reachable("break");
		return this;
	}).exps = true;


	stmt("continue", function () {
		var v = nexttoken.value;

		if (funct["(breakage)"] === 0)
			warning("Unexpected '{a}'.", nexttoken, this.value);

		if (!option.asi)
			nolinebreak(this);

		if (nexttoken.id !== ";") {
			if (token.line === nexttoken.line) {
				if (funct[v] !== "label") {
					warning("'{a}' is not a statement label.", nexttoken, v);
				} else if (scope[v] !== funct) {
					warning("'{a}' is out of scope.", nexttoken, v);
				}
				this.first = nexttoken;
				advance();
			}
		} else if (!funct["(loopage)"]) {
			warning("Unexpected '{a}'.", nexttoken, this.value);
		}
		reachable("continue");
		return this;
	}).exps = true;


	stmt("return", function () {
		if (this.line === nexttoken.line) {
			if (nexttoken.id === "(regexp)")
				warning("Wrap the /regexp/ literal in parens to disambiguate the slash operator.");

			if (nexttoken.id !== ";" && !nexttoken.reach) {
				nonadjacent(token, nexttoken);
				if (peek().value === "=" && !option.boss) {
					warningAt("Did you mean to return a conditional instead of an assignment?",
							  token.line, token.character + 1);
				}
				this.first = expression(0);
			}
		} else if (!option.asi) {
			nolinebreak(this); // always warn (Line breaking error)
		}
		reachable("return");
		return this;
	}).exps = true;


	stmt("throw", function () {
		nolinebreak(this);
		nonadjacent(token, nexttoken);
		this.first = expression(20);
		reachable("throw");
		return this;
	}).exps = true;

	reserve("class");
	reserve("const");
	reserve("enum");
	reserve("export");
	reserve("extends");
	reserve("import");
	reserve("super");

	reserve("let");
	reserve("yield");
	reserve("implements");
	reserve("interface");
	reserve("package");
	reserve("private");
	reserve("protected");
	reserve("public");
	reserve("static");

	function jsonValue() {

		function jsonObject() {
			var o = {}, t = nexttoken;
			advance("{");
			if (nexttoken.id !== "}") {
				for (;;) {
					if (nexttoken.id === "(end)") {
						error("Missing '}' to match '{' from line {a}.",
								nexttoken, t.line);
					} else if (nexttoken.id === "}") {
						warning("Unexpected comma.", token);
						break;
					} else if (nexttoken.id === ",") {
						error("Unexpected comma.", nexttoken);
					} else if (nexttoken.id !== "(string)") {
						warning("Expected a string and instead saw {a}.",
								nexttoken, nexttoken.value);
					}
					if (o[nexttoken.value] === true) {
						warning("Duplicate key '{a}'.",
								nexttoken, nexttoken.value);
					} else if ((nexttoken.value === "__proto__" &&
						!option.proto) || (nexttoken.value === "__iterator__" &&
						!option.iterator)) {
						warning("The '{a}' key may produce unexpected results.",
							nexttoken, nexttoken.value);
					} else {
						o[nexttoken.value] = true;
					}
					advance();
					advance(":");
					jsonValue();
					if (nexttoken.id !== ",") {
						break;
					}
					advance(",");
				}
			}
			advance("}");
		}

		function jsonArray() {
			var t = nexttoken;
			advance("[");
			if (nexttoken.id !== "]") {
				for (;;) {
					if (nexttoken.id === "(end)") {
						error("Missing ']' to match '[' from line {a}.",
								nexttoken, t.line);
					} else if (nexttoken.id === "]") {
						warning("Unexpected comma.", token);
						break;
					} else if (nexttoken.id === ",") {
						error("Unexpected comma.", nexttoken);
					}
					jsonValue();
					if (nexttoken.id !== ",") {
						break;
					}
					advance(",");
				}
			}
			advance("]");
		}

		switch (nexttoken.id) {
		case "{":
			jsonObject();
			break;
		case "[":
			jsonArray();
			break;
		case "true":
		case "false":
		case "null":
		case "(number)":
		case "(string)":
			advance();
			break;
		case "-":
			advance("-");
			if (token.character !== nexttoken.from) {
				warning("Unexpected space after '-'.", token);
			}
			adjacent(token, nexttoken);
			advance("(number)");
			break;
		default:
			error("Expected a JSON value.", nexttoken);
		}
	}
	var itself = function (s, o, g) {
		var a, i, k, x;
		var optionKeys;
		var newOptionObj = {};

		if (o && o.scope) {
			JSHINT.scope = o.scope;
		} else {
			JSHINT.errors = [];
			JSHINT.undefs = [];
			JSHINT.internals = [];
			JSHINT.blacklist = {};
			JSHINT.scope = "(main)";
		}

		predefined = Object.create(standard);
		declared = Object.create(null);
		combine(predefined, g || {});

		if (o) {
			a = o.predef;
			if (a) {
				if (!Array.isArray(a) && typeof a === "object") {
					a = Object.keys(a);
				}
				a.forEach(function (item) {
					var slice;
					if (item[0] === "-") {
						slice = item.slice(1);
						JSHINT.blacklist[slice] = slice;
					} else {
						predefined[item] = true;
					}
				});
			}

			optionKeys = Object.keys(o);
			for (x = 0; x < optionKeys.length; x++) {
				newOptionObj[optionKeys[x]] = o[optionKeys[x]];

				if (optionKeys[x] === "newcap" && o[optionKeys[x]] === false)
					newOptionObj["(explicitNewcap)"] = true;

				if (optionKeys[x] === "indent")
					newOptionObj.white = true;
			}
		}

		option = newOptionObj;

		option.indent = option.indent || 4;
		option.maxerr = option.maxerr || 50;

		tab = "";
		for (i = 0; i < option.indent; i += 1) {
			tab += " ";
		}
		indent = 1;
		global = Object.create(predefined);
		scope = global;
		funct = {
			"(global)":   true,
			"(name)":	  "(global)",
			"(scope)":	  scope,
			"(breakage)": 0,
			"(loopage)":  0,
			"(tokens)":   {},
			"(metrics)":   createMetrics(nexttoken)
		};
		functions = [funct];
		urls = [];
		stack = null;
		member = {};
		membersOnly = null;
		implied = {};
		inblock = false;
		lookahead = [];
		jsonmode = false;
		warnings = 0;
		lines = [];
		unuseds = [];

		if (!isString(s) && !Array.isArray(s)) {
			errorAt("Input is neither a string nor an array of strings.", 0);
			return false;
		}

		if (isString(s) && /^\s*$/g.test(s)) {
			errorAt("Input is an empty string.", 0);
			return false;
		}

		if (s.length === 0) {
			errorAt("Input is an empty array.", 0);
			return false;
		}

		lex.init(s);

		prereg = true;
		directive = {};

		prevtoken = token = nexttoken = syntax["(begin)"];
		for (var name in o) {
			if (is_own(o, name)) {
				checkOption(name, token);
			}
		}

		assume();
		combine(predefined, g || {});
		comma.first = true;
		quotmark = undefined;

		try {
			advance();
			switch (nexttoken.id) {
			case "{":
			case "[":
				option.laxbreak = true;
				jsonmode = true;
				jsonValue();
				break;
			default:
				directives();
				if (directive["use strict"] && !option.globalstrict) {
					warning("Use the function form of \"use strict\".", prevtoken);
				}

				statements();
			}
			advance((nexttoken && nexttoken.value !== ".")	? "(end)" : undefined);

			var markDefined = function (name, context) {
				do {
					if (typeof context[name] === "string") {

						if (context[name] === "unused")
							context[name] = "var";
						else if (context[name] === "unction")
							context[name] = "closure";

						return true;
					}

					context = context["(context)"];
				} while (context);

				return false;
			};

			var clearImplied = function (name, line) {
				if (!implied[name])
					return;

				var newImplied = [];
				for (var i = 0; i < implied[name].length; i += 1) {
					if (implied[name][i] !== line)
						newImplied.push(implied[name][i]);
				}

				if (newImplied.length === 0)
					delete implied[name];
				else
					implied[name] = newImplied;
			};

			var warnUnused = function (name, token) {
				var line = token.line;
				var chr  = token.character;

				if (option.unused)
					warningAt("'{a}' is defined but never used.", line, chr, name);

				unuseds.push({
					name: name,
					line: line,
					character: chr
				});
			};

			var checkUnused = function (func, key) {
				var type = func[key];
				var token = func["(tokens)"][key];

				if (key.charAt(0) === "(")
					return;

				if (type !== "unused" && type !== "unction")
					return;
				if (func["(params)"] && func["(params)"].indexOf(key) !== -1)
					return;

				warnUnused(key, token);
			};
			for (i = 0; i < JSHINT.undefs.length; i += 1) {
				k = JSHINT.undefs[i].slice(0);

				if (markDefined(k[2].value, k[0])) {
					clearImplied(k[2].value, k[2].line);
				} else {
					warning.apply(warning, k.slice(1));
				}
			}

			functions.forEach(function (func) {
				for (var key in func) {
					if (is_own(func, key)) {
						checkUnused(func, key);
					}
				}

				if (!func["(params)"])
					return;

				var params = func["(params)"].slice();
				var param  = params.pop();
				var type;

				while (param) {
					type = func[param];

					if (param === "undefined")
						return;

					if (type !== "unused" && type !== "unction")
						return;

					warnUnused(param, func["(tokens)"][param]);
					param = params.pop();
				}
			});

			for (var key in declared) {
				if (is_own(declared, key) && !is_own(global, key)) {
					warnUnused(key, declared[key]);
				}
			}
		} catch (e) {
			if (e) {
				var nt = nexttoken || {};
				JSHINT.errors.push({
					raw		  : e.raw,
					reason	  : e.message,
					line	  : e.line || nt.line,
					character : e.character || nt.from
				}, null);
			}
		}

		if (JSHINT.scope === "(main)") {
			o = o || {};

			for (i = 0; i < JSHINT.internals.length; i += 1) {
				k = JSHINT.internals[i];
				o.scope = k.elem;
				itself(k.value, o, g);
			}
		}

		return JSHINT.errors.length === 0;
	};
	itself.data = function () {
		var data = {
			functions: [],
			options: option
		};
		var implieds = [];
		var members = [];
		var fu, f, i, j, n, globals;

		if (itself.errors.length) {
			data.errors = itself.errors;
		}

		if (jsonmode) {
			data.json = true;
		}

		for (n in implied) {
			if (is_own(implied, n)) {
				implieds.push({
					name: n,
					line: implied[n]
				});
			}
		}

		if (implieds.length > 0) {
			data.implieds = implieds;
		}

		if (urls.length > 0) {
			data.urls = urls;
		}

		globals = Object.keys(scope);
		if (globals.length > 0) {
			data.globals = globals;
		}

		for (i = 1; i < functions.length; i += 1) {
			f = functions[i];
			fu = {};

			for (j = 0; j < functionicity.length; j += 1) {
				fu[functionicity[j]] = [];
			}

			for (j = 0; j < functionicity.length; j += 1) {
				if (fu[functionicity[j]].length === 0) {
					delete fu[functionicity[j]];
				}
			}

			fu.name = f["(name)"];
			fu.param = f["(params)"];
			fu.line = f["(line)"];
			fu.character = f["(character)"];
			fu.last = f["(last)"];
			fu.lastcharacter = f["(lastcharacter)"];
			data.functions.push(fu);
		}

		if (unuseds.length > 0) {
			data.unused = unuseds;
		}

		members = [];
		for (n in member) {
			if (typeof member[n] === "number") {
				data.member = member;
				break;
			}
		}

		return data;
	};

	itself.jshint = itself;

	return itself;
}());
if (typeof exports === "object" && exports) {
	exports.JSHINT = JSHINT;
}

});