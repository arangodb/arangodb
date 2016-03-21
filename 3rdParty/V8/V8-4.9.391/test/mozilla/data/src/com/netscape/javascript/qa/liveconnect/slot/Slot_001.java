package com.netscape.javascript.qa.liveconnect.slot;

import netscape.javascript.*;
import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Create a JS Array.  Get the length of the array.  Get the members of the
 *  array using JSObject.getSlot.  Set the members of the array using
 *  JSObject.setSlot.  Get the members of the array again using getSlot.
 *
 *  This covers the following JavaScript -> Java cases through getSlot:
 *  <table border = 1>
 *  <tr>
 *  <th>    JavaScript type
 *  <th>    Java type
 *  </tr>
 *  <tr>
 *  <td>    number
 *  <td>    java.lang.Double
 *  </tr>
 *  <tr>   
 *  <td>    string
 *  <td>    java.lang.String
 *  </tr>
 *  <tr>    
 *  <td>    boolean
 *  <td>    java.lang.Boolean
 *  </tr>
 *  <tr>
 *  <td>    null
 *  <td>    null
 *  </tr>
 *  <tr>
 *  <td>    JavaObject
 *  <td>    the unwrapped original java object (use ==, not .equals() )
 *  </tr>
 *  <tr>
 *  <th colspan=2>  Not covered in this test
 *  </tr>
 *  <tr>
 *  <td>    JavaScript object
 *  <td>    netscape.javascript.JSObject
 *  </tr>
 *  </table> 
 *
 *  @see netscape.javascript.JSObject#getSlot
 *  @see netscape.javascript.JSObject#setSlot
 *
 *  @author christine m begle
 */
public class Slot_001 extends LiveConnectTest {
    public Slot_001() {
        super();
    }
    public static void main( String[] args ) {
        Slot_001 test = new Slot_001();
        test.start();
    }

    //  The structure of the arrays is the array expression followed by
    //  a list of java objects, correctly typed for what we expect to get
    //  back from getSlot.  testMatrix is an array of arrays.
    
    Object test1[] = { new String("null"), null };
    Object test2[] = { new String( "" ) };
    Object test3[] = { new String("true,\"hi\",3.14159"), 
        new Boolean(true), new String("hi"), new Double("3.14159") };
        
    Object test4[] = { new String( "new java.lang.Boolean(true), " +
        "new java.lang.String(\"hello\"), java.lang.System.out, "+
        "new java.lang.Integer(5)"),
        new Boolean(true),
        new String("hello"),
        System.out,
        new Integer(5) };
    
    Object testMatrix[] =  { test1, test2, test3, test4 };
    
    public void executeTest() {
        for ( int i = 0; i < testMatrix.length; i++ ) {
            String args = (String) ((Object[]) testMatrix[i])[0];
            
            JSObject jsArray = createJSArray( args );
            
            // get the length of the array using JSObject.getMember
            int javaLength = ((Object[]) testMatrix[i]).length -1;
            
            getLength( jsArray, javaLength );

            for ( int j = 0; j < javaLength ; j++ ) {
                getSlot( jsArray, j, ((Object[]) testMatrix[i])[j+1] );
            }                
        }            
    }
    
    /**
     *  Create a JavaScript Array named jsArray.  
     *
     *  @param newArrayArguments string containing a list of arguments for the
     *  JavaScript Array constructor
     *
     *  @return the JSObject array object
     */
    public JSObject createJSArray( String newArrayArguments ) {
        JSObject jsArray = null;
        String args = "var jsArray =  new Array( " + newArrayArguments +" )";
        String result = "passed!";
        try {
            System.out.println( args );
            global.eval( args );
            jsArray = (JSObject) global.getMember("jsArray");
            
        } catch ( Exception e ) {
            result = "failed!";
            exception = "global.getMember(\"jsArray\")"+
                "threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {            
            addTestCase( 
                "global.eval(\"var jsArray = "+
                "new Array(\""+newArrayArguments+"\"); " +
                "JSObject jsArray = (JSObject) global.getMember(\"jsArray\")",
                "passed!",
                result,
                exception );
        }                
        return jsArray;
    }
    
    /**
     *  Use JSObject.getMember to get the length property of a JSObject.
     *
     *  @param jsArray a JSObject with a property named "length"
     *  @param javaLength the expected length of the JSObject
     */
    public void getLength( JSObject jsArray, int javaLength ) {
        String exception = "";
        int jsLength = 0;
        
        try {
            jsLength = ((Double) jsArray.getMember("length")).intValue();                    
        }  catch ( Exception e ) {
            exception = "jsArray.getMember(\"length\") threw "+
                e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            addTestCase(
                "[length is " + jsLength +"] "+
                "( jsArray.getMember(\"length\")).intValue() == " + 
                javaLength +" )",
                "true",
                (jsLength == javaLength) + "",
                exception );
        }        
    }        
    
    /**
     *  Use JSObject.getSlot to get an indexed member of a JSObject.
     *
     *  @param jsArray the JSObject with indexed members
     *  @param slot the index property to retrieve
     *  @param javaValue a java object whose type and value are what we
     *  expect jsArray.getSlot(slot) to return.
     */
    public void getSlot( JSObject jsArray, int slot, Object javaValue ) {
        String exception = "";
        Object result = null;
        Class  eClass = null;
        Class  aClass = null;

        try {
            result = jsArray.getSlot( slot );
            if ( javaValue != null ) {
                eClass = javaValue.getClass();
                aClass = result.getClass();
            }                
        }  catch ( Exception e ) {
            exception = "jsArray.getSlot( " + slot + " ) "+
                "threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            if ( javaValue == null ) {
                addTestCase(
                    "[jsArray.getSlot(" + slot +") returned " + result +"] "+
                    "( " +javaValue +" == " + result +" )",
                    "true",
                    (javaValue == result ) +"",
                    exception );
                
            } else {                
                addTestCase(
                    "[jsArray.getSlot(" + slot +") returned " + result +"] "+
                    javaValue +".equals( " + result +" ) ",
                    "true",
                    javaValue.equals( result ) +"",
                    exception );
                    
                addTestCase( 
                    "( " + eClass +".equals( " + aClass +" ) )",
                    "true",
                    eClass.equals( aClass ) +"",
                    exception );
            }                    
        }
    }
    
    /**
     *  JSObject.setSlot returns undefined.  This just verifies that we can
     *  call setSlot without throwing an exception.
     *
     *  @param jsArray the JSObject on which setSlot will be called
     *  @param slot the indexed member that will be set
     *  @param javaValue the value to set the indexed member
     */
    public void setSlot( JSObject jsArray, int slot, Object javaValue ) {
        String exception = null;
        String result = "passed!";

        try {
            jsArray.setSlot( slot, javaValue );
        }  catch ( Exception e ) {
            result = "failed!";
            exception = "jsArray.setSlot( " + slot +","+ javaValue +") "+
                "threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            addTestCase(
                "jsArray.setSlot( " + slot +", "+ javaValue +")",
                "passed!",
                result,
                exception );
        }
    }
 }

