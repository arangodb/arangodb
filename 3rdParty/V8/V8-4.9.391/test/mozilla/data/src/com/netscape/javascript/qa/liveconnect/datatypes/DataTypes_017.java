
package	com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *	From JavaScript, call an instance Java method that takes arguments.	 The
 *	argument should	be correctly cast to the type expected by the Java
 *	method.
 *
 *	<p>
 *
 *	This test calls	a setter method.  The test is verified by calling the
 *	getter method, and verifying that the value	is the setter worked
 *	correctly.
 *
 *	<p>
 *
 *	This tests passing JavaScript values as	arguments of the following types:
 *	double,	byte, short, long, float, int.
 *
 *	<p>
 *	Still need tests for the following types:  String, Object, char, JSObject
 *
 *	@see com.netscape.javascript.qa.liveconnect.DataTypesClass
 *	@see netscape.javascript.JSObject
 *
 *	@author	christine m	begle
 */

public class DataTypes_017 extends LiveConnectTest {
	public DataTypes_017() {
		super();
	}

	public static void main( String[] args ) {
		DataTypes_017 test = new DataTypes_017();
		test.start();
	}

	public void	setupTestEnvironment() {
		super.setupTestEnvironment();
		
		global.eval( "var DT = "+
			"Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass");
		global.eval( "var dt = new DT();" );
	}

	// XXX need	to add special cases:  NaN,	Infinity, -Infinity, but won't
	// work	in this	framework

	String jsVals[]	= {	"0", "3.14159",	"-1.159", "-0.01", "0.1",
						"4294967295", "4294967296",	"-4294967296",
						"2147483647", "2147483648",	 "-2147483648" };

    /**
     *  call the setter test with 5 values:
     *  <ul>
     *  <li>    string value to call setters / getters with
     *  <li>    method to set the value
     *  <li>    method to get the value
     *  <li>    field that returns the value
     *  <li>    expected result of getter, field
     *  <li>    whether or not an exception gets thrown
     *  </ul>
     *
     *  The rules for conversion:
     *  <ul>
     *  <li>    if the setter sets the value out of the range, return an exception
     *  <li>    if the value is in range, transform value to Math.floor( value )
     *  <li>    NaN returns 0.
     *  </ul>
     *  
     *
     *
     */
     
    public void	executeTest() {
        for ( int i = 0; i < jsVals.length; i++ ) {
            doSetterTest(jsVals[i],
                         "dt.setDouble", "dt.getDouble", "dt.PUB_DOUBLE",
                         (new Double(jsVals[i]).doubleValue() > Double.MAX_VALUE 
                          ? (Object) EXCEPTION
                          : (new Double(jsVals[i]).doubleValue() < Double.MIN_VALUE
                             ? (Object) EXCEPTION : new Double( jsVals[i] ))
                          ));
						  
/*
			doSetterTest( "dt.setByte",
						  jsVals[i],
						  "dt.getByte",
						  "dt.PUB_BYTE",
						  (Number) new Byte(new Float(jsVals[i]).byteValue()),
						  new Float	(Byte.MAX_VALUE),
						  new Float	(Byte.MIN_VALUE));
						  
			doSetterTest( "dt.setShort",
						  jsVals[i],
						  "dt.getShort",
						  "dt.PUB_SHORT",
						  (Number) new Short(new Double(jsVals[i]).shortValue()),
						  new Float	(Short.MAX_VALUE),
						  new Float	(Short.MIN_VALUE ));

			doSetterTest( "dt.setInteger",
						  jsVals[i],
						  "dt.getInteger",
						  "dt.PUB_INT",
						  (Number) new Integer(new Double(jsVals[i]).intValue()),
						  new Float	(Integer.MAX_VALUE),
						  new Float	(Integer.MIN_VALUE));

			doSetterTest( "dt.setFloat",
						  jsVals[i],
						  "dt.getFloat",
						  "dt.PUB_FLOAT",
						  (Number) new Float(new Double(jsVals[i]).floatValue()),
						  new Float	(Float.MAX_VALUE), 
						  new Float	(Float.MIN_VALUE));


			doSetterTest( "dt.setLong",
						  jsVals[i],
						  "dt.getLong",
						  "dt.PUB_LONG",
						  (Number) new Long(new	Double(jsVals[i]).longValue()),
						  new Float	(Long.MAX_VALUE),
						  new Float	(Long.MIN_VALUE));
*/						  
		}

	}
	/**
	 *	This tests calls a Java	setter method from JavaScript.	It verifies
	 *	that the setter	was	called properly	in two ways:  by checking the
	 *	return value of	the	getter method, and by checking the value of	the
	 *	public field that was set.
	 *
	 *	@param setter	java method	that takes an argument,	and	sets a value
	 *	@param jsValue	JavaScript value that is passed	to the setter
	 *	@param getter	java method	that returns the value set by setter
	 *	@param field	java field that	setter changed
	 *	@param eResult	expected result, which should be of	some Number	type
	 */

    public void	doSetterTest(String jsValue, String setter, String getter,
                             String field, Object eResult )
    {
        String setMethod = setter +"(" + jsValue +");";
        String getMethod = getter +	"();";
        String setterResult = "No exception thrown";
        Double getterResult = null;
        Double fieldResult = null;
        Object expectedResult = null;
        boolean eq = false;
        
        try {
            eq = eResult.getClass().equals(Class.forName("java.lang.String"));
        }
        catch (ClassNotFoundException e) {
            addTestCase (setMethod + " driver error.",
                         "very", "bad", file.exception);
        }
        
        if (eq) {
            try {
                global.eval( setMethod );
            } catch ( Exception e ) {
                setterResult = EXCEPTION;
                file.exception = e.toString();
                e.printStackTrace();
            } finally {
                addTestCase(setMethod +" should throw a JSException",
                            EXCEPTION,
                            setterResult,
                            file.exception );
            }
        } else {    
		
            try	{
                // From	JavaScript,	call the setter
                global.eval( setMethod );
                
                // From	JavaScript,	call the getter
                getterResult = (Double)	global.eval( getMethod );
                
                // From	JavaSript, access the field
                fieldResult	= (Double) global.eval(	field );
                
            } catch	( Exception	e )	{
                e.printStackTrace();
                file.exception = e.toString();
            } finally {
                addTestCase("[value: " + getterResult +"; expected:	" +
                            expectedResult +"] "+ setMethod + getMethod +
                            "( " + expectedResult + ").equals(" + 
                            getterResult +")", "true",
                            expectedResult.equals(getterResult)	+ "",
                            file.exception);

                addTestCase("[value: " + fieldResult + "; expected: " +
                            expectedResult + "] " + setMethod + field +
                            "; (" + expectedResult +").equals(" +
                            fieldResult +")", "true",
                            expectedResult.equals(fieldResult) +"",
                            file.exception );
            }
        }   
    }
	
    String EXCEPTION = "JSException expected";
}
