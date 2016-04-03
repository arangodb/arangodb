package	com.netscape.javascript.qa.liveconnect.member;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.*;

/**
 *	Create JavaScript objects, and set their properties	to a Java object or
 *  JSObject using setMember. Verify the values of the properties using 
 *  getMember.
 *
 *	@see netscape.javascript.JSObject
 *	@see com.netscape.javascript.qa.liveconnect.datatypes.DataTypes_003
 *	@see com.netscape.javascript.qa.liveconnect.datatypes.DataTypes_004
 *
 *	@author	christine m	begle
 */

public class Member_001	extends	LiveConnectTest	{
	public Member_001()	{
		super();
	}

	public static void main( String[] args ) {
		Member_001 test	= new Member_001();
		test.start();
	}

	public void	setupTestEnvironment() {
		super.setupTestEnvironment();
	}

	public void	executeTest() {
		Object data[] =	getDataArray();
		for	( int i	= 0; i < data.length; i++ )	{
			JSObject jsObject =	getJSObject( (Object[]) data[i] );

			// get the initial value of	the	property
			getMember( jsObject, (Object[])	data[i], ((Object[]) data[i])[4] );

			// set the value of	the	property
			setMember( jsObject, (Object[])	data[i]	);

			// verify the value	of the property
			getMember( jsObject, (Object[])	data[i], ((Object[]) data[i])[5] );
		}
	}
	public JSObject	getJSObject( Object[] data ) {
		return (JSObject) global.eval( data[0] +" = " + data[1] );
	}		 
	/**
	 *	Get	the	data array,	which is an	object array data arrays, which	are
	 *	also object	arrays.	  The data arrays consist of 3 items
	 *
	 *	<ul>
	 *	<li>	String passed to global.eval to	create the "this" object for setMember
	 *	<li>	String property	of the JSObject	to get or set
	 *	<li>	Object new value of	the	property (pass to setMember, and expect		
	 *	<li>	Object value of	the	property before	setting	it (expected result	from getMember)
	 *	<li>	String representation of the property, as retrieved	by getMember
	 *	<li>	String class name of the property as retrieved by getMember
	 *	<li>	String typeof as determined	by JavaScript
	 *	</ul>
	 *
	 *	To add test	cases to this test,	modify this	method.
	 *
	 *	@return	the	data array.
	 */
	public Object[]	getDataArray() {
		Object d0[]	= {
		    new String( "d0" ),            // 0 identifier
			new	String(	"new Boolean()"),	// 1 assignment expression
			new	String(	"foo" ),			// 2 property
			new	String(	"bar" ),			// 3 value to assign
			new	String(	"undefined"	),		// 4 initial value
			new	String(	"bar" ),			// 5 value after assignment
			"java.lang.String",				// 6 class of property
			new	String(	"object")			// 7 JS typeof value
		};

		Object d1[]	= {
		    new String( "d1" ),            // 0 identifier
			new	String(	"new String(\"JavaScript\")"),	// 1 assignment expression
			new	String(	"foo" ),			// 2 property
			new	Boolean( "true" ),			// 3 value to assign
			new	String(	"undefined"	),		// 4 initial value
			new	Boolean( "true" ),			// 5 value after assignment
			"java.lang.Boolean",				// 6 class of property
			new	String(	"object")			// 7 JS typeof value
		};
		
		Object d2[] = {
		    new String( "d2" ),            // 0 identifier
			new	String(	"new Number(12345)"), // 1 assignment expression
			new	String(	"foo" ),			// 2 property
			new	Double( "0.2134" ),			// 3 value to assign
			new	String(	"undefined"	),		// 4 initial value
			new	Double( "0.2134" ),			// 5 value after assignment
			"java.lang.Double",				// 6 class of property
			new	String(	"object")			// 7 JS typeof value
		};

		Object d3[] = {
		    new String( "d3" ),            // 0 identifier
			new	String(	"new Number(12345)"), // 1 assignment expression
			new	String(	"foo" ),			// 2 property
			new	Integer( "987654" ),			// 3 value to assign
			new	String(	"undefined"	),		// 4 initial value
			new	Integer( "987654" ),			// 5 value after assignment
			"java.lang.Integer",				// 6 class of property
			new	String(	"object")			// 7 JS typeof value
		};
    
        Object d4[] = {
            new String( "d4" ),
            new String ( "new Object()" ),
            new String( "property" ),
            global,
            new String( "undefined" ),
            global,
            "netscape.javascript.JSObject",
            new String ( "object" )
        };            

		Object dataArray[] = { d0, d1, d3, d4 };
		return dataArray;
	}

    /**
     *  Get the value of a JavaScript property.  Check its class.
     *
     *
     */
	public void	getMember( JSObject	theThis, Object[] data,	Object value ) {
		String exception = "";
		String property	 = (String)	data[2];
		Object eValue	 = value;
		Object aValue	 = null;
		Class  eClass	 = null;
		Class  aClass	 = null;
		
		try	{
			aValue = theThis.getMember(	property );
		   
			if ( aValue	!= null	) {
				eClass = eValue.getClass();
				aClass = aValue.getClass();
			}				 
			
		} catch	( Exception	e )	{
			exception =	theThis	+".getMember( "	+ property + " ) threw " +
				e.toString();
			file.exception += exception;
			e.printStackTrace();
		}  finally {
			if ( aValue	== null	) {
			} else {
				// check the value of the property
				addTestCase( 
					"[ getMember returned "	+ aValue +"	] "+
					data[0]+".getMember( " +property+" ).equals( "+eValue+"	)",
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
				
				// 
				
			}
		}			 
	}		 

    /**
     *  Set the value of a JavaScript property.
     *
     */
	public void	setMember( JSObject	theThis, Object[] data ) {
		String exception = "";
		String result	 = "passed";
		String property	 = (String)	data[2];
		Object value	 = data[3];
		
		try	{
			theThis.setMember( property, value );
		}  catch ( Exception e ) {
			result = "failed!";
			exception =	"("+ theThis+").setMember( " + property	+","+ value	+" )	"+
				"threw " + e.toString();
			file.exception += exception;
			e.printStackTrace();
		} finally {
			addTestCase(
				"("+theThis+").setMember( "+property +", "+	value +" )",
				"passed",
				result,
				exception );
		}
	}
 }
