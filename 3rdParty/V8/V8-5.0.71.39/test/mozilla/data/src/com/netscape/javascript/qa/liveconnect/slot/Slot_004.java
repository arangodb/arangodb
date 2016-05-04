package com.netscape.javascript.qa.liveconnect.slot;

import netscape.javascript.*;
import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Create a JavaScript array.  Set an indexed member of the array.  Get an 
 *  indexed member of the array.
 *
 *  This covers the following JavaScript -> Java cases through getSlot:
 *  <table border = 1>
 *  <tr>
 *  <th>    Java type
 *  <th>    JavaScript type
 *  </tr>
 *  <tr>
 *  <td>    Java Object
 *  <td>    JavaScript JavaObject
 *  </tr>
 *  </table>
 *
 *  @see netscape.javascript.JSObject#getSlot
 *  @see netscape.javascript.JSObject#setSlot
 *
 *  @author christine m begle
 */
public class Slot_004 extends LiveConnectTest {
    public Slot_004() {
        super();
    }
    public static void main( String[] args ) {
        Slot_004 test = new Slot_004();
        test.start();
    }

    public void executeTest() {
        Object testMatrix[] = getDataArray();

        JSObject jsArray = createJSArray();
        getLength( jsArray, 0 );

        for ( int i = 0; i < testMatrix.length; i++ ) {
                setSlot( jsArray, i, (Object[]) testMatrix[i] );
                getSlot( jsArray, i, (Object[]) testMatrix[i] );
        }
        getLength( jsArray, testMatrix.length );
    }

   /**
    *   Get the data array, which is an array of arrays.  Each of the internal
    *   arrays consists of three objects:  a string whose value will be passed
    *   to the JavaScript array constructor; a String which is the expected
    *   string value of retreiving the JSObject via getSlot; and a JSObject,
    *   which is the JSObject's constructor, that allows us to verify the type
    *   of JSObject.
    *
    *   @return the data array.
    */
    public Object[] getDataArray() {
        Object item0[] = {
            new String("Java String"),
            new String ("Java String"),
            "java.lang.String" };

        Object item1[] = {
            new Integer(12345),
            new String( "12345" ),
            "java.lang.Integer" };

        Object item2[] = {
            new Boolean(false),
            new String( "false" ),
            "java.lang.Boolean" };

        Object item3[] = {
            new Double(12345.0),
            new String( "12345.0" ),
            "java.lang.Double" };

        Object dataArray[] = { item0, item1, item2, item3 };
        return dataArray;
    }

    /**
     *  Create an empty JavaScript Array named jsArray.
     *
     *  @return the JSObject array object
     */
    public JSObject createJSArray() {
        JSObject jsArray = null;

        String args = "var jsArray =  new Array()";
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
                "new Array(); " +
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
     *  Use JSObject.getSlot to get an indexed member of a JSObject.  In this
     *  test, the expected class of all objects is JSObject.
     *
     *  @param jsArray the JSObject with indexed members
     *  @param slot the index property to retrieve
     *  @param data object array containing the string representation of the
     *  expected result of jsArray.getSlot(slot) and the JSObjectconstructor
     *  of the expected result, which allows us to verify the value and type
     *  of the result object.
     */
    public void getSlot( JSObject jsArray, int slot, Object[] data ) {
        String exception = "";
        Object result = null;
        Class  eClass = null;
        Class  aClass = null;

        try {
            result = (Object) jsArray.getSlot( slot );
            if ( result != null ) {
                eClass = Class.forName((String) data[2]);
                aClass = result.getClass();
            }
        }  catch ( Exception e ) {
            exception = "jsArray.getSlot( " + slot + " ) "+
                "threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            if ( result == null ) {
                addTestCase(
                    "[jsArray.getSlot(" + slot +").toString().trim() returned " + result +"] "+
                    "( " + data[1] +".equals( " + result +" )",
                    "true",
                    data[1].toString().trim().equals( result.toString() ) +"",
                    exception );

            } else {
                // check the string value of the result
                addTestCase(
                    "[jsArray.getSlot(" + slot +") returned " + result +"] "+
                    data[1] +".toString().equals( " + result +" ) ",
                    "true",
                    data[1].equals( result.toString() ) +"",
                    exception );

                // check the class of the result.  all should be JSObjects.
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
    public void setSlot( JSObject jsArray, int slot, Object[] data ) {
        String exception = null;
        String result = "passed!";

        try {
            jsArray.setSlot( slot, (Object) data[0] );
        }  catch ( Exception e ) {
            result = "failed!";
            exception = "jsArray.setSlot( " + slot +","+ data[0] +") "+
                "threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            addTestCase(
                "jsArray.setSlot( " + slot +", "+ data[0] +")",
                "passed!",
                result,
                exception );
        }
    }
 }
