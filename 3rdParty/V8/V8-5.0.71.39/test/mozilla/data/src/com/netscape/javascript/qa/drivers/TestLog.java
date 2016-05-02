/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

package com.netscape.javascript.qa.drivers;

//import  netscape.security.PrivilegeManager;
import java.io.*;

/**
 *  Class that writes and appends test result information to a file.
 *
 *  @author christine@netscape.com
 *
 */
public class TestLog {
    private String                name;
    private ByteArrayOutputStream outputStream;
    private PrintStream           printStream;
    private String                terminator;

    /**
     *  Create a new TestLog and open associated streams.
     *
     *  @param name name of the log file
     *  @param terminator string that will be appended to the end of each line
     *
     *
     */
    public TestLog  ( String name, String terminator ) {
        this.name = name;
        this.terminator   = terminator;

        openLog();
    }
    
    /**
     *  Write a string to the end TestLog file.
     */
    public void writeLine( String string ) {
        if ( printStream != null ) {
            printStream.println( string + terminator );
            try {
                RandomAccessFile raf = new RandomAccessFile( name, "rw" );
                raf.seek( raf.length() );
                raf.write( outputStream.toByteArray() );
                raf.close();
                outputStream.reset();
            } catch ( Exception e ) {
                System.out.println( "Exception writing to "+ name +".writeLine():"+ e );
            }
        }
    }
    
    /**
     *  Override if privileges are required to write to file system.
     *  The default implemenation does nothing.
     */
    public void enablePrivileges() {
        return;
    }
    
    /**
     *  Close print stream associated with the TestLog file.
    */
    public void closeLog() {
        if ( printStream != null ) {
            printStream.close();
        }
    }
    /**
     *  Create streams associated with this TestLog file.
     */
    public void openLog() {
        enablePrivileges();
        this.outputStream = new ByteArrayOutputStream();
        this.printStream  = new PrintStream( this.outputStream );
    }

    public String toString() {
        return this.name;
    }        
}