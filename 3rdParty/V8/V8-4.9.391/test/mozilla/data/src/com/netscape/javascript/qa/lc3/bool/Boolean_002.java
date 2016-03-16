package com.netscape.javascript.qa.lc3.bool;

/**
 *  3.4 Preferred Argument Conversions
 *  3.4.2 Boolean.
 *  Given a
 *
 */

public class Boolean_002 {
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

    public int ambiguous( int arg ) {
        return INT;
    }

    public int ambiguous( long arg ) {
        return LONG;
    }

    public int ambiguous( String arg ) {
        return STRING;
    }

    public int ambiguous( Object arg ) {
        return OBJECT;
    }

    public int ambiguous( Boolean arg ) {
        return BOOLEAN_OBJECT;
    }
    public int expect() {
        return BOOLEAN_OBJECT;
    }
    
}