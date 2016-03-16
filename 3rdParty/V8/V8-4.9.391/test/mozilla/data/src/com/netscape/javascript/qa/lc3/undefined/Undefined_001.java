package com.netscape.javascript.qa.lc3.undefined;

/**
 *  3.4 Preferred Argument Conversions
 *  3.4.1 Undefined
 *
 *  The JavaScript value of undefined can be passed to methods that expect
 *  either a java.lang.String or a java.lang.Object.
 *
 *  It is an error if undefined is passed to an ambiguous method (the object
 *  contains methods that accept either a java.lang.String or java.lang.Object).
 *
 *  You should be able to call the ambiguous methods explicitly using the
 *  Explicit Method invocation.
 *
 */

public class Undefined_001 {
    public String OBJECT = "OBJECT";
    public String STRING = "STRING";

    public static String STATIC_OBJECT = "OBJECT";
    public static String STATIC_STRING = "STRING";


    public String ambiguous( String arg ) {
        return STRING;
    }

    public String ambiguous( Object arg ) {
        return OBJECT;
    }

    public static String staticAmbiguous( String arg ) {
        return STATIC_STRING;
    }

    public static String staticAmbiguous( Object arg ) {
        return STATIC_OBJECT;
    }

}