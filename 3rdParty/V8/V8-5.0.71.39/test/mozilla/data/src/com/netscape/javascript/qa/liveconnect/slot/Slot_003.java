package com.netscape.javascript.qa.liveconnect.slot;

import netscape.javascript.*;
import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Create a JS Array.  Set the members of the array using JSObject.setSlot.  
 *  Get the members of the array again using getSlot.
 *
 *  This covers the following JavaScript -> Java cases through getSlot:
 *  <table border = 1>
 *  <tr>
 *  <th>    Java type
 *  <th>    JavaScript type
 *  </tr>
 *  <tr>
 *  <td>    JSObject
 *  <td>    the JavaScript object it refers to 
 *  </tr>
 *  </table>
 *
 *  @see netscape.javascript.JSObject#getSlot
 *  @see netscape.javascript.JSObject#setSlot
 *
 *  @author christine m begle
 */
public class Slot_003 extends LiveConnectTest {
    public Slot_003() {
        super();
    }
    public static void main( String[] args ) {
        Slot_003 test = new Slot_003();
        test.start();
    }

    public void executeTest() {
        getBaseObjects();
        Object testMatrix[] = getDataArray();

        JSObject jsArray = createJSArray();
        getLength( jsArray, 0 );

        for ( int i = 0; i < testMatrix.length; i++ ) {
                setSlot( jsArray, i, (Object[]) testMatrix[i] );
                getSlot( jsArray, i, (Object[]) testMatrix[i] );
        }
        getLength( jsArray, testMatrix.length );        
    }

    public JSObject jsNumber;
    public JSObject jsFunction;
    public JSObject jsBoolean;
    public JSObject jsObject;
    public JSObject jsString;
    public JSObject jsMath;
    public JSObject jsDate;
    public JSObject jsArray;
    public JSObject jsRegExp;

    /**
     *  Get the constructor of all the base JavaScript objects.  The test
     *  will compare the instance constructor to the base object constructor
     *  to verify that the JavaScript type of the object is corret.
     *
     */
    public boolean getBaseObjects() {
        try {
            jsNumber   = (JSObject) global.eval( "Number.prototype.constructor" );
            jsString   = (JSObject) global.eval( "String.prototype.constructor" );
            jsFunction = (JSObject) global.eval( "Function.prototype.constructor" );
            jsBoolean  = (JSObject) global.eval( "Boolean.prototype.constructor" );
            jsObject   = (JSObject) global.eval( "Object.prototype.constructor" );
            jsMath     = (JSObject) global.eval("Math");
            jsDate     = (JSObject) global.eval( "Date.prototype.constructor" );
            jsArray    = (JSObject) global.eval( "Array.prototype.constructor" );
            jsRegExp   = (JSObject) global.eval("RegExp.prototype.constructor");

        } catch ( Exception e ) {
            System.err.println( "Failed in getBaseObjects:  " + e.toString() );
            e.printStackTrace();
            return false;
        }

        return true;
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
            global.eval("new String(\"JavaScript String\")"),
            new String ("JavaScript String"),
            jsString };

        Object item1[] = {
            global.eval("new Number(12345)"),
                new String( "12345" ),
                jsNumber };

        Object item2[] = {
            global.eval("new Boolean(false)"),
            new String( "false" ),
            jsBoolean };

        Object item3[] = {
            global.eval("new Array(0,1,2,3,4)"),
            new String( "0,1,2,3,4" ),
            jsArray };

        Object item4[] = {
            global.eval("new Object()"),
            new String( "[object Object]" ),
            jsObject };

        Object dataArray[] = { item0, item1, item2, item3, item4 };
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
        JSObject constructor = null;
        JSObject result = null;
        Class  eClass = null;
        Class  aClass = null;

        try {
            result = (JSObject) jsArray.getSlot( slot );
            if ( result != null ) {
                eClass = Class.forName( "netscape.javascript.JSObject" );
                aClass = result.getClass();
                constructor = (JSObject) result.getMember( "constructor" );
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

                // check the constructor of the result
                addTestCase(
                    "( " + constructor +".equals( " + data[2] +" ) )",
                    "true",
                    constructor.equals( data[2] ) +"",
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
