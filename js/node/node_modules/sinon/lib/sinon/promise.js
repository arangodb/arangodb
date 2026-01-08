"use strict";

const fake = require("./fake");
const isRestorable = require("./util/core/is-restorable");

const STATUS_PENDING = "pending";
const STATUS_RESOLVED = "resolved";
const STATUS_REJECTED = "rejected";

/**
 * Returns a fake for a given function or undefined. If no function is given, a
 * new fake is returned. If the given function is already a fake, it is
 * returned as is. Otherwise the given function is wrapped in a new fake.
 *
 * @param {Function} [executor] The optional executor function.
 * @returns {Function}
 */
function getFakeExecutor(executor) {
    if (isRestorable(executor)) {
        return executor;
    }
    if (executor) {
        return fake(executor);
    }
    return fake();
}

/**
 * Returns a new promise that exposes it's internal `status`, `resolvedValue`
 * and `rejectedValue` and can be resolved or rejected from the outside by
 * calling `resolve(value)` or `reject(reason)`.
 *
 * @param {Function} [executor] The optional executor function.
 * @returns {Promise}
 */
function promise(executor) {
    const fakeExecutor = getFakeExecutor(executor);
    const sinonPromise = new Promise(fakeExecutor);

    sinonPromise.status = STATUS_PENDING;
    sinonPromise
        .then(function (value) {
            sinonPromise.status = STATUS_RESOLVED;
            sinonPromise.resolvedValue = value;
        })
        .catch(function (reason) {
            sinonPromise.status = STATUS_REJECTED;
            sinonPromise.rejectedValue = reason;
        });

    /**
     * Resolves or rejects the promise with the given status and value.
     *
     * @param {string} status
     * @param {*} value
     * @param {Function} callback
     */
    function finalize(status, value, callback) {
        if (sinonPromise.status !== STATUS_PENDING) {
            throw new Error(`Promise already ${sinonPromise.status}`);
        }

        sinonPromise.status = status;
        callback(value);
    }

    sinonPromise.resolve = function (value) {
        finalize(STATUS_RESOLVED, value, fakeExecutor.firstCall.args[0]);
        // Return the promise so that callers can await it:
        return sinonPromise;
    };
    sinonPromise.reject = function (reason) {
        finalize(STATUS_REJECTED, reason, fakeExecutor.firstCall.args[1]);
        // Return a new promise that resolves when the sinon promise was
        // rejected, so that callers can await it:
        return new Promise(function (resolve) {
            sinonPromise.catch(() => resolve());
        });
    };

    return sinonPromise;
}

module.exports = promise;
