////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock logFacility
/// @brief log facility
/// `--log.facility name`
///
/// If this option is set, then in addition to output being directed to the
/// standard output (or to a specified file, in the case that the command line
/// log.file option was set), log output is also sent to the system logging
/// facility. The *arg* is the system log facility to use. See syslog for
/// further details.
///
/// The value of *arg* depends on your syslog configuration. In general it
/// will be *user*. Fatal messages are mapped to *crit*, so if *arg*
/// is *user*, these messages will be logged as *user.crit*.  Error
/// messages are mapped to *err*.  Warnings are mapped to *warn*.  Info
/// messages are mapped to *notice*.  Debug messages are mapped to
/// *info*.  Trace messages are mapped to *debug*.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////