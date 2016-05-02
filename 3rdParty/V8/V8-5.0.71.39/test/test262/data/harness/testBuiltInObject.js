// Copyright 2012 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/**
 * @description Tests that obj meets the requirements for built-in objects
 *     defined by the introduction of chapter 15 of the ECMAScript Language Specification.
 * @param {Object} obj the object to be tested.
 * @param {boolean} isFunction whether the specification describes obj as a function.
 * @param {boolean} isConstructor whether the specification describes obj as a constructor.
 * @param {String[]} properties an array with the names of the built-in properties of obj,
 *     excluding length, prototype, or properties with non-default attributes.
 * @param {number} length for functions only: the length specified for the function
 *     or derived from the argument list.
 * @author Norbert Lindenberg
 */

function testBuiltInObject(obj, isFunction, isConstructor, properties, length) {

    if (obj === undefined) {
        $ERROR("Object being tested is undefined.");
    }

    var objString = Object.prototype.toString.call(obj);
    if (isFunction) {
        if (objString !== "[object Function]") {
            $ERROR("The [[Class]] internal property of a built-in function must be " +
                    "\"Function\", but toString() returns " + objString);
        }
    } else {
        if (objString !== "[object Object]") {
            $ERROR("The [[Class]] internal property of a built-in non-function object must be " +
                    "\"Object\", but toString() returns " + objString);
        }
    }

    if (!Object.isExtensible(obj)) {
        $ERROR("Built-in objects must be extensible.");
    }

    if (isFunction && Object.getPrototypeOf(obj) !== Function.prototype) {
        $ERROR("Built-in functions must have Function.prototype as their prototype.");
    }

    if (isConstructor && Object.getPrototypeOf(obj.prototype) !== Object.prototype) {
        $ERROR("Built-in prototype objects must have Object.prototype as their prototype.");
    }

    // verification of the absence of the [[Construct]] internal property has
    // been moved to the end of the test
    
    // verification of the absence of the prototype property has
    // been moved to the end of the test

    if (isFunction) {
        
        if (typeof obj.length !== "number" || obj.length !== Math.floor(obj.length)) {
            $ERROR("Built-in functions must have a length property with an integer value.");
        }
    
        if (obj.length !== length) {
            $ERROR("Function's length property doesn't have specified value; expected " +
                length + ", got " + obj.length + ".");
        }

        var desc = Object.getOwnPropertyDescriptor(obj, "length");
        if (desc.writable) {
            $ERROR("The length property of a built-in function must not be writable.");
        }
        if (desc.enumerable) {
            $ERROR("The length property of a built-in function must not be enumerable.");
        }
        if (!desc.configurable) {
            $ERROR("The length property of a built-in function must be configurable.");
        }
    }

    properties.forEach(function(prop) {
        var desc = Object.getOwnPropertyDescriptor(obj, prop);
        if (desc === undefined) {
            $ERROR("Missing property " + prop + ".");
        }
        // accessor properties don't have writable attribute
        if (desc.hasOwnProperty("writable") && !desc.writable) {
            $ERROR("The " + prop + " property of this built-in object must be writable.");
        }
        if (desc.enumerable) {
            $ERROR("The " + prop + " property of this built-in object must not be enumerable.");
        }
        if (!desc.configurable) {
            $ERROR("The " + prop + " property of this built-in object must be configurable.");
        }
    });

    // The remaining sections have been moved to the end of the test because
    // unbound non-constructor functions written in JavaScript cannot possibly
    // pass them, and we still want to test JavaScript implementations as much
    // as possible.
    
    var exception;
    if (isFunction && !isConstructor) {
        // this is not a complete test for the presence of [[Construct]]:
        // if it's absent, the exception must be thrown, but it may also
        // be thrown if it's present and just has preconditions related to
        // arguments or the this value that this statement doesn't meet.
        try {
            /*jshint newcap:false*/
            var instance = new obj();
        } catch (e) {
            exception = e;
        }
        if (exception === undefined || exception.name !== "TypeError") {
            $ERROR("Built-in functions that aren't constructors must throw TypeError when " +
                "used in a \"new\" statement.");
        }
    }

    if (isFunction && !isConstructor && obj.hasOwnProperty("prototype")) {
        $ERROR("Built-in functions that aren't constructors must not have a prototype property.");
    }

    // passed the complete test!
    return true;
}

