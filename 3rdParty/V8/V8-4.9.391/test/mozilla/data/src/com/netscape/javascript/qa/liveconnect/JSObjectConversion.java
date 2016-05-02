/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation, All Rights Reserved.
 */
package com.netscape.javascript.qa.liveconnect;

import netscape.javascript.JSObject;

/**
 *
 *  This is the same as DataTypeClass, but only contains conversion fields
 *  and methods for JSObject.  JSObject is not included in DataTypeClass
 *  so that tests can be run on Rhino (which does not implement JSObject).
 *
 *  Attempt to get and set the public fields.
 *
 *  XXX need to override toString, toNumber with values in
 *  static fields.
 *
 *  XXX override both instance and static versions of the method
 *  @author christine m begle
 */

public class JSObjectConversion {
    /**
     *  Override toNumber
     *
     */

    public static double PUB_DOUBLE_REPRESENTATION = 0.2134;

    public double doubleValue() {
        return PUB_DOUBLE_REPRESENTATION;
    }

    public int toNumber() {
        return PUB_NUMBER_REPRESENTATION;
    }

    /**
     *  Override booleanValue
     *
     */
    public boolean booleanValue() {
        return PUB_BOOLEAN_REPRESENTATION;
    }

    public boolean PUB_BOOLEAN_REPRESENTATION = true;

    public static int PUB_NUMBER_REPRESENTATION = 2134;

    /**
     * Override toString
     */

    public String toString() {
        return PUB_STRING_REPRESENTATION;
    }

    public static String PUB_STRING_REPRESENTATION =
        "DataTypeClass Instance";

    public static JSObject staticGetJSObject() {
        return PUB_STATIC_JSOBJECT;
    }

    public JSObject getJSObject() {
        return PUB_JSOBJECT;
    }
    public static void staticSetJSObject(JSObject jso) {
        PUB_STATIC_JSOBJECT = jso;
    }
    public void setJSObject(JSObject jso) {
        PUB_JSOBJECT = jso;
    }

    public JSObject[] createJSObjectArray(int length) {
        PUB_JSOBJECT_ARRAY = new JSObject[length];
        return PUB_JSOBJECT_ARRAY;
    }

    public static JSObject[] staticCreateJSObjectArray(int length) {
        PUB_STATIC_JSOBJECT_ARRAY = new JSObject[length];
        return PUB_STATIC_JSOBJECT_ARRAY;
    }

//  STATIC FIELDS

    public static final JSObject PUB_STATIC_FINAL_JSOBJECT = null;
    public static JSObject       PUB_STATIC_JSOBJECT       = PUB_STATIC_FINAL_JSOBJECT;
    public JSObject              PUB_JSOBJECT              = PUB_STATIC_FINAL_JSOBJECT;
    
    public JSObject[] PUB_JSOBJECT_ARRAY;
    public static JSObject[] PUB_STATIC_JSOBJECT_ARRAY;
}