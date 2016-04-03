// Copyright 2013 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 11.1.1_32
description: >
    Tests that the options minimumSignificantDigits and
    maximumSignificantDigits are read in the right sequence.
author: Norbert Lindenberg
---*/

var read = 0;

function readMinimumSignificantDigits() {
    ++read;
    if (read === 1) {
        return 0; // invalid value, but on first read that's OK
    } else if (read === 3) {
        return 1; // valid value
    } else {
        $ERROR("minimumSignificantDigits read out of sequence: " + read + ".");
    }
}

function readMaximumSignificantDigits() {
    ++read;
    if (read === 2) {
        return 0; // invalid value, but on first read that's OK
    } else if (read === 4) {
        return 1; // valid value
    } else {
        $ERROR("maximumSignificantDigits read out of sequence: " + read + ".");
    }
}

var options = {};
Object.defineProperty(options, "minimumSignificantDigits",
    { get: readMinimumSignificantDigits });
Object.defineProperty(options, "maximumSignificantDigits",
    { get: readMaximumSignificantDigits });

new Intl.NumberFormat("de", options);

if (read !== 4) {
    $ERROR("insuffient number of property reads: " + read + ".");
}
