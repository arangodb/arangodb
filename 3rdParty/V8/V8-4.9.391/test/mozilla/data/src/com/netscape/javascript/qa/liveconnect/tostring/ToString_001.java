package	com.netscape.javascript.qa.liveconnect.tostring;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.*;

/**
 *  SomeJSObject.toString() should return the same String value as
 *  SomeJSObject.call( "toString", null ) and
 *  global.eval( "somejsobjectname +''" );
 *
 *
 *  this test:
 *  constructs a javascript object
 *  gets the javascript object
 *  gets the string value of the object using call
 *  gets the string value of the object using eval +""
 *  gets the string value of the object using toString
 *  
 *
 *  ie it should return the JavaScript string representation of that object.
 *
 *	@see netscape.javascript.JSObject
 *
 *	@author	christine m	begle
 */
public class ToString_001	extends	LiveConnectTest	{
	public ToString_001()	{
		super();
	}

	public static void main( String[] args ) {
		ToString_001 test	= new ToString_001();
		test.start();
	}

	public void	setupTestEnvironment() {
		super.setupTestEnvironment();
	}

	public void	executeTest() {
		Object data[] =	getDataArray();

		for	( int i	= 0; i < data.length; i++ )	{
			JSObject jsObject =	getJSObject( (Object[]) data[i] );
            getStrings( jsObject, (Object[]) data[i] );
		}
	}
    
    public void getStrings( JSObject jsObject, Object[] data ) {
        String eString = (String) data[2];
        String resultOfToString = null;
        String resultOfCall = null;
        String resultOfEval = null;
        
        try {
            resultOfToString = jsObject.toString();
            resultOfCall    = (String) jsObject.call( "toString", null );
        } catch ( Exception e ) {
            file.exception = e.toString();
            p( "getStrings threw " + e.toString() );
            e.printStackTrace();
        } finally {
            
            addTestCase(
                "[ jsObject.toString returned " + jsObject.toString() +" ]"+
                "( " + jsObject +" ).toString().equals( " + eString +" )",
                "true",
                jsObject.toString().equals(eString) +"",
                file.exception );
                
            addTestCase(
                "[ calling toString returned " + resultOfCall +" ]"+
                "( " + jsObject +".call( \"toString\", null ).equals(" + eString+")",
                "true",
                resultOfCall.equals(eString)+"",
                file.exception );
        }            
    }

	/**
	 *  Create and return a JSObject using data in the data array.
	 *
	 *  @param data Object array containing name of JSObject, and assignment
	 *  expression
	 *  @return the JSObject
	 */
	public JSObject	getJSObject( Object[] data ) {
	    global.eval( (String) data[1] );
	    JSObject theThis = (JSObject) global.getMember( (String) data[0] );
		return theThis;
	}

	/**
	 *	Get	the	data array,	which is an	object array data arrays, which	are
	 *	also object	arrays.	  The data arrays consist of 8 items
	 *
	 *	<ul>
	 *	<li>	Identifier for JavaScript object
	 *	<li>	Assignment expression to initialize JavaScript object
	 *  <li>    expected string value of the object
	 *	</ul>
	 *
	 *	To add test	cases to this test,	modify this	method.
	 *
	 *	@return	the	data array.
	 */
	public Object[]	getDataArray() {
		Object d0[]	= {
		    new String( "o" ),            // 0 identifier
			new	String(	"var o = new Object()"),	// 1 complete expression for instantiation or assignment
			new String( "[object Object]" )
		};

		Object d1[] = {
		    new String( "num" ),
		    new String( "num = new Number(12345)" ),
	        new String( "12345" )
        };

        Object d2[] = {
            new String( "number" ),
            new String( "number = new Number(Infinity)"),
            new String( "Infinity" )
        };
        
        Object d3[] = {
            new String( "string" ),
            new String( "string = new String(\"JavaScript\")" ),
            new String( "JavaScript" )
        };           
        
        Object d4[] = {
            new String ("array"),
            new String( "array = new Array(1,2,3,4,5)"),
            new String( "1,2,3,4,5")
        };            

	    Object dataArray[] = { d0, d1, d2, d3, d4 };
		return dataArray;
	}
 }
