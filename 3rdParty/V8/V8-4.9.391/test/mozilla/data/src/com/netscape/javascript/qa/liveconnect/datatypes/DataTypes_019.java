
package com.netscape.javascript.qa.liveconnect.datatypes;

import com.netscape.javascript.qa.liveconnect.*;

/**
 *  Get the string and number values of a Java object that overrides the
 *  default toString and toNumber methods.
 *
 *  @see com.netscape.javascript.qa.liveconnect.DataTypesClass
 *  @see netscape.javascript.JSObject
 *
 *  @author christine m begle
 */

public class DataTypes_019 extends LiveConnectTest {
    public DataTypes_019() {
        super();
    }

    public static void main( String[] args ) {
        DataTypes_019 test = new DataTypes_019();
        test.start();
    }

    public void setupTestEnvironment() {
        super.setupTestEnvironment();
        global.eval( "var DT = "+
            "Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass");
        global.eval( "var dt = new DT();" );
        
        file.bugnumber = "302019";
    }

	public void	executeTest() {
        String expectedStringValue = DataTypeClass.PUB_STRING_REPRESENTATION;
        int expectedNumberValue = DataTypeClass.PUB_NUMBER_REPRESENTATION;
        double expectedDoubleValue = DataTypeClass.PUB_DOUBLE_REPRESENTATION;
        
        String actualStringValue = "";
        double actualNumberValue = 0;
        double actualDoubleValue = 0;
        
        String jsString = "";
        double jsNumber = 0;

		try	{
			actualStringValue =
			    (String) global.eval( "dt.toString()");
			actualNumberValue =
			    ((Double) global.eval( "dt.toNumber()")).doubleValue();
            actualDoubleValue = 
                ((Double) global.eval( "dt.doubleValue()")).doubleValue();
			    
            jsString = 
                (String) global.eval( "var jsString = String(dt); jsString" );
            jsNumber = 
                ((Double) global.eval("var jsNumber = Number(dt); jsNumber")).doubleValue();

        } catch ( Exception e ) {
            e.printStackTrace();
            file.exception = e.toString();
        }

        addTestCase(
            "dt.toString()",
            expectedStringValue,
            actualStringValue,
            file.exception );

        addTestCase(
            "[actualNumberValue: " + expectedNumberValue+"] "+        
            "dt.toNumber(); " + expectedNumberValue+ " == " +actualNumberValue,
            "true",
            (expectedNumberValue==actualNumberValue) +"",
            file.exception );

        addTestCase(
            "[actualDoubleValue: " + expectedDoubleValue+"] "+        
            "dt.doubleValue(); " + expectedDoubleValue+ " == " +actualDoubleValue,
            "true",
            (expectedDoubleValue == actualDoubleValue) +"",
            file.exception );
            
        addTestCase( 
            "global.eval( \"var jsString = String(dt); jsString\" ); "+
            "jsString.equals(" + expectedStringValue +")",
            "true",
            jsString.equals(expectedStringValue) +"",
            file.exception );
            
        addTestCase (       
            "[jsNumber: " + jsNumber+"] "+
            "global.eval( \"var jsNumber = Number(dt); jsNumber\" ); "+
            "jsNumber == " + expectedDoubleValue ,
            "true",
            (jsNumber == expectedDoubleValue) +"",
            file.exception );
    
            
     }
 }
