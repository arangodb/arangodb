
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Attempt to get public instance fields using JSObject.eval.
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

public class DataTypes_005 extends LiveConnectTest {
    public DataTypes_005() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_005 test = new DataTypes_005();
        test.start();
    }

    public void executeTest() {
        doFieldTests( 
            "dt.PUB_BOOLEAN",
            "java.lang.Boolean",
            (Object) new Boolean(DataTypeClass.PUB_STATIC_FINAL_BOOLEAN) );

        doFieldTests( 
            "dt.PUB_BYTE",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_BYTE) );
            
        doFieldTests( 
            "dt.PUB_SHORT",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_SHORT) );

        doFieldTests( 
            "dt.PUB_INT",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_INT) );
            
        doFieldTests( 
            "dt.PUB_LONG",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_LONG) );
            
        doFieldTests( 
            "dt.PUB_FLOAT",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_FLOAT) );
            
        doFieldTests( 
            "dt.PUB_DOUBLE",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_DOUBLE) );
            
        doFieldTests(
            "dt.PUB_CHAR",
            "java.lang.Double",
            (Object) new Double(DataTypeClass.PUB_STATIC_FINAL_CHAR) );

        doFieldTests( 
            "dt.PUB_STRING",
            "java.lang.String",
            (Object) new String(DataTypeClass.PUB_STATIC_FINAL_STRING) );

    }
    
    /**
     *  Call the parent class's setupTestEnvironment method.  Use JSObject.eval
     *  to define the com.netscape.javascript.qa.liveconnect.DataTypeClass. Use
     *  JSObject.eval to define an instance of the DataTypeClass.
     *
     *  @see com.netscape.javascript.qa.liveconnect.DataTypeClass
     */
    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = "+
            "Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass");
        global.eval( "var dt = new DT();" );
    }
    
    /** 
     *  Pass the arguments to the different tests, all of which take the same
     *  arguments.  
     *  
     *  @param field name of the DataTypeClass field
     *  @param className name of the class of the DataTypeClass field
     *  @param value value of the DataTypeClass field
     *
     *  @see com.netscape.javascript.qa.liveconnect.DataTypeClass
     */
    public void doFieldTests( String field, String className, Object value ) {
        getPublicField( field, className, value );
        
        if ( field.startsWith( "dt.PUB_FINAL" )) {
            setPublicFinalField( field, className, value );                
        } else {
            setPublicField( field, className, value );
        }
        
        setJavaScriptVariable( field, className, value );
    }        
    
    /**
     *  Use JSObject.eval to create a JavaScript variable whose value is the
     *  value of a Java object instance.  Use JSObject.getMember to get the
     *  value of the JavaScript object, and verify that the value of the
     *  result object is of the expected class and value.
     *
     *  @param field name of the DataTypeClass field
     *  @param className name of the class of the DataTypeClass field
     *  @param value value of the DataTypeClass field
     *
     *  @see com.netscape.javascript.qa.liveconnect.DataTypeClass
     */
    public void getPublicField( String field, String className, 
        Object value ) 
    {
        String description = field;
        String exception = null;
        Object actual = null;
        String expect = null;
        
        // check the class
        try {
            global.eval( "var myobject = " +description );            
            actual = global.getMember( "myobject" );
            expect  = Class.forName(className).getName();
        } catch ( ClassNotFoundException e ) {
        } catch ( Exception e ) {
            exception = e.toString();
        }            
        
        // might want to do all the interesting stuff here in a try/catch block
        
        addTestCase( "( " + description +" ).getClass()",
            expect,
            actual.getClass().getName(),
            exception );
            
        addTestCase( "( " +description + " == " + value.toString() +" )",
            "true",
            actual.equals( value ) + "",
            exception );

        addTestCase( 
            "\"" +actual.toString() + "\".equals(\"" + value.toString() +"\")",
            "true",
            actual.toString().equals(value.toString()) +"",
            exception );
    }

    /**
     *  Use JSObject.eval to create a JavaScript variable whose value is the
     *  value of a Java object instance.  Use JSObject.eval to reset the value
     *  of the JavaScript variable, and then use JSObject.eval to get the new
     *  value of the JavaScript object.  Verify that the value of the result
     *  object is of the expected class and value.
     *
     *  @param field name of the DataTypeClass field
     *  @param className name of the class of the DataTypeClass field
     *  @param value value of the DataTypeClass field
     *
     *  @see com.netscape.javascript.qa.liveconnect.DataTypeClass
     */
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
            
        addTestCase( 
            "( "+after.toString() +" ).equals(" + newValue.toString() +")",
            "true",
            after.equals( newValue ) + "",
            exception );

        addTestCase( 
            "\"" +after.toString() + "\".equals(\""+newValue.toString()+"\")",
            "true",
            after.toString().equals(newValue.toString()) +"",
            exception );        
    }       
    
    /**
     *  Use JSObject.eval to get the value of a Java object field that has been
     *  defined as final.  Assign a value to a public final instance field.  
     *  The assignment should fail (the value should not change), but there 
     *  should be no error message.
     *
     *  @param field name of the DataTypeClass field
     *  @param className name of the class of the DataTypeClass field
     *  @param value value of the DataTypeClass field
     *
     *  @see com.netscape.javascript.qa.liveconnect.DataTypeClass
     */
    public void setPublicFinalField ( String field, String className, 
        Object value ) 
    {
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

        addTestCase( 
            "\"" +after.toString() + "\".equals(\"" + value.toString() +"\")",
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
    
    /**
     *  Use JSObject.eval to create a JavaScript variable whose value is the
     *  value of a Java object instance.  Use JSObject.setMember to reset the
     *  value of the variable, and then use JSObject.getMember to get the value
     *  of the JavaScript object.  Vverify that the value of the result object
     *  is of the expected class and value.
     *
     *  @param field name of the DataTypeClass field
     *  @param className name of the class of the DataTypeClass field
     *  @param value value of the DataTypeClass field
     *
     *  @see com.netscape.javascript.qa.liveconnect.DataTypeClass
     */
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
 }