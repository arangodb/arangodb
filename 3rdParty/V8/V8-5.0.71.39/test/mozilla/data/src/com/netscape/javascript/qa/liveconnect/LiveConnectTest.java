/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation, All Rights Reserved.
 */

package com.netscape.javascript.qa.liveconnect;

import java.applet.Applet;
import java.io.File;
import java.util.Vector;
import netscape.javascript.*;
//import netscape.security.*;

import com.netscape.javascript.qa.drivers.*;

/**
 *  LiveConnectTest is the parent class for all LiveConnect test applets.
 *
 *  <p> Subclasses must override the executeTest() and main() methods, and 
 *  provide a constructor.  To add test cases, subclasses should use the
 *  addTestCaseMethod.
 *
 *  <p> Each test class creates JavaScript TestCase objects through LiveConnect,
 *  and prints results to standard output, just like the JavaScript language
 *  tests do.
 *
 *  <p> To run a test individually, add the test package to your classpath
 *  and start the LiveConnected JavaScript shell.  Instantiate the test class,
 *  and call the test's run method.  Test results will be written to standard
 *  out.
 * 
 *  <p> To run the test in Navigator, create an HTML page for each test applet.
 *  Load the page in Navigator.  Test results will be written to the Java
 *  console.
 *
 *  <p> To run the test in the LiveConnect test driver, use the class
 *  LiveConnectDrv.  LiveConnectDrv starts an out of process Liveconnect
 *  enabled jsshell.exe, and passes the shell the arguments necessary to
 *  start the test application.
 *
 *  <p> LiveConnectTests write results to the log files, using the static 
 *  methods in com.netscape.javascript.qa.drivers.TestDriver.
 *
 *  After each test file is completed, each applet writes test case failures
 *  to the test case log, and if the test fails, also writes to the file log.
 *
 *  <p> The applet writes a summary of that file's information to a temporary 
 *  log that is parsed by the driver.  The temporary log contains name and 
 *  value pairs of the TestFile properties, which the driver uses to generate 
 *  suite and summary logs.  
 *
 *  <p> Running the test suite is very dependent on LiveConnect working.ft
 *  Not sure whether this is a good thing, or not.
 *
 *  @see LiveConnectTest#addTestCase
 *  @see com.netscape.javascript.qa.drivers.LiveConnectDrv
 *  @see com.netscape.javascript.qa.drivers.LiveConnectEnv
 *  @see com.netscape.javascript.qa.drivers.TestDriver
 *
 *  @author christine@netscape.com
 *  
 */

public class LiveConnectTest extends Applet implements Runnable {
    /**
     *  Create a new LiveConnectTest.
     */
    public LiveConnectTest() {
    }

    /**
     *  Main method.  Used when running as an application.
     */
    public static void main( String[] args ) {
        LiveConnectTest test = new LiveConnectTest();
    }
    
    public void start() {
        run();
    }        
    
    public void stop() {
    }
    
    /**
     *  Main method when run as an Applet. 
     *
     */
     
    public void run() {
        System.out.println("Running the test." );
        setupTestEnvironment();
        file.startTime = TestDriver.getCurrentTime();
        executeTest();
        file.endTime= TestDriver.getCurrentTime();
        getResults();
        cleanupTestEnvironment();
        stop();
    }
    
    /**
     *  Initialize variables, open log files.
     */
    public void setupTestEnvironment() {
        global  = JSObject.getWindow( this );    
      
        getEnvironment();
      
        if ( ENVIRONMENT == BROWSER ) {
//            PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
//            PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );
        }            
        output  = getOutputDirectory();
        if (LOGGING) {
            TestDriver.openLogFiles( output );
            templog = getTempLog( output );
        }            
        testdir = getTestDirectory();
        file   = new TestFile( this.getClass().getName(), testdir.toString() +
            this.getClass().toString() );
            
        file.bugnumber = this.BUGNUMBER;            
        file.description = ( this.getClass().toString() );
    }
    
    public void getEnvironment() {
        this.ENVIRONMENT = ( global.getMember("version").equals( "undefined")) 
                    ? BROWSER
                      : SHELL;
        return;                      
    }        
    
    /**
     *  Create a TestLog object which will be used to write the testclass 
     *  results to a temporary log file.  If there is an existing log,
     *  delete it.
     */
    public TestLog getTempLog( File output ) {
        String templog = "";
        
        try {
            TEMP_LOG_NAME = ((String) global.getMember( "OUTPUT_FILE" )).equals
                ("undefined") 
                ? TEMP_LOG_NAME 
                : (String) global.getMember("OUTPUT_FILE");
       
            templog = output.getAbsolutePath() + TEMP_LOG_NAME;
        } catch ( Exception e ) {
            this.exception = e.toString();
            p("Exception deleting existing templog: " + e.toString() );
        }
        
        return new TestLog( templog, "" );
    }        

