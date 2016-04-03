package	com.netscape.javascript.qa.liveconnect.call;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.*;

/**
 *  Instantiate a JavaScript object.  From java, use JSObject.call to invoke a
 *  method of that object.  Verify the return value.
 *
 *  call global method.  call built in method.  cal user defined method.  call
 *  method that takes no arguments.
 *
 *	@see netscape.javascript.JSObject
 *
 *	@author	christine m	begle
 */
public class Call_001 extends LiveConnectTest {
	public Call_001() {
		super();
	}

	public static void main( String[] args ) {
		Call_001 test	= new Call_001();
		test.start();
	}

	public void	setupTestEnvironment() {
		super.setupTestEnvironment();
	}

	public void	executeTest() {
		Object data[] =	getDataArray();
		for	( int i	= 0; i < data.length; i++ )	{
			JSObject jsObject =	getJSObject( (Object[]) data[i] );
			call( jsObject, (Object[]) data[i] );
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
	    String constructor = data[0] +" = " + data[1];
	    JSObject theThis = (JSObject) global.eval( constructor );
		return theThis;
	}

    /**
     *  Use JSObject call to invoke a JavaScript method.  Verify the value and
     *  class of the object returned.
     *
     *  @param theThis JSObject whose method will be called will be checked
     *  @param data Object array containing the name of the method, arguments
     *  to be passed to that method, and the expected return value of the
     *  method.
     */
	public void	call( JSObject	theThis, Object[] data ) {
		String exception = null;
		String method	 = (String)	data[2];
		Object args[]    = (Object[]) data[3];

		Object eValue	 = data[4];
		Object aValue	 = null;
		Class  eClass	 = null;
		Class  aClass	 = null;

		try	{
			aValue = theThis.call( method, (Object[]) args );
	    	if ( aValue	!= null	) {
				eClass = eValue.getClass();
				aClass = aValue.getClass();
			}
		} catch	( Exception	e )	{
			exception =	theThis	+".call( "	+ method + ", " + args+" ) threw "+
				e.toString();
			file.exception += exception;
			e.printStackTrace();
		}  finally {
			if ( aValue	== null	) {
			} else {
				// check the value of the property
				addTestCase(					
					"[ getMember returned "	+ aValue +"	] "+
					theThis+".call( "+method+", "+args+" ).equals( "+eValue+" )",
					"true",
					aValue.equals(eValue) +"",
					exception );

				// check the class of the property
				addTestCase	(
					"[ "+ aValue+".getClass() returned "+aClass.getName()+"] "+
					aClass.getName() +".equals(	" +eClass.getName()	+" )",
					"true",
					aClass.getName().equals(eClass.getName()) +"",
					exception );
			}
		}
	}

	/**
	 *	Get	the	data array,	which is an	object array data arrays, which	are
	 *	also object	arrays.
	 *
	 *	<ul>
	 *	<li>	Identifier for JavaScript object
	 *	<li>	Assignment expression to initialize JavaScript object
	 *  <li>    Method of the JavaScript object
	 *  <li>    Arguments to pass to the method
	 *	<li>	Value returned by the method
	 *	</ul>
	 *
	 *	To add test	cases to this test,	modify this	method.
	 *
	 *	@return	the	data array.
	 */
	public Object[]	getDataArray() {
		Object d0[]	= {
		    new String( "boo" ),            // 0 identifier
			new	String(	"new Boolean()"),	// 1 assignment expression
			new	String(	"valueOf" ),		// 2 method
			null,	                        // 3 argument array
			new	Boolean( "false" ),		// 4 return value
		};

		Object d1[]	= {
		    new String( "date" ),     
			new	String(	"new Date(0)"),
			new	String(	"getUTCFullYear" ),
			null,	                     
			new	Double( "1970" ),		
		};

	    Object dataArray[] = { d0, d1 };
		return dataArray;
	}
 }
