"use strict";

const push = require("@sinonjs/commons").prototypes.array.push;

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

exports.delegateToCalls = function delegateToCalls(
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
};
