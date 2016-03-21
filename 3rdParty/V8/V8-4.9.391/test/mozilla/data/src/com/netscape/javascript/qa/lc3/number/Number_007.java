package com.netscape.javascript.qa.lc3.number;

/**
 *  3.4 Preferred Argument Conversions
 *
 */

public class Number_007 {
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

    public String ambiguous( Boolean arg ) {
        return BOOLEAN_OBJECT;
    }

    public String ambiguous( boolean arg ) {
        return BOOLEAN;
    }

    public String ambiguous( Object arg ) {
        return OBJECT;
    }

    public String ambiguous( String arg ) {
        return STRING;
    }

    public String ambiguous( byte arg ) {
        return BYTE;
    }

    public String ambiguous( char arg ) {
        return CHAR;
    }

    public String expect() {
        return CHAR;
    }
}