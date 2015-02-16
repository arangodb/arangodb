!function(e){if("object"==typeof exports&&"undefined"!=typeof module)module.exports=e();else if("function"==typeof define&&define.amd)define([],e);else{var f;"undefined"!=typeof window?f=window:"undefined"!=typeof global?f=global:"undefined"!=typeof self&&(f=self),f.Joi=e()}}(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');
var Any = require('./any');
var Cast = require('./cast');
var Ref = require('./ref');
var Errors = require('./errors');


// Declare internals

var internals = {};


internals.Alternatives = function () {

    Any.call(this);
    this._type = 'alternatives';
    this._invalids.remove(null);

    this._inner.matches = [];
};

Hoek.inherits(internals.Alternatives, Any);


internals.Alternatives.prototype._base = function (value, state, options) {

    var errors = [];
    for (var i = 0, il = this._inner.matches.length; i < il; ++i) {
        var item = this._inner.matches[i];
        var schema = item.schema;
        if (!schema) {
            var failed = item.is._validate(item.ref(state.parent, options), null, options, state.parent).errors;
            schema = failed ? item.otherwise : item.then;
            if (!schema) {
                continue;
            }
        }

        var result = schema._validate(value, state, options);
        if (!result.errors) {     // Found a valid match
            return result;
        }

        errors = errors.concat(result.errors);
    }

    return { errors: errors.length ? errors : Errors.create('alternatives.base', null, state, options) };
};


internals.Alternatives.prototype.try = function (/* schemas */) {


    var schemas = Hoek.flatten(Array.prototype.slice.call(arguments));
    Hoek.assert(schemas.length, 'Cannot add other alternatives without at least one schema');

    var obj = this.clone();

    for (var i = 0, il = schemas.length; i < il; ++i) {
        var cast = Cast.schema(schemas[i]);
        if (cast._refs.length) {
            obj._refs = obj._refs.concat(cast._refs)
        }
        obj._inner.matches.push({ schema: cast });
    }

    return obj;
};


internals.Alternatives.prototype.when = function (ref, options) {

    Hoek.assert(Ref.isRef(ref) || typeof ref === 'string', 'Invalid reference:', ref);
    Hoek.assert(options, 'Missing options');
    Hoek.assert(typeof options === 'object', 'Invalid options');
    Hoek.assert(options.hasOwnProperty('is'), 'Missing "is" directive');
    Hoek.assert(options.then !== undefined || options.otherwise !== undefined, 'options must have at least one of "then" or "otherwise"');

    var obj = this.clone();

    var item = {
        ref: Cast.ref(ref),
        is: Cast.schema(options.is),
        then: options.then !== undefined ? Cast.schema(options.then) : undefined,
        otherwise: options.otherwise !== undefined ? Cast.schema(options.otherwise) : undefined
    };

    Ref.push(obj._refs, item.ref);
    obj._refs = obj._refs.concat(item.is._refs);

    if (item.then && item.then._refs) {
        obj._refs = obj._refs.concat(item.then._refs);
    }

    if (item.otherwise && item.otherwise._refs) {
        obj._refs = obj._refs.concat(item.otherwise._refs);
    }

    obj._inner.matches.push(item);

    return obj;
};


internals.Alternatives.prototype.describe = function () {

    var descriptions = [];
    for (var i = 0, il = this._inner.matches.length; i < il; ++i) {
        var item = this._inner.matches[i];
        if (item.schema) {

            // try()

            descriptions.push(item.schema.describe());
        }
        else {

            // when()

            var when = {
                ref: item.ref.toString(),
                is: item.is.describe()
            };

            if (item.then) {
                when.then = item.then.describe();
            }

            if (item.otherwise) {
                when.otherwise = item.otherwise.describe();
            }

            descriptions.push(when);
        }
    }

    return descriptions;
};


module.exports = new internals.Alternatives();

},{"./any":2,"./cast":6,"./errors":8,"./ref":14,"hoek":16}],2:[function(require,module,exports){
(function (Buffer){
// Load modules

var Path = require('path');
var Hoek = require('hoek');
var Ref = require('./ref');
var Errors = require('./errors');
var Alternatives = null;                // Delay-loaded to prevent circular dependencies
var Cast = null;


// Declare internals

var internals = {};


internals.defaults = {
    abortEarly: true,
    convert: true,
    allowUnknown: false,
    skipFunctions: false,
    stripUnknown: false,
    language: {}
    // context: null
};


module.exports = internals.Any = function () {

    this.isJoi = true;
    this._type = 'any';
    this._settings = null;
    this._valids = new internals.Set();
    this._invalids = new internals.Set();
    this._tests = [];
    this._refs = [];
    this._flags = { /*
        presence: 'optional',                   // optional, required, forbidden, ignore
        allowOnly: false,
        allowUnknown: undefined,
        default: undefined,
        forbidden: false,
        encoding: undefined,
        insensitive: false,
        trim: false,
        case: undefined                         // upper, lower
    */};

    this._description = null;
    this._unit = null;
    this._notes = [];
    this._tags = [];
    this._examples = [];
    this._meta = [];

    this._inner = {};                           // Hash of arrays of immutable objects
};


internals.Any.prototype.isImmutable = true;     // Prevents Hoek from deep cloning schema objects


internals.Any.prototype.clone = function () {

    var obj = {};
    obj.__proto__ = Object.getPrototypeOf(this);

    obj.isJoi = true;
    obj._type = this._type;
    obj._settings = internals.concatSettings(this._settings);
    obj._valids = Hoek.clone(this._valids);
    obj._invalids = Hoek.clone(this._invalids);
    obj._tests = this._tests.slice();
    obj._refs = this._refs.slice();
    obj._flags = Hoek.clone(this._flags);

    obj._description = this._description;
    obj._unit = this._unit;
    obj._notes = this._notes.slice();
    obj._tags = this._tags.slice();
    obj._examples = this._examples.slice();
    obj._meta = this._meta.slice();

    obj._inner = {};
    var inners = Object.keys(this._inner);
    for (var i = 0, il = inners.length; i < il; ++i) {
        var key = inners[i];
        obj._inner[key] = this._inner[key] ? this._inner[key].slice() : null;
    }

    return obj;
};


internals.Any.prototype.concat = function (schema) {

    Hoek.assert(schema && schema.isJoi, 'Invalid schema object');
    Hoek.assert(schema._type === 'any' || schema._type === this._type, 'Cannot merge with another type:', schema._type);

    var obj = this.clone();

    obj._settings = obj._settings ? internals.concatSettings(obj._settings, schema._settings) : schema._settings;
    obj._valids.merge(schema._valids, schema._invalids);
    obj._invalids.merge(schema._invalids, schema._valids);
    obj._tests = obj._tests.concat(schema._tests);
    obj._refs = obj._refs.concat(schema._refs);
    Hoek.merge(obj._flags, schema._flags);

    obj._description = schema._description || obj._description;
    obj._unit = schema._unit || obj._unit;
    obj._notes = obj._notes.concat(schema._notes);
    obj._tags = obj._tags.concat(schema._tags);
    obj._examples = obj._examples.concat(schema._examples);
    obj._meta = obj._meta.concat(schema._meta);

    var inners = Object.keys(schema._inner);
    for (var i = 0, il = inners.length; i < il; ++i) {
        var key = inners[i];
        if (schema._inner[key]) {
            obj._inner[key] = (obj._inner[key] ? obj._inner[key].concat(schema._inner[key]) : schema._inner[key].slice());
        }
    }

    return obj;
};


internals.Any.prototype._test = function (name, arg, func) {

    Hoek.assert(!this._flags.allowOnly, 'Cannot define rules when valid values specified');

    var obj = this.clone();
    obj._tests.push({ func: func, name: name, arg: arg });
    return obj;
};


internals.Any.prototype.options = function (options) {

    Hoek.assert(!options.context, 'Cannot override context');

    var obj = this.clone();
    obj._settings = internals.concatSettings(obj._settings, options);
    return obj;
};


internals.Any.prototype.strict = function () {

    var obj = this.clone();
    obj._settings = obj._settings || {};
    obj._settings.convert = false;
    return obj;
};


internals.Any.prototype._allow = function () {

    var values = Hoek.flatten(Array.prototype.slice.call(arguments));
    for (var i = 0, il = values.length; i < il; ++i) {
        var value = values[i];
        this._invalids.remove(value);
        this._valids.add(value, this._refs);
    }
};


internals.Any.prototype.allow = function () {

    var obj = this.clone();
    obj._allow.apply(obj, arguments);
    return obj;
};


internals.Any.prototype.valid = internals.Any.prototype.equal = function () {

    Hoek.assert(!this._tests.length, 'Cannot set valid values when rules specified');

    var obj = this.allow.apply(this, arguments);
    obj._flags.allowOnly = true;
    return obj;
};


internals.Any.prototype.invalid = internals.Any.prototype.not = function (value) {

    var obj = this.clone();
    var values = Hoek.flatten(Array.prototype.slice.call(arguments));
    for (var i = 0, il = values.length; i < il; ++i) {
        var value = values[i];
        obj._valids.remove(value);
        obj._invalids.add(value, this._refs);
    }

    return obj;
};


internals.Any.prototype.required = internals.Any.prototype.exist = function () {

    var obj = this.clone();
    obj._flags.presence = 'required';
    return obj;
};


internals.Any.prototype.optional = function () {

    var obj = this.clone();
    delete obj._flags.presence;     // Defaults to 'optional'
    return obj;
};


internals.Any.prototype.default = function (value) {

    var obj = this.clone();
    obj._flags.default = value;
    Ref.push(obj._refs, value);
    return obj;
};


internals.Any.prototype.forbidden = function () {

    var obj = this.clone();
    obj._flags.presence = 'forbidden';
    return obj;
};


internals.Any.prototype.when = function (ref, options) {

    Hoek.assert(options && typeof options === 'object', 'Invalid options');
    Hoek.assert(options.then !== undefined || options.otherwise !== undefined, 'options must have at least one of "then" or "otherwise"');

    Cast = Cast || require('./cast');
    var then = options.then ? this.concat(Cast.schema(options.then)) : this;
    var otherwise = options.otherwise ? this.concat(Cast.schema(options.otherwise)) : this;

    Alternatives = Alternatives || require('./alternatives');
    var obj = Alternatives.when(ref, { is: options.is, then: then, otherwise: otherwise });
    obj._flags.presence = 'ignore';
    return obj;
};


internals.Any.prototype.description = function (desc) {

    Hoek.assert(desc && typeof desc === 'string', 'Description must be a non-empty string');

    var obj = this.clone();
    obj._description = desc;
    return obj;
};


internals.Any.prototype.notes = function (notes) {

    Hoek.assert(notes && (typeof notes === 'string' || Array.isArray(notes)), 'Notes must be a non-empty string or array');

    var obj = this.clone();
    obj._notes = obj._notes.concat(notes);
    return obj;
};


internals.Any.prototype.tags = function (tags) {

    Hoek.assert(tags && (typeof tags === 'string' || Array.isArray(tags)), 'Tags must be a non-empty string or array');

    var obj = this.clone();
    obj._tags = obj._tags.concat(tags);
    return obj;
};

internals.Any.prototype.meta = function (meta) {

    Hoek.assert(meta !== undefined, 'Meta cannot be undefined');

    var obj = this.clone();
    obj._meta = obj._meta.concat(meta);
    return obj;
};


internals.Any.prototype.example = function (value) {

    Hoek.assert(arguments.length, 'Missing example');
    var result = this._validate(value, null, internals.defaults);
    Hoek.assert(!result.errors, 'Bad example:', result.errors && Errors.process(result.errors, value));

    var obj = this.clone();
    obj._examples = obj._examples.concat(value);
    return obj;
};


internals.Any.prototype.unit = function (name) {

    Hoek.assert(name && typeof name === 'string', 'Unit name must be a non-empty string');

    var obj = this.clone();
    obj._unit = name;
    return obj;
};


internals.Any.prototype._validate = function (value, state, options, reference) {

    var self = this;

    // Setup state and settings

    state = state || { key: '', path: '', parent: null, reference: reference };

    if (this._settings) {
        options = internals.concatSettings(options, this._settings);
    }

    var errors = [];
    var finish = function () {

        return {
            value: (value !== undefined) ? value : (Ref.isRef(self._flags.default) ? self._flags.default(state.parent, options) : self._flags.default),
            errors: errors.length ? errors : null
        };
    };

    // Check presence requirements

    if (this._flags.presence) {                         // 'required', 'forbidden', or 'ignore'
        if (this._flags.presence === 'required' &&
            value === undefined) {

            errors.push(Errors.create('any.required', null, state, options));
            return finish();
        }
        else if (this._flags.presence === 'forbidden') {
            if (value === undefined) {
                return finish();
            }

            errors.push(Errors.create('any.unknown', null, state, options));
            return finish();
        }
    }
    else {                                              // 'optional'
        if (value === undefined) {
            return finish();
        }
    }

    // Check allowed and denied values using the original value

    if (this._valids.has(value, state, options, this._flags.insensitive)) {
        return finish();
    }

    if (this._invalids.has(value, state, options, this._flags.insensitive)) {
        errors.push(Errors.create(value === '' ? 'any.empty' : 'any.invalid', null, state, options));
        if (options.abortEarly ||
            value === undefined) {          // No reason to keep validating missing value

            return finish();
        }
    }

    // Convert value and validate type

    if (this._base) {
        var base = this._base.call(this, value, state, options);
        if (base.errors) {
            value = base.value;
            errors = errors.concat(base.errors);
            return finish();                            // Base error always aborts early
        }

        if (base.value !== value) {
            value = base.value;

            // Check allowed and denied values using the converted value

            if (this._valids.has(value, state, options, this._flags.insensitive)) {
                return finish();
            }

            if (this._invalids.has(value, state, options, this._flags.insensitive)) {
                errors.push(Errors.create('any.invalid', null, state, options));
                if (options.abortEarly) {
                    return finish();
                }
            }
        }
    }

    // Required values did not match

    if (this._flags.allowOnly) {
        errors.push(Errors.create('any.allowOnly', { valids: this._valids.toString(false) }, state, options));
        if (options.abortEarly) {
            return finish();
        }
    }

    // Helper.validate tests

    for (var i = 0, il = this._tests.length; i < il; ++i) {
        var test = this._tests[i];
        var err = test.func.call(this, value, state, options);
        if (err) {
            errors.push(err);
            if (options.abortEarly) {
                return finish();
            }
        }
    }

    return finish();
};


internals.Any.prototype._validateWithOptions = function (value, options, callback) {

    var settings = internals.concatSettings(internals.defaults, options);
    var result = this._validate(value, null, settings);
    var errors = Errors.process(result.errors, value);

    if (callback) {
        return callback(errors, result.value);
    }

    return { error: errors, value: result.value };
};


internals.Any.prototype.validate = function (value, callback) {

    var result = this._validate(value, null, internals.defaults);
    var errors = Errors.process(result.errors, value);

    if (callback) {
        return callback(errors, result.value);
    }

    return { error: errors, value: result.value };
};


internals.Any.prototype.describe = function () {

    var description = {
        type: this._type
    };

    if (Object.keys(this._flags).length) {
        description.flags = this._flags;
    }

    if (this._description) {
        description.description = this._description;
    }

    if (this._notes.length) {
        description.notes = this._notes;
    }

    if (this._tags.length) {
        description.tags = this._tags;
    }

    if (this._meta.length) {
        description.meta = this._meta;
    }

    if (this._examples.length) {
        description.examples = this._examples;
    }

    if (this._unit) {
        description.unit = this._unit;
    }

    var valids = this._valids.values();
    if (valids.length) {
        description.valids = valids;
    }

    var invalids = this._invalids.values();
    if (invalids.length) {
        description.invalids = invalids;
    }

    description.rules = [];

    for (var i = 0, il = this._tests.length; i < il; ++i) {
        var validator = this._tests[i];
        var item = { name: validator.name };
        if (validator.arg) {
            item.arg = validator.arg;
        }
        description.rules.push(item);
    }

    if (!description.rules.length) {
        delete description.rules;
    }

    return description;
};


// Set

internals.Set = function () {

    this._set = [];
};


internals.Set.prototype.add = function (value, refs) {

    Hoek.assert(value === null || value === undefined || value instanceof Date || Buffer.isBuffer(value) || Ref.isRef(value) || (typeof value !== 'function' && typeof value !== 'object'), 'Value cannot be an object or function');

    if (typeof value !== 'function' &&
        this.has(value, null, null, false)) {

        return;
    }

    Ref.push(refs, value);
    this._set.push(value);
};


internals.Set.prototype.merge = function (add, remove) {

    for (var i = 0, il = add._set.length; i < il; ++i) {
        this.add(add._set[i]);
    }

    for (i = 0, il = remove._set.length; i < il; ++i) {
        this.remove(remove._set[i]);
    }
};


internals.Set.prototype.remove = function (value) {

    this._set = this._set.filter(function (item) {

        return value !== item;
    });
};


internals.Set.prototype.has = function (value, state, options, insensitive) {

    for (var i = 0, il = this._set.length; i < il; ++i) {
        var item = this._set[i];

        if (Ref.isRef(item)) {
            item = item(state.reference || state.parent, options);
        }

        if (typeof value !== typeof item) {
            continue;
        }

        if (value === item ||
            (value instanceof Date && item instanceof Date && value.getTime() === item.getTime()) ||
            (insensitive && typeof value === 'string' && value.toLowerCase() === item.toLowerCase()) ||
            (Buffer.isBuffer(value) && Buffer.isBuffer(item) && value.length === item.length && value.toString('binary') === item.toString('binary'))) {

            return true;
        }
    }

    return false;
};


internals.Set.prototype.values = function () {

    return this._set.slice();
};


internals.Set.prototype.toString = function (includeUndefined) {

    var list = '';
    for (var i = 0, il = this._set.length; i < il; ++i) {
        var item = this._set[i];
        if (item !== undefined || includeUndefined) {
            list += (list ? ', ' : '') + internals.stringify(item);
        }
    }

    return list;
};


internals.stringify = function (value) {

    if (value === undefined) {
        return 'undefined';
    }

    if (value === null) {
        return 'null';
    }

    if (typeof value === 'string') {
        return value;
    }

    return value.toString();
};


internals.concatSettings = function (target, source) {

    // Used to avoid cloning context

    if (!target &&
        !source) {

        return null;
    }

    var obj = {};

    if (target) {
        var tKeys = Object.keys(target);
        for (var i = 0, il = tKeys.length; i < il; ++i) {
            var key = tKeys[i];
            obj[key] = target[key];
        }
    }

    if (source) {
        var sKeys = Object.keys(source);
        for (var j = 0, jl = sKeys.length; j < jl; ++j) {
            var key = sKeys[j];
            if (key !== 'language' ||
                !obj.hasOwnProperty(key)) {

                obj[key] = source[key];
            }
            else {
                obj[key] = Hoek.applyToDefaults(obj[key], source[key]);
            }
        }
    }

    return obj;
};

}).call(this,require("buffer").Buffer)
},{"./alternatives":1,"./cast":6,"./errors":8,"./ref":14,"buffer":24,"hoek":16,"path":28}],3:[function(require,module,exports){
// Load modules

var Any = require('./any');
var Cast = require('./cast');
var Errors = require('./errors');
var Hoek = require('hoek');


// Declare internals

var internals = {};


internals.Array = function () {

    Any.call(this);
    this._type = 'array';
    this._inner.inclusions = [];
    this._inner.exclusions = [];
};

Hoek.inherits(internals.Array, Any);


internals.Array.prototype._base = function (value, state, options) {

    var result = {
        value: value
    };

    if (typeof value === 'string' &&
        options.convert) {

        try {
            var converted = JSON.parse(value);
            if (Array.isArray(converted)) {
                result.value = converted;
            }
        }
        catch (e) { }
    }

    if (!Array.isArray(result.value)) {
        result.errors = Errors.create('array.base', null, state, options);
        return result;
    }

    if (this._inner.inclusions.length ||
        this._inner.exclusions.length) {

        for (var v = 0, vl = result.value.length; v < vl; ++v) {
            var item = result.value[v];
            var isValid = false;
            var localState = { key: v, path: (state.path ? state.path + '.' : '') + v, parent: result.value, reference: state.reference };

            // Exclusions

            for (var i = 0, il = this._inner.exclusions.length; i < il; ++i) {
                var res = this._inner.exclusions[i]._validate(item, localState, {});                // Not passing options to use defaults
                if (!res.errors) {
                    result.errors = Errors.create('array.excludes', { pos: v }, { key: state.key, path: localState.path }, options);
                    return result;
                }
            }

            // Inclusions

            for (i = 0, il = this._inner.inclusions.length; i < il; ++i) {
                var res = this._inner.inclusions[i]._validate(item, localState, options);
                if (!res.errors) {
                    result.value[v] = res.value;
                    isValid = true;
                    break;
                }

                // Return the actual error if only one inclusion defined

                if (il === 1) {
                    result.errors = Errors.create('array.includesOne', { pos: v, reason: res.errors }, { key: state.key, path: localState.path }, options);
                    return result;
                }
            }

            if (this._inner.inclusions.length &&
                !isValid) {

                result.errors = Errors.create('array.includes', { pos: v }, { key: state.key, path: localState.path }, options);
                return result;
            }
        }
    }

    return result;
};


internals.Array.prototype.describe = function () {

    var description = Any.prototype.describe.call(this);

    if (this._inner.inclusions.length) {
        description.includes = [];

        for (var i = 0, il = this._inner.inclusions.length; i < il; ++i) {
            description.includes.push(this._inner.inclusions[i].describe());
        }
    }

    if (this._inner.exclusions.length) {
        description.excludes = [];

        for (var i = 0, il = this._inner.exclusions.length; i < il; ++i) {
            description.excludes.push(this._inner.exclusions[i].describe());
        }
    }

    return description;
};


internals.Array.prototype.includes = function () {

    var inclusions = Hoek.flatten(Array.prototype.slice.call(arguments)).map(function (type) {

        return Cast.schema(type);
    });

    var obj = this.clone();
    obj._inner.inclusions = obj._inner.inclusions.concat(inclusions);
    return obj;
};


internals.Array.prototype.excludes = function () {

    var exclusions = Hoek.flatten(Array.prototype.slice.call(arguments)).map(function (type) {

        return Cast.schema(type);
    });

    var obj = this.clone();
    obj._inner.exclusions = obj._inner.exclusions.concat(exclusions);
    return obj;
};


internals.Array.prototype.min = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('min', limit, function (value, state, options) {

        if (value.length >= limit) {
            return null;
        }

        return Errors.create('array.min', { limit: limit }, state, options);
    });
};


internals.Array.prototype.max = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('max', limit, function (value, state, options) {

        if (value.length <= limit) {
            return null;
        }

        return Errors.create('array.max', { limit: limit }, state, options);
    });
};


