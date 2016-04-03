/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

/**     
 *  Equivalent to the JavaScript TestCase object as described in 
 *  TestDriver.java.  For each test case in a TestFile, a TestCase object is 
 *  created and added to the TestFile's caseVector.
 *
 *  @see com.netscape.javascript.qa.drivers.TestDriver
 *  @see com.netscape.javascript.qa.drivers.TestEnvironment
 *
 *  @author christine@netscape.com
 */

public class TestCase {
    public String passed;
    public String name;
    public String description;
    public String expect;
    public String actual;
    public String reason;
    
    /**
     *  Create a new TestCase object.
     *
     *  @param passed       "true" if the test case passed, otherwise "false"
     *  @param name         label associated with the test case
     *  @param description  string showing what got tested, usually JavaScript 
     *  code
     *  @param expect       string representation of the expected result
     *  @param actual       string representation of the actual result
     *  @param reason       reason for test failure
     *
     *  @see com.netscape.javascript.qa.drivers.TestDriver
     *  @see com.netscape.javascript.qa.drivers.TestEnvironment
     *
     */
    public TestCase( String passed, String name, String description,
                      String expect, String actual, String reason ) {

        this.passed = passed;
        this.name   = name;
        this.description = description;
        this.expect = expect;
        this.actual = actual;
        this.reason = reason;
    }
}
