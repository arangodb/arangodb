// Copyright 2011-2012 Norbert Lindenberg. All rights reserved.
// Copyright 2012-2013 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/**
 * This file contains shared functions for the tests in the conformance test
 * suite for the ECMAScript Internationalization API.
 * @author Norbert Lindenberg
 */


/**
 * @description Calls the provided function for every service constructor in
 * the Intl object, until f returns a falsy value. It returns the result of the
 * last call to f, mapped to a boolean.
 * @param {Function} f the function to call for each service constructor in
 *     the Intl object.
 *     @param {Function} Constructor the constructor object to test with.
 * @result {Boolean} whether the test succeeded.
 */
function testWithIntlConstructors(f) {
    var constructors = ["Collator", "NumberFormat", "DateTimeFormat"];
    return constructors.every(function (constructor) {
        var Constructor = Intl[constructor];
        var result;
        try {
            result = f(Constructor);
        } catch (e) {
            e.message += " (Testing with " + constructor + ".)";
            throw e;
        }
        return result;
    });
}


/**
 * Returns the name of the given constructor object, which must be one of
 * Intl.Collator, Intl.NumberFormat, or Intl.DateTimeFormat.
 * @param {object} Constructor a constructor
 * @return {string} the name of the constructor
 */
function getConstructorName(Constructor) {
    switch (Constructor) {
        case Intl.Collator:
            return "Collator";
        case Intl.NumberFormat:
            return "NumberFormat";
        case Intl.DateTimeFormat:
            return "DateTimeFormat";
        default:
            $ERROR("test internal error: unknown Constructor");
    }
}


/**
 * Taints a named data property of the given object by installing
 * a setter that throws an exception.
 * @param {object} obj the object whose data property to taint
 * @param {string} property the property to taint
 */
function taintDataProperty(obj, property) {
    Object.defineProperty(obj, property, {
        set: function(value) {
            $ERROR("Client code can adversely affect behavior: setter for " + property + ".");
        },
        enumerable: false,
        configurable: true
    });
}


/**
 * Taints a named method of the given object by replacing it with a function
 * that throws an exception.
 * @param {object} obj the object whose method to taint
 * @param {string} property the name of the method to taint
 */
function taintMethod(obj, property) {
    Object.defineProperty(obj, property, {
        value: function() {
            $ERROR("Client code can adversely affect behavior: method " + property + ".");
        },
        writable: true,
        enumerable: false,
        configurable: true
    });
}


/**
 * Taints the given properties (and similarly named properties) by installing
 * setters on Object.prototype that throw exceptions.
 * @param {Array} properties an array of property names to taint
 */
function taintProperties(properties) {
    properties.forEach(function (property) {
        var adaptedProperties = [property, "__" + property, "_" + property, property + "_", property + "__"];
        adaptedProperties.forEach(function (property) {
            taintDataProperty(Object.prototype, property);
        });
    });
}


/**
 * Taints the Array object by creating a setter for the property "0" and
 * replacing some key methods with functions that throw exceptions.
 */
function taintArray() {
    taintDataProperty(Array.prototype, "0");
    taintMethod(Array.prototype, "indexOf");
    taintMethod(Array.prototype, "join");
    taintMethod(Array.prototype, "push");
    taintMethod(Array.prototype, "slice");
    taintMethod(Array.prototype, "sort");
}


// auxiliary data for getLocaleSupportInfo
var languages = ["zh", "es", "en", "hi", "ur", "ar", "ja", "pa"];
var scripts = ["Latn", "Hans", "Deva", "Arab", "Jpan", "Hant"];
var countries = ["CN", "IN", "US", "PK", "JP", "TW", "HK", "SG"];
var localeSupportInfo = {};


/**
 * Gets locale support info for the given constructor object, which must be one
 * of Intl.Collator, Intl.NumberFormat, Intl.DateTimeFormat.
 * @param {object} Constructor the constructor for which to get locale support info
 * @return {object} locale support info with the following properties:
 *     supported: array of fully supported language tags
 *     byFallback: array of language tags that are supported through fallbacks
 *     unsupported: array of unsupported language tags
 */
function getLocaleSupportInfo(Constructor) {
    var constructorName = getConstructorName(Constructor);
    if (localeSupportInfo[constructorName] !== undefined) {
        return localeSupportInfo[constructorName];
    }

    var allTags = [];
    var i, j, k;
    var language, script, country;
    for (i = 0; i < languages.length; i++) {
        language = languages[i];
        allTags.push(language);
        for (j = 0; j < scripts.length; j++) {
            script = scripts[j];
            allTags.push(language + "-" + script);
            for (k = 0; k < countries.length; k++) {
                country = countries[k];
                allTags.push(language + "-" + script + "-" + country);
            }
        }
        for (k = 0; k < countries.length; k++) {
            country = countries[k];
            allTags.push(language + "-" + country);
        }
    }
    
    var supported = [];
    var byFallback = [];
    var unsupported = [];
    for (i = 0; i < allTags.length; i++) {
        var request = allTags[i];
        var result = new Constructor([request], {localeMatcher: "lookup"}).resolvedOptions().locale;
         if (request === result) {
            supported.push(request);
        } else if (request.indexOf(result) === 0) {
            byFallback.push(request);
        } else {
            unsupported.push(request);
        }
    }
    
    localeSupportInfo[constructorName] = {
        supported: supported,
        byFallback: byFallback,
        unsupported: unsupported
    };
    
    return localeSupportInfo[constructorName];
}
        

