package com.netscape.javascript.qa.drivers;

//import netscape.javascript.*;
//import org.mozilla.javascript.*;
import java.io.*;

/**
 *  LiveConnectDrv is an application that drives the testing of the LiveConnect
 *  API test suite.  This driver runs the stand-alone JavaScript engine in C.
 *  To run the LiveConnectTest API suite in Navigator, use XXX an as yet unwritten
 *  driver, which will depend on a generated HTML file to create the test applet.
 *  
 *  <p>
 *  The application requires the following arguments:
 *
 *  <table>
 *  <tr>
 *  <td>    -d
 *  <td>    directory in which LiveConnect test applications are located
 *  </tr>
 *  <tr>
 *  <td>    -s
 *  <td>    list of suites to execute (optional)
 *  </tr>
 *  <tr>
 *  <td>    -o
 *  <td>    directory in which log files will be written
 *  </tr>
 *  <tr>
 *  <td>    -e
 *  <td>    path to the LiveConnect shell executable
 *  </table>
 *
 *  LiveConnectDrv checks the directory, finds the suites in that directory,
 *  and in each suite a list of all the class files in that suite.  For each
 *  class file found, LiveConnectDrv creates a new LiveConnectEnv object,
 *  which runs the test.
 *
 *  <p>
 *
 *  LiveConnectDrv also generates a helper file that is passed to the JavaScript
 *  shell.  The helper file contains statements that allow the shell to run the
 *  test class.
 *
 * 
 *  The contents of the helper file look something like this:  
 *
 *  <pre>
 *  var OUTPUT_DIRECTORY = <i>value of OUTPUT_DIRECTORY</i>
 *  TestClassName = arguments[0];
 *  TestClass = eval( TestClassName );
 *  testclass = new TestClass();
 *  quit();
 *  </pre>
 *
 *  <p> Each test class creates JavaScript TestCase objects through LiveConnect,
 *  and prints results to standard output, just like the JavaScript language
 *  tests do.  Additionally, test logs are created.
 *
 *  <p> The parent test class, LiveConnectTest, contains methods from
 *  com.netscape.javascript.qa.drivers.TestDriver that allow it to write
 *  test results directly to the log files.
 *
 *  @see com.netscape.javascript.qa.liveconnect.LiveConnectTest
 *  @see LiveConnectEnv
 *
 *  @author christine@netscape.com
 *
 */

public class LiveConnectDrv extends TestDriver {
    public LiveConnectDrv( String[] args ) {
        super( args );
        setSuffix( ".class");
    }
    public static void main ( String[] args ) {
        LiveConnectDrv d = new LiveConnectDrv( args );
        d.start();
    }        
    public boolean processOptions() {
        int length = ARGS.length;
        
        if (ARGS[0].startsWith("-")) {
            //XXX need to verify that we at least get valid d and -h options

            for (int i=0; i < ARGS.length; i++ ) {
                if ( ARGS[i].equals("-d") ) {
                    this.TEST_DIRECTORY =  
                    ARGS[i].endsWith(File.separator)
                    ? new File( ARGS[++i] )
                    : new File( ARGS[++i] + File.separator );

                    if ( ! ( this.TEST_DIRECTORY ).isDirectory() ) {
                        p( "error:  " +
                        this.TEST_DIRECTORY.getAbsolutePath() +
                            " is not a directory." );
                        return false;
                    } else {
                        continue;
                    }
                }
                if ( ARGS[i].equals("-s") ) {
                    FILES = new String[20] ;
                    for ( int j = ++i, k=0; j < ARGS.length; j++ ) {
                        if ( ARGS[j].startsWith("-") ){
                            break;
                        }
                        FILES[k++] = ARGS[j];
                    }
                }
                if ( ARGS[i].equals("-h") ) {
                    this.HELPER_FUNCTIONS = new File( ARGS[++i] );
                    if ( ! (this.HELPER_FUNCTIONS ).isFile() ) {
                        p( "error:  "+ 
                            this.HELPER_FUNCTIONS.getAbsolutePath()+
                            " file not found." );
                        return false;
                    }
                }
                if ( ARGS[i].equals("-o") ) {
                    String odir = ARGS[++i];
                    
                    OUTPUT_DIRECTORY = new File( 
                        (odir.endsWith(File.separator) ? odir :  
                        odir+File.separator));
                    
                    OUTPUT_DIRECTORY.mkdirs();

                    if ( !OUTPUT_DIRECTORY.exists() || 
                        !OUTPUT_DIRECTORY.isDirectory() ) 
                    {
                        p( "error:  "+
                        OUTPUT_DIRECTORY.getAbsolutePath()+
                        " could not create directory.");
                        return false;
                    }

                }

                if ( ARGS[i].equals("-p") ) {
                    OPT_LEVEL = Integer.parseInt( ARGS[++i] );
                }

                if ( ARGS[i].equals("-db" )) {
                    DEBUG_LEVEL = Integer.parseInt( ARGS[++i] );
                    OPT_LEVEL = 0;
                }

                if ( ARGS[i].equals("-e")) {
                    EXECUTABLE = ARGS[++i];
                }
            }

            return true;

        } else {
            switch ( ARGS.length ) {
            case 0:
                p( "error:  specify location of JavaScript "+
                "tests" );
                return false;
            case 1:
                p( "error:  specify location of JavaScript "+
                "HELPER_FUNCTIONS file" );
                return false;
            case 2:
                this.TEST_DIRECTORY = ARGS[0].endsWith(File.separator)
                ? new File( ARGS[0] )
                : new File( ARGS[0] + File.separator );
                this.HELPER_FUNCTIONS = new File( ARGS[1] );
                if ( ! ( this.TEST_DIRECTORY ).isDirectory() ) {
                    p( "error:  " +
                    this.TEST_DIRECTORY.getAbsolutePath() +
                    " is not a directory." );
                    return false;
                }
                if ( ! (this.HELPER_FUNCTIONS ).isFile() ) {
                    p( "error:  "+ 
                        this.HELPER_FUNCTIONS.getAbsolutePath()+
                        " file not found." );
                    return false;
                }
                return true;
            default:
                p( "could not understand arguments." );
                return false;
            }
        }
         
    }

