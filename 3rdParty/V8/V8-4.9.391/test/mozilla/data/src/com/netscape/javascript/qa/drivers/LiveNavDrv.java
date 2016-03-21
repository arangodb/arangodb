package com.netscape.javascript.qa.drivers;

import netscape.security.PrivilegeManager;
import netscape.javascript.JSObject;

import java.util.Vector;
import java.util.Date;
import java.io.*;
import java.applet.Applet;

/**
 *  LiveNavDrv is a test driver for running the JSObject test applets in Netscape
 *  Navigator.
 *
 *  The only difference between LiveNavDrv and NavDrv is that it creates a 
 *  LiveNavEnv object (rather than a NavEnv) in which the LiveConnect tests 
 *  are evaluated.
 *
 *  @see NavDrv
 *  @see LiveNavEnv
 *
 *  @author christine@netscape.com
 */
public class LiveNavDrv extends NavDrv {
    JSObject window;
    String SUFFIX;

    public LiveNavDrv() {
        super();
        setSuffix(".java");
    }
    
    public static void main ( String[] args ) {
        System.out.println( "main" );
        LiveNavDrv d = new LiveNavDrv();
        d.start();
    }
    
    /**
     *  Iterate through suites.  For each file in each suite, create a 
     *  LiveNavEnv (in this case a Navigator window) that can load and
     *  evaluate the test result.
     *
     *  @see LiveNavEnv#parseResult
     */

    public synchronized void executeSuite( TestSuite suite ) {
        PrivilegeManager.enablePrivilege( "UniversalFileAccess" );
        PrivilegeManager.enablePrivilege( "UniversalFileRead" );        
        PrivilegeManager.enablePrivilege( "UniversalFileWrite" );                
        PrivilegeManager.enablePrivilege( "UniversalPropertyRead" );
        
        LiveNavEnv context;
        TestFile file;

        for ( int i = 0; i < suite.size(); i++ ) {
            synchronized ( suite ) {
                file = (TestFile) suite.elementAt( i );
                context = new LiveNavEnv( file, suite, this );
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
}