/**
 * @description Tests whether locale is a String value representing a
 * structurally valid and canonicalized BCP 47 language tag, as defined in
 * sections 6.2.2 and 6.2.3 of the ECMAScript Internationalization API
 * Specification.
 * @param {String} locale the string to be tested.
 * @result {Boolean} whether the test succeeded.
 */
function isCanonicalizedStructurallyValidLanguageTag(locale) {

    /**
     * Regular expression defining BCP 47 language tags.
     *
     * Spec: RFC 5646 section 2.1.
     */
    var alpha = "[a-zA-Z]",
        digit = "[0-9]",
        alphanum = "(" + alpha + "|" + digit + ")",
        regular = "(art-lojban|cel-gaulish|no-bok|no-nyn|zh-guoyu|zh-hakka|zh-min|zh-min-nan|zh-xiang)",
        irregular = "(en-GB-oed|i-ami|i-bnn|i-default|i-enochian|i-hak|i-klingon|i-lux|i-mingo|i-navajo|i-pwn|i-tao|i-tay|i-tsu|sgn-BE-FR|sgn-BE-NL|sgn-CH-DE)",
        grandfathered = "(" + irregular + "|" + regular + ")",
        privateuse = "(x(-[a-z0-9]{1,8})+)",
        singleton = "(" + digit + "|[A-WY-Za-wy-z])",
        extension = "(" + singleton + "(-" + alphanum + "{2,8})+)",
        variant = "(" + alphanum + "{5,8}|(" + digit + alphanum + "{3}))",
        region = "(" + alpha + "{2}|" + digit + "{3})",
        script = "(" + alpha + "{4})",
        extlang = "(" + alpha + "{3}(-" + alpha + "{3}){0,2})",
        language = "(" + alpha + "{2,3}(-" + extlang + ")?|" + alpha + "{4}|" + alpha + "{5,8})",
        langtag = language + "(-" + script + ")?(-" + region + ")?(-" + variant + ")*(-" + extension + ")*(-" + privateuse + ")?",
        languageTag = "^(" + langtag + "|" + privateuse + "|" + grandfathered + ")$",
        languageTagRE = new RegExp(languageTag, "i");
    var duplicateSingleton = "-" + singleton + "-(.*-)?\\1(?!" + alphanum + ")",
        duplicateSingletonRE = new RegExp(duplicateSingleton, "i"),
        duplicateVariant = "(" + alphanum + "{2,8}-)+" + variant + "-(" + alphanum + "{2,8}-)*\\3(?!" + alphanum + ")",
        duplicateVariantRE = new RegExp(duplicateVariant, "i");


    /**
     * Verifies that the given string is a well-formed BCP 47 language tag
     * with no duplicate variant or singleton subtags.
     *
     * Spec: ECMAScript Internationalization API Specification, draft, 6.2.2.
     */
    function isStructurallyValidLanguageTag(locale) {
        if (!languageTagRE.test(locale)) {
            return false;
        }
        locale = locale.split(/-x-/)[0];
        return !duplicateSingletonRE.test(locale) && !duplicateVariantRE.test(locale);
    }


    /**
     * Mappings from complete tags to preferred values.
     *
     * Spec: IANA Language Subtag Registry.
     */
    var __tagMappings = {
        // property names must be in lower case; values in canonical form

        // grandfathered tags from IANA language subtag registry, file date 2011-08-25
        "art-lojban": "jbo",
        "cel-gaulish": "cel-gaulish",
        "en-gb-oed": "en-GB-oed",
        "i-ami": "ami",
        "i-bnn": "bnn",
        "i-default": "i-default",
        "i-enochian": "i-enochian",
        "i-hak": "hak",
        "i-klingon": "tlh",
        "i-lux": "lb",
        "i-mingo": "i-mingo",
        "i-navajo": "nv",
        "i-pwn": "pwn",
        "i-tao": "tao",
        "i-tay": "tay",
        "i-tsu": "tsu",
        "no-bok": "nb",
        "no-nyn": "nn",
        "sgn-be-fr": "sfb",
        "sgn-be-nl": "vgt",
        "sgn-ch-de": "sgg",
        "zh-guoyu": "cmn",
        "zh-hakka": "hak",
        "zh-min": "zh-min",
        "zh-min-nan": "nan",
        "zh-xiang": "hsn",
        // deprecated redundant tags from IANA language subtag registry, file date 2011-08-25
        "sgn-br": "bzs",
        "sgn-co": "csn",
        "sgn-de": "gsg",
        "sgn-dk": "dsl",
        "sgn-es": "ssp",
        "sgn-fr": "fsl",
        "sgn-gb": "bfi",
        "sgn-gr": "gss",
        "sgn-ie": "isg",
        "sgn-it": "ise",
        "sgn-jp": "jsl",
        "sgn-mx": "mfs",
        "sgn-ni": "ncs",
        "sgn-nl": "dse",
        "sgn-no": "nsl",
        "sgn-pt": "psr",
        "sgn-se": "swl",
        "sgn-us": "ase",
        "sgn-za": "sfs",
        "zh-cmn": "cmn",
        "zh-cmn-hans": "cmn-Hans",
        "zh-cmn-hant": "cmn-Hant",
        "zh-gan": "gan",
        "zh-wuu": "wuu",
        "zh-yue": "yue",
        // deprecated variant with prefix from IANA language subtag registry, file date 2011-08-25
        "ja-latn-hepburn-heploc": "ja-Latn-alalc97"
    };


    /**
     * Mappings from non-extlang subtags to preferred values.
     *
     * Spec: IANA Language Subtag Registry.
     */
    var __subtagMappings = {
        // property names and values must be in canonical case
        // language subtags with Preferred-Value mappings from IANA language subtag registry, file date 2011-08-25
        "in": "id",
        "iw": "he",
        "ji": "yi",
        "jw": "jv",
        "mo": "ro",
        "ayx": "nun",
        "cjr": "mom",
        "cmk": "xch",
        "drh": "khk",
        "drw": "prs",
        "gav": "dev",
        "mst": "mry",
        "myt": "mry",
        "tie": "ras",
        "tkk": "twm",
        "tnf": "prs",
        // region subtags with Preferred-Value mappings from IANA language subtag registry, file date 2011-08-25
        "BU": "MM",
        "DD": "DE",
        "FX": "FR",
        "TP": "TL",
        "YD": "YE",
        "ZR": "CD"
    };


    /**
     * Mappings from extlang subtags to preferred values.
     *
     * Spec: IANA Language Subtag Registry.
     */
    var __extlangMappings = {
        // extlang subtags with Preferred-Value mappings from IANA language subtag registry, file date 2011-08-25
        // values are arrays with [0] the replacement value, [1] (if present) the prefix to be removed
        "aao": ["aao", "ar"],
        "abh": ["abh", "ar"],
        "abv": ["abv", "ar"],
        "acm": ["acm", "ar"],
        "acq": ["acq", "ar"],
        "acw": ["acw", "ar"],
        "acx": ["acx", "ar"],
        "acy": ["acy", "ar"],
        "adf": ["adf", "ar"],
        "ads": ["ads", "sgn"],
        "aeb": ["aeb", "ar"],
        "aec": ["aec", "ar"],
        "aed": ["aed", "sgn"],
        "aen": ["aen", "sgn"],
        "afb": ["afb", "ar"],
        "afg": ["afg", "sgn"],
        "ajp": ["ajp", "ar"],
        "apc": ["apc", "ar"],
        "apd": ["apd", "ar"],
        "arb": ["arb", "ar"],
        "arq": ["arq", "ar"],
        "ars": ["ars", "ar"],
        "ary": ["ary", "ar"],
        "arz": ["arz", "ar"],
        "ase": ["ase", "sgn"],
        "asf": ["asf", "sgn"],
        "asp": ["asp", "sgn"],
        "asq": ["asq", "sgn"],
        "asw": ["asw", "sgn"],
        "auz": ["auz", "ar"],
        "avl": ["avl", "ar"],
        "ayh": ["ayh", "ar"],
        "ayl": ["ayl", "ar"],
        "ayn": ["ayn", "ar"],
        "ayp": ["ayp", "ar"],
        "bbz": ["bbz", "ar"],
        "bfi": ["bfi", "sgn"],
        "bfk": ["bfk", "sgn"],
        "bjn": ["bjn", "ms"],
        "bog": ["bog", "sgn"],
        "bqn": ["bqn", "sgn"],
        "bqy": ["bqy", "sgn"],
        "btj": ["btj", "ms"],
        "bve": ["bve", "ms"],
        "bvl": ["bvl", "sgn"],
        "bvu": ["bvu", "ms"],
        "bzs": ["bzs", "sgn"],
        "cdo": ["cdo", "zh"],
        "cds": ["cds", "sgn"],
        "cjy": ["cjy", "zh"],
        "cmn": ["cmn", "zh"],
        "coa": ["coa", "ms"],
        "cpx": ["cpx", "zh"],
        "csc": ["csc", "sgn"],
        "csd": ["csd", "sgn"],
        "cse": ["cse", "sgn"],
        "csf": ["csf", "sgn"],
        "csg": ["csg", "sgn"],
        "csl": ["csl", "sgn"],
        "csn": ["csn", "sgn"],
        "csq": ["csq", "sgn"],
        "csr": ["csr", "sgn"],
        "czh": ["czh", "zh"],
        "czo": ["czo", "zh"],
        "doq": ["doq", "sgn"],
        "dse": ["dse", "sgn"],
        "dsl": ["dsl", "sgn"],
        "dup": ["dup", "ms"],
        "ecs": ["ecs", "sgn"],
        "esl": ["esl", "sgn"],
        "esn": ["esn", "sgn"],
        "eso": ["eso", "sgn"],
        "eth": ["eth", "sgn"],
        "fcs": ["fcs", "sgn"],
        "fse": ["fse", "sgn"],
        "fsl": ["fsl", "sgn"],
        "fss": ["fss", "sgn"],
        "gan": ["gan", "zh"],
        "gom": ["gom", "kok"],
        "gse": ["gse", "sgn"],
        "gsg": ["gsg", "sgn"],
        "gsm": ["gsm", "sgn"],
        "gss": ["gss", "sgn"],
        "gus": ["gus", "sgn"],
        "hab": ["hab", "sgn"],
        "haf": ["haf", "sgn"],
        "hak": ["hak", "zh"],
        "hds": ["hds", "sgn"],
        "hji": ["hji", "ms"],
        "hks": ["hks", "sgn"],
        "hos": ["hos", "sgn"],
        "hps": ["hps", "sgn"],
        "hsh": ["hsh", "sgn"],
        "hsl": ["hsl", "sgn"],
        "hsn": ["hsn", "zh"],
        "icl": ["icl", "sgn"],
        "ils": ["ils", "sgn"],
        "inl": ["inl", "sgn"],
        "ins": ["ins", "sgn"],
        "ise": ["ise", "sgn"],
        "isg": ["isg", "sgn"],
        "isr": ["isr", "sgn"],
        "jak": ["jak", "ms"],
        "jax": ["jax", "ms"],
        "jcs": ["jcs", "sgn"],
        "jhs": ["jhs", "sgn"],
        "jls": ["jls", "sgn"],
        "jos": ["jos", "sgn"],
        "jsl": ["jsl", "sgn"],
        "jus": ["jus", "sgn"],
        "kgi": ["kgi", "sgn"],
        "knn": ["knn", "kok"],
        "kvb": ["kvb", "ms"],
        "kvk": ["kvk", "sgn"],
        "kvr": ["kvr", "ms"],
        "kxd": ["kxd", "ms"],
        "lbs": ["lbs", "sgn"],
        "lce": ["lce", "ms"],
        "lcf": ["lcf", "ms"],
        "liw": ["liw", "ms"],
        "lls": ["lls", "sgn"],
        "lsg": ["lsg", "sgn"],
        "lsl": ["lsl", "sgn"],
        "lso": ["lso", "sgn"],
        "lsp": ["lsp", "sgn"],
        "lst": ["lst", "sgn"],
        "lsy": ["lsy", "sgn"],
        "ltg": ["ltg", "lv"],
        "lvs": ["lvs", "lv"],
        "lzh": ["lzh", "zh"],
        "max": ["max", "ms"],
        "mdl": ["mdl", "sgn"],
        "meo": ["meo", "ms"],
        "mfa": ["mfa", "ms"],
        "mfb": ["mfb", "ms"],
        "mfs": ["mfs", "sgn"],
        "min": ["min", "ms"],
        "mnp": ["mnp", "zh"],
        "mqg": ["mqg", "ms"],
        "mre": ["mre", "sgn"],
        "msd": ["msd", "sgn"],
        "msi": ["msi", "ms"],
        "msr": ["msr", "sgn"],
        "mui": ["mui", "ms"],
        "mzc": ["mzc", "sgn"],
        "mzg": ["mzg", "sgn"],
        "mzy": ["mzy", "sgn"],
        "nan": ["nan", "zh"],
        "nbs": ["nbs", "sgn"],
        "ncs": ["ncs", "sgn"],
        "nsi": ["nsi", "sgn"],
        "nsl": ["nsl", "sgn"],
        "nsp": ["nsp", "sgn"],
        "nsr": ["nsr", "sgn"],
        "nzs": ["nzs", "sgn"],
        "okl": ["okl", "sgn"],
        "orn": ["orn", "ms"],
        "ors": ["ors", "ms"],
        "pel": ["pel", "ms"],
        "pga": ["pga", "ar"],
        "pks": ["pks", "sgn"],
        "prl": ["prl", "sgn"],
        "prz": ["prz", "sgn"],
        "psc": ["psc", "sgn"],
        "psd": ["psd", "sgn"],
        "pse": ["pse", "ms"],
        "psg": ["psg", "sgn"],
        "psl": ["psl", "sgn"],
        "pso": ["pso", "sgn"],
        "psp": ["psp", "sgn"],
        "psr": ["psr", "sgn"],
        "pys": ["pys", "sgn"],
        "rms": ["rms", "sgn"],
        "rsi": ["rsi", "sgn"],
        "rsl": ["rsl", "sgn"],
        "sdl": ["sdl", "sgn"],
        "sfb": ["sfb", "sgn"],
        "sfs": ["sfs", "sgn"],
        "sgg": ["sgg", "sgn"],
        "sgx": ["sgx", "sgn"],
        "shu": ["shu", "ar"],
        "slf": ["slf", "sgn"],
        "sls": ["sls", "sgn"],
        "sqs": ["sqs", "sgn"],
        "ssh": ["ssh", "ar"],
        "ssp": ["ssp", "sgn"],
        "ssr": ["ssr", "sgn"],
        "svk": ["svk", "sgn"],
        "swc": ["swc", "sw"],
        "swh": ["swh", "sw"],
        "swl": ["swl", "sgn"],
        "syy": ["syy", "sgn"],
        "tmw": ["tmw", "ms"],
        "tse": ["tse", "sgn"],
        "tsm": ["tsm", "sgn"],
        "tsq": ["tsq", "sgn"],
        "tss": ["tss", "sgn"],
        "tsy": ["tsy", "sgn"],
        "tza": ["tza", "sgn"],
        "ugn": ["ugn", "sgn"],
        "ugy": ["ugy", "sgn"],
        "ukl": ["ukl", "sgn"],
        "uks": ["uks", "sgn"],
        "urk": ["urk", "ms"],
        "uzn": ["uzn", "uz"],
        "uzs": ["uzs", "uz"],
        "vgt": ["vgt", "sgn"],
        "vkk": ["vkk", "ms"],
        "vkt": ["vkt", "ms"],
        "vsi": ["vsi", "sgn"],
        "vsl": ["vsl", "sgn"],
        "vsv": ["vsv", "sgn"],
        "wuu": ["wuu", "zh"],
        "xki": ["xki", "sgn"],
        "xml": ["xml", "sgn"],
        "xmm": ["xmm", "ms"],
        "xms": ["xms", "sgn"],
        "yds": ["yds", "sgn"],
        "ysl": ["ysl", "sgn"],
        "yue": ["yue", "zh"],
        "zib": ["zib", "sgn"],
        "zlm": ["zlm", "ms"],
        "zmi": ["zmi", "ms"],
        "zsl": ["zsl", "sgn"],
        "zsm": ["zsm", "ms"]
    };


    /**
     * Canonicalizes the given well-formed BCP 47 language tag, including regularized case of subtags.
     *
     * Spec: ECMAScript Internationalization API Specification, draft, 6.2.3.
     * Spec: RFC 5646, section 4.5.
     */
    function canonicalizeLanguageTag(locale) {

        // start with lower case for easier processing, and because most subtags will need to be lower case anyway
        locale = locale.toLowerCase();

        // handle mappings for complete tags
        if (__tagMappings.hasOwnProperty(locale)) {
            return __tagMappings[locale];
        }

        var subtags = locale.split("-");
        var i = 0;

        // handle standard part: all subtags before first singleton or "x"
        while (i < subtags.length) {
            var subtag = subtags[i];
            if (subtag.length === 1 && (i > 0 || subtag === "x")) {
                break;
            } else if (i !== 0 && subtag.length === 2) {
                subtag = subtag.toUpperCase();
            } else if (subtag.length === 4) {
                subtag = subtag[0].toUpperCase() + subtag.substring(1).toLowerCase();
            }
            if (__subtagMappings.hasOwnProperty(subtag)) {
                subtag = __subtagMappings[subtag];
            } else if (__extlangMappings.hasOwnProperty(subtag)) {
                subtag = __extlangMappings[subtag][0];
                if (i === 1 && __extlangMappings[subtag][1] === subtags[0]) {
                    subtags.shift();
                    i--;
                }
            }
            subtags[i] = subtag;
            i++;
        }
        var normal = subtags.slice(0, i).join("-");

        // handle extensions
        var extensions = [];
        while (i < subtags.length && subtags[i] !== "x") {
            var extensionStart = i;
            i++;
            while (i < subtags.length && subtags[i].length > 1) {
                i++;
            }
            var extension = subtags.slice(extensionStart, i).join("-");
            extensions.push(extension);
        }
        extensions.sort();

        // handle private use
        var privateUse;
        if (i < subtags.length) {
            privateUse = subtags.slice(i).join("-");
        }

        // put everything back together
        var canonical = normal;
        if (extensions.length > 0) {
            canonical += "-" + extensions.join("-");
        }
        if (privateUse !== undefined) {
            if (canonical.length > 0) {
                canonical += "-" + privateUse;
            } else {
                canonical = privateUse;
            }
        }

        return canonical;
    }

    return typeof locale === "string" && isStructurallyValidLanguageTag(locale) &&
            canonicalizeLanguageTag(locale) === locale;
}


