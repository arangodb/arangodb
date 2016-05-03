package com.netscape.javascript.qa.drivers;

import java.io.*;
import com.netscape.javascript.*;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.shell.*;

/**
        This class creates a javax.javascript.Context, which evaluates
        the helper and RhinoFile and returns a result.

        <p>

        If the test throws a Java exception or JavaScript runtime or
        compilation error, the RhinoFile fails, and the exception is stored
        in that RhinoFile's exception variable.

        <p>

        If the test succeeds, the result is parsed and the RhinoFile's test
        result variables are populated.
        
        @author christine@netscape.com
        
*/
public class RhinoEnv implements TestEnvironment {
    public Global global;
    Object cx;
    Object result; 
    TestDriver driver;
    TestSuite suite;
    TestFile file;

    /**
    
    @param f RhinoFile that will be executed in this RhinoEnv
    @param s the RhinoFile's test suite
    @param d the RhinoDrv applet that created this RhinoEnv

    */
    public RhinoEnv( TestFile f, TestSuite s, TestDriver d) {
        this.file = f;
        this.suite = s;
        this.driver = d;
    }
    /**
        Creates the JavaScript Context, which evaluates the contents of a
        RhinoFile and returns a result. The RhinoEnv parses the test
        result, and sets values of the RhinoFile test result properties.
        
        @see com.netscape.javascript.Context#setOptimizationLevel
        @see com.netscape.javascript.Context#setDebugLevel
            
    */
    public synchronized void runTest() {
        this.driver.p( file.name );
        try {
            cx = createContext();
            ((Context) cx).setOptimizationLevel( driver.OPT_LEVEL );
            ((Context) cx).setDebugLevel( driver.DEBUG_LEVEL );
            Object loadFn = executeTestFile(driver.HELPER_FUNCTIONS.getAbsolutePath());

            file.startTime = driver.getCurrentTime();
            result = executeTestFile( file.filePath );
            file.endTime = driver.getCurrentTime();
            parseResult();

        } catch ( Exception e ) {
            suite.passed = false;
            file.passed = false;
            file.exception +=  "file failed with exception:  " + e ;
        }
    }
    /**
     *  Create a new com.netscape.javascript.Context.
     *           
     *  @return the newly instantiated Context
     *
     */
    public Object createContext() {
        // this is stolen from Main.java
        cx = new Context();
        ((Context) cx).enter();

        global = new Global();
        ((Context) cx).initStandardObjects(global);

        String[] names = { "print", "quit", "version", "load", "help",
                           "loadClass" };
        try {
            global.defineFunctionProperties(names, Main.class,
            ScriptableObject.DONTENUM);
        } catch (PropertyException e) {

            throw new Error(e.getMessage());
        }

        return cx;
    }
    public Object executeTestFile() {
        return null;
    }        
    
    /**
            Given a filename,  evaluate the file's contents as a JavaScript
            program.  Return the value of the program.  If the test throws
            a Java exception or JavaScript runtime or compilation error,
            return the string value of the error message.
            
            @param s full path to the file that will be exectued.
            @return test result object.  If the test is positive, result 
            should be an instance of Scriptable.  if the test is negative, 
            the result should be a String, whose value is the message in the 
            JavaScript error or Java exception.
    */
    public Object executeTestFile( String s ) {
        // this bit is stolen from Main.java
        FileReader in = null;
        try {
            in = new FileReader( s );
        } catch (FileNotFoundException ex) {
            driver.p("couldn't open file " + s);
        }

        Object result = null;

        try {
            // Here we evalute the entire contents of the file as
            // as script. Text is printed only if the print() function
            // is called.
            //  cx.evaluateReader((Scriptable) global, in, args[i], 1, null);

            result = ((Scriptable) (((Context) cx).evaluateReader(
                (Scriptable) global, (Reader) in, s, 1, null)));

        }   catch (WrappedException we) {
            driver.p("Wrapped Exception:  "+
            we.getWrappedException().toString());
            result = we.getWrappedException().toString();
        }   catch (Exception jse) {
            driver.p("JavaScriptException: " + jse.getMessage());
            result = jse.getMessage();
        }

        return ( result );
    }
    /**
        Evaluates the RhinoFile result.  If the result is an instance of
        javax.javascript.Scriptable, assume it is a JavaScript Array of
        TestCase objects, as described in RhinoDrv.java.  For each test case in
        the array, add an element to the RhinoFile's test case vector.  If all
        test cases passed, set the RhinoFile's passed value to true; else set
        its passed value to false.

        <p>

        If the result is not a Scriptable object, the test failed.  Set the the
        RhinoFile's exception property to the string value of the result.
        However, negative tests, which should have a "-n.js" extension, are
        expected to fail.  

    */
    public boolean parseResult() {
        FlattenedObject fo = null;
        
        if ( result instanceof Scriptable ) {
            fo = new FlattenedObject( (Scriptable) result );            
            
            try {
                file.totalCases = ((Number) fo.getProperty("length")).intValue();
                for ( int i = 0; i < file.totalCases; i++ ) {
                    Scriptable tc  = (Scriptable) ((Scriptable) result).get( i,
                    (Scriptable) result);

                    TestCase rt = new TestCase(
                        getString(tc.get( "passed", tc )),
                        getString(tc.get( "name", tc )),
                        getString(tc.get( "description", tc )),
                        getString(tc.get( "expect", tc )),
                        getString(tc.get( "actual", tc )),
                        getString(tc.get( "reason", tc ))
                    );

                    file.bugnumber= 
                        (getString(tc.get("bugnumber", tc))).startsWith
                            ("com.netscape.javascript") 
                            ? file.bugnumber
                            : getString(tc.get("bugnumber", tc));
                    
                    file.caseVector.addElement( rt );
                    if ( rt.passed.equals("false") ) {
                        this.file.passed = false;
                        this.suite.passed = false;
                    }
                }
                
                if ( file.totalCases == 0 ) {
                    if ( file.name.endsWith( "-n.js" ) ) {
                        this.file.passed = true;
                    } else {                    
                        this.file.reason  = "File contains no testcases. " + this.file.reason;
                        this.file.passed  = false;
                        this.suite.passed = false;
                    }                    
                }
                
            } catch ( Exception e ) {                
                this.file.exception = "Got a Scriptable result, but failed "+
                    "parsing its arguments.  Exception: " + e.toString() +
                    " Flattened Object is: " + fo.toString();
                    
                this.file.passed = false;
                this.suite.passed = false;

                return false;
            }
        } else {
            // if it's not a scriptable object, test failed.  set the file's
            // exception to the string value of whatever result we did get.
            this.file.exception = result.toString();

            // if the file's name ends in "-n", the test expected an error.

            if ( file.name.endsWith( "-n.js" ) ) {
                this.file.passed = true;
            } else {
                this.file.passed = false;
                this.suite.passed = false;
                return false;
            }
        }
       
        return true;
    }
    /**
            Close the context.
    */
    public void close() {
        try {
            ((Context) cx).exit();
         } catch ( Exception e ) {
            suite.passed = false;
            file.passed = false;
            file.exception =  "file failed with exception:  " + e ;
         }

    }
    /**
     *  Get the JavaScript string associated with a JavaScript object.
     *  
     *  @param object a Java identifier for a JavaScript object
     *  @return the JavaScript string representation of the object
     */
    public String getString( Object object ) {
         return (((Context) cx).toString( object ));
    }
}
