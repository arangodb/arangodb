"use strict";

function map(arr, fn, thisArg) {
    var result = [];
    for (var i = 0; i < arr.length; i++) {
        result.push(fn.call(thisArg, arr[i], i));
    }

    return result;
}

function getProperties(obj) {
    if (typeof obj !== 'object' || !obj) {
        return [];
    }

    return map(Object.keys(obj), function (key) {
        return {
            key: key,
            value: obj[key]
        };
    });
}

function leftDeepCompare(a, b) {
    if (a === null || typeof a !== 'object') {
        return a === b;
    }

    var aprops = getProperties(a);
    for (var i = 0; i < aprops.length; i++) {
        if (!b.hasOwnProperty(aprops[i].key) || !leftDeepCompare(aprops[i].value, b[aprops[i].key])) {
            return false;
        }
    }

    return true;
}

exports.limit = function (callback, limit) {
    var count = 0;
    return function (item) {
        if (++count <= limit) {
            return callback.call(this, item);
        }

        return false;
    };
};

exports.conditional = function (callback, conditions) {
    return function (item) {
        if (leftDeepCompare(conditions, item)) {
            return callback.call(this, item);
        }
    };
};