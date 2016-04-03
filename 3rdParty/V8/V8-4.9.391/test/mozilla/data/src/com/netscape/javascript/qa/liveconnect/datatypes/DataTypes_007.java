
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Attempt to get java values via static methods.
 *  <p>
 *  Assign a Java value to a JavaScript variable.  Set the value of that
 *  variable with JSObject.setMember, and get the new value with
 *  JSObject.getMember.
 *
 *  @see com.netscape.javascript.qa.liveconnect.DataTypesClass
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_007 extends LiveConnectTest {
    public DataTypes_007() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_007 test = new DataTypes_007();
        test.start();
    }

    public void executeTest() {
        doStaticMethodTests(
            "DT.staticGetBooleanObject()",
            "java.lang.Boolean",
            (Object) new Boolean(DataTypeClass.PUB_STATIC_FINAL_BOOLEAN) );

        doStaticMethodTests(
            "DT.staticGetBoolean()",
            "java.lang.Boolean",
            (Object) new Boolean(DataTypeClass.PUB_STATIC_FINAL_BOOLEAN) );

        doStaticMethodTests(
            "DT.staticGetByte()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_BYTE) );
            
        doStaticMethodTests(
            "DT.staticGetByteObject()",
            "java.lang.Byte",
            (Object) new Byte(DataTypeClass.PUB_STATIC_FINAL_BYTE) );
            
        doStaticMethodTests(
            "DT.staticGetChar()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_CHAR) );

        doStaticMethodTests(
            "DT.staticGetCharacter()",
            "java.lang.Character",
            (Object) new Character(DataTypeClass.PUB_STATIC_FINAL_CHAR) );
            
        doStaticMethodTests(
            "DT.staticGetShort()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_SHORT) );

        doStaticMethodTests(
            "DT.staticGetShortObject()",
            "java.lang.Short",
            (Object) new Short(DataTypeClass.PUB_STATIC_FINAL_SHORT) );

        doStaticMethodTests(
            "DT.staticGetInteger()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_INT) );

        doStaticMethodTests(
            "DT.staticGetIntegerObject()",
            "java.lang.Integer",
            (Object) new Integer(DataTypeClass.PUB_STATIC_FINAL_INT) );

        doStaticMethodTests(
            "DT.staticGetInteger()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_INT) );

        doStaticMethodTests(
            "DT.staticGetLong()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_LONG) );

        doStaticMethodTests(
            "DT.staticGetLongObject()",
            "java.lang.Long",
            (Object) new Long(DataTypeClass.PUB_STATIC_FINAL_LONG) );

        doStaticMethodTests(
            "DT.staticGetFloat()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_FLOAT) );

        doStaticMethodTests(
            "DT.staticGetFloatObject()",
            "java.lang.Float",
            (Object) new Float(DataTypeClass.PUB_STATIC_FINAL_FLOAT) );

        doStaticMethodTests(
            "DT.staticGetDouble()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_DOUBLE) );

        doStaticMethodTests(
            "DT.staticGetDoubleObject()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_DOUBLE) );

        doStaticMethodTests(
            "DT.staticGetChar()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_CHAR) );

        doStaticMethodTests(
            "DT.staticGetStringObject()",
            "java.lang.String",
            (Object) new String(DataTypeClass.PUB_STATIC_FINAL_STRING) );
    }
    

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass"  );
    }

    public void doStaticMethodTests( String method, String className, Object value ) {
        getPublicStaticMethod( method, className, value );
        getJavaScriptValue( method, className, value );
    }

    /**
     *  Set the value of a JavaScript variable to a Java value.
     *  Get the value of that variable.
     */

    public void getPublicStaticMethod( String method, String className, Object value ) {
        String description = method;
        String exception = null;
        Object actual = null;
        String expect = null;

        // check the class
        try {
            actual = global.eval( method );
            expect  = Class.forName(className).getName();
        } catch ( ClassNotFoundException e ) {
        } catch ( Exception e ) {
            exception = e.toString();
        }

        // might want to do all the interesting stuff here in a try/catch block.

        addTestCase( "( " + description +" ).getClass()",
            expect,
            actual.getClass().getName(),
            exception );

        addTestCase( "( " +description + " == " + value.toString() +" )",
            "true",
            actual.equals( value ) + "",
            exception );

        addTestCase( "\"" +actual.toString() + "\".equals(\"" + value.toString() +"\")",
            "true",
            actual.toString().equals(value.toString()) +"",
            exception );
    }

    /**
     *  Try to set the value of a public static field using
     *  JSObject.setMember which should succeed.
     */

    public void setPublicStaticField( String field, String className, Object value ) {
        String description = field;
        String exception = null;
        Object before = null;
        Object after = null;
        String expect = null;

        Object newValue =   className.equals("java.lang.Double")
                            ? new Double(0)
                            : className.equals("java.lang.Boolean")
                              ? new Boolean(false)
                              : (Object) new String("New Value!")
                            ;
        try {
            before = global.eval( description );

            // need to quote strings
            if ( className.equals("java.lang.String") ){
                global.eval( description +" = \"" + newValue.toString() +"\"" );
            } else {
                global.eval( description +" = " + newValue );
            }

            after = global.eval( description );
            expect = Class.forName( className ).getName();
        } catch ( Exception e ) {
            exception = e.toString();
        }

        addTestCase( "global.eval(\""+description+" = "+newValue.toString()+
            "\"); after = global.eval( \"" + description +"\" );" +
            "after.getClass().getName()",
            expect,
            after.getClass().getName(),
            exception );

        addTestCase( "( "+after.toString() +" ).equals(" + newValue.toString() +")",
            "true",
            after.equals( newValue ) + "",
            exception );

        addTestCase( "\"" +after.toString() + "\".equals(\"" + newValue.toString() +"\")",
            "true",
            after.toString().equals(newValue.toString()) +"",
            exception );
    }

    /**
     *  Assign a value to a public static final java field.  The assignment
     *  should fail (the value should not change), but there should be no error
     *  message.
     */

    public void setPublicStaticFinalField ( String field, String className, Object value ) {
        String description = field;
        String exception = null;
        Object before = null;
        Object after = null;
        String expect = null;

        Object newValue =   className.equals("java.lang.Double")
                            ? new Double(0)
                            : className.equals("java.lang.Boolean")
                              ? new Boolean(false)
                              : (Object) new String("New Value!")
                            ;

        try {
            expect = Class.forName( className ).getName();
        } catch ( Exception e ) {
            exception = e.toString();
        }
            before = global.eval( description );
            global.eval( description +" = " + newValue.toString() );
            after = global.eval( description );

        // check the class of the result, which should be the same as expect

        addTestCase( "( " + description +" ).getClass()",
            expect,
            after.getClass().getName(),
            exception );

        // the value of the actual result should be the original value

        addTestCase( "( " +description + " == " + value.toString() +" )",
            "true",
            after.equals( value ) + "",
            exception );

        // the string representation of the actual result should be the same
        // as the string representation of the expected value

        addTestCase( "\"" +after.toString() + "\".equals(\"" + value.toString() +"\")",
            "true",
            after.toString().equals(value.toString()) +"",
            exception );

        // getMember( field ) should return the same value before and after the
        // assignment

        addTestCase( "( " + before +".equals(" + after +") ) ",
                    "true",
                    ( before.equals(after) ) +"",
                    exception );

    }

    public void getJavaScriptValue( String field, String className, Object value ) {
        String description = field;
        String exception = null;
        Object actual = null;
        String expect = null;

        try {
            global.eval( "var myobject = " + description );
            actual = global.getMember( "myobject" );
            expect = Class.forName( className ).getName();
        } catch ( Exception e ) {
            exception = e.toString();
        }

        addTestCase( "global.eval(\"var myobject = " + description +"\" ); " +
            "actual = global.getMember( \"myobject\" ); "+
            "actual.getClass().getName()",
            expect,
            actual.getClass().getName(),
            exception );

        addTestCase( "( " + actual + " == " + value.toString() +" )",
            "true",
            actual.equals( value ) + "",
            exception );

        addTestCase( "\"" +actual.toString() + "\".equals(\"" + value.toString() +"\")",
            "true",
            actual.toString().equals(value.toString()) +"",
            exception );
    }
 }