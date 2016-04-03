/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

import java.io.*;
import java.util.*;

/**
 *  For each file found in a test suite directory, the TestDriver creates a
 *  TestFile object.  This object has no methods; it only stores information 
 *  about the program in the file it represents.  TestFile fields are populated
 *  by the TestEnvironment when the program in the TestFile is executed.
 *
 *  @see com.netscape.javascript.qa.drivers.TestDriver
 *  @see com.netscape.javascript.qa.drivers.TestEnvironment
 *
 *  @author christine@netscape.com
 */

public class TestFile extends File {
    public String   description;
    public String   name;
    public String   filePath;
    public boolean  passed;
    public boolean  completed;
    public String   startTime;
    public String   endTime;
    public String   reason;
    public String   program;
    public int      totalCases;
    public int      casesPassed;
    public int      casesFailed;
    public Vector   caseVector;
    public String   exception;
    public String   bugnumber;

    /**
     * Create a new TestFile.
     *
     * @param name name of this test file
     * @param filePath full path to this file
     */

    public TestFile( String name, String filePath ) {
        super( filePath );

        this.name       = name;
        this.filePath   = filePath;
        this.passed     = true;
        this.completed  = false;
        this.startTime  = "00:00:00";
        this.endTime    = "00:00:00";
        this.reason     = "";
        this.program    = "";
        this.totalCases  = 0;
        this.casesPassed = 0;
        this.casesFailed = 0;
        this.caseVector = new Vector();
        this.exception  = "";
        this.bugnumber = "";
     }
}