internals.Array.prototype.length = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('length', limit, function (value, state, options) {

        if (value.length === limit) {
            return null;
        }

        return Errors.create('array.length', { limit: limit }, state, options);
    });
};


module.exports = new internals.Array();

},{"./any":2,"./cast":6,"./errors":8,"hoek":16}],4:[function(require,module,exports){
(function (Buffer){
// Load modules

var Any = require('./any');
var Errors = require('./errors');
var Hoek = require('hoek');


// Declare internals

var internals = {};


internals.Binary = function () {

    Any.call(this);
    this._type = 'binary';
};

Hoek.inherits(internals.Binary, Any);


internals.Binary.prototype._base = function (value, state, options) {

    var result = {
        value: value
    };

    if (typeof value === 'string' &&
        options.convert) {

        try {
            var converted = new Buffer(value, this._flags.encoding);
            result.value = converted;
        }
        catch (e) { }
    }

    result.errors = Buffer.isBuffer(result.value) ? null : Errors.create('binary.base', null, state, options);
    return result;
};


internals.Binary.prototype.encoding = function (encoding) {

    Hoek.assert(Buffer.isEncoding(encoding), 'Invalid encoding:', encoding);

    var obj = this.clone();
    obj._flags.encoding = encoding;
    return obj;
};


internals.Binary.prototype.min = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('min', limit, function (value, state, options) {

        if (value.length >= limit) {
            return null;
        }

        return Errors.create('binary.min', { limit: limit }, state, options);
    });
};


internals.Binary.prototype.max = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('max', limit, function (value, state, options) {

        if (value.length <= limit) {
            return null;
        }

        return Errors.create('binary.max', { limit: limit }, state, options);
    });
};


internals.Binary.prototype.length = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('length', limit, function (value, state, options) {

        if (value.length === limit) {
            return null;
        }

        return Errors.create('binary.length', { limit: limit }, state, options);
    });
};


module.exports = new internals.Binary();
}).call(this,require("buffer").Buffer)
},{"./any":2,"./errors":8,"buffer":24,"hoek":16}],5:[function(require,module,exports){
// Load modules

var Any = require('./any');
var Errors = require('./errors');
var Hoek = require('hoek');


// Declare internals

var internals = {};


internals.Boolean = function () {

    Any.call(this);
    this._type = 'boolean';
};

Hoek.inherits(internals.Boolean, Any);


internals.Boolean.prototype._base = function (value, state, options) {

    var result = {
        value: value
    };

    if (typeof value === 'string' &&
        options.convert) {

        var lower = value.toLowerCase();
        result.value = (lower === 'true' || lower === 'yes' || lower === 'on' ? true
                                                                              : (lower === 'false' || lower === 'no' || lower === 'off' ? false : value));
    }

    result.errors = (typeof result.value === 'boolean') ? null : Errors.create('boolean.base', null, state, options);
    return result;
};


module.exports = new internals.Boolean();
},{"./any":2,"./errors":8,"hoek":16}],6:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');
var Ref = require('./ref');
// Type modules are delay-loaded to prevent circular dependencies


// Declare internals

var internals = {
    any: null,
    date: require('./date'),
    string: require('./string'),
    number: require('./number'),
    boolean: require('./boolean'),
    alt: null,
    object: null
};


exports.schema = function (config) {

    internals.any = internals.any || new (require('./any'))();
    internals.alt = internals.alt || require('./alternatives');
    internals.object = internals.object || require('./object');

    if (config &&
        typeof config === 'object') {

        if (config.isJoi) {
            return config;
        }

        if (Array.isArray(config)) {
            return internals.alt.try(config);
        }

        if (config instanceof RegExp) {
            return internals.string.regex(config);
        }

        if (config instanceof Date) {
            return internals.date.valid(config);
        }

        return internals.object.keys(config);
    }

    if (typeof config === 'string') {
        return internals.string.valid(config);
    }

    if (typeof config === 'number') {
        return internals.number.valid(config);
    }

    if (typeof config === 'boolean') {
        return internals.boolean.valid(config);
    }

    if (Ref.isRef(config)) {
        return internals.any.valid(config);
    }

    Hoek.assert(config === null, 'Invalid schema content:', config);

    return internals.any.valid(null);
};


exports.ref = function (id) {

    return Ref.isRef(id) ? id : Ref.create(id);
};
},{"./alternatives":1,"./any":2,"./boolean":5,"./date":7,"./number":12,"./object":13,"./ref":14,"./string":15,"hoek":16}],7:[function(require,module,exports){
// Load modules

var Any = require('./any');
var Errors = require('./errors');
var Hoek = require('hoek');


// Declare internals

var internals = {};


internals.Date = function () {

    Any.call(this);
    this._type = 'date';
};

Hoek.inherits(internals.Date, Any);


internals.Date.prototype._base = function (value, state, options) {

    var result = {
        value: (options.convert && internals.toDate(value)) || value
    };

    result.errors = (result.value instanceof Date && !isNaN(result.value.getTime())) ? null : Errors.create('date.base', null, state, options);
    return result;
};


internals.toDate = function (value) {

    if (value instanceof Date) {
        return value;
    }

    if (typeof value === 'string' ||
        Hoek.isInteger(value)) {

        if (typeof value === 'string' &&
            /^\d+$/.test(value)) {

            value = parseInt(value, 10);
        }

        var date = new Date(value);
        if (!isNaN(date.getTime())) {
            return date;
        }
    }

    return null;
};


internals.Date.prototype.min = function (date) {

    date = internals.toDate(date);
    Hoek.assert(date, 'Invalid date format');

    return this._test('min', date, function (value, state, options) {

        if (value.getTime() >= date.getTime()) {
            return null;
        }

        return Errors.create('date.min', { limit: date }, state, options);
    });
};


internals.Date.prototype.max = function (date) {

    date = internals.toDate(date);
    Hoek.assert(date, 'Invalid date format');

    return this._test('max', date, function (value, state, options) {

        if (value.getTime() <= date.getTime()) {
            return null;
        }

        return Errors.create('date.max', { limit: date }, state, options);
    });
};


module.exports = new internals.Date();

},{"./any":2,"./errors":8,"hoek":16}],8:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');
var Language = require('./language');


// Declare internals

var internals = {};


internals.Err = function (type, context, state, options) {

    this.type = type;
    this.context = context || {};
    this.context.key = state.key;
    this.path = state.path;
    this.options = options;
};


internals.Err.prototype.toString = function () {

    var self = this;

    var localized = this.options.language;
    this.context.key = this.context.key || localized.root || Language.errors.root;

    var format = Hoek.reach(localized, this.type) || Hoek.reach(Language.errors, this.type);
    var hasKey = /\{\{\!?key\}\}/.test(format);
    format = (hasKey ? format : '{{!key}} ' + format);
    var message = format.replace(/\{\{(\!?)([^}]+)\}\}/g, function ($0, isSecure, name) {

        var value = Hoek.reach(self.context, name);
        var normalized = Array.isArray(value) ? value.join(', ') : value.toString();
        return (isSecure ? Hoek.escapeHtml(normalized) : normalized);
    });

    return message;
};


exports.create = function (type, context, state, options) {

    return new internals.Err(type, context, state, options);
};


exports.process = function (errors, object) {

    if (!errors || !errors.length) {
        return null;
    }

    var details = [];
    for (var i = 0, il = errors.length; i < il; ++i) {
        var item = errors[i];
        details.push({
            message: item.toString(),
            path: item.path || item.context.key,
            type: item.type
        });
    }

    // Construct error

    var message = '';
    details.forEach(function (error) {

        message += (message ? '. ' : '') + error.message;
    });

    var error = new Error(message);
    error.name = 'ValidationError';
    error.details = details;
    error._object = object;
    error.annotate = internals.annotate;
    return error;
};


internals.annotate = function () {

    var obj = Hoek.clone(this._object || {});

    var lookup = {};
    var el = this.details.length;
    for (var e = el - 1; e >= 0; --e) {        // Reverse order to process deepest child first
        var pos = el - e;
        var error = this.details[e];
        var path = error.path.split('.');
        var ref = obj;
        for (var i = 0, il = path.length; i < il && ref; ++i) {
            var seg = path[i];
            if (i + 1 < il) {
                ref = ref[seg];
            }
            else {
                var value = ref[seg];
                if (value !== undefined) {
                    delete ref[seg];
                    var label = seg + '_$key$_' + pos + '_$end$_';
                    ref[label] = value;
                    lookup[error.path] = label;
                }
                else if (lookup[error.path]) {
                    var replacement = lookup[error.path];
                    var appended = replacement.replace('_$end$_', ', ' + pos + '_$end$_');
                    ref[appended] = ref[replacement];
                    lookup[error.path] = appended;
                    delete ref[replacement];
                }
                else {
                    ref['_$miss$_' + seg + '|' + pos + '_$end$_'] = '__missing__';
                }
            }
        }
    }

    var annotated = JSON.stringify(obj, null, 2);

    annotated = annotated.replace(/_\$key\$_([, \d]+)_\$end\$_\"/g, function ($0, $1) {

        return '" \u001b[31m[' + $1 + ']\u001b[0m';
    });

    var message = annotated.replace(/\"_\$miss\$_([^\|]+)\|(\d+)_\$end\$_\"\: \"__missing__\"/g, function ($0, $1, $2) {

        return '\u001b[41m"' + $1 + '"\u001b[0m\u001b[31m [' + $2 + ']: -- missing --\u001b[0m';
    });

    message += '\n\u001b[31m';

    for (e = 0; e < el; ++e) {
        message += '\n[' + (e + 1) + '] ' + this.details[e].message;
    }

    message += '\u001b[0m';

    return message;
};


},{"./language":11,"hoek":16}],9:[function(require,module,exports){
// Load modules

var Any = require('./any');
var Errors = require('./errors');
var Hoek = require('hoek');


// Declare internals

var internals = {};


internals.Function = function () {

    Any.call(this);
    this._type = 'func';
};

Hoek.inherits(internals.Function, Any);


internals.Function.prototype._base = function (value, state, options) {

    return {
        value: value,
        errors: (typeof value === 'function') ? null : Errors.create('function.base', null, state, options)
    };
};


module.exports = new internals.Function();
},{"./any":2,"./errors":8,"hoek":16}],10:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');
var Any = require('./any');
var Cast = require('./cast');
var Ref = require('./ref');


// Declare internals

var internals = {
    alternatives: require('./alternatives'),
    array: require('./array'),
    boolean: require('./boolean'),
    binary: require('./binary'),
    date: require('./date'),
    func: require('./function'),
    number: require('./number'),
    object: require('./object'),
    string: require('./string')
};


internals.root = function () {

    var any = new Any();

    var root = any.clone();
    root.any = function () {

        return any;
    };

    root.alternatives = root.alt = function () {

        return arguments.length ? internals.alternatives.try.apply(internals.alternatives, arguments) : internals.alternatives;
    };

    root.array = function () {

        return internals.array;
    };

    root.boolean = root.bool = function () {

        return internals.boolean;
    };

    root.binary = function () {

        return internals.binary;
    };

    root.date = function () {

        return internals.date;
    };

    root.func = function () {

        return internals.func;
    };

    root.number = function () {

        return internals.number;
    };

    root.object = function () {

        return arguments.length ? internals.object.keys.apply(internals.object, arguments) : internals.object;
    };

    root.string = function () {

        return internals.string;
    };

    root.ref = function () {

        return Ref.create.apply(null, arguments);
    };

    root.isRef = function (ref) {

        return Ref.isRef(ref);
    };

    root.validate = function (value /*, [schema], [options], callback */) {

        var last = arguments[arguments.length - 1];
        var callback = typeof last === 'function' ? last : null;

        var count = arguments.length - (callback ? 1 : 0);
        if (count === 1) {
            return any.validate(value, callback);
        }

        var options = count === 3 ? arguments[2] : {};
        var schema = Cast.schema(arguments[1]);

        return schema._validateWithOptions(value, options, callback);
    };

    root.describe = function () {

        var schema = arguments.length ? Cast.schema(arguments[0]) : any;
        return schema.describe();
    };

    root.compile = function (schema) {

        return Cast.schema(schema);
    };

    root.assert = function (value, schema) {

        var error = root.validate(value, schema).error;
        if (error) {
            throw new Error(error.annotate());
        }
    };

    return root;
};


