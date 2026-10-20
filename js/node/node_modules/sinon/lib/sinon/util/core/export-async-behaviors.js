'use strict';

var commons = require('@sinonjs/commons');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);

const { prototypes } = commons__default.default;

const { reduce } = prototypes.array;

/**
 * Exports async versions of behavior methods.
 *
 * @param {object} behaviorMethods The behavior methods to export
 * @returns {object} The async versions of the behavior methods
 */
function exportAsyncBehaviors(behaviorMethods) {
    return reduce(
        Object.keys(behaviorMethods),
        function (acc, method) {
            // need to avoid creating another async versions of the newly added async methods
            if (method.match(/^(callsArg|yields)/) && !method.match(/Async/)) {
                acc[`${method}Async`] = function () {
                    const result = behaviorMethods[method].apply(
                        this,
                        arguments,
                    );
                    this.callbackAsync = true;
                    return result;
                };
            }
            return acc;
        },
        {},
    );
}

module.exports = exportAsyncBehaviors;
