'use strict';

var extend = require('./core/extend.js');
var FakeTimers = require('@sinonjs/fake-timers');
var commons = require('@sinonjs/commons');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var FakeTimers__default = /*#__PURE__*/_interopDefault(FakeTimers);
var commons__default = /*#__PURE__*/_interopDefault(commons);

const { global: globalObject } = commons__default.default;

/**
 *
 * @param config
 * @param globalCtx
 *
 * @returns {object} the clock, after installing it on the global context, if given
 */
function createClock(config, globalCtx) {
    let FakeTimersCtx = FakeTimers__default.default;
    if (globalCtx !== null && typeof globalCtx === "object") {
        FakeTimersCtx = FakeTimers__default.default.withGlobal(globalCtx);
    }
    const clock = FakeTimersCtx.install(config);
    clock.restore = clock.uninstall;
    return clock;
}

/**
 *
 * @param obj
 * @param globalPropName
 */
function addIfDefined(obj, globalPropName) {
    const globalProp = globalObject[globalPropName];
    if (typeof globalProp !== "undefined") {
        obj[globalPropName] = globalProp;
    }
}

/**
 * @param {number|Date|object} dateOrConfig The unix epoch value to install with (default 0)
 * @returns {object} Returns a lolex clock instance
 */
function useFakeTimers(dateOrConfig) {
    const hasArguments = typeof dateOrConfig !== "undefined";
    const argumentIsDateLike =
        (typeof dateOrConfig === "number" || dateOrConfig instanceof Date) &&
        arguments.length === 1;
    const argumentIsObject =
        dateOrConfig !== null &&
        typeof dateOrConfig === "object" &&
        arguments.length === 1;

    if (!hasArguments) {
        return createClock({
            now: 0,
        });
    }

    if (argumentIsDateLike) {
        return createClock({
            now: dateOrConfig,
        });
    }

    if (argumentIsObject) {
        const config = extend.nonEnum({}, dateOrConfig);
        const globalCtx = config.global;
        delete config.global;
        return createClock(config, globalCtx);
    }

    throw new TypeError(
        "useFakeTimers expected epoch or config object. See https://github.com/sinonjs/sinon",
    );
}

const clock = {
    create: function (now) {
        return FakeTimers__default.default.createClock(now);
    },
};

const timers = {
    setTimeout: setTimeout,
    clearTimeout: clearTimeout,
    setInterval: setInterval,
    clearInterval: clearInterval,
    Date: Date,
};
addIfDefined(timers, "setImmediate");
addIfDefined(timers, "clearImmediate");

exports.clock = clock;
exports.timers = timers;
exports.useFakeTimers = useFakeTimers;
