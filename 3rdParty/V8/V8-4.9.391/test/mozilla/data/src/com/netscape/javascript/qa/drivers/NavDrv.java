package com.netscape.javascript.qa.drivers;

import netscape.security.PrivilegeManager;
import netscape.javascript.JSObject;

import java.util.Vector;
import java.util.Date;
import java.io.*;
import java.applet.Applet;

import com.netscape.javascript.qa.drivers.*;

/**
 *  NavDrv is a test driver for running JavaScript tests in Netscape
 *  Navigator.
 *
 *  NavDrv expects to find test files locally, as well as on an HTTP server,
 *  since in some tests, JavaScript applications may behave differently with 
 *  local versus cached files.
 *
 *  To run the test without signing the classfiles, add this to the test 
 *  machine's preferences:
 *  <pre>
 *  user_pref("signed.applets.codebase_principal_support", true);
 *  </pre>
 *
 *  The HTML file in which the applet is defined needs to define the following
 *  parameters:
 *  <table>
 *  <tr>
 *  <th>    Parameter
 *  <th>    Value
 *  </tr>
 *
 *  <tr>
 *  <td>    directory
 *  <td>    full path to the directory in which tests are installed
 *  </tr>
 *  <tr>
 *  <td>    output
 *  <td>    full path to the directory in which log files should be written
 *  </tr>
 *  <tr>
 *  <td>    http_path
 *  <td>    location on an http server in which test files are installed
 *  </tr>
 *  </table>
 *
 *  @see RhinoDrv
 *  @author christine@netscape.com
 */
public class NavDrv extends com.netscape.javascript.qa.drivers.TestDriver {
    JSObject window;
    String SUFFIX = ".html";

    public NavDrv() {
        super(null);
        System.out.println( "constructor");
        setSuffix(".html");
    }

    public static void main ( String[] args ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );
        
        System.out.println( "main" );
        NavDrv d = new NavDrv();
        d.start();
    }

    public boolean processOptions( ) {
        System.out.println( "NavDrv.processOptions()" );
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        window = (JSObject) JSObject.getWindow( this );

        // Get parameters

        String d  = getParameter("directory");
        String o  = getParameter("output");
        
        HTTP_PATH = getParameter("http_path");
        TEST_DIRECTORY   = new File( d );
        OUTPUT_DIRECTORY = new File( o );

        System.out.println( "http_path: " + HTTP_PATH );
        System.out.println( "directory: " + TEST_DIRECTORY );
        System.out.println( "output:    " + OUTPUT_DIRECTORY );

        if ( ! TEST_DIRECTORY.isDirectory() ) {
            System.err.println( "error:  " +
            TEST_DIRECTORY.getAbsolutePath() +" is not a directory." );
            return false;
        }
        if ( ! OUTPUT_DIRECTORY.isDirectory() ) {
            System.err.println( "error:  " +
            OUTPUT_DIRECTORY.getAbsolutePath() +" is not a directory." );
            return false;
        }

        return true;
    }

    public static void openLogFiles( File o ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        TestDriver.openLogFiles( o );
    }

    public Vector getSuites( String[] files ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        
        return ( super.getSuites( files ));
    }
    public void getCases( TestSuite suite ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                

        super.getCases( suite );
    }

    public static TestLog getLog(File output, String filename ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        return  TestDriver.getLog( output, filename );
    }        

    public void writeLogHeaders( File output ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        super.writeLogHeaders( output );
    }        

    public static void writeSuiteResult( TestSuite suite, File output) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        TestDriver.writeSuiteResult( suite, output );
    }        

    public static void writeSuiteSummary( TestSuite suite, File output) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        TestDriver.writeSuiteSummary( suite, output );
    }        

    public static void writeFileResult( TestFile file, TestSuite suite, File output) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        TestDriver.writeFileResult( file, suite, output );
    }        
    
    public static void writeCaseResults( TestFile file, TestSuite suite, File output) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        TestDriver.writeCaseResults( file, suite, output );
    }        
    
    public static void writeDateToLogs( String separator, File output) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        TestDriver.writeDateToLogs( separator, output);
    }        
    

    public synchronized void executeSuite( TestSuite suite ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );
        
        NavEnv context;
        TestFile file;

        for ( int i = 0; i < suite.size(); i++ ) {
            synchronized ( suite ) {
                file = (TestFile) suite.elementAt( i );
                context = new NavEnv( file, suite, this );
                context.runTest();

                writeFileResult( file, suite, OUTPUT_DIRECTORY );
                writeCaseResults(file, suite, OUTPUT_DIRECTORY );
                context.close();
                context = null;

                if ( ! file.passed ) {
                    suite.passed = false;
                }
            }
        }
        writeSuiteResult( suite, OUTPUT_DIRECTORY );
        writeSuiteSummary( suite, OUTPUT_DIRECTORY );
    }

    public void stop() {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        super.stop();
    }        
    public void start() {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );

        super.start();
    }        
    
}
