package com.netscape.javascript.qa.lc3.string;

/**
 *  3.4 Preferred Argument Conversions
 *
 */

public class String_003 {
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

    public String ambiguous( byte arg ) {

        return BYTE;
    }

    public String ambiguous( short arg ) {
        return SHORT;
    }

    public String ambiguous( int arg ) {
        return INT;
    }

    public String ambiguous( long arg ) {
        return LONG;
    }

    public String ambiguous( float arg ) {
        return FLOAT;
    }

    public String ambiguous( Double arg ) {
        return DOUBLE_OBJECT;
    }

    public String ambiguous( double arg ) {
        return DOUBLE;
    }

   public String ambiguous( char arg ) {
        return CHAR;
   }

   public String expect() {
        return CHAR;
   }

}