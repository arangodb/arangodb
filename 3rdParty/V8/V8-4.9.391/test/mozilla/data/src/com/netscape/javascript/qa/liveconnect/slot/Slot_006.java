package com.netscape.javascript.qa.liveconnect.slot;

import netscape.javascript.*;
import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Create a JS String.  Get the values of characters in that string using
 *  getSlot.
 *
 *  This covers the following JavaScript -> Java cases through getSlot:
 *  <table border = 1>
 *  <tr>
 *  <th>    Java type
 *  <th>    JavaScript type
 *  </tr>
 *  <tr>
 *  <td>    string
 *  <td>    java.lang.String
 *  </tr>
 *  </table>
 *
 *  @see netscape.javascript.JSObject#getSlot
 *  @see netscape.javascript.JSObject#setSlot
 *
 *  @author christine m begle
 */
public class Slot_006 extends LiveConnectTest {
    public Slot_006() {
        super();
    }
    public static void main( String[] args ) {
        Slot_006 test = new Slot_006();
        test.start();
    }

    public void executeTest() {
        Object testMatrix[] = getDataArray();

        for ( int i = 0; i < testMatrix.length; i++ ) {
                JSObject jsObject = getJSString( (Object[]) testMatrix[i] );
                
                for ( int j = 0; 
                    j < ((String) ((Object[]) testMatrix[i])[1]).length(); 
                    j++ ) 
                {
                    getSlot( jsObject, j, (Object[]) testMatrix[i] );
                }                    
        }
    }

    public JSObject getJSString ( Object[] data ) {
        return (JSObject) data[0];
    }

   /**
    *   Data in this test contains of :
    *   JavaScript String object
    *   Java object with the same string value
    *
    *   @return the data array.
    */
    public Object[] getDataArray() {
        Object item0[] = {
            global.eval("new String(\"passed!\")"),
            new String("passed!")
         };

        Object dataArray[] = { item0 };
        return dataArray;
    }

    /**
     *  Use JSObject.getSlot to get an indexed member of a JSObject.  In this
     *  test, the expected class of all objects is JSObject.
     *
     *  XXX propbably  need to add a bunch of unicode characters here.
     *
     *  @param jsSTring the JSObject with indexed members
     *  @param slot the index property to retrieve
     *  @param data object array containing the string representation of the
     *  expected result of jsArray.getSlot(slot) and the JSObjectconstructor
     *  of the expected result, which allows us to verify the value and type
     *  of the result object.
     */
    public void getSlot( JSObject jsString, int slot, Object[] data ) {
        String exception = "";
        JSObject constructor = null;
        Class  eClass = null;
        Class  aClass = null;
        String result = null;        
        String eResult = null;

        try {
            eResult = new Character( ((String)data[1]).charAt(slot) ).toString();
            result = (String) jsString.getSlot( slot );
            if ( result != null ) {
                eClass = Class.forName( "java.lang.String" );
                aClass = result.getClass();
            }
        }  catch ( Exception e ) {
            exception = "jsString.getSlot( " + slot + " ) "+
                "threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            if ( result == null ) {
                addTestCase(
                    "[jsString.getSlot(" + slot +").toString(). returned " + result +"] "+
                    "( " + eResult +".equals( " + result +" )",
                    "true",
                    eResult.equals( result +"") +"",
                    exception );

            } else {
                // check the string value of the result
                addTestCase(
                    "[jsString.getSlot(" + slot +") returned " + result +"] "+
                    eResult +".toString().equals( " + result +" ) ",
                    "true",
                    eResult.equals( result.toString() ) +"",
                    exception );

                // check the class of the result.  all should be java.lang.Strings.
                addTestCase(
                    "( " + eClass +".equals( " + aClass +" ) )",
                    "true",
                    eClass.equals( aClass ) +"",
                    exception );
            }                    
        }
    }
 }