    /**
     *  Get the OUTPUT_FILE if defined, which is where the temp log is written.
     *  If the file is not defined, assume we are running in the shell, and do
     *  not write any output.
     *
     *  XXX change this to match description above.
     *
     *  @see com.netscape.javascript.drivers.LiveConnectDrv
     */
    public File getOutputDirectory() {
        String o = "";
        
        if ( this.ENVIRONMENT == BROWSER ) {
            String outputParam = this.getParameter( "output" );
            
            return new File( getParameter( "output" ) );
        } else {
            try {
                o = (String) global.getMember( "OUTPUT_DIRECTORY" );
                
                if ( ! o.equals( "undefined") ) {
                    LOGGING = true;                    
                }                    
            } catch ( Exception e ) {
                System.err.println( "OUTPUT_DIRECTORY threw: " + e );
                e.printStackTrace();
                System.exit(1);
            }
            
            System.out.println( "Output file is " + o.toString() );
            
            return new File( o.toString() );            
        }            
    }        
    
    /**
     *  Get the TEST_DIRECTORY variable, which must be defined in the 
     *  JavaScript helper file.
     *
     *  @see com.netscape.javascript.drivers.LiveConnectDrv
     */
    public File getTestDirectory() {
        try {
            String o = (String) global.getMember( "TEST_DIRECTORY" );
            o = o.endsWith( File.separator ) ? o : o + File.separator;
            return new File( o.toString() );
        } catch ( Exception e ) {
            System.err.println( "TEST_DIRECTORY is not defined: " + e );
            return new File( "." );
        }        
    }        

    /**
     *  Execute the test.  Subclasses must implement this method.  The 
     *  default implemenation does nothing.  This method should instantiate 
     *  TestCases, and add them to the testcase Vector.
     *
     */
    public void executeTest() {
        return;
    }
    
    /**
     *  Add a TestCase to the testcase Vector.
     *
     *  @param description a description of the test case
     *  @param expect a string representation of the expected result
     *  @param actual a string representation of the actual result
     *  @param exception the message in any JavaScript runtime error
     *         or Java exception that was thrown.
     *
     */
    
    public void addTestCase( String description, String expect, String actual,
        String exception ) 
    {
        String result = ( expect == actual ) ? "true" : "false";
        TestCase tc = new TestCase( result, this.getClass().getName().toString(), 
            description, expect, actual, exception );
        file.caseVector.addElement( tc );            
        file.totalCases++;        
        if ( result == "false" ) {
            file.passed = false;
            file.casesFailed++;
         } else {
            file.casesPassed++;
         }
        return;            
    }
    
    /**
     *  Close all logs.  
     *
     */
    public void closeLogs() {
        TestDriver.getLog( output, TestDriver.SUMMARY_LOG_NAME ).closeLog();
        TestDriver.getLog( output, TestDriver.SUITE_LOG_NAME ).closeLog();
        TestDriver.getLog( output, TestDriver.FILE_LOG_NAME ).closeLog();
        TestDriver.getLog( output, TestDriver.CASE_LOG_NAME ).closeLog();
        templog.closeLog();
        templog = null;
    }        
    
    /**
     *  Iterate through the testcase Vector.  Populate the properties of the
     *  TestFile object.  
     */
    public void getResults() {
        displayResults();
        if (LOGGING) {
            writeResultsToCaseLog();
            writeResultsToFileLog();
            writeResultsToTempLog();
        }
    }
    
    /**
     *  Write a summary of the TestCase to standard out.
     *
     */
    public void displayResults() {
        for ( int i = 0; i < file.caseVector.size(); i++ ) {
            TestCase tc = (TestCase) file.caseVector.elementAt(i);
            p( tc.description +" = "+ tc.actual+ 
                ( tc.expect != tc.actual 
                ?  " FAILED!  expected: " + tc.expect
                :  " PASSED!" ) );
        }
        getFailedCases();
    }        
    
    /**
     *  If any test cases did not pass, write a summary of the failed cases
     *  to the CASE_LOG using static TestDriver methods.
     *
     *  @see com.netscape.javascript.qa.drivers.TestDriver#writeCaseResults
     */
    public void writeResultsToCaseLog() {
        if ( !file.passed ) {
            TestDriver.writeCaseResults( file, file.description, output );
        }            
    }        
    
