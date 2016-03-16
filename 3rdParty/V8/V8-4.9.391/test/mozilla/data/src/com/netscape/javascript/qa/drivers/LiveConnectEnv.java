/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

import com.netscape.javascript.qa.liveconnect.LiveConnectTest;
import java.lang.*;
import java.io.*;
import java.util.Vector;

/**
 *  The LiveConnect test environment.  Creates a new LiveConnect enabled
 *  JavaScript shell, and passes it three arguments
 *
 *  <pre>
 *  jsshell helper.js Packages.com.netscape.javascript.qa.liveconnect.LiveConnectTestClass js00000.tmp
 *  </pre>
 
 *  where helper.js contains some statements that allow the shell to run
 *  the test, and js00000.tmp is the temporary file whose name is generated
 *  by this class, and specifies where test results should be written.
 *
 *  Unlike the other TestEnvironments, LiveConnectEnv does not parse the test
 *  result.  The LiveConnectEnv uses LiveConnect to get and parse the test
 *  result, using LiveConnect to get the JSObject that contains the testcases.
 *
 *  The LiveConnectEnv gets File and Suite result information from a
 *  temporary file that the test applet writes to the output directory
 *  the test
 *
 *  @see com.netscape.javascript.qa.drivers.TestEnvironment
 *  @see com.netscape.javascript.qa.drivers.ObservedTask
 *  @see com.netscape.javascript.qa.drivers.LiveConnectDrv
 *  @see com.netscape.javascript.qa.liveconnect.LiveConnectTest
 *
 *  @author christine@netscape.com
 */

public class LiveConnectEnv implements TestEnvironment {
    TestFile        file;
    TestSuite       suite;
    LiveConnectDrv  driver;
    ObservedTask    task;
    String          TEMP_LOG_NAME;
    File            helper;

    /**
     *  Create a new LiveConnectEnv.
     */
    public LiveConnectEnv( TestFile f, TestSuite s, LiveConnectDrv d) {
        this.file = f;
        this.suite = s;
        this.driver = d;
        this.TEMP_LOG_NAME = "js" + getRandomFileName() +".tmp";
    }

    /**
     *   Called by the driver to execute the test program.
     */
    public synchronized void runTest() {
        try {
            createContext();
            file.startTime = driver.getCurrentTime();            
            executeTestFile();
            file.endTime = driver.getCurrentTime();

            if (task.getExitValue() != 0) {
                System.out.println( "Abmormal program termination.  "+
                    "Exit value: " + task.getExitValue() );
                if ( file.name.endsWith( "-n.js" )) {
                    file.passed = true;
                } else {
                    file.exception = "Process exit value: " + task.getExitValue();
                    suite.passed   = false;
                    file.passed    = false;
                }
            }
            parseResult();

        } catch ( Exception e ) {
            suite.passed = false;
            file.passed  = false;
            e.printStackTrace();                
        }        
    }
    
    /**
     *  Instantiate a new JavaScript shell, passing the helper file and the
     *  name of the test class as arguments.
     */
    public Object createContext() {
        task = new ObservedTask( driver.EXECUTABLE + "  " +
                driver.HELPER_FUNCTIONS.getAbsolutePath() + " " +
                file.filePath +" "+
                TEMP_LOG_NAME,
                this );
        return (Object) task;
    }

    /**
     *  Start the shell process.
     */
    public Object executeTestFile() {
        try {
            task.exec();
        }   catch ( IOException e ) {
            System.err.println( e );
            file.exception = e.toString();
        }
        return null;
    }

    /**
     *  Parse the results file for this test.  The which contains data in the
     *  following format:
     *  <pre>
     *  CLASSNAME LiveConnectTest
     *  PASSED    [true, false]
     *  LENGTH    [number of testcases in this test]  
     *  NO_PASSED [number of testcases that passed]
     *  NO_FAILED [number of testcases that failed]
     *  </pre>
     *
     *  @see com.netscape.javascript.qa.liveconnect.LiveConnectTest#writeResultsToTempFile
     */
    public synchronized boolean parseResult() {
        String line;
        BufferedReader buf = new BufferedReader(new StringReader(
            new String(task.getInput())));
        try {
            do {
                line = buf.readLine();
                System.out.println( line );
            }  while( line != null ) ;              
        } catch ( IOException e ) {
            System.err.println( "Exception reading process output:" + 
                e.toString() );
            file.exception  = e.toString();
            return false;
        }            

        Vector label = null;
        Vector value = null;
        
        String t = null;
        
        try {
            FileReader fr = new FileReader(
                driver.OUTPUT_DIRECTORY.getAbsolutePath()+ 
                TEMP_LOG_NAME );
                
            BufferedReader br = new BufferedReader( fr );
            String classname  = br.readLine();
            boolean passed    = (new Boolean(br.readLine())).booleanValue();
            int length        = (new Double(br.readLine())).intValue();
            int no_passed     = (new Double(br.readLine())).intValue();
            int no_failed     = (new Double(br.readLine())).intValue();
            String bugnumber  = br.readLine();
            
            if ( ! passed ) {
                this.file.passed = false;
                this.suite.passed = false;
            }
            this.file.totalCases   += length;
            this.suite.totalCases  += length;
            this.file.casesPassed  += no_passed;
            this.suite.casesPassed += no_passed;
            this.file.casesFailed  += no_failed;
            this.suite.casesFailed += no_failed;
            this.file.bugnumber    = bugnumber;
            
        } catch ( IOException e ) {
            System.err.println( e );
            e.printStackTrace();
        }
        return true;
    }        

    public String getRandomFileName() {
        return (Integer.toString((new Double(Math.random()*100000)).intValue()));
    }

    /**
     *  Delete the temp log associated with this context.
     *
     */
    public void close(){
        String templog = driver.OUTPUT_DIRECTORY + TEMP_LOG_NAME;
        try {
            File f = new File ( templog );            
            if ( f.exists() ) {
                f.delete();
            }
        } catch ( Exception e ) {
            e.printStackTrace();
        }
        return;
    }
    
    void p( String s ) {
        System.out.println( s );
    }        
    
}
