
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.*;

/**
 *  From JavaScript, call a static Java method that takes arguments.  The 
 *  argument should be correctly cast to the type expected by the Java 
 *  method.  
 *
 *  <p>
 *
 *  This test calls a setter method.  The test is verified by calling the
 *  getter method, and verifying that the value is the setter worked
 *  correctly.
 *
 *  <p>
 *
 *  This tests passing JavaScript values as arguments of the following types:
 *  double, byte, short, long, float, int.
 *
 *  If JavaScript passes a value that is larger than th MAX_VALUE of that type,
 *  expect a JSException.
 *
 *  <p>
 *  Still need tests for the following types:  String, Object, char, JSObject
 *
 *  @see com.netscape.javascript.qa.liveconnect.DataTypesClass
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_016 extends LiveConnectTest {
    public DataTypes_016() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_016 test = new DataTypes_016();
        test.start();
    }

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = "+
            "Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass");
        global.eval( "var dt = new DT();" );
    }
    
    //XXX need to add special cases:  NaN, Infinity, -Infinity, but won't
    // work in this framework

    String jsVals[] = { "0.0", "3.14159", "-1.159", "-0.01", "0.1",
                        "4294967295", "4294967296", "-4294967296", 
                        "2147483647", "2147483648",  "-2147483648" };

    public boolean checkByte( String v ) {
        double max = new Double( Byte.MAX_VALUE ).doubleValue();
        double min = new Double( Byte.MIN_VALUE ).doubleValue();
        double value = new Double(v).doubleValue();
        
        if ( value >= min && value <= max )
            return true;
        return false;
    }
    public boolean checkInteger( String v ) {
        double max = new Double(Integer.MAX_VALUE ).doubleValue();
        double min = new Double( Integer.MIN_VALUE ).doubleValue();
        double value = new Double(v).doubleValue();
        
        if ( value >= min && value <= max )
            return true;
        return false;
    }
    public boolean checkShort( String v ) {
        double max = new Double( Short.MAX_VALUE ).doubleValue();
        double min = new Double( Short.MIN_VALUE ).doubleValue();
        double value = new Double(v).doubleValue();
        if ( value >= min && value <= max )
            return true;
        return false;
    }
    public boolean checkFloat( String v ) {
        double max = new Double( Float.MAX_VALUE ).doubleValue();
        double min = new Double( Float.MIN_VALUE ).doubleValue();
        double value = new Double(v).doubleValue();        
        if ( value >= min && value <= max )
            return true;
        return false;
    }
    public boolean checkLong( String v ) {
        double max = new Double( Long.MAX_VALUE ).doubleValue();
        double min = new Double( Long.MIN_VALUE ).doubleValue();
        double value = new Double(v).doubleValue();        
        if ( value >= min && value <= max )
            return true;
        return false;
    }
    public boolean checkDouble( String v ) {
        double max = new Double( Double.MAX_VALUE ).doubleValue();
        double min = new Double( Double.MIN_VALUE ).doubleValue();
        double value = new Double(v).doubleValue();        
        if ( value >= min && value <= max )
            return true;
        return false;
    }

	public void	executeTest() {
	    for ( int i = 0; i < jsVals.length; i++ ) {
    	    doSetterTest( "DT.staticSetDouble", 
	                      jsVals[i], 
	                      "DT.staticGetDouble", 
	                      "DT.PUB_STATIC_DOUBLE",
	                      (Number) new Double(jsVals[i]),
	                      (Number) new Double(DataTypeClass.PUB_STATIC_DOUBLE),
	                      true );

    	    doSetterTest( "DT.staticSetByte", 
	                      jsVals[i], 
	                      "DT.staticGetByte", 
	                      "DT.PUB_STATIC_BYTE",
	                      (Number) new Byte(new Double(jsVals[i]).byteValue()),
	                      (Number) new Byte(DataTypeClass.PUB_STATIC_BYTE),
	                      checkByte( jsVals[i]) );
                      
            doSetterTest( "DT.staticSetShort",
	                      jsVals[i], 
	                      "DT.staticGetShort", 
	                      "DT.PUB_STATIC_SHORT",
	                      (Number) new Short(new Double(jsVals[i]).shortValue()),
	                      (Number) new Short(DataTypeClass.PUB_STATIC_SHORT),
	                      checkShort( jsVals[i]) );

            doSetterTest( "DT.staticSetInteger",
	                      jsVals[i], 
	                      "DT.staticGetInteger", 
	                      "DT.PUB_STATIC_INT",
	                      (Number) new Integer(new Double(jsVals[i]).intValue()),
	                      (Number) new Integer(DataTypeClass.PUB_STATIC_INT),
	                      checkInteger( jsVals[i]) );

            doSetterTest( "DT.staticSetFloat",
	                      jsVals[i], 
	                      "DT.staticGetFloat", 
	                      "DT.PUB_STATIC_FLOAT",
	                      (Number) new Float(new Double(jsVals[i]).floatValue()),
	                      (Number) new Float(DataTypeClass.PUB_STATIC_FLOAT),
	                      true );

            doSetterTest( "DT.staticSetLong",
	                      jsVals[i], 
	                      "DT.staticGetLong", 
	                      "DT.PUB_STATIC_LONG",
	                      (Number) new Long(new Double(jsVals[i]).longValue()),
	                      (Number) new Long(DataTypeClass.PUB_STATIC_LONG),
	                      checkLong( jsVals[i]) );
        }            	                      
	}
	/**
	 *  This tests calls a Java setter method from JavaScript.  It verifies
	 *  that the setter was called properly in two ways:  by checking the 
	 *  return value of the getter method, and by checking the value of the
	 *  public field that was set.
	 *
	 *	@param setter   java method that takes an argument, and sets a value
	 *  @param jsValue  JavaScript value that is passed to the setter
	 *  @param getter   java method that returns the value set by setter
	 *  @param field    java field that setter changed
	 *  @param eResult  expected result, which should be of some Number type
	 *  @param inRange  whether or not the value is in range for the particular 
	 *  type.  if not, expect a JSExcedeption
	 */
	public void	doSetterTest( String setter, String jsValue, String getter, 
	                            String field, Number eResult, Number fieldValue,
	                            boolean inRange ) 
    {
        String setMethod = setter +"(" + jsValue +");";
        String getMethod = getter + "();";
        
        Double expectedResult = (inRange) ? 
                                new Double( eResult.doubleValue() ) : 
                                new Double(fieldValue.doubleValue());
        Double getterResult=null;
        Object fieldResult=null;
		
		
		String setResult = "no exception";
		String expectedSetResult = setResult;
		
		try	{
            expectedSetResult = (inRange) ? "no exception" :
                Class.forName("netscape.javascript.JSException").getName();
        
		    // From JavaScript, call the setter.  will throw exception if
		    // ! inRange
			global.eval( setMethod );
        } catch ( Exception e ) {
            setResult = e.getClass().getName();
            file.exception = e.toString();
        } finally {
            addTestCase(
                setMethod,
                expectedSetResult,
                setResult,
                file.exception );
        }
        
        try {
			// From JavaScript, call the getter
			getterResult = (Double) global.eval( getMethod );
            
			// From JavaSript, access the field
			fieldResult = (Double) global.eval( field );
        } catch ( Exception e ) {
            e.printStackTrace();
            file.exception = e.toString();
            fieldResult = e.getClass().getName();
        } finally {
            addTestCase(
                "[value: " + getterResult +"; expected: " + expectedResult +"] "+        
                setMethod + "( " + expectedResult +").equals(" + getterResult +")",
                "true",
                expectedResult.equals(getterResult) +"",
                file.exception );
            
            addTestCase(
                "[value: " + fieldResult +"; expected: " + expectedResult +"] "+
                setMethod + "(" + expectedResult +").equals(" + fieldResult +")",
                "true",
                expectedResult.equals(fieldResult) +"",
                file.exception );
        }            
    }
 }
 