package	com.netscape.javascript.qa.liveconnect.exception;

import com.netscape.javascript.qa.liveconnect.*;
import netscape.javascript.JSObject;
import netscape.javascript.JSException;

/**
 *	Evaluate a string that should cause a JavaScript exception.  Verify
 *  that an exception was thrown, and get the exception message.
 *
 *	@see netscape.javascript.JSObject
 *
 *	@author	christine m	begle
 */

public class Exception_001	extends	LiveConnectTest	{
	public Exception_001()	{
		super();
	}

	public static void main( String[] args ) {
		Exception_001 test	= new Exception_001();
		test.start();
	}
	public void	executeTest() {
	    String result = "No exception thrown.";
	    
	    try {
	        // the following statement should throw a JavaScript exception,
	        // since foo is not defined

	        global.eval( "foo.bar = 999;" );

        } catch (Exception e) {
            if ( e instanceof JSException ) {
                result = "JSException thrown!";
            } else {
                result = "Some random exception thrown!";
            }
            
            file.exception = e.toString();
            e.printStackTrace();
        } finally {
            addTestCase(
                "global.eval(\"foo.bar = 999\") should throw a JavaScript exception:",
                "JSException thrown!",
                result,
                file.exception );
        }
	}
 }