    public synchronized void executeSuite( TestSuite suite ) {
        p( "LiveConnectDrv.executeSuite " + suite.name );
        TestEnvironment context;
        TestFile file;
        
        if ( EXECUTABLE != null ) {
            generateHelperFile();
        }
        
        for ( int i = 0; i < suite.size(); i++ ) {
            synchronized ( suite ) {
                file = (TestFile) suite.elementAt( i );

                p( file.name );
                
//                if ( EXECUTABLE != null ) {
                    context = new LiveConnectEnv( file, suite, this );
//                } else {
//                    context = new LiveRhinoEnv( file, suite, this );
//                }
                    
                synchronized( context ) {
                    context.runTest();
                
                   /*
                    * The following two lines are used by the other test drivers,
                    * but are not used by LiveConnectDrv, since each 
                    * LiveConnecTest writes its results to these log files.
                    */
                
                // writeFileResult( file, suite, OUTPUT_DIRECTORY );
                // writeCaseResults(file, suite, OUTPUT_DIRECTORY );
                
                    context.close();
                    context = null;

                    if ( ! file.passed ) {
                        suite.passed = false;
                    }
                }
            }
        }
        writeSuiteResult( suite, OUTPUT_DIRECTORY );
        writeSuiteSummary( suite, OUTPUT_DIRECTORY );
    }
    
    public void generateHelperFile() {
        try {
            this.HELPER_FUNCTIONS = new File( OUTPUT_DIRECTORY, "helper.js" );
               
            p( "HELPER FUNCTIONS FILE IS " + HELPER_FUNCTIONS );
            FileOutputStream fos = new FileOutputStream( HELPER_FUNCTIONS );    
            
            fos.write( ("var OUTPUT_DIRECTORY = \"" + OUTPUT_DIRECTORY + "\";").getBytes());
            fos.write( ("var OUTPUT_FILE = arguments[1];").getBytes() );
            fos.write( ("var TestClassName = arguments[0];" ).getBytes());
            fos.write( ("var TestClass = eval( TestClassName );" ).getBytes());
            fos.write( ("var testclass = new TestClass();" ).getBytes());
            fos.write( ("testclass.run();").getBytes() );
            fos.write( ("quit();" ).getBytes());
            fos.close();
            
        } catch ( Exception e ) {
            p( "generateHelperFile threw " + e.toString() );
            e.printStackTrace();
        }            
    }        

    /**
     *  For each class file in the suite directory, create a TestFile object.
     *  For LiveConnect, the test class name needs to be the name of the class
     *  and the file path needs to be the full package name, prepended with the 
     *  JavaScript keyword "Packages", and without the ".class" suffix.
     */

    public void getCases( TestSuite suite ) {
        enablePrivileges();
        
        File dir = new File ( suite.filePath );
        String[] files      = dir.list();
        
        // XXX hardcoded package name.  need to fix this.
        
        String filename = "Packages.com.netscape.javascript.qa.liveconnect." + 
            suite.name +".";

        for ( int i = 0; i < files.length; i++ ) {
            if ( files[i].endsWith( getSuffix() )) {
                TestFile item = new TestFile( files[i],
                    filename + (files[i].substring(0,files[i].length() - 
                    getSuffix().length())));
                    
                 p( item.filePath );                    
                suite.addElement(item);
            }                
        }
    }    
}    