    /**
     *  If the test failed, write a summary of this test to the FILE_LOG.  Use
     *  static TestDriver methods to write to the FILE_LOG.
     *
     *  @see com.netscape.javascript.qa.drivers.TestDriver#writeFileResult
     */
    public void writeResultsToFileLog() {
        if ( !file.passed ) {
            TestDriver.writeFileResult(file, null, output);
        }            
    }     
    
    /**
     *  Write to a file containing the results of this TestFile. The content 
     *  of this file is parsed by the parseResult() method of LiveConnectEnv,
     *  which maintains information about all files and suites executed.  The
     *  format of the temp log is a list of name value pairs, one per line.
     *  The temp log is overwritten each time the LiveConnectDrv executes a
     *  test.
     *
     *  Changes to the format of the templog file will require changes to 
     *  LiveConnectDrv.parseResult.
     *
     *  <pre>
     *  CLASSNAME LiveConnectTest
     *  PASSED    [true, false]
     *  LENGTH    [number of testcases in this test]  
     *  NO_PASSED [number of testcases that passed]
     *  NO_FAILED [number of testcases that failed]
     *  </pre>
     *
     *  XXX may also want to write bugnumber, time completed, etc?.  probably
     *  what this should do is enumerate all the properties of a TestFile object,
     *  that the driver can parse and treat as though it had a normal TestFile
     *  object.  
     *
     *  @see com.netscape.javascript.qa.drivers.LiveConnectDrv#parseResult
     */
    public void writeResultsToTempLog(){
        System.out.println( "Writing results to " + templog.toString() );
        
        templog.writeLine( file.description );
        templog.writeLine( file.passed + "" );
        templog.writeLine( file.caseVector.size() +"" );
        templog.writeLine( file.casesPassed + "");
        templog.writeLine( file.casesFailed + "");
        templog.writeLine( file.bugnumber );
        
        p( file.name );
        p( "passed:       " + passed );
        p( "total cases:  " + file.caseVector.size() );
        p( "cases passed: " + file.casesPassed );
        p( "cases failed: " + file.casesFailed );
        p( "bugnumber:    " + file.bugnumber );
        return;       
    }        

    /**
     *  Close logs, and set the value of the "completed" variable in the 
     *  JavaScript environment so that the driver knows the test has 
     *  finished.
     */
    public void cleanupTestEnvironment() {
        try {
            if ( LOGGING ) {
//                templog.closeLog();            
            }                
//            global.eval("var completed = true;");
        } catch ( Exception e ) {
            p( "exception in cleanupTestEnvironment: " + e.toString() );
            e.printStackTrace();
        }            
    }        
    
    public void p (String s ) {
        System.out.println( s );
    }        
    
    /** 
     *  This dandy little hack from Nix that is used to test things that 
     *  should fail. It runs a method on a JavaScript object, catching any 
     *  exceptions.  It returns the detail message from the exception, or 
     *  NO_EXCEPTION if it succeeds.  This is from nix.
     *
     */
    public static String catchException(JSObject self, String method,
                                        Object args[]) {
        Object rval;
        String msg;
        try {
            rval = self.call(method, args);
            msg = NO_EXCEPTION;
        } catch (Throwable e) {
            msg = e.getMessage();
        }
        return msg;
    }
    
    public void getFailedCases() {
        if ( file.passed ) {
            return;
        }
        
        p( "********** Failed Test Cases **********" );            
        
        for ( int i = 0; i < file.caseVector.size(); i++ ) {
            TestCase tc = (TestCase) file.caseVector.elementAt(i);
            
            if ( tc.passed != "true" ) {
                
                p( tc.description +" = "+ tc.actual+ 
                    ( tc.expect != tc.actual 
                    ?  " FAILED!  expected: " + tc.expect
                    :  " PASSED!" 
                    ) +
                    ( tc.reason.length() > 0
                    ?  ": " + tc.reason
                    :  ""
                    )
                );
            }                    
        }
    }        
    
    public File output;
    public File testdir;
    
    public TestFile file;
    
    public Vector testcase;
    public JSObject global;
    
    public boolean  passed = true;
    public Vector   failedVector = new Vector();
    public TestLog  templog;
    public String   exception = "";
    
    public String BUGNUMBER = "";    
    
    boolean LOGGING = false;
    public static String TEMP_LOG_NAME = "result.tmp";
        
    public static final int BROWSER = 1;
    public static final int SHELL   = 0;
    
    public int ENVIRONMENT;
    
    public static final String NO_EXCEPTION = "Expected exception not thrown.";
    
    public static final String DATA_CLASS = "com.netscape.javascript.qa.liveconnect.DataTypeClass";
}
