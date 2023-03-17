namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief external process stati of processes exited while being monitored
////////////////////////////////////////////////////////////////////////////////

extern std::map<TRI_pid_t, ExternalProcessStatus> ExitedExternalProcessStatus;


extern ExternalProcessStatus const *getHistoricStatus(TRI_pid_t pid);

////////////////////////////////////////////////////////////////////////////////
/// @brief external process being monitored
////////////////////////////////////////////////////////////////////////////////

extern std::vector<ExternalId> monitoredProcesses;

extern void addMonitorPID(ExternalId& pid);
extern void removeMonitorPID(ExternalId const& pid);
extern void launchMonitorThread(arangodb::application_features::ApplicationServer& server);
extern void terminateMonitorThread(arangodb::application_features::ApplicationServer& server);

}