module.exports = internals.root();

},{"./alternatives":1,"./any":2,"./array":3,"./binary":4,"./boolean":5,"./cast":6,"./date":7,"./function":9,"./number":12,"./object":13,"./ref":14,"./string":15,"hoek":16}],11:[function(require,module,exports){
// Load modules


// Declare internals

var internals = {};


exports.errors = {
    root: 'value',
    any: {
        unknown: 'is not allowed',
        invalid: 'contains an invalid value',
        empty: 'is not allowed to be empty',
        required: 'is required',
        allowOnly: 'must be one of {{valids}}'
    },
    alternatives: {
        base: 'not matching any of the allowed alternatives'
    },
    array: {
        base: 'must be an array',
        includes: 'position {{pos}} does not match any of the allowed types',
        includesOne: 'position {{pos}} fails because {{reason}}',
        excludes: 'position {{pos}} contains an excluded value',
        min: 'must contain at least {{limit}} items',
        max: 'must contain less than or equal to {{limit}} items',
        length: 'must contain {{limit}} items'
    },
    boolean: {
        base: 'must be a boolean'
    },
    binary: {
        base: 'must be a buffer or a string',
        min: 'must be at least {{limit}} bytes',
        max: 'must be less than or equal to {{limit}} bytes',
        length: 'must be {{limit}} bytes'
    },
    date: {
        base: 'must be a number of milliseconds or valid date string',
        min: 'must be larger than or equal to {{limit}}',
        max: 'must be less than or equal to {{limit}}'
    },
    function: {
        base: 'must be a Function'
    },
    object: {
        base: 'must be an object',
        min: 'must have at least {{limit}} children',
        max: 'must have less than or equal to {{limit}} children',
        length: 'must have {{limit}} children',
        allowUnknown: 'is not allowed',
        with: 'missing required peer {{peer}}',
        without: 'conflict with forbidden peer {{peer}}',
        missing: 'must contain at least one of {{peers}}',
        xor: 'contains a conflict between exclusive peers {{peers}}',
        or: 'must contain at least one of {{peers}}',
        and: 'contains {{present}} without its required peers {{missing}}',
        assert: 'validation failed because {{ref}} failed to {{message}}',
        rename: {
            multiple: 'cannot rename child {{from}} because multiple renames are disabled and another key was already renamed to {{to}}',
            override: 'cannot rename child {{from}} because override is disabled and target {{to}} exists'
        }
    },
    number: {
        base: 'must be a number',
        min: 'must be larger than or equal to {{limit}}',
        max: 'must be less than or equal to {{limit}}',
        float: 'must be a float or double',
        integer: 'must be an integer',
        negative: 'must be a negative number',
        positive: 'must be a positive number'
    },
    string: {
        base: 'must be a string',
        min: 'length must be at least {{limit}} characters long',
        max: 'length must be less than or equal to {{limit}} characters long',
        length: 'length must be {{limit}} characters long',
        alphanum: 'must only contain alpha-numeric characters',
        token: 'must only contain alpha-numeric and underscore characters',
        regex: 'fails to match the required pattern',
        email: 'must be a valid email',
        isoDate: 'must be a valid ISO 8601 date',
        guid: 'must be a valid GUID',
        hostname: 'must be a valid hostname',
        lowercase: 'must only contain lowercase characters',
        uppercase: 'must only contain uppercase characters',
        trim: 'must not have leading or trailing whitespace'
    }
};

},{}],12:[function(require,module,exports){
// Load modules

var Any = require('./any');
var Errors = require('./errors');
var Hoek = require('hoek');


// Declare internals

var internals = {};


internals.Number = function () {

    Any.call(this);
    this._type = 'number';
};

Hoek.inherits(internals.Number, Any);


internals.Number.prototype._base = function (value, state, options) {

    var result = {
        errors: null,
        value: value
    };

    if (typeof value === 'string' &&
        options.convert) {

        var number = parseFloat(value);
        result.value = (isNaN(number) || !isFinite(value)) ? NaN : number;
    }

    result.errors = (typeof result.value === 'number' && !isNaN(result.value)) ? null : Errors.create('number.base', null, state, options);
    return result;
};


internals.Number.prototype.min = function (limit) {

    Hoek.assert(Hoek.isInteger(limit), 'limit must be an integer');

    return this._test('min', limit, function (value, state, options) {

        if (value >= limit) {
            return null;
        }

        return Errors.create('number.min', { limit: limit }, state, options);
    });
};


internals.Number.prototype.max = function (limit) {

    Hoek.assert(Hoek.isInteger(limit), 'limit must be an integer');

    return this._test('max', limit, function (value, state, options) {

        if (value <= limit) {
            return null;
        }

        return Errors.create('number.max', { limit: limit }, state, options);
    });
};


internals.Number.prototype.integer = function () {

    return this._test('integer', undefined, function (value, state, options) {

        return Hoek.isInteger(value) ? null : Errors.create('number.integer', null, state, options);
    });
};


internals.Number.prototype.negative = function () {

    return this._test('negative', undefined, function (value, state, options) {

        if (value < 0) {
            return null;
        }

        return Errors.create('number.negative', null, state, options);
    });
};


internals.Number.prototype.positive = function () {

    return this._test('positive', undefined, function (value, state, options) {

        if (value > 0) {
            return null;
        }

        return Errors.create('number.positive', null, state, options);
    });
};


module.exports = new internals.Number();

},{"./any":2,"./errors":8,"hoek":16}],13:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');
var Topo = require('topo');
var Any = require('./any');
var Cast = require('./cast');
var Ref = require('./ref');
var Errors = require('./errors');


// Declare internals

var internals = {};


internals.Object = function () {

    Any.call(this);
    this._type = 'object';
    this._inner.children = null;
    this._inner.renames = [];
    this._inner.dependencies = [];
    this._inner.patterns = [];
};

Hoek.inherits(internals.Object, Any);


internals.Object.prototype._base = function (value, state, options) {

    var target = value;
    var errors = [];
    var finish = function () {

        return {
            value: target,
            errors: errors.length ? errors : null
        };
    };

    if (typeof value === 'string' &&
        options.convert) {

        try {
            value = JSON.parse(value);
        }
        catch (err) { }
    }

    if (!value ||
        typeof value !== 'object' ||
        Array.isArray(value)) {

        errors.push(Errors.create('object.base', null, state, options));
        return finish();
    }

    // Ensure target is a local copy (parsed) or shallow copy

    if (target === value) {
        target = {};
        target.__proto__ = Object.getPrototypeOf(value);
        var valueKeys = Object.keys(value);
        for (var t = 0, tl = valueKeys.length; t < tl; ++t) {
            target[valueKeys[t]] = value[valueKeys[t]];
        }
    }
    else {
        target = value;
    }

    // Rename keys

    var renamed = {};
    for (var r = 0, rl = this._inner.renames.length; r < rl; ++r) {
        var item = this._inner.renames[r];

        if (target[item.from] === undefined) {
            continue;
        }

        if (!item.options.multiple &&
            renamed[item.to]) {

            errors.push(Errors.create('object.rename.multiple', { from: item.from, to: item.to }, state, options));
            if (options.abortEarly) {
                return finish();
            }
        }

        if (target.hasOwnProperty(item.to) &&
            !item.options.override &&
            !renamed[item.to]) {

            errors.push(Errors.create('object.rename.override', { from: item.from, to: item.to }, state, options));
            if (options.abortEarly) {
                return finish();
            }
        }

        target[item.to] = target[item.from];
        renamed[item.to] = true;

        if (!item.options.alias) {
            delete target[item.from];
        }
    }

    // Validate dependencies

    for (var d = 0, dl = this._inner.dependencies.length; d < dl; ++d) {
        var dep = this._inner.dependencies[d];
        var err = internals[dep.type](dep.key !== null && value[dep.key], dep.peers, target, { key: dep.key, path: (state.path ? state.path + '.' : '') + dep.key }, options);
        if (err) {
            errors.push(err);
            if (options.abortEarly) {
                return finish();
            }
        }
    }

    // Validate schema

    if (!this._inner.children &&            // null allows any keys
        !this._inner.patterns.length) {

        return finish();
    }

    var unprocessed = Hoek.mapToObject(Object.keys(target));
    var key;

    if (this._inner.children) {
        for (var i = 0, il = this._inner.children.length; i < il; ++i) {
            var child = this._inner.children[i];
            var key = child.key;
            var item = target[key];

            delete unprocessed[key];

            var localState = { key: key, path: (state.path ? state.path + '.' : '') + key, parent: target, reference: state.reference };
            var result = child.schema._validate(item, localState, options);
            if (result.errors) {
                errors = errors.concat(result.errors);
                if (options.abortEarly) {
                    return finish();
                }
            }

            if (result.value !== undefined) {
                target[key] = result.value;
            }
        }
    }

    // Unknown keys

    var unprocessedKeys = Object.keys(unprocessed);
    if (unprocessedKeys.length &&
        this._inner.patterns.length) {

        for (i = 0, il = unprocessedKeys.length; i < il; ++i) {
            var key = unprocessedKeys[i];

            for (var p = 0, pl = this._inner.patterns.length; p < pl; ++p) {
                var pattern = this._inner.patterns[p];

                if (pattern.regex.test(key)) {
                    delete unprocessed[key];

                    var item = target[key];
                    var localState = { key: key, path: (state.path ? state.path + '.' : '') + key, parent: target, reference: state.reference };
                    var result = pattern.rule._validate(item, localState, options);
                    if (result.errors) {
                        errors = errors.concat(result.errors);
                        if (options.abortEarly) {
                            return finish();
                        }
                    }

                    if (result.value !== undefined) {
                        target[key] = result.value;
                    }
                }
            }
        }

        unprocessedKeys = Object.keys(unprocessed);
    }

    if (unprocessedKeys.length) {
        if (options.stripUnknown ||
            options.skipFunctions) {

            var hasFunctions = false;
            for (var k = 0, kl = unprocessedKeys.length; k < kl; ++k) {
                key = unprocessedKeys[k];

                if (options.stripUnknown) {
                    delete target[key];
                }
                else if (typeof target[key] === 'function') {
                    delete unprocessed[key];
                    hasFunctions = true;
                }
            }

            if (options.stripUnknown) {
                return finish();
            }

            if (hasFunctions) {
                unprocessedKeys = Object.keys(unprocessed);
            }
        }

        if (unprocessedKeys.length &&
            (this._flags.allowUnknown !== undefined ? !this._flags.allowUnknown : !options.allowUnknown)) {

            for (var e = 0, el = unprocessedKeys.length; e < el; ++e) {
                errors.push(Errors.create('object.allowUnknown', null, { key: unprocessedKeys[e], path: state.path }, options));
            }
        }
    }

    return finish();
};


internals.Object.prototype.keys = function (schema) {

    Hoek.assert(schema === null || schema === undefined || typeof schema === 'object', 'Object schema must be a valid object');
    Hoek.assert(!schema || !schema.isJoi, 'Object schema cannot be a joi schema');

    var obj = this.clone();

    if (!schema) {
        obj._inner.children = null;
        return obj;
    }

    var children = Object.keys(schema);

    if (!children.length) {
        obj._inner.children = [];
        return obj;
    }

    var topo = new Topo();
    if (obj._inner.children) {
        for (var i = 0, il = obj._inner.children.length; i < il; ++i) {
            var child = obj._inner.children[i];
            topo.add(child, { after: child._refs, group: child.key });
        }
    }

    for (var c = 0, cl = children.length; c < cl; ++c) {
        var key = children[c];
        var child = schema[key];
        var cast = Cast.schema(child);
        topo.add({ key: key, schema: cast }, { after: cast._refs, group: key });
    }

    obj._inner.children = topo.nodes;

    return obj;
};


internals.Object.prototype.unknown = function (allow) {

    var obj = this.clone();
    obj._flags.allowUnknown = (allow !== false);
    return obj;
};


internals.Object.prototype.length = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('length', limit, function (value, state, options) {

        if (Object.keys(value).length === limit) {
            return null;
        }

        return Errors.create('object.length', { limit: limit }, state, options);
    });
};


internals.Object.prototype.min = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('min', limit, function (value, state, options) {

        if (Object.keys(value).length >= limit) {
            return null;
        }

        return Errors.create('object.min', { limit: limit }, state, options);
    });
};


internals.Object.prototype.max = function (limit) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');

    return this._test('max', limit, function (value, state, options) {

        if (Object.keys(value).length <= limit) {
            return null;
        }

        return Errors.create('object.max', { limit: limit }, state, options);
    });
};


internals.Object.prototype.pattern = function (regex, schema) {

    Hoek.assert(regex instanceof RegExp, 'Invalid regular expression');
    Hoek.assert(schema !== undefined, 'Invalid rule');

    var obj = this.clone();
    obj._inner.patterns.push({ regex: regex, rule: Cast.schema(schema) });
    return obj;
};


internals.Object.prototype.with = function (key, peers) {

    return this._dependency('with', key, peers);
};


internals.Object.prototype.without = function (key, peers) {

    return this._dependency('without', key, peers);
};


internals.Object.prototype.xor = function () {

    var peers = Hoek.flatten(Array.prototype.slice.call(arguments));
    return this._dependency('xor', null, peers);
};


internals.Object.prototype.or = function () {

    var peers = Hoek.flatten(Array.prototype.slice.call(arguments));
    return this._dependency('or', null, peers);
};


internals.Object.prototype.and = function () {

    var peers = Hoek.flatten(Array.prototype.slice.call(arguments));
    return this._dependency('and', null, peers);
};


internals.renameDefaults = {
    alias: false,                   // Keep old value in place
    multiple: false,                // Allow renaming multiple keys into the same target
    override: false                 // Overrides an existing key
};


internals.Object.prototype.rename = function (from, to, options) {

    Hoek.assert(typeof from === 'string', 'Rename missing the from argument');
    Hoek.assert(typeof to === 'string', 'Rename missing the to argument');
    Hoek.assert(to !== from, 'Cannot rename key to same name:', from);

    for (var i = 0, il = this._inner.renames.length; i < il; ++i) {
        Hoek.assert(this._inner.renames[i].from !== from, 'Cannot rename the same key multiple times');
    }

    var obj = this.clone();

    obj._inner.renames.push({
        from: from,
        to: to,
        options: Hoek.applyToDefaults(internals.renameDefaults, options || {})
    });

    return obj;
};


internals.Object.prototype._dependency = function (type, key, peers) {

    peers = [].concat(peers);
    for (var i = 0, li = peers.length; i < li; i++) {
        Hoek.assert(typeof peers[i] === 'string', type, 'peers must be a string or array of strings');
    }

    var obj = this.clone();
    obj._inner.dependencies.push({ type: type, key: key, peers: peers });
    return obj;
};


internals.with = function (value, peers, parent, state, options) {

    if (value === undefined) {
        return null;
    }

    for (var i = 0, il = peers.length; i < il; ++i) {
        var peer = peers[i];
        if (!parent.hasOwnProperty(peer) ||
            parent[peer] === undefined) {

            return Errors.create('object.with', { peer: peer }, state, options);
        }
    }

    return null;
};


internals.without = function (value, peers, parent, state, options) {

    if (value === undefined) {
        return null;
    }

    for (var i = 0, il = peers.length; i < il; ++i) {
        var peer = peers[i];
        if (parent.hasOwnProperty(peer) &&
            parent[peer] !== undefined) {

            return Errors.create('object.without', { peer: peer }, state, options);
        }
    }

    return null;
};


internals.xor = function (value, peers, parent, state, options) {

    var present = [];
    for (var i = 0, il = peers.length; i < il; ++i) {
        var peer = peers[i];
        if (parent.hasOwnProperty(peer) &&
            parent[peer] !== undefined) {

            present.push(peer);
        }
    }

    if (present.length === 1) {
        return null;
    }

    if (present.length === 0) {
        return Errors.create('object.missing', { peers: peers }, state, options);
    }

    return Errors.create('object.xor', { peers: peers }, state, options);
};


internals.or = function (value, peers, parent, state, options) {

    for (var i = 0, il = peers.length; i < il; ++i) {
        var peer = peers[i];
        if (parent.hasOwnProperty(peer) &&
            parent[peer] !== undefined) {
            return null;
        }
    }

    return Errors.create('object.missing', { peers: peers }, state, options);
};


internals.and = function (value, peers, parent, state, options) {

    var missing = [];
    var present = [];
    var count = peers.length;
    for (var i = 0; i < count; ++i) {
        var peer = peers[i];
        if (!parent.hasOwnProperty(peer) ||
            parent[peer] === undefined) {

            missing.push(peer);
        }
        else {
            present.push(peer);
        }
    }

    var aon = (missing.length === count || present.length === count);
    return !aon ? Errors.create('object.and', { present: present, missing: missing }, state, options) : null;
};


internals.Object.prototype.describe = function (shallow) {

    var description = Any.prototype.describe.call(this);

    if (this._inner.children &&
        !shallow) {

        description.children = {};
        for (var i = 0, il = this._inner.children.length; i < il; ++i) {
            var child = this._inner.children[i];
            description.children[child.key] = child.schema.describe();
        }
    }

    if (this._inner.dependencies.length) {
        description.dependencies = Hoek.clone(this._inner.dependencies);
    }

    return description;
};


internals.Object.prototype.assert = function (ref, schema, message) {

    ref = Cast.ref(ref);
    Hoek.assert(ref.isContext || ref.depth > 1, 'Cannot use assertions for root level references - use direct key rules instead');

    var cast = Cast.schema(schema);

    return this._test('assert', { cast: cast, ref: ref }, function (value, state, options) {

        var result = cast._validate(ref(value), null, options, value);
        if (!result.errors) {
            return null;
        }

        return Errors.create('object.assert', { ref: ref.path.join('.'), message: message }, state, options);
    });
};


module.exports = new internals.Object();

},{"./any":2,"./cast":6,"./errors":8,"./ref":14,"hoek":16,"topo":21}],14:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');


// Declare internals

var internals = {};


exports.create = function (key, options) {

    Hoek.assert(typeof key === 'string', 'Invalid reference key:', key);

    var settings = Hoek.clone(options);         // options can be reused and modified

    var ref = function (value, validationOptions) {

        return Hoek.reach(ref.isContext ? validationOptions.context : value, ref.key, settings);
    };

    ref.isContext = (key[0] === ((settings && settings.contextPrefix) || '$'));
    ref.key = (ref.isContext ? key.slice(1) : key);
    ref.path = ref.key.split((settings && settings.separator) || '.');
    ref.depth = ref.path.length;
    ref.root = ref.path[0];
    ref.isJoi = true;

    ref.toString = function () {

        return (ref.isContext ? 'context:' : 'ref:') + ref.key;
    };

    return ref;
};


exports.isRef = function (ref) {

    return typeof ref === 'function' && ref.isJoi;
};


