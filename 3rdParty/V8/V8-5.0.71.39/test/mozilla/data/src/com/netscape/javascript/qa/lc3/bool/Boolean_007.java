package com.netscape.javascript.qa.lc3.bool;

/**
 *  3.4 Preferred Argument Conversions
 *  3.4.2 Boolean.
 *  Given a Java object that has several ambiguous methods, if JavaScript calls
 *  the ambiguous method with a boolean, the order of preference should be as
 *  follows, regardless of the order in which the methods are declared.
 *  boolean
 *  java.lang.Boolean
 *  java.lang.Object
 *  java.lang.String
 *  long, int, short, char, byte
 *  double, float
 *
 *  In this case, the expected result depends on the specific method that is
 *  invoked.  Invoking with using explicit method invocation results in a 
 *  runtime error.
 *
 */

public class Boolean_007 {
    public int BOOLEAN = 0;
    public int BOOLEAN_OBJECT = 1;
    public int OBJECT = 2;
    public int STRING = 4;
    public int LONG   = 8;
    public int INT    = 16;
    public int SHORT  = 32;
    public int CHAR   = 64;
    public int BYTE   = 128;
    public int DOUBLE = 256;
    public int FLOAT  = 512;

    public int ambiguous( float arg ) {
        return FLOAT;
    }

    public int ambiguous( double arg ) {
        return DOUBLE;
    }

    public int ambiguous( byte arg ) {
        return BYTE;
    }

    public int ambiguous( char arg ) {
        return CHAR;
    }

    public int ambiguous( short arg ) {
        return SHORT;
    }

}