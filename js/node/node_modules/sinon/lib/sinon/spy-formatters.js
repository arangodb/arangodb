'use strict';

var commons = require('@sinonjs/commons');
var colorizer$1 = require('./colorizer.js');
var samsam = require('@sinonjs/samsam');
var timesInWords = require('./util/core/times-in-words.js');
var util = require('util');
var jsDiff = require('diff');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

function _interopNamespace(e) {
    if (e && e.__esModule) return e;
    var n = Object.create(null);
    if (e) {
        Object.keys(e).forEach(function (k) {
            if (k !== 'default') {
                var d = Object.getOwnPropertyDescriptor(e, k);
                Object.defineProperty(n, k, d.get ? d : {
                    enumerable: true,
                    get: function () { return e[k]; }
                });
            }
        });
    }
    n.default = e;
    return Object.freeze(n);
}

var commons__default = /*#__PURE__*/_interopDefault(commons);
var samsam__default = /*#__PURE__*/_interopDefault(samsam);
var jsDiff__namespace = /*#__PURE__*/_interopNamespace(jsDiff);

const { prototypes } = commons__default.default;
const { createMatcher: match } = samsam__default.default;
const { join, map, push, slice } = prototypes.array;
const colorizer = new colorizer$1();

/**
 *
 * @param matcher
 * @param calledArg
 * @param calledArgMessage
 *
 * @returns {string} the colored text
 */
function colorSinonMatchText(matcher, calledArg, calledArgMessage) {
    let calledArgumentMessage = calledArgMessage;
    let matcherMessage = matcher.message;
    if (!matcher.test(calledArg)) {
        matcherMessage = colorizer.red(matcher.message);
        if (calledArgumentMessage) {
            calledArgumentMessage = colorizer.green(calledArgumentMessage);
        }
    }
    return `${calledArgumentMessage} ${matcherMessage}`;
}

/**
 * @param diff
 *
 * @returns {string} the colored diff
 */
function colorDiffText(diff) {
    const objects = map(diff, function (part) {
        let text = part.value;
        if (part.added) {
            text = colorizer.green(text);
        } else if (part.removed) {
            text = colorizer.red(text);
        }
        if (diff.length === 2) {
            text += " "; // format simple diffs
        }
        return text;
    });
    return join(objects, "");
}

/**
 *
 * @param value
 * @returns {string} a quoted string
 */
function quoteStringValue(value) {
    if (typeof value === "string") {
        return JSON.stringify(value);
    }
    return value;
}

var formatters = {
    c: function (spyInstance) {
        return timesInWords(spyInstance.callCount);
    },

    n: function (spyInstance) {
        return spyInstance.toString();
    },

    D: function (spyInstance, args) {
        let message = "";

        for (let i = 0, l = spyInstance.callCount; i < l; ++i) {
            // describe multiple calls
            if (l > 1) {
                message += `\nCall ${i + 1}:`;
            }
            const calledArgs = spyInstance.getCall(i).args;
            const expectedArgs = slice(args);

            for (
                let j = 0;
                j < calledArgs.length || j < expectedArgs.length;
                ++j
            ) {
                let calledArg = calledArgs[j];
                let expectedArg = expectedArgs[j];
                if (calledArg) {
                    calledArg = quoteStringValue(calledArg);
                }

                if (expectedArg) {
                    expectedArg = quoteStringValue(expectedArg);
                }

                message += "\n";

                const calledArgMessage =
                    j < calledArgs.length ? util.inspect(calledArg) : "";
                if (match.isMatcher(expectedArg)) {
                    message += colorSinonMatchText(
                        expectedArg,
                        calledArg,
                        calledArgMessage,
                    );
                } else {
                    const expectedArgMessage =
                        j < expectedArgs.length ? util.inspect(expectedArg) : "";
                    const diff = jsDiff__namespace.diffJson(
                        calledArgMessage,
                        expectedArgMessage,
                    );
                    message += colorDiffText(diff);
                }
            }
        }

        return message;
    },

    C: function (spyInstance) {
        const calls = [];

        for (let i = 0, l = spyInstance.callCount; i < l; ++i) {
            let stringifiedCall = `    ${spyInstance.getCall(i).toString()}`;
            if (/\n/.test(calls[i - 1])) {
                stringifiedCall = `\n${stringifiedCall}`;
            }
            push(calls, stringifiedCall);
        }

        return calls.length > 0 ? `\n${join(calls, "\n")}` : "";
    },

    t: function (spyInstance) {
        const objects = [];

        for (let i = 0, l = spyInstance.callCount; i < l; ++i) {
            push(objects, util.inspect(spyInstance.thisValues[i]));
        }

        return join(objects, ", ");
    },

    "*": function (spyInstance, args) {
        return join(
            map(args, function (arg) {
                return util.inspect(arg);
            }),
            ", ",
        );
    },
};

module.exports = formatters;
