HTTP Interface for Administration and Monitoring {#HttpSystem}
==============================================================

@NAVIGATE_HttpSystem
@EMBEDTOC{HttpSystemTOC}

This is an introduction to ArangoDB's Http interface for administration and
monitoring of the server.

@anchor HttpSystemLog
@copydetails triagens::admin::RestAdminLogHandler::execute

@CLEARPAGE
@anchor HttpSystemStatus
@copydetails JSF_GET_admin_status

@CLEARPAGE
@anchor HttpSystemFlushServerModules
@copydetails JSF_GET_admin_modules_flush

@CLEARPAGE
@anchor HttpSystemRoutingReloads
@copydetails JSF_GET_admin_routing_reloads

@CLEARPAGE
@anchor HttpSystemConnectionStatistics
@copydetails triagens::arango::ConnectionStatisticsHandler::compute

@CLEARPAGE
@anchor HttpSystemRequestStatistics
@copydetails triagens::arango::RequestStatisticsHandler::compute
