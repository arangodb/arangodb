package com.netscape.javascript.qa.lc3.bool;

/**
 *  3.4 Preferred Argument Conversions
 *  3.4.2 Boolean.
 *  Given a
 *
 */

public class Boolean_001 {
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

    public String ambiguous( float arg ) {
        return FLOAT;
    }

    public String ambiguous( double arg ) {
        return DOUBLE;
    }

    public String ambiguous( byte arg ) {
        return BYTE;
    }

    public String ambiguous( char arg ) {
        return CHAR;
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

    public String ambiguous( String arg ) {
        return STRING;
    }

    public String ambiguous( Object arg ) {
        return OBJECT;
    }

    public String ambiguous( Boolean arg ) {
        return BOOLEAN_OBJECT;
    }

    public String ambiguous( boolean arg ) {
        return BOOLEAN;
    }
    public String expect(){ 
        return BOOLEAN;
    }
    
}