exports.push = function (array, ref) {

    if (exports.isRef(ref) &&
        !ref.isContext) {

        array.push(ref.root);
    }
};
},{"hoek":16}],15:[function(require,module,exports){
(function (Buffer){
// Load modules

var Net = require('net');
var Hoek = require('hoek');
var Isemail = require('isemail');
var Any = require('./any');
var Errors = require('./errors');


// Declare internals

var internals = {};


internals.String = function () {

    Any.call(this);
    this._type = 'string';
    this._invalids.add('');
};

Hoek.inherits(internals.String, Any);


internals.String.prototype._base = function (value, state, options) {

    if (typeof value === 'string' &&
        options.convert) {

        if (this._flags.case) {
            value = (this._flags.case === 'upper' ? value.toLocaleUpperCase() : value.toLocaleLowerCase());
        }

        if (this._flags.trim) {
            value = value.trim();
        }
    }

    return {
        value: value,
        errors: (typeof value === 'string') ? null : Errors.create('string.base', null, state, options)
    };
};


internals.String.prototype.insensitive = function () {

    var obj = this.clone();
    obj._flags.insensitive = true;
    return obj;
};


internals.String.prototype.min = function (limit, encoding) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');
    Hoek.assert(!encoding || Buffer.isEncoding(encoding), 'Invalid encoding:', encoding);

    return this._test('min', limit, function (value, state, options) {

        var length = encoding ? Buffer.byteLength(value, encoding) : value.length;
        if (length >= limit) {
            return null;
        }

        return Errors.create('string.min', { limit: limit }, state, options);
    });
};


internals.String.prototype.max = function (limit, encoding) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');
    Hoek.assert(!encoding || Buffer.isEncoding(encoding), 'Invalid encoding:', encoding);

    return this._test('max', limit, function (value, state, options) {

        var length = encoding ? Buffer.byteLength(value, encoding) : value.length;
        if (length <= limit) {
            return null;
        }

        return Errors.create('string.max', { limit: limit }, state, options);
    });
};


internals.String.prototype.length = function (limit, encoding) {

    Hoek.assert(Hoek.isInteger(limit) && limit >= 0, 'limit must be a positive integer');
    Hoek.assert(!encoding || Buffer.isEncoding(encoding), 'Invalid encoding:', encoding);

    return this._test('length', limit, function (value, state, options) {

        var length = encoding ? Buffer.byteLength(value, encoding) : value.length;
        if (length === limit) {
            return null;
        }

        return Errors.create('string.length', { limit: limit }, state, options);
    });
};


internals.String.prototype.regex = function (pattern) {

    Hoek.assert(pattern instanceof RegExp, 'pattern must be a RegExp');

    return this._test('regex', pattern, function (value, state, options) {

        if (pattern.test(value)) {
            return null;
        }

        return Errors.create('string.regex', null, state, options);
    });
};


internals.String.prototype.alphanum = function () {

    return this._test('alphanum', undefined, function (value, state, options) {

        if (/^[a-zA-Z0-9]+$/.test(value)) {
            return null;
        }

        return Errors.create('string.alphanum', null, state, options);
    });
};


internals.String.prototype.token = function () {

    return this._test('token', undefined, function (value, state, options) {

        if (/^\w+$/.test(value)) {
            return null;
        }

        return Errors.create('string.token', null, state, options);
    });
};


internals.String.prototype.email = function () {

    return this._test('email', undefined, function (value, state, options) {

        if (Isemail(value)) {
            return null;
        }

        return Errors.create('string.email', null, state, options);
    });
};


internals.String.prototype.isoDate = function () {

    var regex = /^(\d{4}-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d:[0-5]\d\.\d+([+-][0-2]\d:[0-5]\d|Z))|(\d{4}-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d:[0-5]\d([+-][0-2]\d:[0-5]\d|Z))|(\d{4}-[01]\d-[0-3]\dT[0-2]\d:[0-5]\d([+-][0-2]\d:[0-5]\d|Z))$/;

    return this._test('isoDate', undefined, function (value, state, options) {

        if (regex.test(value)) {
            return null;
        }

        return Errors.create('string.isoDate', null, state, options);
    });
};


internals.String.prototype.guid = function () {

    var regex = /^[A-F0-9]{8}(?:-?[A-F0-9]{4}){3}-?[A-F0-9]{12}$/i;
    var regex2 = /^\{[A-F0-9]{8}(?:-?[A-F0-9]{4}){3}-?[A-F0-9]{12}\}$/i;

    return this._test('guid', undefined, function (value, state, options) {

        if (regex.test(value) || regex2.test(value)) {
            return null;
        }

        return Errors.create('string.guid', null, state, options);
    });
};


internals.String.prototype.hostname = function () {

    var regex = /^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$/;

    return this._test('hostname', undefined, function (value, state, options) {

        if ((value.length <= 255 && regex.test(value)) ||
            Net.isIPv6(value)) {

            return null;
        }

        return Errors.create("string.hostname", null, state, options);
    });
};


internals.String.prototype.lowercase = function () {

    var obj = this._test('lowercase', undefined, function (value, state, options) {

        if (options.convert ||
            value === value.toLocaleLowerCase()) {

            return null;
        }

        return Errors.create('string.lowercase', null, state, options);
    });

    obj._flags.case = 'lower';
    return obj;
};


internals.String.prototype.uppercase = function (options) {

    var obj = this._test('uppercase', undefined, function (value, state, options) {

        if (options.convert ||
            value === value.toLocaleUpperCase()) {

            return null;
        }

        return Errors.create('string.uppercase', null, state, options);
    });

    obj._flags.case = 'upper';
    return obj;
};


internals.String.prototype.trim = function () {

    var obj = this._test('trim', undefined, function (value, state, options) {

        if (options.convert ||
            value === value.trim()) {

            return null;
        }

        return Errors.create('string.trim', null, state, options);
    });

    obj._flags.trim = true;
    return obj;
};


module.exports = new internals.String();

}).call(this,require("buffer").Buffer)
},{"./any":2,"./errors":8,"buffer":24,"hoek":16,"isemail":19,"net":23}],16:[function(require,module,exports){
module.exports = require('./lib');
},{"./lib":18}],17:[function(require,module,exports){
(function (Buffer){
// Declare internals

var internals = {};


exports.escapeJavaScript = function (input) {

    if (!input) {
        return '';
    }

    var escaped = '';

    for (var i = 0, il = input.length; i < il; ++i) {

        var charCode = input.charCodeAt(i);

        if (internals.isSafe(charCode)) {
            escaped += input[i];
        }
        else {
            escaped += internals.escapeJavaScriptChar(charCode);
        }
    }

    return escaped;
};


exports.escapeHtml = function (input) {

    if (!input) {
        return '';
    }

    var escaped = '';

    for (var i = 0, il = input.length; i < il; ++i) {

        var charCode = input.charCodeAt(i);

        if (internals.isSafe(charCode)) {
            escaped += input[i];
        }
        else {
            escaped += internals.escapeHtmlChar(charCode);
        }
    }

    return escaped;
};


internals.escapeJavaScriptChar = function (charCode) {

    if (charCode >= 256) {
        return '\\u' + internals.padLeft('' + charCode, 4);
    }

    var hexValue = new Buffer(String.fromCharCode(charCode), 'ascii').toString('hex');
    return '\\x' + internals.padLeft(hexValue, 2);
};


internals.escapeHtmlChar = function (charCode) {

    var namedEscape = internals.namedHtml[charCode];
    if (typeof namedEscape !== 'undefined') {
        return namedEscape;
    }

    if (charCode >= 256) {
        return '&#' + charCode + ';';
    }

    var hexValue = new Buffer(String.fromCharCode(charCode), 'ascii').toString('hex');
    return '&#x' + internals.padLeft(hexValue, 2) + ';';
};


internals.padLeft = function (str, len) {

    while (str.length < len) {
        str = '0' + str;
    }

    return str;
};


internals.isSafe = function (charCode) {

    return (typeof internals.safeCharCodes[charCode] !== 'undefined');
};


internals.namedHtml = {
    '38': '&amp;',
    '60': '&lt;',
    '62': '&gt;',
    '34': '&quot;',
    '160': '&nbsp;',
    '162': '&cent;',
    '163': '&pound;',
    '164': '&curren;',
    '169': '&copy;',
    '174': '&reg;'
};


internals.safeCharCodes = (function () {

    var safe = {};

    for (var i = 32; i < 123; ++i) {

        if ((i >= 97) ||                    // a-z
            (i >= 65 && i <= 90) ||         // A-Z
            (i >= 48 && i <= 57) ||         // 0-9
            i === 32 ||                     // space
            i === 46 ||                     // .
            i === 44 ||                     // ,
            i === 45 ||                     // -
            i === 58 ||                     // :
            i === 95) {                     // _

            safe[i] = null;
        }
    }

    return safe;
}());
}).call(this,require("buffer").Buffer)
},{"buffer":24}],18:[function(require,module,exports){
(function (process,Buffer){
// Load modules

var Path = require('path');
var Util = require('util');
var Escape = require('./escape');


// Declare internals

var internals = {};


// Clone object or array

exports.clone = function (obj, seen) {

    if (typeof obj !== 'object' ||
        obj === null) {

        return obj;
    }

    seen = seen || { orig: [], copy: [] };

    var lookup = seen.orig.indexOf(obj);
    if (lookup !== -1) {
        return seen.copy[lookup];
    }

    var newObj;
    var cloneDeep = false;

    if (!Array.isArray(obj)) {
        if (Buffer.isBuffer(obj)) {
            newObj = new Buffer(obj);
        }
        else if (obj instanceof Date) {
            newObj = new Date(obj.getTime());
        }
        else if (obj instanceof RegExp) {
            newObj = new RegExp(obj);
        }
        else {
            var proto = Object.getPrototypeOf(obj);
            if (!proto || proto.isImmutable) {
                newObj = obj;
            }
            else {
                newObj = {};
                newObj.__proto__ = proto;
                cloneDeep = true;
            }
        }
    }
    else {
        newObj = [];
        cloneDeep = true;
    }

    seen.orig.push(obj);
    seen.copy.push(newObj);

    if (cloneDeep) {
        for (var i in obj) {
            if (obj.hasOwnProperty(i)) {
                newObj[i] = exports.clone(obj[i], seen);
            }
        }
    }

    return newObj;
};


// Merge all the properties of source into target, source wins in conflict, and by default null and undefined from source are applied

exports.merge = function (target, source, isNullOverride /* = true */, isMergeObjects /* = true */) {

    exports.assert(target && typeof target === 'object', 'Invalid target value: must be an object');
    exports.assert(source === null || source === undefined || typeof source === 'object', 'Invalid source value: must be null, undefined, or an object');

    if (!source) {
        return target;
    }

    if (Array.isArray(source)) {
        exports.assert(Array.isArray(target), 'Cannot merge array onto an object');
        if (isMergeObjects === false) {                                                  // isMergeObjects defaults to true
            target.length = 0;                                                          // Must not change target assignment
        }

        for (var i = 0, il = source.length; i < il; ++i) {
            target.push(exports.clone(source[i]));
        }

        return target;
    }

    var keys = Object.keys(source);
    for (var k = 0, kl = keys.length; k < kl; ++k) {
        var key = keys[k];
        var value = source[key];
        if (value &&
            typeof value === 'object') {

            if (!target[key] ||
                typeof target[key] !== 'object' ||
                (Array.isArray(target[key]) ^ Array.isArray(value)) ||
                value instanceof Date ||
                Buffer.isBuffer(value) ||
                value instanceof RegExp) {

                target[key] = exports.clone(value);
            }
            else {
                exports.merge(target[key], value, isNullOverride, isMergeObjects);
            }
        }
        else {
            if (value !== null &&
                value !== undefined) {                              // Explicit to preserve empty strings

                target[key] = value;
            }
            else if (isNullOverride !== false) {                    // Defaults to true
                target[key] = value;
            }
        }
    }

    return target;
};


// Apply options to a copy of the defaults

exports.applyToDefaults = function (defaults, options) {

    exports.assert(defaults && typeof defaults === 'object', 'Invalid defaults value: must be an object');
    exports.assert(!options || options === true || typeof options === 'object', 'Invalid options value: must be true, falsy or an object');

    if (!options) {                                                 // If no options, return null
        return null;
    }

    var copy = exports.clone(defaults);

    if (options === true) {                                         // If options is set to true, use defaults
        return copy;
    }

    return exports.merge(copy, options, false, false);
};


// Clone an object except for the listed keys which are shallow copied

exports.cloneWithShallow = function (source, keys) {

    if (!source ||
        typeof source !== 'object') {

        return source;
    }

    return internals.shallow(source, keys, function () {

        return exports.clone(source);
    });
};


// Apply options to defaults except for the listed keys which are shallow copied from option without merging

exports.applyToDefaultsWithShallow = function (defaults, options, keys) {

    return internals.shallow(options, keys, function () {

        return exports.applyToDefaults(defaults, options);
    });
};


internals.shallow = function (source, keys, clone) {

    // Move shallow copy items to storage

    var storage = {};
    for (var i = 0, il = keys.length; i < il; ++i) {
        var key = keys[i];
        if (source.hasOwnProperty(key)) {
            storage[key] = source[key];
            source[key] = undefined;
        }
    }

    // Deep copy the rest

    var copy = clone();

    // Shallow copy the stored items

    for (i = 0; i < il; ++i) {
        var key = keys[i];
        if (storage.hasOwnProperty(key)) {
            source[key] = storage[key];
            copy[key] = storage[key];
        }
    }

    return copy;
};


// Remove duplicate items from array

exports.unique = function (array, key) {

    var index = {};
    var result = [];

    for (var i = 0, il = array.length; i < il; ++i) {
        var id = (key ? array[i][key] : array[i]);
        if (index[id] !== true) {

            result.push(array[i]);
            index[id] = true;
        }
    }

    return result;
};


// Convert array into object

exports.mapToObject = function (array, key) {

    if (!array) {
        return null;
    }

    var obj = {};
    for (var i = 0, il = array.length; i < il; ++i) {
        if (key) {
            if (array[i][key]) {
                obj[array[i][key]] = true;
            }
        }
        else {
            obj[array[i]] = true;
        }
    }

    return obj;
};


// Find the common unique items in two arrays

exports.intersect = function (array1, array2, justFirst) {

    if (!array1 || !array2) {
        return [];
    }

    var common = [];
    var hash = (Array.isArray(array1) ? exports.mapToObject(array1) : array1);
    var found = {};
    for (var i = 0, il = array2.length; i < il; ++i) {
        if (hash[array2[i]] && !found[array2[i]]) {
            if (justFirst) {
                return array2[i];
            }

            common.push(array2[i]);
            found[array2[i]] = true;
        }
    }

    return (justFirst ? null : common);
};


// Flatten array

exports.flatten = function (array, target) {

    var result = target || [];

    for (var i = 0, il = array.length; i < il; ++i) {
        if (Array.isArray(array[i])) {
            exports.flatten(array[i], result);
        }
        else {
            result.push(array[i]);
        }
    }

    return result;
};


// Convert an object key chain string ('a.b.c') to reference (object[a][b][c])

exports.reach = function (obj, chain, options) {

    options = options || {};
    if (typeof options === 'string') {
        options = { separator: options };
    }

    var path = chain.split(options.separator || '.');
    var ref = obj;
    for (var i = 0, il = path.length; i < il; ++i) {
        if (!ref ||
            !ref.hasOwnProperty(path[i]) ||
            (typeof ref !== 'object' && options.functions === false)) {         // Only object and function can have properties

            exports.assert(!options.strict || i + 1 === il, 'Missing segment', path[i], 'in reach path ', chain);
            exports.assert(typeof ref === 'object' || options.functions === true || typeof ref !== 'function', 'Invalid segment', path[i], 'in reach path ', chain);
            ref = options.default || undefined;
            break;
        }

        ref = ref[path[i]];
    }

    return ref;
};


exports.formatStack = function (stack) {

    var trace = [];
    for (var i = 0, il = stack.length; i < il; ++i) {
        var item = stack[i];
        trace.push([item.getFileName(), item.getLineNumber(), item.getColumnNumber(), item.getFunctionName(), item.isConstructor()]);
    }

    return trace;
};


exports.formatTrace = function (trace) {

    var display = [];

    for (var i = 0, il = trace.length; i < il; ++i) {
        var row = trace[i];
        display.push((row[4] ? 'new ' : '') + row[3] + ' (' + row[0] + ':' + row[1] + ':' + row[2] + ')');
    }

    return display;
};


exports.callStack = function (slice) {

    // http://code.google.com/p/v8/wiki/JavaScriptStackTraceApi

    var v8 = Error.prepareStackTrace;
    Error.prepareStackTrace = function (err, stack) {

        return stack;
    };

    var capture = {};
    Error.captureStackTrace(capture, arguments.callee);
    var stack = capture.stack;

    Error.prepareStackTrace = v8;

    var trace = exports.formatStack(stack);

    if (slice) {
        return trace.slice(slice);
    }

    return trace;
};


exports.displayStack = function (slice) {

    var trace = exports.callStack(slice === undefined ? 1 : slice + 1);

    return exports.formatTrace(trace);
};


exports.abortThrow = false;


exports.abort = function (message, hideStack) {

    if (process.env.NODE_ENV === 'test' || exports.abortThrow === true) {
        throw new Error(message || 'Unknown error');
    }

    var stack = '';
    if (!hideStack) {
        stack = exports.displayStack(1).join('\n\t');
    }
    console.log('ABORT: ' + message + '\n\t' + stack);
    process.exit(1);
};


exports.assert = function (condition /*, msg1, msg2, msg3 */) {

    if (condition) {
        return;
    }

    var msgs = [];
    for (var i = 1, il = arguments.length; i < il; ++i) {
        msgs.push(arguments[i]);            // Avoids Array.slice arguments leak, allowing for V8 optimizations
    }

    msgs = msgs.map(function (msg) {

        return typeof msg === 'string' ? msg : msg instanceof Error ? msg.message : JSON.stringify(msg);
    });
    throw new Error(msgs.join(' ') || 'Unknown error');
};


exports.Timer = function () {

    this.ts = 0;
    this.reset();
};


exports.Timer.prototype.reset = function () {

    this.ts = Date.now();
};


exports.Timer.prototype.elapsed = function () {

    return Date.now() - this.ts;
};


exports.Bench = function () {

    this.ts = 0;
    this.reset();
};


exports.Bench.prototype.reset = function () {

    this.ts = exports.Bench.now();
};


exports.Bench.prototype.elapsed = function () {

    return exports.Bench.now() - this.ts;
};


exports.Bench.now = function () {

    var ts = process.hrtime();
    return (ts[0] * 1e3) + (ts[1] / 1e6);
};


// Escape string for Regex construction

exports.escapeRegex = function (string) {

    // Escape ^$.*+-?=!:|\/()[]{},
    return string.replace(/[\^\$\.\*\+\-\?\=\!\:\|\\\/\(\)\[\]\{\}\,]/g, '\\$&');
};


// Base64url (RFC 4648) encode

exports.base64urlEncode = function (value, encoding) {

    var buf = (Buffer.isBuffer(value) ? value : new Buffer(value, encoding || 'binary'));
    return buf.toString('base64').replace(/\+/g, '-').replace(/\//g, '_').replace(/\=/g, '');
};


// Base64url (RFC 4648) decode

exports.base64urlDecode = function (value, encoding) {

    if (value &&
        !/^[\w\-]*$/.test(value)) {

        return new Error('Invalid character');
    }

    try {
        var buf = new Buffer(value, 'base64');
        return (encoding === 'buffer' ? buf : buf.toString(encoding || 'binary'));
    }
    catch (err) {
        return err;
    }
};


// Escape attribute value for use in HTTP header

exports.escapeHeaderAttribute = function (attribute) {

    // Allowed value characters: !#$%&'()*+,-./:;<=>?@[]^_`{|}~ and space, a-z, A-Z, 0-9, \, "

    exports.assert(/^[ \w\!#\$%&'\(\)\*\+,\-\.\/\:;<\=>\?@\[\]\^`\{\|\}~\"\\]*$/.test(attribute), 'Bad attribute value (' + attribute + ')');

    return attribute.replace(/\\/g, '\\\\').replace(/\"/g, '\\"');                             // Escape quotes and slash
};


exports.escapeHtml = function (string) {

    return Escape.escapeHtml(string);
};


exports.escapeJavaScript = function (string) {

    return Escape.escapeJavaScript(string);
};


exports.nextTick = function (callback) {

    return function () {

        var args = arguments;
        process.nextTick(function () {

            callback.apply(null, args);
        });
    };
};


exports.once = function (method) {

    if (method._hoekOnce) {
        return method;
    }

    var once = false;
    var wrapped = function () {

        if (!once) {
            once = true;
            method.apply(null, arguments);
        }
    };

    wrapped._hoekOnce = true;

    return wrapped;
};


exports.isAbsolutePath = function (path, platform) {

    if (!path) {
        return false;
    }

    if (Path.isAbsolute) {                      // node >= 0.11
        return Path.isAbsolute(path);
    }

    platform = platform || process.platform;

    // Unix

    if (platform !== 'win32') {
        return path[0] === '/';
    }

    // Windows

    return !!/^(?:[a-zA-Z]:[\\\/])|(?:[\\\/]{2}[^\\\/]+[\\\/]+[^\\\/])/.test(path);        // C:\ or \\something\something
};


exports.isInteger = function (value) {

    return (typeof value === 'number' &&
            parseFloat(value) === parseInt(value, 10) &&
            !isNaN(value));
};


exports.ignore = function () { };


exports.inherits = Util.inherits;


exports.transform = function (source, transform, options) {

    exports.assert(source == null || typeof source === 'object', 'Invalid source object: must be null, undefined, or an object');

    var result = {};
    var keys = Object.keys(transform);

    for (var k = 0, kl = keys.length; k < kl; ++k) {
        var key = keys[k];
        var path = key.split('.');
        var sourcePath = transform[key];

        exports.assert(typeof sourcePath === 'string', 'All mappings must be "." delineated strings');

        var segment;
        var res = result;

        while (path.length > 1) {
            segment = path.shift();
            if (!res[segment]) {
                res[segment] = {};
            }
            res = res[segment];
        }
        segment = path.shift();
        res[segment] = exports.reach(source, sourcePath, options);
    }

    return result;
};

}).call(this,require('_process'),require("buffer").Buffer)
},{"./escape":17,"_process":29,"buffer":24,"path":28,"util":31}],19:[function(require,module,exports){
module.exports = require('./lib/isemail');

},{"./lib/isemail":20}],20:[function(require,module,exports){
(function (process){
/**
 * To validate an email address according to RFCs 5321, 5322 and others
 *
 * Copyright © 2008-2011, Dominic Sayers
 * Test schema documentation Copyright © 2011, Daniel Marschall
 * Port for Node.js Copyright © 2013, GlobeSherpa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   - Neither the name of Dominic Sayers nor the names of its contributors may
 *     be used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author      Dominic Sayers <dominic@sayers.cc>
 * @author      Eli Skeggs <eskeggs@globesherpa.com>
 * @copyright   2008-2011 Dominic Sayers
 * @copyright   2013-2014 GlobeSherpa
 * @license     http://www.opensource.org/licenses/bsd-license.php BSD License
 * @link        http://www.dominicsayers.com/isemail
 * @link        https://github.com/globesherpa/isemail
 * @version     1.0.1 - Increase test coverage, minor style fixes.
 */

// lazy-loaded
var dns, HAS_REQUIRE = typeof require !== 'undefined';

// categories
var ISEMAIL_VALID_CATEGORY = 1;
var ISEMAIL_DNSWARN = 7;
var ISEMAIL_RFC5321 = 15;
var ISEMAIL_CFWS = 31;
var ISEMAIL_DEPREC = 63;
var ISEMAIL_RFC5322 = 127;
var ISEMAIL_ERR = 255;

// diagnoses
// address is valid
var ISEMAIL_VALID = 0;
// address is valid but a DNS check was not successful
var ISEMAIL_DNSWARN_NO_MX_RECORD = 5;
var ISEMAIL_DNSWARN_NO_RECORD = 6;
// address is valid for SMTP but has unusual elements
var ISEMAIL_RFC5321_TLD = 9;
var ISEMAIL_RFC5321_TLDNUMERIC = 10;
var ISEMAIL_RFC5321_QUOTEDSTRING = 11;
var ISEMAIL_RFC5321_ADDRESSLITERAL = 12;
var ISEMAIL_RFC5321_IPV6DEPRECATED = 13;
// address is valid within the message but cannot be used unmodified for the envelope
var ISEMAIL_CFWS_COMMENT = 17;
var ISEMAIL_CFWS_FWS = 18;
// address contains deprecated elements but may still be valid in restricted contexts
var ISEMAIL_DEPREC_LOCALPART = 33;
var ISEMAIL_DEPREC_FWS = 34;
var ISEMAIL_DEPREC_QTEXT = 35;
var ISEMAIL_DEPREC_QP = 36;
var ISEMAIL_DEPREC_COMMENT = 37;
var ISEMAIL_DEPREC_CTEXT = 38;
var ISEMAIL_DEPREC_CFWS_NEAR_AT = 49;
// the address is only valid according to the broad definition of RFC 5322, but otherwise invalid
var ISEMAIL_RFC5322_DOMAIN = 65;
var ISEMAIL_RFC5322_TOOLONG = 66;
var ISEMAIL_RFC5322_LOCAL_TOOLONG = 67;
var ISEMAIL_RFC5322_DOMAIN_TOOLONG = 68;
var ISEMAIL_RFC5322_LABEL_TOOLONG = 69;
var ISEMAIL_RFC5322_DOMAINLITERAL = 70;
var ISEMAIL_RFC5322_DOMLIT_OBSDTEXT = 71;
var ISEMAIL_RFC5322_IPV6_GRPCOUNT = 72;
var ISEMAIL_RFC5322_IPV6_2X2XCOLON = 73;
var ISEMAIL_RFC5322_IPV6_BADCHAR = 74;
var ISEMAIL_RFC5322_IPV6_MAXGRPS = 75;
var ISEMAIL_RFC5322_IPV6_COLONSTRT = 76;
var ISEMAIL_RFC5322_IPV6_COLONEND = 77;
// address is invalid for any purpose
var ISEMAIL_ERR_EXPECTING_DTEXT = 129;
var ISEMAIL_ERR_NOLOCALPART = 130;
var ISEMAIL_ERR_NODOMAIN = 131;
var ISEMAIL_ERR_CONSECUTIVEDOTS = 132;
var ISEMAIL_ERR_ATEXT_AFTER_CFWS = 133;
var ISEMAIL_ERR_ATEXT_AFTER_QS = 134;
var ISEMAIL_ERR_ATEXT_AFTER_DOMLIT = 135;
var ISEMAIL_ERR_EXPECTING_QPAIR = 136;
var ISEMAIL_ERR_EXPECTING_ATEXT = 137;
var ISEMAIL_ERR_EXPECTING_QTEXT = 138;
var ISEMAIL_ERR_EXPECTING_CTEXT = 139;
var ISEMAIL_ERR_BACKSLASHEND = 140;
var ISEMAIL_ERR_DOT_START = 141;
var ISEMAIL_ERR_DOT_END = 142;
var ISEMAIL_ERR_DOMAINHYPHENSTART = 143;
var ISEMAIL_ERR_DOMAINHYPHENEND = 144;
var ISEMAIL_ERR_UNCLOSEDQUOTEDSTR = 145;
var ISEMAIL_ERR_UNCLOSEDCOMMENT = 146;
var ISEMAIL_ERR_UNCLOSEDDOMLIT = 147;
var ISEMAIL_ERR_FWS_CRLF_X2 = 148;
var ISEMAIL_ERR_FWS_CRLF_END = 149;
var ISEMAIL_ERR_CR_NO_LF = 150;

// function control
var ISEMAIL_THRESHOLD = 16;
// email parts
var ISEMAIL_COMPONENT_LOCALPART = 0;
var ISEMAIL_COMPONENT_DOMAIN = 1;
var ISEMAIL_COMPONENT_LITERAL = 2;
var ISEMAIL_CONTEXT_COMMENT = 3;
var ISEMAIL_CONTEXT_FWS = 4;
var ISEMAIL_CONTEXT_QUOTEDSTRING = 5;
var ISEMAIL_CONTEXT_QUOTEDPAIR = 6;
// US-ASCII visible characters not valid for atext (http://tools.ietf.org/html/rfc5322#section-3.2.3)
var SPECIALS = '()<>[]:;@\\,."';

// matches valid IPv4 addresses from the end of a string
var IPv4_REGEX = /\b(?:(?:25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(?:25[0-5]|2[0-4]\d|[01]?\d\d?)$/;
var IPv6_REGEX = /^[a-fA-F\d]{0,4}$/, IPv6_REGEX_TEST = IPv6_REGEX.test.bind(IPv6_REGEX);

/**
 * Get the largest number in the array.
 *
 * Returns -Infinity if the array is empty.
 *
 * @param {Array.<number>} array The array to scan.
 * @return {number} The largest number contained.
 */
function maxValue(array) {
  var v = -Infinity, i = 0, n = array.length;

  for (; i < n; i++) {
    if (array[i] > v) {
      v = array[i];
    }
  }

  return v;
}

/**
 * Check that an email address conforms to RFCs 5321, 5322 and others
 *
 * As of Version 3.0, we are now distinguishing clearly between a Mailbox
 * as defined by RFC 5321 and an addr-spec as defined by RFC 5322. Depending
 * on the context, either can be regarded as a valid email address. The
 * RFC 5321 Mailbox specification is more restrictive (comments, white space
 * and obsolete forms are not allowed).
 *
 * @param {string} email The email address to check.
 * @param {boolean} checkDNS If true then will check DNS for MX records. If true
 *   this isEmail _will_ be asynchronous.
 * @param {*} errorLevel Determines the boundary between valid and invalid
 *   addresses. Status codes above this number will be returned as-is, status
 *   codes below will be returned as ISEMAIL_VALID. Thus the calling program can
 *   simply look for ISEMAIL_VALID if it is only interested in whether an
 *   address is valid or not. The errorLevel will determine how "picky"
 *   isEmail() is about the address. If omitted or passed as false then
 *   isEmail() will return true or false rather than an integer error or
 *   warning. NB Note the difference between errorLevel = false and
 *   errorLevel = 0.
 * @return {*}
 */
function isEmail(email, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = {};
  }
  options || (options = {});

  var threshold, diagnose;
  if (typeof options.errorLevel === 'number') {
    diagnose = true;
    threshold = options.errorLevel;
  } else {
    diagnose = !!options.errorLevel;
    threshold = ISEMAIL_VALID;
  }

  var result = [ISEMAIL_VALID];

  var context = {
    now: ISEMAIL_COMPONENT_LOCALPART,
    prev: ISEMAIL_COMPONENT_LOCALPART,
    stack: [ISEMAIL_COMPONENT_LOCALPART]
  };

  var token = '', prevToken = '', charCode = 0, parseData = {}, atomList = {};
  parseData[ISEMAIL_COMPONENT_LOCALPART] = '';
  parseData[ISEMAIL_COMPONENT_DOMAIN] = '';
  atomList[ISEMAIL_COMPONENT_LOCALPART] = [''];
  atomList[ISEMAIL_COMPONENT_DOMAIN] = [''];

  var elementCount = 0, elementLength = 0, hyphenFlag = false, assertEnd = false;
  var crlfCount = 0;

  for (var i = 0; i < email.length; i++) {
    token = email[i];

    switch (context.now) {
    // local-part
    case ISEMAIL_COMPONENT_LOCALPART:
      // http://tools.ietf.org/html/rfc5322#section-3.4.1
      //   local-part      =   dot-atom / quoted-string / obs-local-part
      //
      //   dot-atom        =   [CFWS] dot-atom-text [CFWS]
      //
      //   dot-atom-text   =   1*atext *("." 1*atext)
      //
      //   quoted-string   =   [CFWS]
      //                       DQUOTE *([FWS] qcontent) [FWS] DQUOTE
      //                       [CFWS]
      //
      //   obs-local-part  =   word *("." word)
      //
      //   word            =   atom / quoted-string
      //
      //   atom            =   [CFWS] 1*atext [CFWS]
      switch (token) {
      // comment
      case '(':
        if (elementLength === 0) {
          // comments are OK at the beginning of an element
          result.push(elementCount === 0 ? ISEMAIL_CFWS_COMMENT : ISEMAIL_DEPREC_COMMENT);
        } else {
          result.push(ISEMAIL_CFWS_COMMENT);
          assertEnd = true; // can't start a comment in an element, should be end
        }
        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_COMMENT;
        break;
      // next dot-atom element
      case '.':
        if (elementLength === 0) {
          // another dot, already?
          result.push(elementCount === 0 ? ISEMAIL_ERR_DOT_START : ISEMAIL_ERR_CONSECUTIVEDOTS);
        } else {
          // the entire local-part can be a quoted string for RFC 5321
          // if it's just one atom that is quoted then it's an RFC 5322 obsolete form
          if (assertEnd) {
            result.push(ISEMAIL_DEPREC_LOCALPART);
          }

          // CFWS & quoted strings are OK again now we're at the beginning of an element (although they are obsolete forms)
          assertEnd = false;
          elementLength = 0;
          elementCount++;
          parseData[ISEMAIL_COMPONENT_LOCALPART] += token;
          atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] = ''; // TODO: push?
        }
        break;
      // quoted string
      case '"':
        if (elementLength === 0) {
          // the entire local-part can be a quoted string for RFC 5321
          // if it's just one atom that is quoted then it's an RFC 5322 obsolete form
          result.push(elementCount === 0 ? ISEMAIL_RFC5321_QUOTEDSTRING : ISEMAIL_DEPREC_LOCALPART);

          parseData[ISEMAIL_COMPONENT_LOCALPART] += token;
          atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] += token;
          elementLength++;
          assertEnd = true; // quoted string must be the entire element
          context.stack.push(context.now);
          context.now = ISEMAIL_CONTEXT_QUOTEDSTRING;
        } else {
          result.push(ISEMAIL_ERR_EXPECTING_ATEXT);
        }
        break;
      // folding white space
      case '\r':
      case ' ':
      case '\t':
        if (token === '\r' && ((++i === email.length) || email[i] !== '\n')) {
          // fatal error
          result.push(ISEMAIL_ERR_CR_NO_LF);
          break;
        }
        if (elementLength === 0) {
          result.push(elementCount === 0 ? ISEMAIL_CFWS_FWS : ISEMAIL_DEPREC_FWS);
        } else {
          // we can't start FWS in the middle of an element, better be end
          assertEnd = true;
        }

        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_FWS;
        prevToken = token;
        break;
      // @
      case '@':
        // at this point we should have a valid local-part
        /* istanbul ignore next: logically unreachable */
        if (context.stack.length !== 1) {
          throw new Error('unexpected item on context stack');
        }

        if (parseData[ISEMAIL_COMPONENT_LOCALPART].length === 0) {
          // fatal error
          result.push(ISEMAIL_ERR_NOLOCALPART);
        } else if (elementLength === 0) {
          // fatal error
          result.push(ISEMAIL_ERR_DOT_END);
        // http://tools.ietf.org/html/rfc5321#section-4.5.3.1.1
        //   the maximum total length of a user name or other local-part is 64
        //   octets
        } else if (parseData[ISEMAIL_COMPONENT_LOCALPART].length > 64) {
          result.push(ISEMAIL_RFC5322_LOCAL_TOOLONG);
        // http://tools.ietf.org/html/rfc5322#section-3.4.1
        //   comments and folding white space
        //   SHOULD NOT be used around the "@" in the addr-spec
        //
        // http://tools.ietf.org/html/rfc2119
        // 4. SHOULD NOT  this phrase, or the phrase "NOT RECOMMENDED" mean that
        //    there may exist valid reasons in particular circumstances when the
        //    particular behavior is acceptable or even useful, but the full
        //    implications should be understood and the case carefully weighed
        //    before implementing any behavior described with this label
        } else if ((context.prev === ISEMAIL_CONTEXT_COMMENT) || (context.prev === ISEMAIL_CONTEXT_FWS)) {
          result.push(ISEMAIL_DEPREC_CFWS_NEAR_AT);
        }

        // clear everything down for the domain parsing
        context.now = ISEMAIL_COMPONENT_DOMAIN; // where we are
        context.stack = [ISEMAIL_COMPONENT_DOMAIN]; // where we have been
        elementCount = 0;
        elementLength = 0;
        assertEnd = false; // CFWS can only appear at the end of the element
        break;
      // atext
      default:
        // http://tools.ietf.org/html/rfc5322#section-3.2.3
        //    atext = ALPHA / DIGIT / ; Printable US-ASCII
        //            "!" / "#" /     ;  characters not including
        //            "$" / "%" /     ;  specials.  Used for atoms.
        //            "&" / "'" /
        //            "*" / "+" /
        //            "-" / "/" /
        //            "=" / "?" /
        //            "^" / "_" /
        //            "`" / "{" /
        //            "|" / "}" /
        //            "~"
        if (assertEnd) {
          // we have encountered atext where it is no longer valid
          switch (context.prev) {
          case ISEMAIL_CONTEXT_COMMENT:
          case ISEMAIL_CONTEXT_FWS:
            result.push(ISEMAIL_ERR_ATEXT_AFTER_CFWS);
            break;
          case ISEMAIL_CONTEXT_QUOTEDSTRING:
            result.push(ISEMAIL_ERR_ATEXT_AFTER_QS);
            break;
          /* istanbul ignore next: logically unreachable */
          default:
            throw new Error('more atext found where none is allowed, but unrecognized prev context: ' + context.prev);
          }
        } else {
          context.prev = context.now;
          charCode = token.charCodeAt(0);

          if (charCode < 33 || charCode > 126 || charCode === 10 || ~SPECIALS.indexOf(token)) {
            // fatal error
            result.push(ISEMAIL_ERR_EXPECTING_ATEXT);
          }

          parseData[ISEMAIL_COMPONENT_LOCALPART] += token;
          atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] += token;
          elementLength++;
        }
      }
      break;
    case ISEMAIL_COMPONENT_DOMAIN:
      // http://tools.ietf.org/html/rfc5322#section-3.4.1
      //   domain          =   dot-atom / domain-literal / obs-domain
      //
      //   dot-atom        =   [CFWS] dot-atom-text [CFWS]
      //
      //   dot-atom-text   =   1*atext *("." 1*atext)
      //
      //   domain-literal  =   [CFWS] "[" *([FWS] dtext) [FWS] "]" [CFWS]
      //
      //   dtext           =   %d33-90 /          ; Printable US-ASCII
      //                       %d94-126 /         ;  characters not including
      //                       obs-dtext          ;  "[", "]", or "\"
      //
      //   obs-domain      =   atom *("." atom)
      //
      //   atom            =   [CFWS] 1*atext [CFWS]

      // http://tools.ietf.org/html/rfc5321#section-4.1.2
      //   Mailbox        = Local-part "@" ( Domain / address-literal )
      //
      //   Domain         = sub-domain *("." sub-domain)
      //
      //   address-literal  = "[" ( IPv4-address-literal /
      //                    IPv6-address-literal /
      //                    General-address-literal ) "]"
      //                    ; See Section 4.1.3

      // http://tools.ietf.org/html/rfc5322#section-3.4.1
      //      Note: A liberal syntax for the domain portion of addr-spec is
      //      given here.  However, the domain portion contains addressing
      //      information specified by and used in other protocols (e.g.,
      //      [RFC1034], [RFC1035], [RFC1123], [RFC5321]).  It is therefore
      //      incumbent upon implementations to conform to the syntax of
      //      addresses for the context in which they are used.
      // is_email() author's note: it's not clear how to interpret this in
      // the context of a general email address validator. The conclusion I
      // have reached is this: "addressing information" must comply with
      // RFC 5321 (and in turn RFC 1035), anything that is "semantically
      // invisible" must comply only with RFC 5322.
      switch (token) {
      // comment
      case '(':
        if (elementLength === 0) {
          // comments at the start of the domain are deprecated in the text
          // comments at the start of a subdomain are obs-domain
          // (http://tools.ietf.org/html/rfc5322#section-3.4.1)
          result.push(elementCount === 0 ? ISEMAIL_DEPREC_CFWS_NEAR_AT : ISEMAIL_DEPREC_COMMENT);
        } else {
          result.push(ISEMAIL_CFWS_COMMENT);
          assertEnd = true; // can't start a comment mid-element, better be end
        }

        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_COMMENT;
        break;
      // next dot-atom element
      case '.':
        if (elementLength === 0) {
          // another dot, already?
          result.push(elementCount === 0 ? ISEMAIL_ERR_DOT_START : ISEMAIL_ERR_CONSECUTIVEDOTS); // fatal error
        } else if (hyphenFlag) {
          // previous subdomain ended in a hyphen
          result.push(ISEMAIL_ERR_DOMAINHYPHENEND); // fatal error
        } else if (elementLength > 63) {
          // Nowhere in RFC 5321 does it say explicitly that the
          // domain part of a Mailbox must be a valid domain according
          // to the DNS standards set out in RFC 1035, but this *is*
          // implied in several places. For instance, wherever the idea
          // of host routing is discussed the RFC says that the domain
          // must be looked up in the DNS. This would be nonsense unless
          // the domain was designed to be a valid DNS domain. Hence we
          // must conclude that the RFC 1035 restriction on label length
          // also applies to RFC 5321 domains.
          //
          // http://tools.ietf.org/html/rfc1035#section-2.3.4
          // labels          63 octets or less

          result.push(ISEMAIL_RFC5322_LABEL_TOOLONG);
        }

        // CFWS is OK again now we're at the beginning of an element (although it may be obsolete CFWS)
        assertEnd = false;
        elementLength = 0;
        elementCount++;
        atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount] = '';
        parseData[ISEMAIL_COMPONENT_DOMAIN] += token;

        break;
      // domain literal
      case '[':
        if (parseData[ISEMAIL_COMPONENT_DOMAIN].length === 0) {
          // domain literal must be the only component
          assertEnd = true;
          elementLength++;
          context.stack.push(context.now);
          context.now = ISEMAIL_COMPONENT_LITERAL;
          parseData[ISEMAIL_COMPONENT_DOMAIN] += token;
          atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount] += token;
          parseData[ISEMAIL_COMPONENT_LITERAL] = '';
        } else {
          // fatal error
          result.push(ISEMAIL_ERR_EXPECTING_ATEXT);
        }
        break;
      // folding white space
      case '\r':
      case ' ':
      case '\t':
        if (token === '\r' && ((++i === email.length) || email[i] !== '\n')) {
          // fatal error
          result.push(ISEMAIL_ERR_CR_NO_LF);
          break;
        }

        if (elementLength === 0) {
          result.push(elementCount === 0 ? ISEMAIL_DEPREC_CFWS_NEAR_AT : ISEMAIL_DEPREC_FWS);
        } else {
          // We can't start FWS in the middle of an element, so this better be the end
          result.push(ISEMAIL_CFWS_FWS);
          assertEnd = true;
        }

        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_FWS;
        prevToken = token;
        break;
      // atext
      default:
        // RFC 5322 allows any atext...
        // http://tools.ietf.org/html/rfc5322#section-3.2.3
        //    atext = ALPHA / DIGIT / ; Printable US-ASCII
        //            "!" / "#" /     ;  characters not including
        //            "$" / "%" /     ;  specials.  Used for atoms.
        //            "&" / "'" /
        //            "*" / "+" /
        //            "-" / "/" /
        //            "=" / "?" /
        //            "^" / "_" /
        //            "`" / "{" /
        //            "|" / "}" /
        //            "~"

        // But RFC 5321 only allows letter-digit-hyphen to comply with DNS rules (RFCs 1034 & 1123)
        // http://tools.ietf.org/html/rfc5321#section-4.1.2
        //   sub-domain     = Let-dig [Ldh-str]
        //
        //   Let-dig        = ALPHA / DIGIT
        //
        //   Ldh-str        = *( ALPHA / DIGIT / "-" ) Let-dig
        //
        if (assertEnd) {
          // we have encountered atext where it is no longer valid
          switch (context.prev) {
          case ISEMAIL_CONTEXT_COMMENT:
          case ISEMAIL_CONTEXT_FWS:
            result.push(ISEMAIL_ERR_ATEXT_AFTER_CFWS);
            break;
          case ISEMAIL_COMPONENT_LITERAL:
            result.push(ISEMAIL_ERR_ATEXT_AFTER_DOMLIT);
            break;
          /* istanbul ignore next: logically unreachable */
          default:
            throw new Error('more atext found where none is allowed, but unrecognized prev context: ' + context.prev);
          }
        }

        charCode = token.charCodeAt(0);
        hyphenFlag = false; // assume this token isn't a hyphen unless we discover it is

        if (charCode < 33 || charCode > 126 || ~SPECIALS.indexOf(token)) {
          // fatal error
          result.push(ISEMAIL_ERR_EXPECTING_ATEXT);
        } else if (token === '-') {
          if (elementLength === 0) {
            // hyphens can't be at the beginning of a subdomain
            result.push(ISEMAIL_ERR_DOMAINHYPHENSTART); // fatal error
          }

          hyphenFlag = true;
        } else if (!((charCode > 47 && charCode < 58) || (charCode > 64 && charCode < 91) || (charCode > 96 && charCode < 123))) {
          // not an RFC 5321 subdomain, but still OK by RFC 5322
          result.push(ISEMAIL_RFC5322_DOMAIN);
        }

        parseData[ISEMAIL_COMPONENT_DOMAIN] += token;
        atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount] += token;
        elementLength++;
      }
      break;
    // domain literal
    case ISEMAIL_COMPONENT_LITERAL:
      // http://tools.ietf.org/html/rfc5322#section-3.4.1
      //   domain-literal  =   [CFWS] "[" *([FWS] dtext) [FWS] "]" [CFWS]
      //
      //   dtext           =   %d33-90 /          ; Printable US-ASCII
      //                       %d94-126 /         ;  characters not including
      //                       obs-dtext          ;  "[", "]", or "\"
      //
      //   obs-dtext       =   obs-NO-WS-CTL / quoted-pair
      switch (token) {
      // end of domain literal
      case ']':
        if (maxValue(result) < ISEMAIL_DEPREC) {
          // Could be a valid RFC 5321 address literal, so let's check

          // http://tools.ietf.org/html/rfc5321#section-4.1.2
          //   address-literal  = "[" ( IPv4-address-literal /
          //                    IPv6-address-literal /
          //                    General-address-literal ) "]"
          //                    ; See Section 4.1.3
          //
          // http://tools.ietf.org/html/rfc5321#section-4.1.3
          //   IPv4-address-literal  = Snum 3("."  Snum)
          //
          //   IPv6-address-literal  = "IPv6:" IPv6-addr
          //
          //   General-address-literal  = Standardized-tag ":" 1*dcontent
          //
          //   Standardized-tag  = Ldh-str
          //                     ; Standardized-tag MUST be specified in a
          //                     ; Standards-Track RFC and registered with IANA
          //
          //   dcontent       = %d33-90 / ; Printable US-ASCII
          //                  %d94-126 ; excl. "[", "\", "]"
          //
          //   Snum           = 1*3DIGIT
          //                  ; representing a decimal integer
          //                  ; value in the range 0 through 255
          //
          //   IPv6-addr      = IPv6-full / IPv6-comp / IPv6v4-full / IPv6v4-comp
          //
          //   IPv6-hex       = 1*4HEXDIG
          //
          //   IPv6-full      = IPv6-hex 7(":" IPv6-hex)
          //
          //   IPv6-comp      = [IPv6-hex *5(":" IPv6-hex)] "::"
          //                  [IPv6-hex *5(":" IPv6-hex)]
          //                  ; The "::" represents at least 2 16-bit groups of
          //                  ; zeros.  No more than 6 groups in addition to the
          //                  ; "::" may be present.
          //
          //   IPv6v4-full    = IPv6-hex 5(":" IPv6-hex) ":" IPv4-address-literal
          //
          //   IPv6v4-comp    = [IPv6-hex *3(":" IPv6-hex)] "::"
          //                  [IPv6-hex *3(":" IPv6-hex) ":"]
          //                  IPv4-address-literal
          //                  ; The "::" represents at least 2 16-bit groups of
          //                  ; zeros.  No more than 4 groups in addition to the
          //                  ; "::" and IPv4-address-literal may be present.
          //
          // is_email() author's note: We can't use ip2long() to validate
          // IPv4 addresses because it accepts abbreviated addresses
          // (xxx.xxx.xxx), expanding the last group to complete the address.
          // filter_var() validates IPv6 address inconsistently (up to PHP 5.3.3
          // at least) -- see http://bugs.php.net/bug.php?id=53236 for example

          // TODO: var here?
          var maxGroups = 8, matchesIP, index = false;
          var addressLiteral = parseData[ISEMAIL_COMPONENT_LITERAL];

          // extract IPv4 part from the end of the address-literal (if applicable)
          if (matchesIP = IPv4_REGEX.exec(addressLiteral)) {
            if ((index = matchesIP.index) !== 0) {
              // convert IPv4 part to IPv6 format for futher testing
              addressLiteral = addressLiteral.substr(0, matchesIP.index) + '0:0';
            }
          }

          if (index === 0) {
            // nothing there except a valid IPv4 address, so...
            result.push(ISEMAIL_RFC5321_ADDRESSLITERAL);
          } else if (addressLiteral.slice(0, 5).toLowerCase() !== 'ipv6:') {
            result.push(ISEMAIL_RFC5322_DOMAINLITERAL);
          } else {
            var match = addressLiteral.substr(5);
            matchesIP = match.split(':');
            index = match.indexOf('::');

            if (!~index) {
              // need exactly the right number of groups
              if (matchesIP.length !== maxGroups) {
                result.push(ISEMAIL_RFC5322_IPV6_GRPCOUNT);
              }
            } else if (index !== match.lastIndexOf('::')) {
              result.push(ISEMAIL_RFC5322_IPV6_2X2XCOLON);
            } else {
              if (index === 0 || index === match.length - 2) {
                // RFC 4291 allows :: at the start or end of an address with 7 other groups in addition
                maxGroups++;
              }

              if (matchesIP.length > maxGroups) {
                result.push(ISEMAIL_RFC5322_IPV6_MAXGRPS);
              } else if (matchesIP.length === maxGroups) {
                // eliding a single "::"
                result.push(ISEMAIL_RFC5321_IPV6DEPRECATED);
              }
            }

            // IPv6 testing strategy
            if (match[0] === ':' && match[1] !== ':') {
              result.push(ISEMAIL_RFC5322_IPV6_COLONSTRT);
            } else if (match[match.length - 1] === ':' && match[match.length - 2] !== ':') {
              result.push(ISEMAIL_RFC5322_IPV6_COLONEND);
            } else if (matchesIP.every(IPv6_REGEX_TEST)) {
              result.push(ISEMAIL_RFC5321_ADDRESSLITERAL);
            } else {
              result.push(ISEMAIL_RFC5322_IPV6_BADCHAR);
            }
          }
        } else {
          result.push(ISEMAIL_RFC5322_DOMAINLITERAL);
        }

        parseData[ISEMAIL_COMPONENT_DOMAIN] += token;
        atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount] += token;
        elementLength++;
        context.prev = context.now;
        context.now = context.stack.pop();
        break;
      case '\\':
        result.push(ISEMAIL_RFC5322_DOMLIT_OBSDTEXT);
        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_QUOTEDPAIR;
        break;
      // folding white space
      case '\r':
      case ' ':
      case '\t':
        if (token === '\r' && ((++i === email.length) || email[i] !== '\n')) {
          // fatal error
          result.push(ISEMAIL_ERR_CR_NO_LF);
          break;
        }

        result.push(ISEMAIL_CFWS_FWS);

        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_FWS;
        prevToken = token;
        break;
      // dtext
      default:
        // http://tools.ietf.org/html/rfc5322#section-3.4.1
        //   dtext         =   %d33-90 /  ; Printable US-ASCII
        //                     %d94-126 / ;  characters not including
        //                     obs-dtext  ;  "[", "]", or "\"
        //
        //   obs-dtext     =   obs-NO-WS-CTL / quoted-pair
        //
        //   obs-NO-WS-CTL =   %d1-8 /    ; US-ASCII control
        //                     %d11 /     ;  characters that do not
        //                     %d12 /     ;  include the carriage
        //                     %d14-31 /  ;  return, line feed, and
        //                     %d127      ;  white space characters
        charCode = token.charCodeAt(0);

        // CR, LF, SP & HTAB have already been parsed above
        if (charCode > 127 || charCode === 0 || token === '[') {
          // fatal error
          result.push(ISEMAIL_ERR_EXPECTING_DTEXT);
          break;
        } else if (charCode < 33 || charCode === 127) {
          result.push(ISEMAIL_RFC5322_DOMLIT_OBSDTEXT);
        }

        parseData[ISEMAIL_COMPONENT_LITERAL] += token;
        parseData[ISEMAIL_COMPONENT_DOMAIN] += token;
        atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount] += token;
        elementLength++;
      }
      break;
    // quoted string
    case ISEMAIL_CONTEXT_QUOTEDSTRING:
      // http://tools.ietf.org/html/rfc5322#section-3.2.4
      //   quoted-string = [CFWS]
      //                   DQUOTE *([FWS] qcontent) [FWS] DQUOTE
      //                   [CFWS]
      //
      //   qcontent      = qtext / quoted-pair
      switch (token) {
      // quoted pair
      case '\\':
        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_QUOTEDPAIR;
        break;
      // folding white space
      // inside a quoted string, spaces are allowed as regular characters
      // it's only FWS if we include HTAB or CRLF
      case '\r':
      case '\t':
        if (token === '\r' && ((++i === email.length) || email[i] !== '\n')) {
          // fatal error
          result.push(ISEMAIL_ERR_CR_NO_LF);
          break;
        }

        // http://tools.ietf.org/html/rfc5322#section-3.2.2
        //   Runs of FWS, comment, or CFWS that occur between lexical tokens in a
        //   structured header field are semantically interpreted as a single
        //   space character.

        // http://tools.ietf.org/html/rfc5322#section-3.2.4
        //   the CRLF in any FWS/CFWS that appears within the quoted-string [is]
        //   semantically "invisible" and therefore not part of the quoted-string

        parseData[ISEMAIL_COMPONENT_LOCALPART] += ' ';
        atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] += ' ';
        elementLength++;

        result.push(ISEMAIL_CFWS_FWS);
        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_FWS;
        prevToken = token;
        break;
      // end of quoted string
      case '"':
        parseData[ISEMAIL_COMPONENT_LOCALPART] += token;
        atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] += token;
        elementLength++;
        context.prev = context.now;
        context.now = context.stack.pop();
        break;
      // qtext
      default:
        // http://tools.ietf.org/html/rfc5322#section-3.2.4
        //   qtext           =   %d33 /             ; Printable US-ASCII
        //                       %d35-91 /          ;  characters not including
        //                       %d93-126 /         ;  "\" or the quote character
        //                       obs-qtext
        //
        //   obs-qtext       =   obs-NO-WS-CTL
        //
        //   obs-NO-WS-CTL   =   %d1-8 /            ; US-ASCII control
        //                       %d11 /             ;  characters that do not
        //                       %d12 /             ;  include the carriage
        //                       %d14-31 /          ;  return, line feed, and
        //                       %d127              ;  white space characters
        charCode = token.charCodeAt(0);

        if (charCode > 127 || charCode === 0 || charCode === 10) {
          result.push(ISEMAIL_ERR_EXPECTING_QTEXT);
        } else if (charCode < 32 || charCode === 127) {
          result.push(ISEMAIL_DEPREC_QTEXT);
        }

        parseData[ISEMAIL_COMPONENT_LOCALPART] += token;
        atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] += token;
        elementLength++;
      }

      // http://tools.ietf.org/html/rfc5322#section-3.4.1
      //   If the string can be represented as a dot-atom (that is, it contains
      //   no characters other than atext characters or "." surrounded by atext
      //   characters), then the dot-atom form SHOULD be used and the quoted-
      //   string form SHOULD NOT be used.

      break;
    // quoted pair
    case ISEMAIL_CONTEXT_QUOTEDPAIR:
      // http://tools.ietf.org/html/rfc5322#section-3.2.1
      //   quoted-pair     =   ("\" (VCHAR / WSP)) / obs-qp
      //
      //   VCHAR           =  %d33-126   ; visible (printing) characters
      //   WSP             =  SP / HTAB  ; white space
      //
      //   obs-qp          =   "\" (%d0 / obs-NO-WS-CTL / LF / CR)
      //
      //   obs-NO-WS-CTL   =   %d1-8 /   ; US-ASCII control
      //                       %d11 /    ;  characters that do not
      //                       %d12 /    ;  include the carriage
      //                       %d14-31 / ;  return, line feed, and
      //                       %d127     ;  white space characters
      //
      // i.e. obs-qp       =  "\" (%d0-8, %d10-31 / %d127)
      charCode = token.charCodeAt(0);

      if (charCode > 127) {
        // fatal error
        result.push(ISEMAIL_ERR_EXPECTING_QPAIR);
      } else if ((charCode < 31 && charCode !== 9) || charCode === 127) {
        // SP & HTAB are allowed
        result.push(ISEMAIL_DEPREC_QP);
      }

      // At this point we know where this qpair occurred so
      // we could check to see if the character actually
      // needed to be quoted at all.
      // http://tools.ietf.org/html/rfc5321#section-4.1.2
      //   the sending system SHOULD transmit the
      //   form that uses the minimum quoting possible.

      // To do: check whether the character needs to be quoted (escaped) in this context

      context.prev = context.now;
      context.now = context.stack.pop(); // end of qpair
      token = '\\' + token;

      switch (context.now) {
      case ISEMAIL_CONTEXT_COMMENT: break;
      case ISEMAIL_CONTEXT_QUOTEDSTRING:
        parseData[ISEMAIL_COMPONENT_LOCALPART] += token;
        atomList[ISEMAIL_COMPONENT_LOCALPART][elementCount] += token;
        elementLength += 2; // the maximum sizes specified by RFC 5321 are octet counts, so we must include the backslash
        break;
      case ISEMAIL_COMPONENT_LITERAL:
        parseData[ISEMAIL_COMPONENT_DOMAIN] += token;
        atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount] += token;
        elementLength += 2; // the maximum sizes specified by RFC 5321 are octet counts, so we must include the backslash
        break;
      /* istanbul ignore next: logically unreachable */
      default:
        throw new Error('quoted pair logic invoked in an invalid context: ' + context.now);
      }
      break;
    // comment
    case ISEMAIL_CONTEXT_COMMENT:
      // http://tools.ietf.org/html/rfc5322#section-3.2.2
      //   comment  = "(" *([FWS] ccontent) [FWS] ")"
      //
      //   ccontent = ctext / quoted-pair / comment
      switch (token) {
      // nested comment
      case '(':
        // nested comments are ok
        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_COMMENT;
        break;
      // end of comment
      case ')':
        context.prev = context.now;
        context.now = context.stack.pop();

        // http://tools.ietf.org/html/rfc5322#section-3.2.2
        //   Runs of FWS, comment, or CFWS that occur between lexical tokens in a
        //   structured header field are semantically interpreted as a single
        //   space character.
        //
        // isEmail() author's note: This *cannot* mean that we must add a
        // space to the address wherever CFWS appears. This would result in
        // any addr-spec that had CFWS outside a quoted string being invalid
        // for RFC 5321.
//      if (context.now === ISEMAIL_COMPONENT_LOCALPART || context.now === ISEMAIL_COMPONENT_DOMAIN) {
//        parseData[context.now] += ' ';
//        atomList[context.now][elementCount] += ' ';
//        elementLength++;
//      }

        break;
      // quoted pair
      case '\\':
        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_QUOTEDPAIR;
        break;
      // folding white space
      case '\r':
      case ' ':
      case '\t':
        if (token === '\r' && ((++i === email.length) || email[i] !== '\n')) {
          // fatal error
          result.push(ISEMAIL_ERR_CR_NO_LF);
          break;
        }

        result.push(ISEMAIL_CFWS_FWS);

        context.stack.push(context.now);
        context.now = ISEMAIL_CONTEXT_FWS;
        prevToken = token;
        break;
      // ctext
      default:
        // http://tools.ietf.org/html/rfc5322#section-3.2.3
        //   ctext         = %d33-39 /  ; Printable US-ASCII
        //                   %d42-91 /  ;  characters not including
        //                   %d93-126 / ;  "(", ")", or "\"
        //                   obs-ctext
        //
        //   obs-ctext     = obs-NO-WS-CTL
        //
        //   obs-NO-WS-CTL = %d1-8 /    ; US-ASCII control
        //                   %d11 /     ;  characters that do not
        //                   %d12 /     ;  include the carriage
        //                   %d14-31 /  ;  return, line feed, and
        //                   %d127      ;  white space characters
        charCode = token.charCodeAt(0);

        if (charCode > 127 || charCode === 0 || charCode === 10) {
          // fatal error
          result.push(ISEMAIL_ERR_EXPECTING_CTEXT);
          break;
        } else if (charCode < 32 || charCode === 127) {
          result.push(ISEMAIL_DEPREC_CTEXT);
        }
      }
      break;
    // folding white space
    case ISEMAIL_CONTEXT_FWS:
      // http://tools.ietf.org/html/rfc5322#section-3.2.2
      //   FWS     =   ([*WSP CRLF] 1*WSP) /  obs-FWS
      //                                   ; Folding white space

      // But note the erratum:
      // http://www.rfc-editor.org/errata_search.php?rfc=5322&eid=1908:
      //   In the obsolete syntax, any amount of folding white space MAY be
      //   inserted where the obs-FWS rule is allowed.  This creates the
      //   possibility of having two consecutive "folds" in a line, and
      //   therefore the possibility that a line which makes up a folded header
      //   field could be composed entirely of white space.
      //
      //   obs-FWS =   1*([CRLF] WSP)

      if (prevToken === '\r') {
        if (token === '\r') {
          // fatal error
          result.push(ISEMAIL_ERR_FWS_CRLF_X2);
          break;
        }

        if (++crlfCount > 1) {
          // multiple folds = obsolete FWS
          result.push(ISEMAIL_DEPREC_FWS);
        } else {
          crlfCount = 1;
        }
      }

      switch (token) {
      case '\r':
        if ((++i === email.length) || email[i] !== '\n') {
          // fatal error
          result.push(ISEMAIL_ERR_CR_NO_LF);
        }
        break;
      case ' ':
      case '\t':
        break;
      default:
        if (prevToken === '\r') {
          // fatal error
          result.push(ISEMAIL_ERR_FWS_CRLF_END);
        }

        crlfCount = 0;

        context.prev = context.now;
        context.now = context.stack.pop(); // end of FWS

        // http://tools.ietf.org/html/rfc5322#section-3.2.2
        //   Runs of FWS, comment, or CFWS that occur between lexical tokens in a
        //   structured header field are semantically interpreted as a single
        //   space character.
        //
        // isEmail() author's note: This *cannot* mean that we must add a
        // space to the address wherever CFWS appears. This would result in
        // any addr-spec that had CFWS outside a quoted string being invalid
        // for RFC 5321.
//      if ((context.now === ISEMAIL_COMPONENT_LOCALPART) || (context.now === ISEMAIL_COMPONENT_DOMAIN)) {
//        parseData[context.now] += ' ';
//        atomList[context.now][elementCount] += ' ';
//        elementLength++;
//      }

        i--; // look at this token again in the parent context
      }
      prevToken = token;
      break;
    // unexpected context
    /* istanbul ignore next: logically unreachable */
    default:
      throw new Error('unknown context: ' + context.now);
    } // primary state machine

    if (maxValue(result) > ISEMAIL_RFC5322) {
      // fatal error, no point continuing
      break;
    }
  } // token loop

  // check for errors
  if (maxValue(result) < ISEMAIL_RFC5322) {
    // fatal errors
    if      (context.now === ISEMAIL_CONTEXT_QUOTEDSTRING) result.push(ISEMAIL_ERR_UNCLOSEDQUOTEDSTR);
    else if (context.now === ISEMAIL_CONTEXT_QUOTEDPAIR)   result.push(ISEMAIL_ERR_BACKSLASHEND);
    else if (context.now === ISEMAIL_CONTEXT_COMMENT)      result.push(ISEMAIL_ERR_UNCLOSEDCOMMENT);
    else if (context.now === ISEMAIL_COMPONENT_LITERAL)    result.push(ISEMAIL_ERR_UNCLOSEDDOMLIT);
    else if (token === '\r')                               result.push(ISEMAIL_ERR_FWS_CRLF_END);
    else if (parseData[ISEMAIL_COMPONENT_DOMAIN].length === 0) result.push(ISEMAIL_ERR_NODOMAIN);
    else if (elementLength === 0)                          result.push(ISEMAIL_ERR_DOT_END);
    else if (hyphenFlag)                                   result.push(ISEMAIL_ERR_DOMAINHYPHENEND);
    // other errors
    // http://tools.ietf.org/html/rfc5321#section-4.5.3.1.2
    //   The maximum total length of a domain name or number is 255 octets.
    else if (parseData[ISEMAIL_COMPONENT_DOMAIN].length > 255) {
      result.push(ISEMAIL_RFC5322_DOMAIN_TOOLONG);
    // http://tools.ietf.org/html/rfc5321#section-4.1.2
    //   Forward-path   = Path
    //
    //   Path           = "<" [ A-d-l ":" ] Mailbox ">"
    //
    // http://tools.ietf.org/html/rfc5321#section-4.5.3.1.3
    //   The maximum total length of a reverse-path or forward-path is 256
    //   octets (including the punctuation and element separators).
    //
    // Thus, even without (obsolete) routing information, the Mailbox can
    // only be 254 characters long. This is confirmed by this verified
    // erratum to RFC 3696:
    //
    // http://www.rfc-editor.org/errata_search.php?rfc=3696&eid=1690
    //   However, there is a restriction in RFC 2821 on the length of an
    //   address in MAIL and RCPT commands of 254 characters.  Since addresses
    //   that do not fit in those fields are not normally useful, the upper
    //   limit on address lengths should normally be considered to be 254.
    } else if (parseData[ISEMAIL_COMPONENT_LOCALPART].length +
      parseData[ISEMAIL_COMPONENT_DOMAIN].length +
      1 /* '@' symbol */ > 254) {
      result.push(ISEMAIL_RFC5322_TOOLONG);
    // http://tools.ietf.org/html/rfc1035#section-2.3.4
    // labels   63 octets or less
    } else if (elementLength > 63) {
      result.push(ISEMAIL_RFC5322_LABEL_TOOLONG);
    }
  } // check for errors

  var dnsPositive = false;

  if (options.checkDNS && (maxValue(result) < ISEMAIL_DNSWARN) && HAS_REQUIRE) {
    dns || (dns = require('dns'));
    // http://tools.ietf.org/html/rfc5321#section-2.3.5
    //   Names that can
    //   be resolved to MX RRs or address (i.e., A or AAAA) RRs (as discussed
    //   in Section 5) are permitted, as are CNAME RRs whose targets can be
    //   resolved, in turn, to MX or address RRs.
    //
    // http://tools.ietf.org/html/rfc5321#section-5.1
    //   The lookup first attempts to locate an MX record associated with the
    //   name.  If a CNAME record is found, the resulting name is processed as
    //   if it were the initial name. ... If an empty list of MXs is returned,
    //   the address is treated as if it was associated with an implicit MX
    //   RR, with a preference of 0, pointing to that host.
    //
    // isEmail() author's note: We will regard the existence of a CNAME to be
    // sufficient evidence of the domain's existence. For performance reasons
    // we will not repeat the DNS lookup for the CNAME's target, but we will
    // raise a warning because we didn't immediately find an MX record.
    if (elementCount === 0) {
      // checking TLD DNS seems to work only if you explicitly check from the root
      parseData[ISEMAIL_COMPONENT_DOMAIN] += '.';
    }

    var dnsDomain = parseData[ISEMAIL_COMPONENT_DOMAIN];
    dns.resolveMx(dnsDomain, function(err, records) {
      if ((err && err.code !== dns.NODATA) || (!err && !records)) {
        result.push(ISEMAIL_DNSWARN_NO_RECORD);
        return finish();
      }
      if (records && records.length) {
        dnsPositive = true;
        return finish();
      }
      var done = false, count = 3;
      result.push(ISEMAIL_DNSWARN_NO_MX_RECORD);
      dns.resolveCname(dnsDomain, handleRecords);
      dns.resolve4(dnsDomain, handleRecords);
      dns.resolve6(dnsDomain, handleRecords);
      function handleRecords(err, records) {
        if (done) return;
        count--;
        if (!err && records && records.length) {
          done = true;
          return finish();
        }
        if (count === 0) {
          // no usable records for the domain can be found
          result.push(ISEMAIL_DNSWARN_NO_RECORD);
          done = true;
          finish();
        }
      }
    });
  } else if (options.checkDNS) {
    // guarantee asynchronicity
    process.nextTick(finish);
  } else {
    return finish();
  } // checkDNS

  function finish() {
    if (!dnsPositive && (maxValue(result) < ISEMAIL_DNSWARN)) {
      if (elementCount === 0) {
        result.push(ISEMAIL_RFC5321_TLD);
      } else {
        var charCode = atomList[ISEMAIL_COMPONENT_DOMAIN][elementCount].charCodeAt(0);
        if (charCode >= 48 && charCode <= 57) {
          result.push(ISEMAIL_RFC5321_TLDNUMERIC);
        }
      }
    }

    // make unique (TODO: how efficient is this?)
    var has = {}, index = 0;
    while (index < result.length) {
      if (has[result[index]]) {
        result.splice(index, 1);
      } else {
        has[result[index]] = true;
        index++;
      }
    }
    // TODO: optimize all these Math.max calls!
    var finalStatus = maxValue(result);

    if (result.length !== 1) {
      // remove redundant ISEMAIL_VALID
      result.shift();
    }

    if (finalStatus < threshold) {
      finalStatus = ISEMAIL_VALID;
    }

    finalStatus = diagnose ? finalStatus : finalStatus < ISEMAIL_THRESHOLD;
    callback && callback(finalStatus);

    return finalStatus;
  } // finish
} // isEmail

