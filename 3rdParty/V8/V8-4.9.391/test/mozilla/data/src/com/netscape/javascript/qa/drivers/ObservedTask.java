package com.netscape.javascript.qa.drivers;

import java.io.*;

/**
 * This class is simple utility class that is used to run a process
 * which expects no user input. ObserverdTask stores the exit status of
 * the process along with standard output and error.
 *
 * This class is used by the harness only when testing the C version
 * of the JavaScript Engine.  It is not used by Rhino (JavaScript in
 * Java).
 *
 *  @author christine@netscape.com
 *  @author Nick Lerissa
 */
 
public class ObservedTask {
    public String commandLine;

    public StringBuffer input = new StringBuffer();
    public StringBuffer error = new StringBuffer();
    int exitValue;
    
    public Object observer;

    public ObservedTask(String cl, Object observer) {
        this.commandLine = cl;
        this.observer = observer;
    }

    public StringBuffer getInput() {
        return input;
    }

    public StringBuffer getError() {
        return error;
    }

    int getExitValue() {
        return exitValue;
    }
    /**
     * Execute the process and return when the process is complete.
     *
     */
    public void exec() throws IOException {
        Runtime rt = Runtime.getRuntime();
        
        try {
            Process proc = rt.exec(commandLine);
            OutputStream os = rt.getLocalizedOutputStream(proc.getOutputStream());
            
            if ( this.observer instanceof RefEnv ) {
                os.write("quit();\n".getBytes());
            }                

            os.flush();
            os.close();

            InputStreamReader is;
        
            is = new InputStreamReader(proc.getErrorStream());
            (new Thread(new StreamReader(error,is))).start();

            is = new InputStreamReader(proc.getInputStream());
            (new Thread(new StreamReader(input,is))).start();

            proc.waitFor();
            exitValue = proc.exitValue();
            
            // unfortunately the following pause seems to
            // need to be here otherwise we get a crash
            
            // On AIX, Process.waitFor() doesn't seem to wait for
            // the process to complete before continuing.  Bad.
            // Need to find a workaround.
            
            if ( System.getProperty("os.name").startsWith("AIX") ||
                System.getProperty("os.name").startsWith("HP") ) {
                pause(20000);
            } else {                
                pause( 10000 );
            }
        
        } catch ( Exception e ) {
            java.lang.System.out.println( e );
            e.printStackTrace();
        }            
    }

    /**
     * Simple print method used for debugging
     */
    public void print() {
        System.out.println("Input Stream of Process:");
        System.out.println(input);
        System.out.println("Error Stream of Process:");
        System.out.println(error);
        System.out.println("Exit Value of Process: " + exitValue);
    }

    /**
     * Simple pause method used for debugging.
     */
    static void pause(int length) {
        try {
            Thread.currentThread().sleep(length);
        } catch (InterruptedException ex) {
            System.err.println( ex );
        }
    }

    /**
     * main used for debugging.
     */
    static public void main(String args[]) {
        if (args.length < 1) {
            System.err.println("USAGE: java RunIt <command line>");
            System.exit(1);
        }
        try {
            ObservedTask task = new ObservedTask(args[0], null);
            task.exec();
            task.print();
            pause(10000);
        } catch (Exception e) {
            System.err.println("ERROR Exception thrown: " + e);
            System.exit(2);
        }
    }

    /**
     * A simple class that reads the contents of and InputStreamReader
     * This class is used to read the output and error streams of the process.
     */
    class StreamReader implements Runnable {
        StringBuffer buffer;
        InputStreamReader inputStreamReader;

        StreamReader(StringBuffer b, InputStreamReader i) {
            buffer = b;
            inputStreamReader = i;
        }
        
        public void run() {
            try {
                int ch;
                while ((ch = inputStreamReader.read()) != -1) {
                    buffer.append((char) ch);
                }
            } catch (IOException ex) {
                System.err.println("Error IOException thrown " + ex);
                ex.printStackTrace();
            }
        }
    }    
}
