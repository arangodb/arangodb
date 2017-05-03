'use strict';

// Load modules


// Declare internals

const internals = {};


exports.serial = function (array, method, callback) {

    if (!array.length) {
        callback();
    }
    else {
        let i = 0;
        const iterate = function () {

            const done = function (err) {

                if (err) {
                    callback(err);
                }
                else {
                    i = i + 1;
                    if (i < array.length) {
                        iterate();
                    }
                    else {
                        callback();
                    }
                }
            };

            method(array[i], done, i);
        };

        iterate();
    }
};


exports.parallel = function (array, method, callback) {

    if (!array.length) {
        callback();
    }
    else {
        let count = 0;
        let errored = false;

        const done = function (err) {

            if (!errored) {
                if (err) {
                    errored = true;
                    callback(err);
                }
                else {
                    count = count + 1;
                    if (count === array.length) {
                        callback();
                    }
                }
            }
        };

        for (let i = 0; i < array.length; ++i) {
            method(array[i], done, i);
        }
    }
};


exports.parallel.execute = function (fnObj, callback) {

    const result = {};
    if (!fnObj) {
        return callback(null, result);
    }

    const keys = Object.keys(fnObj);
    let count = 0;
    let errored = false;

    if (!keys.length) {
        return callback(null, result);
    }

    const done = function (key) {

        return function (err, val) {

            if (!errored) {
                if (err) {
                    errored = true;
                    callback(err);
                }
                else {
                    result[key] = val;
                    if (++count === keys.length) {
                        callback(null, result);
                    }
                }
            }
        };
    };

    for (let i = 0; i < keys.length; ++i) {
        if (!errored) {
            const key = keys[i];
            fnObj[key](done(key));
        }
    }
};
