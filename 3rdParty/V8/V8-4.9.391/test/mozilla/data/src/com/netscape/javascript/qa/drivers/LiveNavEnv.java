/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

import java.io.*;

import java.applet.Applet;
import java.util.Vector;
import netscape.security.PrivilegeManager;
import netscape.javascript.JSObject;
import com.netscape.javascript.qa.liveconnect.LiveConnectTest;
import com.netscape.javascript.qa.liveconnect.jsobject.JSObject_001;

/**
 *  TestEnvironment for running JSObject test applets in Navigator.  LiveNavDrv
 *  uses LiveConnect to create Navigator windows in which JavaScript tests are
 *  opened and evaluated.
 *
 *  @see com.netscape.javascript.qa.drivers.LiveNavDrv
 *  @see com.netscape.javascript.qa.drivers.NavDrv
 *  @see com.netscape.javascript.qa.drivers.NavEnv
 *
 *  @author christine@netscape.com
 *
 */
public class LiveNavEnv extends NavEnv {
    TestFile file;
    TestSuite suite;
    LiveNavDrv driver;
    private JSObject result;
    JSObject opener;
    JSObject testcases;
    JSObject location;

    boolean evaluatedSuccessfully = false;
    JSObject window;
    
    Applet applet;

    private String   WINDOW_NAME;

    /**
     *  Construct a new LiveNavEnv.
     *
     *  @param file TestFile whose program will be evaluated in this 
     *  environment
     *  @param suite TestSuite which this TestFile belongs to
     *  @param driver   TestDriver that is currently running
     */
    public LiveNavEnv( TestFile file, TestSuite suite, LiveNavDrv driver ) {
        super( file, suite, driver );
    }
    
    /**
     *  Open the current TestFile in the window by using LiveConnect to set the
     *  window's location.href property to a URL where the TestFile can be 
     *  found.  
     *
     *  XXX need to generate HTML file on the fly.
     */
    public Object executeTestFile() {
        try {
            location = (JSObject) window.getMember( "location" );
            driver.p( file.name );

            //  we need to look for the html file in the test directory.  
            //  the name of the file is the class name + ".html"

            String classname  = (file.name.substring(0, file.name.length() -
                ".class".length()) + ".html" );
            
            String s = driver.HTTP_PATH + classname; 
            
            System.out.println( "trying to set browser window to " + s );

            location.setMember( "href", s );
            evaluatedSuccessfully = waitForCompletion();

        } catch ( Exception e ) {
            driver.p( file.name + " failed with exception: " + e );
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
     *  Get a reference to the LiveConnectTest applet.  Unfortunately, due
     *  to reasons I don't understand, we can't just get the TestFile object
     *  from the LiveConnectTest via LiveConnect.  Attempting to cast the
     *  applet to LiveConnectTest fails with a java.lang.ClassCastException,
     *  so we continue to use the temporary file hack in Navigator.  Need a
     *  reference to the applet so we can properly destroy it before closing
     *  the Navigator window.
     *
     *  This is http://scopus/bugsplat/show_bug.cgi?id=300350, and when that's
     *  fixed we can ue the getAppletClass method below to directly access
     *  the LiveConnectTest's TestFile file object so we don't have use the
     *  temporary file hack.
     *
     */
     public Applet getApplet() {
        Object document   = (Object) window.getMember("document");            
        Object applets    = ((JSObject) document).getMember("applets");
        return  (Applet) ((JSObject) applets).getSlot( 0 );
     }  
     
     /**
      * Currently does not work.
      *
      * @see #getApplet
      */
     
     public void getAppletClass(Applet applet) {
        try {
            driver.p( "the class of applet is " + 
                applet.getClass().toString() );

            driver.p( "is it a JSObject_001? " + 
                (applet instanceof JSObject_001 ));

            driver.p( "is it a LiveConnectTest? " + 
                (applet instanceof LiveConnectTest ));

            driver.p( "is it an Applet? " + 
                (applet instanceof Applet ));

            driver.p( "Try to cast applet to JSObject_001" );

            // this throws the ClassCastException
                driver.p( ((JSObject_001) applet).toString() );

        } catch ( Exception e ) {
            driver.p( "parseResult threw exception: " + e );
        }
     }        

    /**
     *  Parse the results file for this test.  The which contains data in the
     *  following format:
     *  <pre>
     *  CLASSNAME [LiveConnectTest class name]
     *  PASSED    [true, false]
     *  LENGTH    [number of testcases in this test]  
     *  NO_PASSED [number of testcases that passed]
     *  NO_FAILED [number of testcases that failed]
     *  </pre>
     *
     *  @see com.netscape.javascript.qa.liveconnect.LiveConnectTest#writeResultsToTempFile
     */
    public boolean parseResult() {
        applet = getApplet();
        
        Vector label = null;
        Vector value = null;
        
        try {
            FileReader fr = new FileReader(
                driver.OUTPUT_DIRECTORY.getAbsolutePath()+ 
                (driver.OUTPUT_DIRECTORY.getAbsolutePath().endsWith(File.separator) 
                ? ""
                : File.separator ) +
                LiveConnectTest.TEMP_LOG_NAME);
            BufferedReader br = new BufferedReader( fr );
            String classname  = br.readLine();
            boolean passed    = (new Boolean(br.readLine())).booleanValue();
            int length        = (new Double(br.readLine())).intValue();
            int no_passed     = (new Double(br.readLine())).intValue();
            int no_failed     = (new Double(br.readLine())).intValue();
            if ( ! passed ) {
                this.file.passed = false;
                this.suite.passed = false;
            }
            this.file.totalCases   += length;
            
            this.file.casesPassed  += no_passed;
            this.suite.casesPassed += no_passed;
            
            this.file.casesFailed  += no_failed;
            this.suite.casesFailed += no_failed;

        } catch ( IOException e ) {
            driver.p( e.toString() );
            return false;
        }
        return true;
    }        
    
    /**
     *  Destroy the test applet, close Navigator window, and delete the 
     *  reference to the test applet's window.
     */
    public void close() {
        applet.destroy();
        opener.eval( WINDOW_NAME +".close()" );
        opener.eval( "delete " + WINDOW_NAME );
    }
}