/**
 * Tests whether the named options property is correctly handled by the given constructor.
 * @param {object} Constructor the constructor to test.
 * @param {string} property the name of the options property to test.
 * @param {string} type the type that values of the property are expected to have
 * @param {Array} [values] an array of allowed values for the property. Not needed for boolean.
 * @param {any} fallback the fallback value that the property assumes if not provided.
 * @param {object} testOptions additional options:
 *     @param {boolean} isOptional whether support for this property is optional for implementations.
 *     @param {boolean} noReturn whether the resulting value of the property is not returned.
 *     @param {boolean} isILD whether the resulting value of the property is implementation and locale dependent.
 *     @param {object} extra additional option to pass along, properties are value -> {option: value}.
 * @return {boolean} whether the test succeeded.
 */
function testOption(Constructor, property, type, values, fallback, testOptions) {
    var isOptional = testOptions !== undefined && testOptions.isOptional === true;
    var noReturn = testOptions !== undefined && testOptions.noReturn === true;
    var isILD = testOptions !== undefined && testOptions.isILD === true;
    
    function addExtraOptions(options, value, testOptions) {
        if (testOptions !== undefined && testOptions.extra !== undefined) {
            var extra;
            if (value !== undefined && testOptions.extra[value] !== undefined) {
                extra = testOptions.extra[value];
            } else if (testOptions.extra.any !== undefined) {
                extra = testOptions.extra.any;
            }
            if (extra !== undefined) {
                Object.getOwnPropertyNames(extra).forEach(function (prop) {
                    options[prop] = extra[prop];
                });
            }
        }
    }

    var testValues, options, obj, expected, actual, error;

    // test that the specified values are accepted. Also add values that convert to specified values.
    if (type === "boolean") {
        if (values === undefined) {
            values = [true, false];
        }
        testValues = values.slice(0);
        testValues.push(888);
        testValues.push(0);
    } else if (type === "string") {
        testValues = values.slice(0);
        testValues.push({toString: function () { return values[0]; }});
    }
    testValues.forEach(function (value) {
        options = {};
        options[property] = value;
        addExtraOptions(options, value, testOptions);
        obj = new Constructor(undefined, options);
        if (noReturn) {
            if (obj.resolvedOptions().hasOwnProperty(property)) {
                $ERROR("Option property " + property + " is returned, but shouldn't be.");
            }
        } else {
            actual = obj.resolvedOptions()[property];
            if (isILD) {
                if (actual !== undefined && values.indexOf(actual) === -1) {
                    $ERROR("Invalid value " + actual + " returned for property " + property + ".");
                }
            } else {
                if (type === "boolean") {
                    expected = Boolean(value);
                } else if (type === "string") {
                    expected = String(value);
                }
                if (actual !== expected && !(isOptional && actual === undefined)) {
                    $ERROR("Option value " + value + " for property " + property +
                        " was not accepted; got " + actual + " instead.");
                }
            }
        }
    });

    // test that invalid values are rejected
    if (type === "string") {
        var invalidValues = ["invalidValue", -1, null];
        // assume that we won't have values in caseless scripts
        if (values[0].toUpperCase() !== values[0]) {
            invalidValues.push(values[0].toUpperCase());
        } else {
            invalidValues.push(values[0].toLowerCase());
        }
        invalidValues.forEach(function (value) {
            options = {};
            options[property] = value;
            addExtraOptions(options, value, testOptions);
            error = undefined;
            try {
                obj = new Constructor(undefined, options);
            } catch (e) {
                error = e;
            }
            if (error === undefined) {
                $ERROR("Invalid option value " + value + " for property " + property + " was not rejected.");
            } else if (error.name !== "RangeError") {
                $ERROR("Invalid option value " + value + " for property " + property + " was rejected with wrong error " + error.name + ".");
            }
        });
    }

    // test that fallback value or another valid value is used if no options value is provided
    if (!noReturn) {
        options = {};
        addExtraOptions(options, undefined, testOptions);
        obj = new Constructor(undefined, options);
        actual = obj.resolvedOptions()[property];
        if (!(isOptional && actual === undefined)) {
            if (fallback !== undefined) {
                if (actual !== fallback) {
                    $ERROR("Option fallback value " + fallback + " for property " + property +
                        " was not used; got " + actual + " instead.");
                }
            } else {
                if (values.indexOf(actual) === -1 && !(isILD && actual === undefined)) {
                    $ERROR("Invalid value " + actual + " returned for property " + property + ".");
                }
            }
        }
    }

    return true;
}


