package com.netscape.javascript.qa.lc3.string;

/**
 *  3.4 Preferred Argument Conversions
 *
 */

public class String_005 {
    public String BOOLEAN = "BOOLEAN";
    public String BOOLEAN_OBJECT = "BOOLEAN_OBJECT";
    public String DOUBLE_OBJECT =  "DOUBLE_OBJECT";
    public String OBJECT = "OBJECT";
    public String STRING = "STRING";
    public String LONG   = "LONG";
    public String INT    = "INT";
    public String SHORT  = "SHORT";
    public String CHAR   = "CHAR";
    public String BYTE   = "BYTE";
    public String DOUBLE = "DOUBLE";
    public String FLOAT  = "FLOAT";

    public String ambiguous( boolean arg ) {
        return BOOLEAN;
    }

    public String ambiguous( Boolean arg ) {
        return BOOLEAN_OBJECT;
    }

    public String ambiguous( Double arg ) {
        return DOUBLE_OBJECT;
    }

    public String expect() {
        return BOOLEAN;
    }
}