package com.netscape.javascript.qa.liveconnect.jsobject;

import netscape.javascript.*;

import com.netscape.javascript.qa.liveconnect.LiveConnectTest;

/**
 *  Try to get properties of the global object.  This test verifies that 
 *  getMember can access default properties of the global object, and that
 *  the Object returned is an instance of netscape.javascript.JSObject 
 *  for objects, and java.lang.Double for numbers (NaN, Infinity).
 *
 *  @see com.netscape.javascript.qa.liveconnect.LiveConnectTest
 *
 *  @author christine@netscape.com
 *
 */
public class JSObject_001 extends LiveConnectTest {
    public JSObject_001() {
        super();
    }

    public static void main( String[] args ) {
        JSObject_001 test = new JSObject_001();
        test.start();
    }

    public void executeTest() {
        getDouble( "NaN" );        
        getDouble( "Infinity" );        
        getJSObject( "parseInt" );
        getJSObject( "parseFloat" );
        getJSObject( "eval" );
        getJSObject( "escape" );
        getJSObject( "unescape" );
        getJSObject( "isNaN" );
        getJSObject( "isFinite" );
        getJSObject( "Object" );
        getJSObject( "Function" );
        getJSObject( "Array" );
        getJSObject( "String" );
        getJSObject( "Boolean" );
        getJSObject( "Number" );        
        getJSObject( "Date" );
        getJSObject( "Math" );
    }
    
    /**
     *  Try to get a property of the JavaScript Global object.  The
     *  type of this property should be a java.lang.Double.  If it
     *  is not, the test will fail with a java.lang.ClassCastException.
     *
     *  @param property name of the JavaScript property to get.
     */
    public void getDouble( String property ) {
        Object jsobject  = null;
        String exception = null;

        String identifier   = this.getClass().toString();
        String description  = "Object jsobject = global.getMember(\""+ 
            property +"\"); jsobject instanceof java.lang.Double ";

        try {
            jsobject = global.getMember( property );
        } catch ( Exception e ) {
            exception = ("Exception getting " + property +": "+ e.toString());
            System.err.println( exception );
        }

        String expect = "true";
        String actual = (jsobject instanceof java.lang.Double) +"";

        addTestCase( description, expect, actual, exception );    
    }
    
    /**
     *  Try to get a property of the JavaScript Global object.  The type of 
     *  this property should be a netscape.javascript.JSObject.  If it is not,
     *  the test will fail with a java.lang.ClassCastException.
     *
     *  @param property name of the JavaScript property to get.
     */
    public void getJSObject( String property ) {
        Object jsobject  = null;
        String exception = null;

        String identifier   = this.getClass().toString();
        String description  = "Object jsobject = global.getMember(\""+ 
            property +"\"); jsobject instanceof JSObject ";

        try {
            jsobject = global.getMember( property );
        } catch ( Exception e ) {
            exception = ("Exception getting " + property +": "+ e.toString());
            System.err.println( exception );
        }

        String expect = "true";
        String actual = (jsobject instanceof JSObject) +"";

        addTestCase( description, expect, actual, exception );
    }        
}
