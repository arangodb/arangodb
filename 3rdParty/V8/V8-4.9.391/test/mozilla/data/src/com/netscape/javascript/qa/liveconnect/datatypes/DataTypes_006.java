
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Attempt to get java values via instance methods.
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

public class DataTypes_006 extends LiveConnectTest {
    public DataTypes_006() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_006 test = new DataTypes_006();
        test.start();
    }

    public void executeTest() {
        doMethodTests(
            "dt.getBooleanObject()",
            "java.lang.Boolean",
            (Object) new Boolean(DataTypeClass.PUB_STATIC_FINAL_BOOLEAN) );
            
        doMethodTests(
            "dt.getBoolean()",
            "java.lang.Boolean",
            (Object) new Boolean(DataTypeClass.PUB_STATIC_FINAL_BOOLEAN) );

        doMethodTests(
            "dt.getByte()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_BYTE) );

        doMethodTests(
            "dt.getByteObject()",
            "java.lang.Byte",
            (Object) new Byte(DataTypeClass.PUB_STATIC_FINAL_BYTE) );

        doMethodTests(
            "dt.getShort()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_SHORT) );

        doMethodTests(
            "dt.getShortObject()",
            "java.lang.Short",
            (Object) new Short(DataTypeClass.PUB_STATIC_FINAL_SHORT) );

        doMethodTests(
            "dt.getInteger()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_INT) );

        doMethodTests(
            "dt.getIntegerObject()",
            "java.lang.Integer",
            (Object) new Integer(DataTypeClass.PUB_STATIC_FINAL_INT) );

        doMethodTests(
            "dt.getLong()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_LONG) );

        doMethodTests(
            "dt.getLongObject()",
            "java.lang.Long",
            (Object) new Long(DataTypeClass.PUB_STATIC_FINAL_LONG) );

        doMethodTests(
            "dt.getFloat()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_FLOAT) );

        doMethodTests(
            "dt.getFloatObject()",
            "java.lang.Float",
            (Object) new Float(DataTypeClass.PUB_STATIC_FINAL_FLOAT) );

        doMethodTests(
            "dt.getDouble()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_DOUBLE) );

        doMethodTests(
            "dt.getDoubleObject()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_DOUBLE) );

        doMethodTests(
            "dt.getChar()",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_CHAR) );

        doMethodTests(
            "dt.getCharacter()",
            "java.lang.Character",
            (Object) new Character(DataTypeClass.PUB_STATIC_FINAL_CHAR) );

        doMethodTests(
            "dt.getStringObject()",
            "java.lang.String",
            (Object) new String(DataTypeClass.PUB_STATIC_FINAL_STRING) );
    }

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass"  );
        global.eval( "var dt = new DT();" );
    }

    public void doMethodTests( String method, String className, Object value ) {
        getPublicMethod( method, className, value );
    }

    /**
     *  Set the value of a JavaScript variable to a Java value.
     *  Get the value of that variable.
     */

    public void getPublicMethod( String method, String className, Object value ) {
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
/*
    public void setPublicField( String field, String className, Object value ) {
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

    public void setPublicField ( String field, String className, Object value ) {
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
/*
    public void setJavaScriptVariable( String field, String className, Object value ) {
        String description = field;
        String exception = null;
        Object actual = null;
        String expect = null;

        Object newValue =   className.equals("java.lang.Double")
                            ? new Double(0)
                            : className.equals("java.lang.Boolean")
                              ? new Boolean(false)
                              : (Object) new String("New Value!")
                            ;
        try {
            global.eval( "var myobject = " + description );
            global.setMember( "myobject", newValue );
            actual = global.getMember( "myobject" );
            expect = Class.forName( className ).getName();
        } catch ( Exception e ) {
            exception = e.toString();
        }

        addTestCase( "global.eval(\"var myobject = " + description +"\" ); " +
            "global.setMember( \"myobject\", " + newValue.toString() +");" +
            "actual = global.getMember( \"myobject\" ); "+
            "actual.getClass().getName()",
            expect,
            actual.getClass().getName(),
            exception );

        addTestCase( "( " + description + " == " + newValue.toString() +" )",
            "true",
            actual.equals( newValue ) + "",
            exception );

        addTestCase( "\"" +actual.toString() + "\".equals(\"" + newValue.toString() +"\")",
            "true",
            actual.toString().equals(newValue.toString()) +"",
            exception );
    }
    */
 }