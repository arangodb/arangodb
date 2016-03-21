/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

/**
 *  TestEnvironment creates the JavaScript environment in which the program in
 *  the TestFile is evaluated.  It expects the test result to be a JavaScript 
 *  array of JavaScript TestCase objects. TestEnvironment creates a 
 *  com.netscape.javascript.qa.drivers.TestCase object for each TestCase it 
 *  finds, and populates the TestCase, TestFile, and TestSuite object 
 *  properties.
 *
 *  @see com.netscape.javascript.qa.drivers.TestDriver
 *  @author christine@netscape.com
 *
 */
public interface TestEnvironment {
    /**
     *  Pass the JavaScript program to the JavaScript environment.
     */
    
    public void runTest();

    /**
     *  Create a new JavaScript context in which to evaluate a JavaScript 
     *  program. 
     *  
     *  @return the JavaScript context
     *
     */
    public Object createContext();
    
    /**
     * Given a filename, evaluate the file's contents as a JavaScript
     * program.  
     *  
     * @return the return value of the JavaScript program.  If the test throws
     * a Java exception or JavaScript runtime or compilation error, return the
     * string value of the error message.
     */
    public Object executeTestFile();
    
    /**
     *  Evaluate the result of the JavaScript program.  Attempt to get the
     *  JavaScript Array of TestCase objects.  For each TestCase object  
     *  found, create a com.netscape.javascript.qa.drivers.TestCase object
     *  and populate its fields.
     *
     *  @return true if the the result could be parsed successfully; false
     *  otherwise.
     */
    
    public boolean parseResult();

    /**
     *  Close the context.  Destroy the JavaScript environment.
     */
    public void close();
}