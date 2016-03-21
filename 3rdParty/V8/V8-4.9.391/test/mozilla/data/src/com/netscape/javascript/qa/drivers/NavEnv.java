/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

import java.io.*;

import netscape.javascript.JSObject;
import com.netscape.javascript.qa.drivers.*;

/**
 *  TestEnvironment for running JavaScript tests in Navigator.  NavDrv uses
 *  LiveConnect to create Navigator windows in which JavaScript tests are
 *  opened and evaluated.
 *
 *
 *  @see com.netscape.javascript.qa.drivers.NavDrv
 *
 *  @author christine@netscape.com
 *
 */
public class NavEnv implements TestEnvironment {
    TestFile file;
    TestSuite suite;
    NavDrv driver;
    private JSObject result;
    JSObject opener;
    JSObject testcases;
    JSObject location;

    boolean evaluatedSuccessfully = false;
    JSObject window;
   
    private String   WINDOW_NAME;
    
    /**
     *  Constructor a new NavEnv.  
     */
    public NavEnv( TestFile f, TestSuite s, NavDrv d ) {
        this.file = f;
        this.suite = s;
        this.driver =d;

        this.opener = (JSObject) JSObject.getWindow( d );
        this.window = null;
        this.WINDOW_NAME = "js" + getRandomWindowName();
    }
    /**
     *  Called by NavDrv to run the current TestFile.
     */
    public synchronized void runTest() {
        int i = 0;
        System.out.println( file.name );
        try {
            createContext();
            file.startTime = driver.getCurrentTime();
            System.out.println( i++ );
            executeTestFile();
            System.out.println( i++ );
            
            if ( evaluatedSuccessfully ) {
            System.out.println( i++ );
                
                parseResult();
            System.out.println( i++ );
                
            }
            file.endTime = driver.getCurrentTime();            

        } catch ( Exception e ) {
            suite.passed = false;
            file.passed = false;
            file.exception = "file failed with exception: " + e ;
        }
    }

    /**
     *  Creates a new JavaScript context, in this case a Navigator window, and
     *  returns the JSObject associated with that window.
     */
    public Object createContext() {
        System.out.println( "opening window" );
        opener.eval( WINDOW_NAME +" = window.open( '', '" + WINDOW_NAME + "' )" );
        window = (JSObject) opener.getMember( WINDOW_NAME );
        return window;
    }

    /**
     *  Open the current TestFile in the window by using LiveConnect to set the
     *  window's location.href property to a URL where the TestFile can be 
     *  found.  
     */
    public Object executeTestFile() {
        System.out.println( "executeTestFile()" );
        try {
            location = (JSObject) window.getMember( "location" );
            System.out.println( file.name );
            String s = driver.HTTP_PATH + suite.name +"/" + file.name;
            location.setMember( "href", driver.HTTP_PATH + suite.name +"/" + 
                file.name );
            evaluatedSuccessfully = waitForCompletion();

        } catch ( Exception e ) {
            System.err.println( file.name + " failed with exception: " + e );
            file.exception = e.toString();            
            if ( file.name.endsWith( "-n.js" ) ) {
                this.file.passed = true;
                evaluatedSuccessfully = true;
            } else {
                this.file.passed = false;
                this.suite.passed = false;
                evaluatedSuccessfully = false;                
            }
        }         
        return null;
    }
    
    /**
     *  Checks the value of the variable "completed", which is set by the 
     *  stopTest() function in each test.
     *
     *  If the stopTest() function is not called in 20 seconds, the test 
     *  fails.
     *
     *  Negative tests will still succeed, since the onerror handler should
     *  call the stopTest() function.
     *
     */
    public boolean waitForCompletion() {
        int counter = 0;
        if ( ! window.getMember( "completed" ).toString().equals("true") ) {
            while (!window.getMember("completed").toString().equals("true")) {
                try {
                    if ( counter > 20 ) {
                        file.passed = false;
                        file.exception += "test failed to complete";
                        System.out.println( "test failed to complete" );
                        return false;
                    }
                    System.out.println(".");
                    driver.sleep( 1000 );
                    counter++;
                } catch ( Exception e ) {
                    System.out.println( "sleep failed:  " + e );
                    return false;
                }
            }
        }
        return true;
    }

    /**
     *  Use LiveConnect to get the Navigator window's testcases property, which
     *  is defined in all tests.  Parse the testcases array, and create a new
     *  TestCase object for each TestCase object it finds.
     */
    public synchronized boolean parseResult() {
        try {
           JSObject testcases = (JSObject) window.getMember("testcases");
            file.totalCases = ((Number) ((JSObject) testcases).getMember("length")).intValue();
            System.out.println( "testcases.length is " + file.totalCases );
            for ( int i = 0; i < file.totalCases; i++ ) {
                JSObject tc  = (JSObject) ((JSObject) testcases).getSlot(i);

                TestCase nc = new TestCase(
                    tc.getMember("passed") == null ? "null" : tc.getMember("passed").toString(),
                    tc.getMember("name") == null ? "null " : tc.getMember("name").toString(),
                    tc.getMember("description") == null ? "null " : tc.getMember("description").toString(),
                    tc.getMember("expect") == null ? "null " : tc.getMember("expect").toString(),
                    tc.getMember("actual") == null ? "null " : tc.getMember("actual").toString(),
                    tc.getMember("reason") == null ? "null " : tc.getMember("reason").toString()
                );

                file.caseVector.addElement( nc );

                if ( nc.passed.equals("false") ) {
                    if ( file.name.endsWith( "-n.js" ) ) {
                        this.file.passed = true;
                    } else {
                        this.file.passed = false;
                        this.suite.passed = false;
                    }
                }
            }
        } catch ( Exception e ) {
            System.out.println( e );
             file.exception = e.toString();
            if ( file.name.endsWith( "-n.html" ) ) {
                file.passed = true;
            } else {
                file.passed = false;
                suite.passed = false;
            }
        }

        // if the file failed, try to get the file's BUGNUMBER.
        if ( this.file.passed == false ) {
            try {
                this.file.bugnumber = window.getMember("BUGNUMBER").toString();
            }   catch ( Exception e ) {
                // do nothing                
            }                
        }            
        
        return true;
    }
    
    /**
     *  Close Navigator window.
     */
    public void close() {
        opener.eval( WINDOW_NAME +".close()" );
        opener.eval( "delete " + WINDOW_NAME );
    }
    /**
     *  Generate a random window name, so that each test is associated with a 
     *  unique window.
     */
    public String getRandomWindowName() {
        return (Integer.toString((new Double(Math.random()*100000)).intValue()));
    }
}