/**
 * Tests whether the named property of the given object has a valid value
 * and the default attributes of the properties of an object literal.
 * @param {Object} obj the object to be tested.
 * @param {string} property the name of the property
 * @param {Function|Array} valid either a function that tests value for validity and returns a boolean,
 *     an array of valid values.
 * @exception if the property has an invalid value.
 */
function testProperty(obj, property, valid) {
    var desc = Object.getOwnPropertyDescriptor(obj, property);
    if (!desc.writable) {
        $ERROR("Property " + property + " must be writable.");
    }
    if (!desc.enumerable) {
        $ERROR("Property " + property + " must be enumerable.");
    }
    if (!desc.configurable) {
        $ERROR("Property " + property + " must be configurable.");
    }
    var value = desc.value;
    var isValid = (typeof valid === "function") ? valid(value) : (valid.indexOf(value) !== -1);
    if (!isValid) {
        $ERROR("Property value " + value + " is not allowed for property " + property + ".");
    }
}


/**
 * Tests whether the named property of the given object, if present at all, has a valid value
 * and the default attributes of the properties of an object literal.
 * @param {Object} obj the object to be tested.
 * @param {string} property the name of the property
 * @param {Function|Array} valid either a function that tests value for validity and returns a boolean,
 *     an array of valid values.
 * @exception if the property is present and has an invalid value.
 */
