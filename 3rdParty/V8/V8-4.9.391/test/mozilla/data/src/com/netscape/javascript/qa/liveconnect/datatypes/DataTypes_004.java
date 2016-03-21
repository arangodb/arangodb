
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Get the value of JavaScript variable that is set to null.  
 *  It would be nice if nulls were wrapped by JSObjects, but instead they
 *  are Java nulls. 
 *
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_004 extends LiveConnectTest {
    public DataTypes_004() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_004 test = new DataTypes_004();
        test.start();
    }

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = "+
            "Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass"
        );
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
        getJSVarWithEval( "null", "netscape.javascript.JSObject", "null" );
        getJSVarWithGetMember( "null", "netscape.javascript.JSObject", "null" );
    }

    /**
     *  Use JSObject.eval to create a JavaScript variable of a JavaScript type.
     *  Get the value of the variable using JSObject.getMember and
     *  JSObject.eval.  The type and value of the object returned by getMember and
     *  eval should be the same.  Add the testcase.
     *
     *  @param rightExpr right-hand side to the JavaScript assignment
     *  expression
     *  @param className string representation of expected type of the result
     *  @param eValue expected value of the result
     */

    public void getJSVarWithEval( String rightExpr, String className,
        String value ) {

        String varName = "jsVar";
        Object result;
        Class expectedClass = null;

        try {
            expectedClass = Class.forName( className );
            // Create the variable in the JavaScript context.
            global.eval( "var " + varName +" = " + rightExpr +";" );

            // get the value of varName from the JavaScript context.
            result = global.eval( varName );
            
            System.out.println( "result is " + result );

        } catch ( Exception e ) {
            System.err.println( "setJSVarWithEval threw " + e.toString() +
                " with arguments ( " + rightExpr +", "+
                expectedClass.getName() +", "+ value.toString() );

            e.printStackTrace();

            exception = e.toString();
            result = new Object();
        }

        try {
            addTestCase( 
                "global.eval( \"var "+ varName +" = " + rightExpr +"); "+
                "result = global.eval( "+ varName +"); " +
                "(result == null)",
                "true",
                (result == null) + "",
                exception );

        } catch ( Exception e ) {
            file.exception = e.toString();
        }

    }
    public void getJSVarWithGetMember( String rightExpr, String className,
        String value ) {

        String varName = "jsVar";
        Object result;
        Class expectedClass = null;

        try {
            expectedClass = Class.forName( className );
            // Create the variable in the JavaScript context.
            global.eval( "var " + varName +" = " + rightExpr +";" );

            // get the value of varName from the JavaScript context.
            result = global.getMember( varName );
        } catch ( Exception e ) {
            System.err.println( "setJSVarWithGetMember threw " + e.toString() +
                " with arguments ( " + rightExpr +", "+
                expectedClass.getName() +", "+ value.toString() );
            e.printStackTrace();
            exception = e.toString();
            result = new Object();
        }

        try {
            addTestCase( 
                "global.eval( \"var "+ varName +" = " + rightExpr +"); "+
                "result = global.getMember( "+ varName +"); " +
                "(result == null)",
                "true",
                (result == null) + "",
                exception );

        } catch ( Exception e ) {
            file.exception = e.toString();
        }
    }        
 }