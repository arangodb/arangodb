// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
es5id: 10.3.3
description: >
    Tests that the object returned by
    Intl.Collator.prototype.resolvedOptions  has the right properties.
author: Norbert Lindenberg
includes: [testIntl.js]
---*/

var actual = new Intl.Collator().resolvedOptions();

var actual2 = new Intl.Collator().resolvedOptions();
if (actual2 === actual) {
    $ERROR("resolvedOptions returned the same object twice.");
}

// source: CLDR file common/bcp47/collation.xml; version CLDR 21.
var collations = [
    "default", // added
    "big5han", 
    "dict",
    "direct",
    "ducet",
    "gb2312",
    "phonebk",
    "phonetic",
    "pinyin",
    "reformed",
    // "search", // excluded
    "searchjl",
    // "standard", // excluded
    "stroke",
    "trad",
    "unihan"
];

// this assumes the default values where the specification provides them
mustHaveProperty(actual, "locale", isCanonicalizedStructurallyValidLanguageTag);
mustHaveProperty(actual, "usage", ["sort"]);
mustHaveProperty(actual, "sensitivity", ["variant"]);
mustHaveProperty(actual, "ignorePunctuation", [false]);
mustHaveProperty(actual, "collation", collations);
mayHaveProperty(actual, "numeric", [true, false]);
mayHaveProperty(actual, "caseFirst", ["upper", "lower", "false"]);