function mayHaveProperty(obj, property, valid) {
    if (obj.hasOwnProperty(property)) {
        testProperty(obj, property, valid);
    }
}


/**
 * Tests whether the given object has the named property with a valid value
 * and the default attributes of the properties of an object literal.
 * @param {Object} obj the object to be tested.
 * @param {string} property the name of the property
 * @param {Function|Array} valid either a function that tests value for validity and returns a boolean,
 *     an array of valid values.
 * @exception if the property is missing or has an invalid value.
 */
function mustHaveProperty(obj, property, valid) {
    if (!obj.hasOwnProperty(property)) {
        $ERROR("Object is missing property " + property + ".");
    }
    testProperty(obj, property, valid);
}


/**
 * Tests whether the given object does not have the named property.
 * @param {Object} obj the object to be tested.
 * @param {string} property the name of the property
 * @exception if the property is present.
 */
function mustNotHaveProperty(obj, property) {
    if (obj.hasOwnProperty(property)) {
        $ERROR("Object has property it mustn't have: " + property + ".");
    }
}


/**
 * Properties of the RegExp constructor that may be affected by use of regular
 * expressions, and the default values of these properties. Properties are from
 * https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Deprecated_and_obsolete_features#RegExp_Properties
 */
var regExpProperties = ["$1", "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
    "$_", "$*", "$&", "$+", "$`", "$'",
    "input", "lastMatch", "lastParen", "leftContext", "rightContext"
];

