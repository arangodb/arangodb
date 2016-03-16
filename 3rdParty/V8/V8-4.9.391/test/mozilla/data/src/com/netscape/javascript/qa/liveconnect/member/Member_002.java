package	com.netscape.javascript.qa.liveconnect.member;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.*;

/**
 *	Create JavaScript objects, and set their properties	to a JavaScript object
 *  using eval or call. Verify the values of the properties using getMember.
 *
 *	@see netscape.javascript.JSObject
 *	@see com.netscape.javascript.qa.liveconnect.datatypes.DataTypes_003
 *	@see com.netscape.javascript.qa.liveconnect.datatypes.DataTypes_004
 *
 *	@author	christine m	begle
 */
public class Member_002	extends	LiveConnectTest	{
	public Member_002()	{
		super();
	}

	public static void main( String[] args ) {
		Member_002 test	= new Member_002();
		test.start();
	}

	public void	setupTestEnvironment() {
		super.setupTestEnvironment();
	}

	public void	executeTest() {
		Object data[] =	getDataArray();
		for	( int i	= 0; i < data.length; i++ )	{
			JSObject jsObject =	getJSObject( (Object[]) data[i] );
			setMember( (Object[]) data[i] );
			getMember( jsObject, (Object[]) data[i] );
			removeMember( jsObject, (Object[]) data[i] );
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
	 *  Use JSObject.eval to assign a JavaScript value to a property of a
	 *  JavaScript object.  Verify that the expression returns the value of
	 *  the assignment expression.
	 *
	 *  @param data Object array that has items corresponding to the name of 
	 *  a JSObject, a property of that object, and a JavaScript assignment 
	 *  expression for that property.
	 */
	public void setMember( Object[] data ) {
        Object result = null;	    
	    String evalString = (String) data[0] + "." + (String) data[2] +" = "+ 
	            (String) data[3];
	    try {
    	    result = global.eval( evalString );
        } catch ( Exception e ) {
            exception = evalString + " threw " + e.toString();
            file.exception += exception;
            e.printStackTrace();
        } finally {
            addTestCase(
                "[ " + evalString+ " returned "+result+" ] " +
                result +".equals( " + data[5] +")",
                "true",
                result.equals(data[5]) +"",
                exception );
        }            
    }	    

    /**
     *  Get the value of a JavaScript property.  Check its type and value.
     *
     *  @param theThis JSObject whose property will be checked
     *  @param data Object array containing the name of the JSObject property
     *  and the expected value of that property.
     */
	public void	getMember( JSObject	theThis, Object[] data ) {
		String exception = null;
		String property	 = (String)	data[2];
		Object eValue	 = data[6];
		Object aValue	 = null;
		Class  eClass	 = null;
		Class  aClass	 = null;
		String eType     = (String) data[7];
		String aType     = null;

		try	{
			aValue = theThis.getMember(	property );
	    	if ( aValue	!= null	) {
				eClass = eValue.getClass();
				aClass = aValue.getClass();
				aType  = (String) global.eval( "typeof " + ((String) data[0]) +
				    "." + ((String) data[2]) );
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
					theThis+".getMember( " +property+" ).equals( "+eValue+"	)",
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

                // check the JS type of the property value
                addTestCase(
                    "[ typeof " +aValue +" returned " + aType +" ] "+
                    aType +" .equals( " +eType + " )",
                    "true",
                    aType.equals( eType ) +"",
                    exception );
			}
		}
	}	
	/**
	 *  Delete a JSObject using JSObject.removeMember.  After removing the
	 *  member, check the member's JavaScript value and type.
	 *
	 *
	 
	 *
	 *
	 */
	public void removeMember( JSObject theThis, Object[] data ) {
	    String exception = null;
	    String property = (String) data[2];
	    Object eValue   = data[8];
	    Object aValue   = null;
	    Class  eClass   = null;
	    Class  aClass   = null;
	    String eType    = (String) data[9];
	    String aType    = null;
	    
		try	{
			theThis.removeMember( property );
			aValue = theThis.getMember( property );
	    	if ( aValue	!= null	) {
				eClass = eValue.getClass();
				aClass = aValue.getClass();
				aType  = (String) global.eval( "typeof " + ((String) data[0]) +
				    "." + ((String) data[2]) );
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
					"[ after removing, getMember returned "	+ aValue +"	] "+
					theThis+".getMember( " +property+" ).equals( "+eValue+"	)",
					"true",
					aValue.equals(eValue) +"",
					exception );

				// check the class of the property
				addTestCase	(
					"[ after removing, "+ aValue+".getClass() returned "+aClass.getName()+"] "+
					aClass.getName() +".equals(	" +eClass.getName()	+" )",
					"true",
					aClass.getName().equals(eClass.getName()) +"",
					exception );

                // check the JS type of the property value
                addTestCase(
                    "[ after removing, typeof " +aValue +" returned " + aType +" ] "+
                    aType +" .equals( " +eType + " )",
                    "true",
                    aType.equals( eType ) +"",
                    exception );
			}
		}
	    
    }	    
	
	/**
	 *	Get	the	data array,	which is an	object array data arrays, which	are
	 *	also object	arrays.	  The data arrays consist of 8 items
	 *
	 *	<ul>
	 *	<li>	Identifier for JavaScript object
	 *	<li>	Assignment expression to initialize JavaScript object
	 *  <li>    Property of JavaScript object to set / get
	 *  <li>    Value to assign to property
	 *	<li>	Initial value of property
	 *	<li>	Value of property before calling setMember
	 *	<li>	Value returned by setMember
	 *	<li>	Value of property after calling setMember
	 *	<li>	String typeof as determined	by JavaScript
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
			new	String(	"foo" ),			// 2 property
			new	String(	"\"bar\"" ),	    // 3 JavaScript value to assign 
			new	String(	"undefined"	),		// 4 value before assignment
			new String( "bar" ),            // 5 value returned by setter
			new	String(	"bar" ),			// 6 value after assignment
			new	String(	"string"),			// 7 JS typeof value
			new String( "undefined"),       // 8 value after removing
			new String( "undefined")        // 9 typeof after removing
		};
		
		Object d1[] = {
		    new String( "num" ),
		    new String( "new Number(12345)" ),
		    new String( "someProperty" ),
		    new String( ".02134" ),
		    new String( "undefined" ),
		    new Double ( "0.02134"),
		    new Double ( "0.02134"),
		    new String ("number"),
			new String( "undefined"),       // 8 value after removing
			new String( "undefined")        // 9 typeof after removing
		    
        };		    
        
        Object d2[] = {            
            new String( "number" ),
            new String( "Number" ),
            new String( "POSITIVE_INFINITY" ),
            new String( "0" ),
            new Double( Double.POSITIVE_INFINITY ),
            new Double("0" ),
            new Double( Double.POSITIVE_INFINITY ),
            new String( "number" ) ,
            new Double( Double.POSITIVE_INFINITY ),
            new String( "number" ) 
        };            
        
	    Object dataArray[] = { d0, d1, d2 };
		return dataArray;
	}
 }
