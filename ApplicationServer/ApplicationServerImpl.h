////////////////////////////////////////////////////////////////////////////////
/// @brief application server implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_APPLICATION_SERVER_APPLICATION_SERVER_IMPL_H
#define TRIAGENS_APPLICATION_SERVER_APPLICATION_SERVER_IMPL_H 1

#include <Basics/Common.h>

#include <Basics/ProgramOptions.h>
#include <Basics/ProgramOptionsDescription.h>
#include <Rest/ApplicationServer.h>

////////////////////////////////////////////////////////////////////////////////
/// @page CommandLineLoggingTOC
///
/// <ol>
///  <li>General Logging Options</li>
///   <ol>
///    <li>log.file</li>
///    <li>log.severity</li>
///    <li>log.syslog</li>
///   </ol>
///  <li>Human Readable Logging</li>
///   <ol>
///    <li>log.level</li>
///    <li>log.line-number</li>
///    <li>log.prefix</li>
///    <li>log.thread</li>
///   </ol>
///  <li>Machine Readable Logging</li>
///   <ol>
///    <li>log.application</li>
///    <li>log.facility</li>
///    <li>log.format</li>
///    <li>log.hostname</li>
///   </ol>
/// </ol>
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page CommandLineLogging Command-Line Options for Logging
///
/// There are two different kinds of logs. Human-readable logs and
/// machine-readable logs. The human-readable logs are used to provide an
/// administration with information about the server. The machine-readable logs
/// are used to provide statistics about executed requests and timings about
/// computation steps.
///
/// <hr>
/// @copydoc CommandLineLoggingTOC
///
/// @section GeneralLogging General Logging Options
///
/// @copydetails triagens::rest::ApplicationServerImpl::logFile
///
/// @copydetails triagens::rest::ApplicationServerImpl::logSeverity
///
/// @copydetails triagens::rest::ApplicationServerImpl::logSyslog
///
/// @section HumanReadableLogging Human Readable Logging
///
/// @copydetails triagens::rest::ApplicationServerImpl::logLevel
///
/// @copydetails triagens::rest::ApplicationServerImpl::logLineNumber
///
/// @copydetails triagens::rest::ApplicationServerImpl::logPrefix
///
/// @copydetails triagens::rest::ApplicationServerImpl::logThreadId
///
/// @section MaschineReadableLogging Maschine Readable Logging
///
/// @copydetails triagens::rest::ApplicationServerImpl::logApplicationName
///
/// @copydetails triagens::rest::ApplicationServerImpl::logFacility
///
/// @copydetails triagens::rest::ApplicationServerImpl::logFormat
///
/// @copydetails triagens::rest::ApplicationServerImpl::logHostName
///
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief application server implementation
    ////////////////////////////////////////////////////////////////////////////////

    class ApplicationServerImpl : virtual public ApplicationServer {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        ApplicationServerImpl (string const& description, string const& version);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructor
        ////////////////////////////////////////////////////////////////////////////////

        ~ApplicationServerImpl ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void addFeature (ApplicationFeature*);

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setSystemConfigFile (string const& name) {
          systemConfigFile = name;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setUserConfigFile (string const& name) {
          userConfigFile = name;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        basics::ProgramOptions& programOptions () {
          return options;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        vector<string> programArguments () {
          return arguments;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        bool parse (int argc, char* argv[], map<string, basics::ProgramOptionsDescription>);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void start ();

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void wait ();

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void beginShutdown ();

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void shutdown ();

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief add options to description
        ////////////////////////////////////////////////////////////////////////////////

        virtual void setupOptions (map<string, basics::ProgramOptionsDescription>&);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief parsing phase 1, before config file
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool parsePhase1 ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief parsing phase 2, after config file
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool parsePhase2 ();

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief program options
        ////////////////////////////////////////////////////////////////////////////////

        basics::ProgramOptions options;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief defined features
        ////////////////////////////////////////////////////////////////////////////////

        vector<ApplicationFeature*> features;

      private:
        void setupLogging ();
        bool readConfigurationFile ();
        void dropPriviliges ();

      private:
        string title;
        string version;

        string initFile;
        string userConfigFile;
        string systemConfigFile;

        string uid;
        string gid;

////////////////////////////////////////////////////////////////////////////////
/// @brief log application name
///
/// @CMDOPT{--log.application @CA{name}}
///
/// Specifies the @CA{name} of the application which should be logged if this item of
/// information is to be logged.
////////////////////////////////////////////////////////////////////////////////

        string logApplicationName;

////////////////////////////////////////////////////////////////////////////////
/// @brief log host name
///
/// @CMDOPT{--log.hostname @CA{name}}
///
/// Specifies the @CA{name} of the operating environment (the "hostname") which
/// should be logged if this item of information is to be logged. Note that
/// there is no default hostname.
////////////////////////////////////////////////////////////////////////////////

        string logHostName;

////////////////////////////////////////////////////////////////////////////////
/// @brief log facility
///
/// @CMDOPT{--log.facility @CA{name}}
///
/// Specifies the name of the server instance which should be logged if this
/// item of information is to be logged.
////////////////////////////////////////////////////////////////////////////////

        string logFacility;

////////////////////////////////////////////////////////////////////////////////
/// @brief log level
///
/// @CMDOPT{--log.level @CA{level}}
///
/// Allows the user to choose the level of information which is logged by the
/// server. The argument @CA{level} is specified as a string and can be one of
/// the values listed below. Note that, fatal errors, that is, errors which
/// cause the server to terminate, are always logged irrespective of the log
/// level assigned by the user.
///
/// - fatal: Logs errors which cause the server to terminate.
///
/// Fatal errors generally indicate some inconsistency with the manner in which
/// the server has been coded. Fatal errors may also indicate a problem with the
/// platform on which the server is running. Fatal errors always cause the
/// server to terminate. For example,
///
/// @LIT{2010-09-20T07:32:12Z [4742] FATAL a http server has already been created}
///
/// - error: Logs errors which the server has encountered.
///
/// These errors may not necessarily result in the termination of the
/// server. For example,
///
/// @LIT{2010-09-17T13:10:22Z [13967] ERROR strange log level 'errors'\, going to 'warning'}
///
/// - warning: Provides information on errors encountered by the server,
/// which are not necessarily detrimental to it's continued operation.
///
/// For example,
///
/// @LIT{2010-09-20T08:15:26Z [5533] WARNING got corrupted HTTP request 'POS?'}
///
/// Note that, setting the log level to warning will also result in all errors
/// to be logged as well.
///
/// - info: Logs information about the status of the server.
///
/// For example,
///
/// @LIT{2010-09-20T07:40:38Z [4998] INFO SimpleVOC ready for business}
///
/// Note that, setting the log level to info will also result in all errors and
/// warnings to be logged as well.
///
/// - debug: Logs all errors, all warnings and debug information.
///
/// Debug log information is generally useful to find out the state of the
/// server in the case of an error. For example,
///
/// @LIT{2010-09-17T13:02:53Z [13783] DEBUG opened port 7000 for any}
///
/// Note that, setting the log level to debug will also result in all errors,
/// warnings and server status information to be logged as well.
///
/// - trace: As the name suggests, logs information which may be useful to trace
/// problems encountered with using the server.
///
/// For example,
///
/// @LIT{2010-09-20T08:23:12Z [5687] TRACE trying to open port 8000}
///
/// Note that, setting the log level to trace will also result in all errors,
/// warnings, status information, and debug information to be logged as well.
////////////////////////////////////////////////////////////////////////////////

        string logLevel;

////////////////////////////////////////////////////////////////////////////////
/// @brief log format
///
/// @CMDOPT{--log.format @CA{format}}
///
/// @copydetails triagens::basics::Logger::setLogFormat
////////////////////////////////////////////////////////////////////////////////

        string logFormat;

////////////////////////////////////////////////////////////////////////////////
/// @brief log severity
///
/// @CMDOPT{--log.severity @CA{severity}}
///
/// This parameter provides a set of standard log severities which can be
/// used. The current arguments accepted are:
///
/// - exception
/// - technical
/// - functional
/// - development
/// - human
/// - all (human and non-human)
/// - non-human (exception, technical, functional, and development)
///
/// The default is all.
////////////////////////////////////////////////////////////////////////////////

        string logSeverity;

////////////////////////////////////////////////////////////////////////////////
/// @brief log level
///
/// @CMDOPT{--log.file @CA{filename}}
///
/// This option allows the user to specify the name of a file to which
/// information is logged. By default, if no log file is specified, the standard
/// output is used. Note that, if the file named by @CA{filename} does not
/// exist, it will be created. However, if the file already exists, output is
/// appended to the file. Use @LIT{+} to log to standard error. Use @LIT{-} to
/// log to standard output. Use @LIT{""} to disable logging to file.
////////////////////////////////////////////////////////////////////////////////

        string logFile;

////////////////////////////////////////////////////////////////////////////////
/// @brief log prefix
///
/// @CMDOPT{--log.prefix @CA{prefix}}
///
/// This option is used specify an prefix to logged text.
////////////////////////////////////////////////////////////////////////////////

        string logPrefix;

////////////////////////////////////////////////////////////////////////////////
/// @brief log level
///
/// @CMDOPT{--log.syslog @CA{arg}}
///
/// If this option is set, then in addition to output being directed to the
/// standard output (or to a specified file, in the case that the command line
/// log.file option was set), log output is also sent to the system logging
/// facility. The @CA{arg} is the system log facility to use. See syslog for
/// further details.
////////////////////////////////////////////////////////////////////////////////

        string logSyslog;

////////////////////////////////////////////////////////////////////////////////
/// @brief log thread identifier
///
/// @CMDOPT{--log.thread}
///
/// Whenever log output is generated, the process ID is written as part of the
/// log information. Setting this option appends the thread id of the calling
/// thread to the process id. For example,
///
/// @LIT{2010-09-20T13:04:01Z [19355] INFO ready for business}
///
/// when no thread is logged and
///
/// @LIT{2010-09-20T13:04:17Z [19371-18446744072487317056] ready for business}
///
/// when this command line option is set.
////////////////////////////////////////////////////////////////////////////////

        bool logThreadId;

////////////////////////////////////////////////////////////////////////////////
/// @brief log line number
///
/// @CMDOPT{--log.line-number}
///
/// Normally, if an human readable fatal, error, warning or info message is
/// logged, no information about the file and line number is provided. The file
/// and line number is only logged for debug and trace message. This option can
/// be use to always log these pieces of information.
////////////////////////////////////////////////////////////////////////////////

        bool logLineNumber;

        uint32_t randomGenerator;

        basics::ProgramOptionsDescription description;
        basics::ProgramOptionsDescription descriptionFile;

        vector<string> arguments;
    };
  }
}

#endif
