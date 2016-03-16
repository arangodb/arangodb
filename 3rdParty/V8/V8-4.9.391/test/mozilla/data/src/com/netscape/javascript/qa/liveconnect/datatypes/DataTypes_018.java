
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Given a Java class that has a method and field with the same identifier,
 *  attempt to call the method and get the value of the field.
 *
 *  @see com.netscape.javascript.qa.liveconnect.DataTypesClass#amIAFieldOrAMethod
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_018 extends LiveConnectTest {
 
    public DataTypes_018() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_018 test = new DataTypes_018();
        test.start();
    }

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = "+
            "Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass");
        global.eval( "var dt = new DT();" );
        
        file.bugnumber ="301981";
        p( "BUGNUMBER: " + file.bugnumber );
    }

	public void	executeTest() {
	    p( "executing test" );
	    
        String expectedFieldValue = "FIELD!";
        String expectedMethodValue = "METHOD!";

        String actualFieldValue;
        String actualMethodValue;

        String ambiguousReference = "amIAFieldOrAMethod";

		try	{
		    // From JavaScript, call the setter
			actualFieldValue =
			    (String) global.eval( "dt." + ambiguousReference );
			actualMethodValue =
			    (String) global.eval( "dt." + ambiguousReference +"()" );

        } catch ( Exception e ) {
            e.printStackTrace();
            file.exception = e.toString();
            actualFieldValue = "";
            actualMethodValue= "";
        }

        addTestCase(
            "dt." + ambiguousReference,
            expectedFieldValue,
            actualFieldValue,
            file.exception );

        addTestCase(
            "dt." + ambiguousReference +"()",
            expectedMethodValue,
            actualMethodValue,
            file.exception );

     }
}
