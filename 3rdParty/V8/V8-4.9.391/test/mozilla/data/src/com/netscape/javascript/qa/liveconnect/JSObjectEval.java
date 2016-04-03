package com.netscape.javascript.qa.liveconnect;

import netscape.javascript.JSObject;


/**
 *  Class with one static method that exercises JSObject.eval with
 *  any JSObject and any JS code.  Used by tests in
 *   ns/js/tests/lc3/exceptions
 *
 *
 */
public class JSObjectEval {
    /**
     *  Given a JSObject and some JavaScript code, have the object
     *  evaluate the JavaScript code.
     *
     */
    public static Object eval(JSObject obj, String code) {
    	obj.eval(code);
	    return null;
    }
}
