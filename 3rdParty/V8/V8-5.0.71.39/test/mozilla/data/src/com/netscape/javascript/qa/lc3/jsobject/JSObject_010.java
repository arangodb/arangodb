package com.netscape.javascript.qa.lc3.jsobject;

/**
 *  3.4 Preferred Argument Conversions
 *  3.4.2 Boolean.
 */

import netscape.javascript.JSObject;

public class JSObject_010 {

    public String BOOLEAN = "BOOLEAN";
    public String BOOLEAN_OBJECT = "BOOLEAN_OBJECT";
    public String OBJECT = "OBJECT";
    public String STRING = "STRING";
    public String LONG   = "LONG";
    public String INT    = "INT";
    public String SHORT  = "SHORT";
    public String CHAR   = "CHAR";
    public String BYTE   = "BYTE";
    public String DOUBLE = "DOUBLE";
    public String FLOAT  = "FLOAT";
    public String JSOBJECT="JSOBJECT";

    public String ambiguous( Boolean arg ) {
        return BOOLEAN_OBJECT;
    }

    public String ambiguous( boolean arg ) {
        return BOOLEAN;
    }

    public String ambiguous( byte arg ) {
        return BYTE;
    }
}