var regExpPropertiesDefaultValues = (function () {
    var values = Object.create(null);
    regExpProperties.forEach(function (property) {
        values[property] = RegExp[property];
    });
    return values;
}());


/**
 * Tests that executing the provided function (which may use regular expressions
 * in its implementation) does not create or modify unwanted properties on the
 * RegExp constructor.
 */
function testForUnwantedRegExpChanges(testFunc) {
    regExpProperties.forEach(function (property) {
        RegExp[property] = regExpPropertiesDefaultValues[property];
    });
    testFunc();
    regExpProperties.forEach(function (property) {
        if (RegExp[property] !== regExpPropertiesDefaultValues[property]) {
            $ERROR("RegExp has unexpected property " + property + " with value " +
                RegExp[property] + ".");
        }
    });
}


/**
 * Tests whether name is a valid BCP 47 numbering system name
 * and not excluded from use in the ECMAScript Internationalization API.
 * @param {string} name the name to be tested.
 * @return {boolean} whether name is a valid BCP 47 numbering system name and
 *     allowed for use in the ECMAScript Internationalization API.
 */

function isValidNumberingSystem(name) {
    
    // source: CLDR file common/bcp47/number.xml; version CLDR 21.
    var numberingSystems = [
        "arab",
        "arabext",
        "armn",
        "armnlow",
        "bali",
        "beng",
        "brah",
        "cakm",
        "cham",
        "deva",
        "ethi",
        "finance",
        "fullwide",
        "geor",
        "grek",
        "greklow",
        "gujr",
        "guru",
        "hanidec",
        "hans",
        "hansfin",
        "hant",
        "hantfin",
        "hebr",
        "java",
        "jpan",
        "jpanfin",
        "kali",
        "khmr",
        "knda",
        "osma",            
        "lana",
        "lanatham",
        "laoo",
        "latn",
        "lepc",
        "limb",
        "mlym",
        "mong",
        "mtei",
        "mymr",
        "mymrshan",
        "native",
        "nkoo",
        "olck",
        "orya",
        "roman",
        "romanlow",
        "saur",
        "shrd",
        "sora",
        "sund",
        "talu",
        "takr",
        "taml",
        "tamldec",
        "telu",
        "thai",
        "tibt",
        "traditio",
        "vaii"
    ];
    
    var excluded = [
        "finance",
        "native",
        "traditio"
    ];
        
    
    return numberingSystems.indexOf(name) !== -1 && excluded.indexOf(name) === -1;
}


