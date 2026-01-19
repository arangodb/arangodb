"use strict";

const extend = require("./core/extend");
const FakeTimers = require("@sinonjs/fake-timers");
const globalObject = require("@sinonjs/commons").global;

/**
 *
 * @param config
 * @param globalCtx
 *
 * @returns {object} the clock, after installing it on the global context, if given
 */
function createClock(config, globalCtx) {
    let FakeTimersCtx = FakeTimers;
    if (globalCtx !== null && typeof globalCtx === "object") {
        FakeTimersCtx = FakeTimers.withGlobal(globalCtx);
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
exports.useFakeTimers = function (dateOrConfig) {
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
};

exports.clock = {
    create: function (now) {
        return FakeTimers.createClock(now);
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

exports.timers = timers;
