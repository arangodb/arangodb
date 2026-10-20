'use strict';

var commons = require('@sinonjs/commons');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);

const { prototypes } = commons__default.default;

const { push } = prototypes.array;

/**
 * @callback SinonFunction
 * @param {...unknown} args
 * @returns {unknown}
 */

/**
 * @callback DelegateCallback
 * @param {...unknown} args
 * @returns {unknown}
 */

/**
 * Increments the call count of a proxy and updates related properties.
 *
 * @param {object} proxy The proxy object
 */
function incrementCallCount(proxy) {
    proxy.called = true;
    proxy.callCount += 1;
    proxy.notCalled = false;
    proxy.calledOnce = proxy.callCount === 1;
    proxy.calledTwice = proxy.callCount === 2;
    proxy.calledThrice = proxy.callCount === 3;
}

/**
 * Creates properties on a proxy that point to specific calls (first, second, third, last).
 *
 * @param {object} proxy The proxy object
 */
function createCallProperties(proxy) {
    proxy.firstCall = proxy.getCall(0);
    proxy.secondCall = proxy.getCall(1);
    proxy.thirdCall = proxy.getCall(2);
    proxy.lastCall = proxy.getCall(proxy.callCount - 1);
}

/**
 * Delegates a proxy method call to all its individual calls.
 *
 * @param {object} proxy The proxy object
 * @param {string} method The method name
 * @param {boolean} matchAny Whether to return true if any call matches
 * @param {string} [actual] The actual method name on the call object
 * @param {boolean} [returnsValues] Whether to return an array of return values
 * @param {DelegateCallback} [notCalled] Function to call if the proxy has not been called
 * @param {number} [totalCallCount] The expected total call count
 */
function delegateToCalls(
    proxy,
    method,
    matchAny,
    actual,
    returnsValues,
    notCalled,
    totalCallCount,
) {
    proxy[method] = function () {
        if (!this.called) {
            if (notCalled) {
                return notCalled.apply(this, arguments);
            }
            return false;
        }

        if (totalCallCount !== undefined && this.callCount !== totalCallCount) {
            return false;
        }

        let currentCall;
        let matches = 0;
        const returnValues = [];

        for (let i = 0, l = this.callCount; i < l; i += 1) {
            currentCall = this.getCall(i);
            const returnValue = currentCall[actual || method].apply(
                currentCall,
                arguments,
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
}

exports.createCallProperties = createCallProperties;
exports.delegateToCalls = delegateToCalls;
exports.incrementCallCount = incrementCallCount;
