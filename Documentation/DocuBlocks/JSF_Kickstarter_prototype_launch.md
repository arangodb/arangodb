////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_launch
///
/// `Kickstarter.launch()`
///
/// This starts up a cluster as described in the plan which was given to
/// the constructor. To this end, other dispatchers are contacted as
/// necessary. All startup commands for the local dispatcher are
/// executed immediately.
///
/// The result is an object that contains information about the started
/// processes, this object is also stored in the Kickstarter object
/// itself. We do not go into details here about the data structure,
/// but the most important information are the process IDs of the
/// started processes. The corresponding
/// [see shutdown method](../ModulePlanner/README.md#shutdown) needs this 
/// information to shut down all processes.
///
/// Note that all data in the DBservers and all log files and all agency
/// information in the cluster is deleted by this call. This is because
/// it is intended to set up a cluster for the first time. See
/// the [relaunch method](../ModulePlanner/README.md#relaunch)
/// for restarting a cluster without data loss.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////