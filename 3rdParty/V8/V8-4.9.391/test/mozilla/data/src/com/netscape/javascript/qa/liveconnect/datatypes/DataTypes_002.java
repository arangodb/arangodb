
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.*;

/**
 *  Get and set a public static string.  Same as cases in DataTypes_001, but
 *  pulling out string cases separately, since they need to be quoted right.
 *
 *  @see com.netscape.javascript.qa.liveconnect.DataTypesClass
 *  @see com.netscape.javascript.qa.datatypes.DataTypes_001
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_002 extends LiveConnectTest {
    public DataTypes_002() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_002 test = new DataTypes_002();
        test.start();
    }

    public void executeTest() {
        doTestOne();
        doTestTwo();
        doTestThree();
        doTestFour();
    }
    /**
     *  String literal gets passed JSObject.eval, and this test succeeds.
     */
     
    public void doTestOne() {
        Object before;
        Object newValue = (Object) (new String("Test One New Value!"));
        Object after;
        
        before = global.eval( "DT.PUB_STATIC_STRING" );
        global.eval( "DT.PUB_STATIC_STRING = \"Test One New Value!\"" );
        after = global.eval( "DT.PUB_STATIC_STRING" );
        
        addTestCase( "global.eval(\"DT_PUBLIC_STRING = \"Test One New Value!\"); "+
                "after = global.eval( \"DT.PUB_STATIC_STRING\" );" +
                "(\""+after +"\").toString().equals(\""+newValue.toString()+"\")",
                "true",
                after.toString().equals(newValue.toString()) + "",
                null);
    }
    
    /**
     *  New string value is Object.toString().  This succeeds with the extra quoting.
     *
     */
    
    public void doTestTwo() {
        Object before;
        Object newValue = (Object) (new String("Test Two New Value!"));
        Object after;
        
        before = global.eval( "DT.PUB_STATIC_STRING" );
        global.eval( "DT.PUB_STATIC_STRING = \"" + newValue.toString()+"\"" );
        after = global.eval( "DT.PUB_STATIC_STRING" );
                
        addTestCase( "Object newValue = (Object) new String(\"Test Two New Value!\"); " +
                "global.eval(\"DT_PUBLIC_STRING = newValue.toString()); "+
                "after = global.eval( \"DT.PUB_STATIC_STRING\" );" +
                "(\""+after +"\").toString().equals(\""+newValue.toString()+"\")",
                "true",
                after.toString().equals(newValue.toString()) + "",
                null);
    }        
    
    /**
     *  More extra quoting stuff.  This succeeds.
     */

    public void doTestThree() {
        Object before;
        Object newValue = (Object) (new String("Test Three New Value!"));
        Object after;
        
        String evalArgs = "DT.PUB_STATIC_STRING = \'" + newValue.toString() +"\'";
        
        before = global.eval( "DT.PUB_STATIC_STRING" );
        global.eval( evalArgs.toString() );
        after = global.eval( "DT.PUB_STATIC_STRING" );
        
        addTestCase( "String evalArgs = "+ evalArgs.toString() +"; " +
                "global.eval( evalArgs.toString() )"+
                "after = global.eval( \"DT.PUB_STATIC_STRING\" );" +
                "(\""+after +"\").toString().equals(\""+newValue.toString()+"\")",
                "true",
                after.toString().equals(newValue.toString()) + "",
                null);
    }        
    
    /**
     *  This throws an exception.  This does not use the extra quoting.
     */
    public void doTestFour() {
        Object before;
        Object newValue = (Object) (new String("Test Four New Value!"));
        Object after;
        String exception;
        
        String evalArgs = "DT.PUB_STATIC_STRING = " + newValue.toString() +";";
        
        String EXCEPTION = "JSException thrown!";

        try {
            before = global.eval( "DT.PUB_STATIC_STRING" );
            global.eval( evalArgs.toString() );
            after = global.eval( "DT.PUB_STATIC_STRING" );
            exception = "";
        } catch ( Exception e ) {
            if ( e instanceof JSException ) {
                after = EXCEPTION;
            } else {
                after = "Some random exception thrown!";
            }
            file.exception = e.toString();
        }
        
        addTestCase( "String evalArgs = "+ evalArgs.toString() +"; " +
                "global.eval( evalArgs.toString() )"+
                "after = global.eval( \"DT.PUB_STATIC_STRING\" );" +
                "(\""+after +"\").toString().equals(\""+newValue.toString()+"\")",
                EXCEPTION,
                after.toString(),
                file.exception );
    }                

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass");
    }
 }