/**
 * Provides the digits of numbering systems with simple digit mappings,
 * as specified in 11.3.2.
 */

var numberingSystemDigits = {
    arab: "٠١٢٣٤٥٦٧٨٩",
    arabext: "۰۱۲۳۴۵۶۷۸۹",
    beng: "০১২৩৪৫৬৭৮৯",
    deva: "०१२३४५६७८९",
    fullwide: "０１２３４５６７８９",
    gujr: "૦૧૨૩૪૫૬૭૮૯",
    guru: "੦੧੨੩੪੫੬੭੮੯",
    hanidec: "〇一二三四五六七八九",
    khmr: "០១២៣៤៥៦៧៨៩",
    knda: "೦೧೨೩೪೫೬೭೮೯",
    laoo: "໐໑໒໓໔໕໖໗໘໙",
    latn: "0123456789",
    mlym: "൦൧൨൩൪൫൬൭൮൯",
    mong: "᠐᠑᠒᠓᠔᠕᠖᠗᠘᠙",
    mymr: "၀၁၂၃၄၅၆၇၈၉",
    orya: "୦୧୨୩୪୫୬୭୮୯",
    tamldec: "௦௧௨௩௪௫௬௭௮௯",
    telu: "౦౧౨౩౪౫౬౭౮౯",
    thai: "๐๑๒๓๔๕๖๗๘๙",
    tibt: "༠༡༢༣༤༥༦༧༨༩"
};


/**
 * Tests that number formatting is handled correctly. The function checks that the
 * digit sequences in formatted output are as specified, converted to the
 * selected numbering system, and embedded in consistent localized patterns.
 * @param {Array} locales the locales to be tested.
 * @param {Array} numberingSystems the numbering systems to be tested.
 * @param {Object} options the options to pass to Intl.NumberFormat. Options
 *     must include {useGrouping: false}, and must cause 1.1 to be formatted
 *     pre- and post-decimal digits.
 * @param {Object} testData maps input data (in ES5 9.3.1 format) to expected output strings
 *     in unlocalized format with Western digits.
 */

