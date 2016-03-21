
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Tests JavaScript -> Java data type conversion.  This test creates a
 *  JavaScript and sets the value of a variable in the JavaScript context, 
 *  using JSObject.eval.  The test gets the value of the JavaScript variable
 *  in two ways:  using JSObject.eval( varname ) and 
 *  JSObject.getMember( varname).
 *
 *  <p>
 *
 *  The test then verifies that the result object is of the correct type, and
 *  also verifies the value of the object, using the rules defined by JSObject.
 *
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_003 extends LiveConnectTest {
    public DataTypes_003() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_003 test = new DataTypes_003();
        test.start();
    }

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass"  );
    }

    /**
     *
     * Values passed from JavaScript to Java are converted as follows:
     * <ul>
     * <li >
     * <li > objects that are wrappers around java objects are unwrapped 
     * <li > other objects are wrapped with a JSObject 
     * <li> strings, numbers and booleans are converted to String, Float, and 
     *  Boolean objects, respectively 
     *
     *  @see netscape.javascript.JSObject
     */
    public void executeTest() {
        // java objects are unwrapped
        doGetJSVarTests( 
            "Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass",
            "java.lang.Class",
            "class com.netscape.javascript.qa.liveconnect.DataTypeClass" );

        doGetJSVarTests( "new java.lang.String(\"java string\")",
            "java.lang.String",
            "java string" );

        //  strings numbers and booleans are converted to String, Double and 
        //  Boolean objects
        doGetJSVarTests( "Boolean(true)", "java.lang.Boolean", "true" );
        doGetJSVarTests( "Boolean()", "java.lang.Boolean", "false" );
        doGetJSVarTests( "Boolean(false)", "java.lang.Boolean", "false" );        
        
        doGetJSVarTests( "Number(12345)",  "java.lang.Double",  new Double("12345").toString() );
        doGetJSVarTests( "12345",  "java.lang.Double",  new Double("12345").toString() );        
        
        doGetJSVarTests( "String(\"hello\")",  "java.lang.String", "hello" );
        doGetJSVarTests( "\"hello\"",  "java.lang.String", "hello" );
        
        doGetJSVarTests( "true", "java.lang.Boolean", "true" );        
        doGetJSVarTests( "false", "java.lang.Boolean", "false" );
        
        // other objects are wrapped with a JSObject
        
        doGetJSVarTests( "new Number(0)",   "netscape.javascript.JSObject", "0" );

//      Throws nullPointerException -- put in its own test        
//        doGetJSVarTests( "null",            "netscape.javascript.JSObject", "null" );

//      4.05 returns a java.lang.String which i guess we need to maintain for 
//      backward compatilibilty
        doGetJSVarTests( "void 0",          "java.lang.String", "undefined" );

        doGetJSVarTests( "new Number(999)",   "netscape.javascript.JSObject",   "999" );
        
        doGetJSVarTests( "Math",    "netscape.javascript.JSObject",     "[object Math]" );
        
        doGetJSVarTests( "new Boolean(true)", "netscape.javascript.JSObject", "true" );        
        doGetJSVarTests( "new Boolean(false)", "netscape.javascript.JSObject", "false" );
        
        doGetJSVarTests( "new String()",   "netscape.javascript.JSObject", "" );
        doGetJSVarTests( "new String(\"hi\")",   "netscape.javascript.JSObject", "hi" );
        
        doGetJSVarTests( "new Array(1,2,3,4,5)", "netscape.javascript.JSObject", "1,2,3,4,5" );
        doGetJSVarTests( "[5,4,3,2,1]", "netscape.javascript.JSObject", "5,4,3,2,1" );
        
    }
    
    /**
     *  Pass the same arguments to two different tests.
     *
     *  @param rightExpr right-hand side to a JavaScript assignment expression
     *  @param className expected name of the class of the JavaScript object,
     *  when its value is retrieved from the JavaScript context using 
     *  JSObject.eval or JSObject.getMember
     *  @param value    string representation of the value of the JavaScript
     *  object.
     *
     *  @see #getJSVarWithEval
     *  @see #getJSVarWithGetMember
     */
    
    public void doGetJSVarTests( String rightExpr, String className,
        String value ) {
            getJSVarWithEval( rightExpr, className, value );
            getJSVarWithGetMember( rightExpr, className, value );
    }

    /**
     *  Use JSObject.eval to create a JavaScript variable of a JavaScript type.
     *  Get the value of the variable using JSObject.getMember and
     *  JSObject.eval.  The type and value of the object returned by getMember
     *  and eval should be the same.  Add the testcase.
     *
     *  @param rightExpr right-hand side to a JavaScript assignment expression
     *  @param className string representation of expected type of the result
     *  @param eValue expected value of the result
     */
    public void getJSVarWithEval( String rightExpr, String className,
        String value ) {

        String varName = "jsVar";
        Object jsObject;
        Object result;
        Class expectedClass = null;

        try {
            expectedClass = Class.forName( className );
            
            // Create the variable in the JavaScript context.
            global.eval( "var " + varName +" = " + rightExpr +";" );

            // get the value of varName from the JavaScript context.
            jsObject = global.eval( varName );
        
        } catch ( Exception e ) {
            System.err.println( "setJSVarWithEval threw " + e.toString() +
                " with arguments ( " + rightExpr +", "+ 
                expectedClass.getName() +", "+ value.toString() );
            e.printStackTrace();                
            exception = e.toString();
            jsObject = new Object();
        }
        
        // compare the class of the jsObject to the expected class
        
        addTestCase( 
            "global.eval( \"var "+ varName +" = " + rightExpr +"); "+
            "jsObject = global.eval( "+ varName +"); " +
            "jsObject.getClass.getName().equals("+expectedClass.getName()+")"+
            "[ jsObject class is " + jsObject.getClass().getName() +"]",
            "true",
            jsObject.getClass().getName().equals(expectedClass.getName())+ "",
            exception );
        
        //  compare the value of the string value of the jsObject to the 
        //  expected string value
        
        addTestCase(
            "("+jsObject.toString() +".equals(" + value.toString() +"))",
            "true",
            jsObject.toString().equals( value ) +"",
            exception );
    }

    /**
     *  Use JSObject.eval to create a JavaScript variable of a JavaScript type.
     *  Get the value of the variable using JSObject.getMember.  Add the test
     *  cases.
     *
     *  @param rightExpr right-hand side to a JavaScript assignment expression
     *  @param className string representation of expected type of the result
     *  @param eValue expected value of the result
     */
    public void getJSVarWithGetMember( String rightExpr, String className,
        String value ) {

        String varName = "jsVar";
        Object jsObject;
        Class expectedClass = null;

        try {
            expectedClass = Class.forName( className );
            
            // Create the variable in the JavaScript context.
            global.eval( "var " + varName +" = " + rightExpr +";" );

            // get the value of varName from the JavaScript context.
            jsObject = global.getMember( varName );
            
        } catch ( Exception e ) {
            System.err.println( "getJSVarWithGetMember threw " + e.toString() +
                " with arguments ( " + rightExpr +", "+ 
                expectedClass.getName() +", "+ value.toString() );
            e.printStackTrace();                
            exception = e.toString();
            jsObject = new Object();
        }
        
        // check the class of the jsObject object
        
        addTestCase( 
            "global.eval( \"var "+ varName +" = " + rightExpr +"); "+
                "jsObject = global.getMember( "+ varName +"); " +
                "jsObject.getClass.getName().equals(" + 
                expectedClass.getName() +")"+
                "[ jsObject class is "+jsObject.getClass().getName()+"]",
            "true",
            jsObject.getClass().getName().equals(expectedClass.getName()) +"",
            exception );

        // check the string representation of the jsObject

        addTestCase(
            "("+jsObject.toString() +".equals(" + value.toString() +"))",
            "true",
            jsObject.toString().equals( value ) +"",
            exception );
    }    
 }