/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

import java.io.*;
import java.util.*;

/**
 *  For each directory that the TestDriver finds, the TestDriver creates a
 *  TestSuite object.  The TestDriver executes the JavaScript tests it finds
 *  in that directory, and updates the TestSuite object properties, maintaing
 *  the total number of test cases, and the number of test cases that have 
 *  failed.
 *
 *  @see com.netscape.javascript.qa.drivers.TestDriver
 *  @see com.netscape.javascript.qa.drivers.TestEnvironment
 *
 *  @author christine@netscape.com
 */

public class TestSuite extends Vector {
    public String  name;
    public String  filePath;
    public boolean passed;
    public int     totalCases;
    public int     casesPassed;
    public int     casesFailed;

    /**
     *  Create a new TestSuite object.
     *
     *  @param name     name of the TestSuite
     *  @param filePath full path to TestSuite directory
     */

    public TestSuite( String name, String filePath ) {
        this.name       = name;
        this.filePath   = filePath;
        this.passed     = true;
        this.totalCases  = 0;
        this.casesPassed = 0;
        this.casesFailed = 0;
    }
}