function testNumberFormat(locales, numberingSystems, options, testData) {
    locales.forEach(function (locale) {
        numberingSystems.forEach(function (numbering) {
            var digits = numberingSystemDigits[numbering];
            var format = new Intl.NumberFormat([locale + "-u-nu-" + numbering], options);
    
            function getPatternParts(positive) {
                var n = positive ? 1.1 : -1.1;
                var formatted = format.format(n);
                var oneoneRE = "([^" + digits + "]*)[" + digits + "]+([^" + digits + "]+)[" + digits + "]+([^" + digits + "]*)";
                var match = formatted.match(new RegExp(oneoneRE));
                if (match === null) {
                    $ERROR("Unexpected formatted " + n + " for " +
                        format.resolvedOptions().locale + " and options " +
                        JSON.stringify(options) + ": " + formatted);
                }
                return match;
            }
            
            function toNumbering(raw) {
                return raw.replace(/[0-9]/g, function (digit) {
                    return digits[digit.charCodeAt(0) - "0".charCodeAt(0)];
                });
            }
            
            function buildExpected(raw, patternParts) {
                var period = raw.indexOf(".");
                if (period === -1) {
                    return patternParts[1] + toNumbering(raw) + patternParts[3];
                } else {
                    return patternParts[1] + 
                        toNumbering(raw.substring(0, period)) +
                        patternParts[2] +
                        toNumbering(raw.substring(period + 1)) +
                        patternParts[3];
                }
            }
            
            if (format.resolvedOptions().numberingSystem === numbering) {
                // figure out prefixes, infixes, suffixes for positive and negative values
                var posPatternParts = getPatternParts(true);
                var negPatternParts = getPatternParts(false);
                
                Object.getOwnPropertyNames(testData).forEach(function (input) {
                    var rawExpected = testData[input];
                    var patternParts;
                    if (rawExpected[0] === "-") {
                        patternParts = negPatternParts;
                        rawExpected = rawExpected.substring(1);
                    } else {
                        patternParts = posPatternParts;
                    }
                    var expected = buildExpected(rawExpected, patternParts);
                    var actual = format.format(input);
                    if (actual !== expected) {
                        $ERROR("Formatted value for " + input + ", " +
                        format.resolvedOptions().locale + " and options " +
                        JSON.stringify(options) + " is " + actual + "; expected " + expected + ".");
                    }
                });
            }
        });
    });
}


/**
 * Return the components of date-time formats.
 * @return {Array} an array with all date-time components.
 */

function getDateTimeComponents() {
    return ["weekday", "era", "year", "month", "day", "hour", "minute", "second", "timeZoneName"];
}


/**
 * Return the valid values for the given date-time component, as specified
 * by the table in section 12.1.1.
 * @param {string} component a date-time component.
 * @return {Array} an array with the valid values for the component.
 */

function getDateTimeComponentValues(component) {
    
    var components = {
        weekday: ["narrow", "short", "long"],
        era: ["narrow", "short", "long"],
        year: ["2-digit", "numeric"],
        month: ["2-digit", "numeric", "narrow", "short", "long"],
        day: ["2-digit", "numeric"],
        hour: ["2-digit", "numeric"],
        minute: ["2-digit", "numeric"],
        second: ["2-digit", "numeric"],
        timeZoneName: ["short", "long"]
    };
    
    var result = components[component];
    if (result === undefined) {
        $ERROR("Internal error: No values defined for date-time component " + component + ".");
    }
    return result;
}


/**
 * Tests that the given value is valid for the given date-time component.
 * @param {string} component a date-time component.
 * @param {string} value the value to be tested.
 * @return {boolean} true if the test succeeds.
 * @exception if the test fails.
 */

function testValidDateTimeComponentValue(component, value) {
    if (getDateTimeComponentValues(component).indexOf(value) === -1) {
        $ERROR("Invalid value " + value + " for date-time component " + component + ".");
    }
    return true;
}


/**
 * @description Tests whether timeZone is a String value representing a
 * structurally valid and canonicalized time zone name, as defined in
 * sections 6.4.1 and 6.4.2 of the ECMAScript Internationalization API
 * Specification.
 * @param {String} timeZone the string to be tested.
 * @result {Boolean} whether the test succeeded.
 */

function isCanonicalizedStructurallyValidTimeZoneName(timeZone) {
    /**
     * Regular expression defining IANA Time Zone names.
     *
     * Spec: IANA Time Zone Database, Theory file
     */
    var fileNameComponent = "(?:[A-Za-z_]|\\.(?!\\.?(?:/|$)))[A-Za-z.\\-_]{0,13}";
    var fileName = fileNameComponent + "(?:/" + fileNameComponent + ")*";
    var etcName = "(?:Etc/)?GMT[+-]\\d{1,2}";
    var systemVName = "SystemV/[A-Z]{3}\\d{1,2}(?:[A-Z]{3})?";
    var legacyName = etcName + "|" + systemVName + "|CST6CDT|EST5EDT|MST7MDT|PST8PDT|NZ|Canada/East-Saskatchewan";
    var zoneNamePattern = new RegExp("^(?:" + fileName + "|" + legacyName + ")$");

    if (typeof timeZone !== "string") {
        return false;
    }
    // 6.4.2 CanonicalizeTimeZoneName (timeZone), step 3
    if (timeZone === "UTC") {
        return true;
    }
    // 6.4.2 CanonicalizeTimeZoneName (timeZone), step 3
    if (timeZone === "Etc/UTC" || timeZone === "Etc/GMT") {
        return false;
    }
    return zoneNamePattern.test(timeZone);
}


/**
 * Verifies that the actual array matches the expected one in length, elements,
 * and element order.
 * @param {Array} expected the expected array.
 * @param {Array} actual the actual array.
 * @return {boolean} true if the test succeeds.
 * @exception if the test fails.
 */
function testArraysAreSame(expected, actual) {
    var i;
    for (i = 0; i < Math.max(actual.length, expected.length); i++) {
        if (actual[i] !== expected[i]) {
            $ERROR("Result array element at index " + i + " should be \"" +
                expected[i] + "\" but is \"" + actual[i] + "\".");
        }
    }
    return true;
}