module.exports = isEmail;

}).call(this,require('_process'))
},{"_process":29,"dns":23}],21:[function(require,module,exports){
arguments[4][16][0].apply(exports,arguments)
},{"./lib":22}],22:[function(require,module,exports){
// Load modules

var Hoek = require('hoek');


// Declare internals

var internals = {};


exports = module.exports = internals.Topo = function () {

    this._items = [];
    this.nodes = [];
};


internals.Topo.prototype.add = function (nodes, options) {

    var self = this;

    options = options || {};

    // Validate rules

    var before = [].concat(options.before || []);
    var after = [].concat(options.after || []);
    var group = options.group || '?';

    Hoek.assert(before.indexOf(group) === -1, 'Item cannot come before itself:', group);
    Hoek.assert(before.indexOf('?') === -1, 'Item cannot come before unassociated items');
    Hoek.assert(after.indexOf(group) === -1, 'Item cannot come after itself:', group);
    Hoek.assert(after.indexOf('?') === -1, 'Item cannot come after unassociated items');

    ([].concat(nodes)).forEach(function (node, i) {

        var item = {
            seq: self._items.length,
            before: before,
            after: after,
            group: group,
            node: node
        };

        self._items.push(item);
    });

    // Insert event

    var error = this._sort();
    Hoek.assert(!error, 'item', (group !== '?' ? 'added into group ' + group : ''), 'created a dependencies error');

    return this.nodes;
};


internals.Topo.prototype._sort = function () {

    // Construct graph

    var groups = {};
    var graph = {};
    var graphAfters = {};

    for (var i = 0, il = this._items.length; i < il; ++i) {
        var item = this._items[i];
        var seq = item.seq;                         // Unique across all items
        var group = item.group;

        // Determine Groups

        groups[group] = groups[group] || [];
        groups[group].push(seq);

        // Build intermediary graph using 'before'

        graph[seq] = [item.before];

        // Build second intermediary graph with 'after'

        var after = item.after;
        for (var j = 0, jl = after.length; j < jl; ++j) {
            graphAfters[after[j]] = (graphAfters[after[j]] || []).concat(seq);
        }
    }

    // Expand intermediary graph

    var graphNodes = Object.keys(graph);
    for (i = 0, il = graphNodes.length; i < il; ++i) {
        var node = graphNodes[i];
        var expandedGroups = [];

        var graphNodeItems = Object.keys(graph[node]);
        for (j = 0, jl = graphNodeItems.length; j < jl; ++j) {
            var group = graph[node][graphNodeItems[j]];
            groups[group] = groups[group] || [];
            groups[group].forEach(function (d) {

                expandedGroups.push(d);
            });
        }
        graph[node] = expandedGroups;
    }

    // Merge intermediary graph using graphAfters into final graph

    var afterNodes = Object.keys(graphAfters);
    for (i = 0, il = afterNodes.length; i < il; ++i) {
        var group = afterNodes[i];

        if (groups[group]) {
            for (j = 0, jl = groups[group].length; j < jl; ++j) {
                var node = groups[group][j];
                graph[node] = graph[node].concat(graphAfters[group]);
            }
        }
    }

    // Compile ancestors

    var ancestors = {};
    graphNodes = Object.keys(graph);
    for (i = 0, il = graphNodes.length; i < il; ++i) {
        var node = graphNodes[i];
        var children = graph[node];

        for (j = 0, jl = children.length; j < jl; ++j) {
            ancestors[children[j]] = (ancestors[children[j]] || []).concat(node);
        }
    }

    // Topo sort

    var visited = {};
    var sorted = [];

    for (i = 0, il = this._items.length; i < il; ++i) {
        var next = i;

        if (ancestors[i]) {
            next = null;
            for (j = 0, jl = this._items.length; j < jl; ++j) {
                if (visited[j] === true) {
                    continue;
                }

                if (!ancestors[j]) {
                    ancestors[j] = [];
                }

                var shouldSeeCount = ancestors[j].length;
                var seenCount = 0;
                for (var l = 0, ll = shouldSeeCount; l < ll; ++l) {
                    if (sorted.indexOf(ancestors[j][l]) >= 0) {
                        ++seenCount;
                    }
                }

                if (seenCount === shouldSeeCount) {
                    next = j;
                    break;
                }
            }
        }

        if (next !== null) {
            next = next.toString();         // Normalize to string TODO: replace with seq
            visited[next] = true;
            sorted.push(next);
        }
    }

    if (sorted.length !== this._items.length) {
        return new Error('Invalid dependencies');
    }

    var seqIndex = {};
    this._items.forEach(function (item) {

        seqIndex[item.seq] = item;
    });

    var sortedNodes = [];
    this._items = sorted.map(function (value) {

        var item = seqIndex[value];
        sortedNodes.push(item.node);
        return item;
    });

    this.nodes = sortedNodes;
};

},{"hoek":16}],23:[function(require,module,exports){

},{}],24:[function(require,module,exports){
/*!
 * The buffer module from node.js, for the browser.
 *
 * @author   Feross Aboukhadijeh <feross@feross.org> <http://feross.org>
 * @license  MIT
 */

var base64 = require('base64-js')
var ieee754 = require('ieee754')

exports.Buffer = Buffer
exports.SlowBuffer = Buffer
exports.INSPECT_MAX_BYTES = 50
Buffer.poolSize = 8192

/**
 * If `TYPED_ARRAY_SUPPORT`:
 *   === true    Use Uint8Array implementation (fastest)
 *   === false   Use Object implementation (most compatible, even IE6)
 *
 * Browsers that support typed arrays are IE 10+, Firefox 4+, Chrome 7+, Safari 5.1+,
 * Opera 11.6+, iOS 4.2+.
 *
 * Note:
 *
 * - Implementation must support adding new properties to `Uint8Array` instances.
 *   Firefox 4-29 lacked support, fixed in Firefox 30+.
 *   See: https://bugzilla.mozilla.org/show_bug.cgi?id=695438.
 *
 *  - Chrome 9-10 is missing the `TypedArray.prototype.subarray` function.
 *
 *  - IE10 has a broken `TypedArray.prototype.subarray` function which returns arrays of
 *    incorrect length in some situations.
 *
 * We detect these buggy browsers and set `TYPED_ARRAY_SUPPORT` to `false` so they will
 * get the Object implementation, which is slower but will work correctly.
 */
var TYPED_ARRAY_SUPPORT = (function () {
  try {
    var buf = new ArrayBuffer(0)
    var arr = new Uint8Array(buf)
    arr.foo = function () { return 42 }
    return 42 === arr.foo() && // typed array instances can be augmented
        typeof arr.subarray === 'function' && // chrome 9-10 lack `subarray`
        new Uint8Array(1).subarray(1, 1).byteLength === 0 // ie10 has broken `subarray`
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

  // Find the length
  var length
  if (type === 'number')
    length = subject > 0 ? subject >>> 0 : 0
  else if (type === 'string') {
    if (encoding === 'base64')
      subject = base64clean(subject)
    length = Buffer.byteLength(subject, encoding)
  } else if (type === 'object' && subject !== null) { // assume object is array-like
    if (subject.type === 'Buffer' && isArray(subject.data))
      subject = subject.data
    length = +subject.length > 0 ? Math.floor(+subject.length) : 0
  } else
    throw new Error('First argument needs to be a number, array or string.')

  var buf
  if (TYPED_ARRAY_SUPPORT) {
    // Preferred: Return an augmented `Uint8Array` instance for best performance
    buf = Buffer._augment(new Uint8Array(length))
  } else {
    // Fallback: Return THIS instance of Buffer (created by `new`)
    buf = this
    buf.length = length
    buf._isBuffer = true
  }

  var i
  if (TYPED_ARRAY_SUPPORT && typeof subject.byteLength === 'number') {
    // Speed optimization -- use set if we're copying from a typed array
    buf._set(subject)
  } else if (isArrayish(subject)) {
    // Treat array-ish objects as a byte array
    if (Buffer.isBuffer(subject)) {
      for (i = 0; i < length; i++)
        buf[i] = subject.readUInt8(i)
    } else {
      for (i = 0; i < length; i++)
        buf[i] = ((subject[i] % 256) + 256) % 256
    }
  } else if (type === 'string') {
    buf.write(subject, 0, encoding)
  } else if (type === 'number' && !TYPED_ARRAY_SUPPORT && !noZero) {
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
  return !!(b != null && b._isBuffer)
}

Buffer.byteLength = function (str, encoding) {
  var ret
  str = str.toString()
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
  assert(isArray(list), 'Usage: Buffer.concat(list[, length])')

  if (list.length === 0) {
    return new Buffer(0)
  } else if (list.length === 1) {
    return list[0]
  }

  var i
  if (totalLength === undefined) {
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

Buffer.compare = function (a, b) {
  assert(Buffer.isBuffer(a) && Buffer.isBuffer(b), 'Arguments must be Buffers')
  var x = a.length
  var y = b.length
  for (var i = 0, len = Math.min(x, y); i < len && a[i] === b[i]; i++) {}
  if (i !== len) {
    x = a[i]
    y = b[i]
  }
  if (x < y) {
    return -1
  }
  if (y < x) {
    return 1
  }
  return 0
}

// BUFFER INSTANCE METHODS
// =======================

function hexWrite (buf, string, offset, length) {
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
  return i
}

function utf8Write (buf, string, offset, length) {
  var charsWritten = blitBuffer(utf8ToBytes(string), buf, offset, length)
  return charsWritten
}

function asciiWrite (buf, string, offset, length) {
  var charsWritten = blitBuffer(asciiToBytes(string), buf, offset, length)
  return charsWritten
}

function binaryWrite (buf, string, offset, length) {
  return asciiWrite(buf, string, offset, length)
}

function base64Write (buf, string, offset, length) {
  var charsWritten = blitBuffer(base64ToBytes(string), buf, offset, length)
  return charsWritten
}

function utf16leWrite (buf, string, offset, length) {
  var charsWritten = blitBuffer(utf16leToBytes(string), buf, offset, length)
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
      ret = hexWrite(this, string, offset, length)
      break
    case 'utf8':
    case 'utf-8':
      ret = utf8Write(this, string, offset, length)
      break
    case 'ascii':
      ret = asciiWrite(this, string, offset, length)
      break
    case 'binary':
      ret = binaryWrite(this, string, offset, length)
      break
    case 'base64':
      ret = base64Write(this, string, offset, length)
      break
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = utf16leWrite(this, string, offset, length)
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
  end = (end === undefined) ? self.length : Number(end)

  // Fastpath empty strings
  if (end === start)
    return ''

  var ret
  switch (encoding) {
    case 'hex':
      ret = hexSlice(self, start, end)
      break
    case 'utf8':
    case 'utf-8':
      ret = utf8Slice(self, start, end)
      break
    case 'ascii':
      ret = asciiSlice(self, start, end)
      break
    case 'binary':
      ret = binarySlice(self, start, end)
      break
    case 'base64':
      ret = base64Slice(self, start, end)
      break
    case 'ucs2':
    case 'ucs-2':
    case 'utf16le':
    case 'utf-16le':
      ret = utf16leSlice(self, start, end)
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

Buffer.prototype.equals = function (b) {
  assert(Buffer.isBuffer(b), 'Argument must be a Buffer')
  return Buffer.compare(this, b) === 0
}

Buffer.prototype.compare = function (b) {
  assert(Buffer.isBuffer(b), 'Argument must be a Buffer')
  return Buffer.compare(this, b)
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

  if (len < 100 || !TYPED_ARRAY_SUPPORT) {
    for (var i = 0; i < len; i++) {
      target[i + target_start] = this[i + start]
    }
  } else {
    target._set(this.subarray(start, start + len), target_start)
  }
}

function base64Slice (buf, start, end) {
  if (start === 0 && end === buf.length) {
    return base64.fromByteArray(buf)
  } else {
    return base64.fromByteArray(buf.slice(start, end))
  }
}

function utf8Slice (buf, start, end) {
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

function asciiSlice (buf, start, end) {
  var ret = ''
  end = Math.min(buf.length, end)

  for (var i = start; i < end; i++) {
    ret += String.fromCharCode(buf[i])
  }
  return ret
}

function binarySlice (buf, start, end) {
  return asciiSlice(buf, start, end)
}

function hexSlice (buf, start, end) {
  var len = buf.length

  if (!start || start < 0) start = 0
  if (!end || end < 0 || end > len) end = len

  var out = ''
  for (var i = start; i < end; i++) {
    out += toHex(buf[i])
  }
  return out
}

function utf16leSlice (buf, start, end) {
  var bytes = buf.slice(start, end)
  var res = ''
  for (var i = 0; i < bytes.length; i += 2) {
    res += String.fromCharCode(bytes[i] + bytes[i + 1] * 256)
  }
  return res
}

Buffer.prototype.slice = function (start, end) {
  var len = this.length
  start = ~~start
  end = end === undefined ? len : ~~end

  if (start < 0) {
    start += len;
    if (start < 0)
      start = 0
  } else if (start > len) {
    start = len
  }

  if (end < 0) {
    end += len
    if (end < 0)
      end = 0
  } else if (end > len) {
    end = len
  }

  if (end < start)
    end = start

  if (TYPED_ARRAY_SUPPORT) {
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

function readUInt16 (buf, offset, littleEndian, noAssert) {
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
  return readUInt16(this, offset, true, noAssert)
}

Buffer.prototype.readUInt16BE = function (offset, noAssert) {
  return readUInt16(this, offset, false, noAssert)
}

function readUInt32 (buf, offset, littleEndian, noAssert) {
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
  return readUInt32(this, offset, true, noAssert)
}

Buffer.prototype.readUInt32BE = function (offset, noAssert) {
  return readUInt32(this, offset, false, noAssert)
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

function readInt16 (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 1 < buf.length, 'Trying to read beyond buffer length')
  }

  var len = buf.length
  if (offset >= len)
    return

  var val = readUInt16(buf, offset, littleEndian, true)
  var neg = val & 0x8000
  if (neg)
    return (0xffff - val + 1) * -1
  else
    return val
}

Buffer.prototype.readInt16LE = function (offset, noAssert) {
  return readInt16(this, offset, true, noAssert)
}

Buffer.prototype.readInt16BE = function (offset, noAssert) {
  return readInt16(this, offset, false, noAssert)
}

function readInt32 (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset !== undefined && offset !== null, 'missing offset')
    assert(offset + 3 < buf.length, 'Trying to read beyond buffer length')
  }

  var len = buf.length
  if (offset >= len)
    return

  var val = readUInt32(buf, offset, littleEndian, true)
  var neg = val & 0x80000000
  if (neg)
    return (0xffffffff - val + 1) * -1
  else
    return val
}

Buffer.prototype.readInt32LE = function (offset, noAssert) {
  return readInt32(this, offset, true, noAssert)
}

Buffer.prototype.readInt32BE = function (offset, noAssert) {
  return readInt32(this, offset, false, noAssert)
}

function readFloat (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset + 3 < buf.length, 'Trying to read beyond buffer length')
  }

  return ieee754.read(buf, offset, littleEndian, 23, 4)
}

Buffer.prototype.readFloatLE = function (offset, noAssert) {
  return readFloat(this, offset, true, noAssert)
}

Buffer.prototype.readFloatBE = function (offset, noAssert) {
  return readFloat(this, offset, false, noAssert)
}

function readDouble (buf, offset, littleEndian, noAssert) {
  if (!noAssert) {
    assert(typeof littleEndian === 'boolean', 'missing or invalid endian')
    assert(offset + 7 < buf.length, 'Trying to read beyond buffer length')
  }

  return ieee754.read(buf, offset, littleEndian, 52, 8)
}

Buffer.prototype.readDoubleLE = function (offset, noAssert) {
  return readDouble(this, offset, true, noAssert)
}

Buffer.prototype.readDoubleBE = function (offset, noAssert) {
  return readDouble(this, offset, false, noAssert)
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
  return offset + 1
}

function writeUInt16 (buf, value, offset, littleEndian, noAssert) {
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
  return offset + 2
}

Buffer.prototype.writeUInt16LE = function (value, offset, noAssert) {
  return writeUInt16(this, value, offset, true, noAssert)
}

Buffer.prototype.writeUInt16BE = function (value, offset, noAssert) {
  return writeUInt16(this, value, offset, false, noAssert)
}

function writeUInt32 (buf, value, offset, littleEndian, noAssert) {
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
  return offset + 4
}

Buffer.prototype.writeUInt32LE = function (value, offset, noAssert) {
  return writeUInt32(this, value, offset, true, noAssert)
}

Buffer.prototype.writeUInt32BE = function (value, offset, noAssert) {
  return writeUInt32(this, value, offset, false, noAssert)
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
  return offset + 1
}

function writeInt16 (buf, value, offset, littleEndian, noAssert) {
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
    writeUInt16(buf, value, offset, littleEndian, noAssert)
  else
    writeUInt16(buf, 0xffff + value + 1, offset, littleEndian, noAssert)
  return offset + 2
}

Buffer.prototype.writeInt16LE = function (value, offset, noAssert) {
  return writeInt16(this, value, offset, true, noAssert)
}

Buffer.prototype.writeInt16BE = function (value, offset, noAssert) {
  return writeInt16(this, value, offset, false, noAssert)
}

function writeInt32 (buf, value, offset, littleEndian, noAssert) {
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
    writeUInt32(buf, value, offset, littleEndian, noAssert)
  else
    writeUInt32(buf, 0xffffffff + value + 1, offset, littleEndian, noAssert)
  return offset + 4
}

Buffer.prototype.writeInt32LE = function (value, offset, noAssert) {
  return writeInt32(this, value, offset, true, noAssert)
}

Buffer.prototype.writeInt32BE = function (value, offset, noAssert) {
  return writeInt32(this, value, offset, false, noAssert)
}

function writeFloat (buf, value, offset, littleEndian, noAssert) {
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
  return offset + 4
}

Buffer.prototype.writeFloatLE = function (value, offset, noAssert) {
  return writeFloat(this, value, offset, true, noAssert)
}

Buffer.prototype.writeFloatBE = function (value, offset, noAssert) {
  return writeFloat(this, value, offset, false, noAssert)
}

function writeDouble (buf, value, offset, littleEndian, noAssert) {
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
  return offset + 8
}

Buffer.prototype.writeDoubleLE = function (value, offset, noAssert) {
  return writeDouble(this, value, offset, true, noAssert)
}

Buffer.prototype.writeDoubleBE = function (value, offset, noAssert) {
  return writeDouble(this, value, offset, false, noAssert)
}

// fill(value, start=0, end=buffer.length)
Buffer.prototype.fill = function (value, start, end) {
  if (!value) value = 0
  if (!start) start = 0
  if (!end) end = this.length

  assert(end >= start, 'end < start')

  // Fill 0 bytes; we're done
  if (end === start) return
  if (this.length === 0) return

  assert(start >= 0 && start < this.length, 'start out of bounds')
  assert(end >= 0 && end <= this.length, 'end out of bounds')

  var i
  if (typeof value === 'number') {
    for (i = start; i < end; i++) {
      this[i] = value
    }
  } else {
    var bytes = utf8ToBytes(value.toString())
    var len = bytes.length
    for (i = start; i < end; i++) {
      this[i] = bytes[i % len]
    }
  }

  return this
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
    if (TYPED_ARRAY_SUPPORT) {
      return (new Buffer(this)).buffer
    } else {
      var buf = new Uint8Array(this.length)
      for (var i = 0, len = buf.length; i < len; i += 1) {
        buf[i] = this[i]
      }
      return buf.buffer
    }
  } else {
    throw new Error('Buffer.toArrayBuffer not supported in this browser')
  }
}

// HELPER FUNCTIONS
// ================

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
  arr.equals = BP.equals
  arr.compare = BP.compare
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

var INVALID_BASE64_RE = /[^+\/0-9A-z]/g

function base64clean (str) {
  // Node strips out invalid characters like \n and \t from the string, base64-js does not
  str = stringtrim(str).replace(INVALID_BASE64_RE, '')
  // Node allows for non-padded base64 strings (missing trailing ===), base64-js does not
  while (str.length % 4 !== 0) {
    str = str + '='
  }
  return str
}

function stringtrim (str) {
  if (str.trim) return str.trim()
  return str.replace(/^\s+|\s+$/g, '')
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
    if (b <= 0x7F) {
      byteArray.push(b)
    } else {
      var start = i
      if (b >= 0xD800 && b <= 0xDFFF) i++
      var h = encodeURIComponent(str.slice(start, i+1)).substr(1).split('%')
      for (var j = 0; j < h.length; j++) {
        byteArray.push(parseInt(h[j], 16))
      }
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

},{"base64-js":25,"ieee754":26}],25:[function(require,module,exports){
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

},{}],26:[function(require,module,exports){
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

},{}],27:[function(require,module,exports){
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

},{}],28:[function(require,module,exports){
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

// resolves . and .. elements in a path array with directory names there
// must be no slashes, empty elements, or device names (c:\) in the array
// (so also no leading and trailing slashes - it does not distinguish
// relative and absolute paths)
function normalizeArray(parts, allowAboveRoot) {
  // if the path tries to go above the root, `up` ends up > 0
  var up = 0;
  for (var i = parts.length - 1; i >= 0; i--) {
    var last = parts[i];
    if (last === '.') {
      parts.splice(i, 1);
    } else if (last === '..') {
      parts.splice(i, 1);
      up++;
    } else if (up) {
      parts.splice(i, 1);
      up--;
    }
  }

  // if the path is allowed to go above the root, restore leading ..s
  if (allowAboveRoot) {
    for (; up--; up) {
      parts.unshift('..');
    }
  }

  return parts;
}

// Split a filename into [root, dir, basename, ext], unix version
// 'root' is just a slash, or nothing.
var splitPathRe =
    /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
var splitPath = function(filename) {
  return splitPathRe.exec(filename).slice(1);
};

// path.resolve([from ...], to)
// posix version
exports.resolve = function() {
  var resolvedPath = '',
      resolvedAbsolute = false;

  for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
    var path = (i >= 0) ? arguments[i] : process.cwd();

    // Skip empty and invalid entries
    if (typeof path !== 'string') {
      throw new TypeError('Arguments to path.resolve must be strings');
    } else if (!path) {
      continue;
    }

    resolvedPath = path + '/' + resolvedPath;
    resolvedAbsolute = path.charAt(0) === '/';
  }

  // At this point the path should be resolved to a full absolute path, but
  // handle relative paths to be safe (might happen when process.cwd() fails)

  // Normalize the path
  resolvedPath = normalizeArray(filter(resolvedPath.split('/'), function(p) {
    return !!p;
  }), !resolvedAbsolute).join('/');

  return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
};

// path.normalize(path)
// posix version
exports.normalize = function(path) {
  var isAbsolute = exports.isAbsolute(path),
      trailingSlash = substr(path, -1) === '/';

  // Normalize the path
  path = normalizeArray(filter(path.split('/'), function(p) {
    return !!p;
  }), !isAbsolute).join('/');

  if (!path && !isAbsolute) {
    path = '.';
  }
  if (path && trailingSlash) {
    path += '/';
  }

  return (isAbsolute ? '/' : '') + path;
};

// posix version
exports.isAbsolute = function(path) {
  return path.charAt(0) === '/';
};

// posix version
exports.join = function() {
  var paths = Array.prototype.slice.call(arguments, 0);
  return exports.normalize(filter(paths, function(p, index) {
    if (typeof p !== 'string') {
      throw new TypeError('Arguments to path.join must be strings');
    }
    return p;
  }).join('/'));
};


// path.relative(from, to)
// posix version
exports.relative = function(from, to) {
  from = exports.resolve(from).substr(1);
  to = exports.resolve(to).substr(1);

  function trim(arr) {
    var start = 0;
    for (; start < arr.length; start++) {
      if (arr[start] !== '') break;
    }

    var end = arr.length - 1;
    for (; end >= 0; end--) {
      if (arr[end] !== '') break;
    }

    if (start > end) return [];
    return arr.slice(start, end - start + 1);
  }

  var fromParts = trim(from.split('/'));
  var toParts = trim(to.split('/'));

  var length = Math.min(fromParts.length, toParts.length);
  var samePartsLength = length;
  for (var i = 0; i < length; i++) {
    if (fromParts[i] !== toParts[i]) {
      samePartsLength = i;
      break;
    }
  }

  var outputParts = [];
  for (var i = samePartsLength; i < fromParts.length; i++) {
    outputParts.push('..');
  }

  outputParts = outputParts.concat(toParts.slice(samePartsLength));

  return outputParts.join('/');
};

exports.sep = '/';
exports.delimiter = ':';

exports.dirname = function(path) {
  var result = splitPath(path),
      root = result[0],
      dir = result[1];

  if (!root && !dir) {
    // No dirname whatsoever
    return '.';
  }

  if (dir) {
    // It has a dirname, strip trailing slash
    dir = dir.substr(0, dir.length - 1);
  }

  return root + dir;
};


exports.basename = function(path, ext) {
  var f = splitPath(path)[2];
  // TODO: make this comparison case-insensitive on windows?
  if (ext && f.substr(-1 * ext.length) === ext) {
    f = f.substr(0, f.length - ext.length);
  }
  return f;
};


exports.extname = function(path) {
  return splitPath(path)[3];
};

function filter (xs, f) {
    if (xs.filter) return xs.filter(f);
    var res = [];
    for (var i = 0; i < xs.length; i++) {
        if (f(xs[i], i, xs)) res.push(xs[i]);
    }
    return res;
}

// String.prototype.substr - negative index don't work in IE8
var substr = 'ab'.substr(-1) === 'b'
    ? function (str, start, len) { return str.substr(start, len) }
    : function (str, start, len) {
        if (start < 0) start = str.length + start;
        return str.substr(start, len);
    }
;

}).call(this,require('_process'))
},{"_process":29}],29:[function(require,module,exports){
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

},{}],30:[function(require,module,exports){
module.exports = function isBuffer(arg) {
  return arg && typeof arg === 'object'
    && typeof arg.copy === 'function'
    && typeof arg.fill === 'function'
    && typeof arg.readUInt8 === 'function';
}
},{}],31:[function(require,module,exports){
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

exports.isBuffer = require('./support/isBuffer');

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
exports.inherits = require('inherits');

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

}).call(this,require('_process'),typeof global !== "undefined" ? global : typeof self !== "undefined" ? self : typeof window !== "undefined" ? window : {})
},{"./support/isBuffer":30,"_process":29,"inherits":27}],"joi":[function(require,module,exports){
arguments[4][16][0].apply(exports,arguments)
},{"./lib":10}]},{},[])("joi")
});