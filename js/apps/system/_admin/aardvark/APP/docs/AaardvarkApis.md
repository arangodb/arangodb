# Route Overview - Aardvark Web Interface

This document provides a comprehensive overview of all routes defined in the Aardvark web interface, their descriptions, and code locations, grouped by their migration destination.

---

## ✅ Completed - Moved to UI

### Cluster Routes (Replaced with `/_admin/cluster/health` API)

#### 10. GET /cluster/amICoordinator ✅
- **Description**: Returns true if and only if the current server is a coordinator node.
- **Original Location**: `cluster.js:44-48` (deleted)
- **Solution**: Now uses `frontendConfig.isCluster` directly. The `isCoordinator()` function in `arango.js` returns this value.
- **Completed**: Branch `FE-932-cluster-health`

#### 11. GET /cluster/DBServers ✅
- **Description**: Returns a list of all running and expected DBServers within the cluster.
- **Original Location**: `cluster.js:51-71` (deleted)
- **Solution**: `ClusterServers` collection now uses `arangoHelper.getHealthModels('DBServer')` which reads from cached `window.App.lastHealthCheckResult.Health` data.
- **Completed**: Branch `FE-932-cluster-health`

#### 12. GET /cluster/Coordinators ✅
- **Description**: Returns a list of all coordinators in the cluster.
- **Original Location**: `cluster.js:73-91` (deleted)
- **Solution**: `ClusterCoordinators` and `CoordinatorCollection` now use `arangoHelper.getHealthModels('Coordinator')` which reads from cached health data.
- **Completed**: Branch `FE-932-cluster-health`

### Job Tracking Routes (Replaced with direct `_frontend` collection access)

#### 5. POST /job ✅
- **Description**: Stores a job ID entry in the _frontend collection for tracking running jobs.
- **Original Location**: `aardvark.js:355-369` (deleted)
- **Solution**: Index operations now use the built-in progress indicator. Job tracking code removed from collections UI.
- **Completed**: Branch `FE-932-cluster-health`

#### 6. DELETE /job ✅
- **Description**: Deletes all job entries from the _frontend collection.
- **Original Location**: `aardvark.js:371-399` (deleted)
- **Solution**: No longer needed - index operations have built-in progress tracking.
- **Completed**: Branch `FE-932-cluster-health`

#### 7. DELETE /job/:id ✅
- **Description**: Deletes a specific job entry by ID from the _frontend collection.
- **Original Location**: `aardvark.js:401-426` (deleted)
- **Solution**: No longer needed - index operations have built-in progress tracking.
- **Completed**: Branch `FE-932-cluster-health`

#### 8. GET /job ✅
- **Description**: Returns all job IDs of currently running jobs from the _frontend collection.
- **Original Location**: `aardvark.js:428-440` (deleted)
- **Solution**: No longer needed - index operations have built-in progress tracking.
- **Completed**: Branch `FE-932-cluster-health`

### Query Routes (Moved to client-side)

#### 15. POST /query/result/download ✅
- **Description**: Executes a query and downloads the results as a JSON file attachment.
- **Original Location**: `aardvark.js:313-328` (deleted)
- **Solution**: Now uses `downloadLocalData()` helper in `QueryExecuteResult.tsx` to download query results directly from UI state. No server round-trip needed.
- **Completed**: Branch `FE-936-query-download-client-side`

---

## ⏳ Pending - Needs to move to Core

### Main Application Routes

#### 1. GET /config.js
- **Description**: Returns frontend configuration as JavaScript. Includes database name, enterprise status, authentication settings, cluster info, engine type, statistics/metrics flags, Foxx settings, replication factors, and other system configuration.
- **Code Location**: `aardvark.js:69-102`
- **Action**: Look in detail what is needed, find alternatives if necessary.

#### 2. POST /query/debugDump
- **Description**: Creates a debug output for a query in a zip file. Includes query plan, anonymized test data, and collection information. Useful for submitting query-based issues to the ArangoDB team.
- **Code Location**: `aardvark.js:200-246`
- **Action**: Needs to move to Core, move to DebugContainer

---

## ⏳ Pending - Needs to move to Core (Swagger/API Documentation)

### Main Application Routes

#### 13. GET /api/*
- **Description**: Mounts the system API documentation (Swagger/OpenAPI). Serves the API documentation JSON from api-docs.json.
- **Code Location**: `aardvark.js:125-135`
- **Action**: Needs to move to core, swagger is part of core build process

---

## ⏳ Pending - Needs to move to UI

### Main Application Routes

#### 14. POST /graph-examples/create/:name
- **Description**: Creates one of the predefined example graphs (knows_graph, social, routeplanner, traversalGraph, kShortestPathsGraph, mps_graph, worldCountry, connectedComponentsGraph).
- **Code Location**: `aardvark.js:313-336`
- **Action**: Needs to move to UI

#### 16. GET /graphs-v2/:name
- **Description**: Returns vertices and edges for a specific graph in an optimized format for visualization. Enhanced version with better performance, support for multiple attributes, and improved layout configurations. Supports various graph visualization options.
- **Code Location**: `aardvark.js:1675-1845`
- **Action**: Move to UI

---

## ⏳ Pending - Replaced by setup of the UI

### Main Application Routes

#### 17. GET /index.html
- **Description**: Serves the main HTML file for the web interface. Supports gzip encoding if the client accepts it.
- **Code Location**: `aardvark.js:54-67`
- **Action**: Replaced by setup of the UI

---

## ⏳ Pending - Needs to move to New BringYourOwnCode Service, or be deprecated and removed

### Foxx Service Routes

#### 18. PUT /foxxes/store?mount=...
- **Description**: Downloads a Foxx service from the store and installs it at the given mount point. Requires mount query parameter (URL-encoded).
- **Code Location**: `foxxes.js:194-204`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 19. PUT /foxxes/git?mount=...
- **Description**: Installs a Foxx service from GitHub. Takes user/repository and version (defaults to master). Requires mount query parameter.
- **Code Location**: `foxxes.js:206-217`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 20. PUT /foxxes/url?mount=...
- **Description**: Installs a Foxx service from a direct URL. Requires mount query parameter.
- **Code Location**: `foxxes.js:219-229`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 21. PUT /foxxes/generate?mount=...
- **Description**: Generates a new empty Foxx service based on a generator configuration and installs it at the given mount point. Requires mount query parameter.
- **Code Location**: `foxxes.js:231-252`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 22. PUT /foxxes/zip?mount=...
- **Description**: Installs a Foxx service from a temporary zip file path (created via _api/upload). Requires mount query parameter.
- **Code Location**: `foxxes.js:254-271`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 23. PUT /foxxes/raw?mount=...
- **Description**: Installs a Foxx service from a direct upload (raw request body). Requires mount query parameter.
- **Code Location**: `foxxes.js:273-279`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 24. DELETE /foxxes?mount=...
- **Description**: Uninstalls the Foxx service at the given mount point. Optionally runs teardown. Requires mount query parameter.
- **Code Location**: `foxxes.js:281-297`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 25. GET /foxxes
- **Description**: Returns a list of all installed Foxx services with their metadata (mount, name, description, author, version, configuration, dependencies, scripts, etc.).
- **Code Location**: `foxxes.js:299-320`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 26. GET /foxxes/thumbnail?mount=...
- **Description**: Returns the thumbnail image for a specific Foxx service. Falls back to default thumbnail if none is available. Requires mount query parameter.
- **Code Location**: `foxxes.js:322-330`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 27. GET /foxxes/config?mount=...
- **Description**: Returns the configuration options for a specific Foxx service. Requires mount query parameter.
- **Code Location**: `foxxes.js:332-340`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 28. GET /foxxes/deps?mount=...
- **Description**: Returns the dependency options for a specific Foxx service. Requires mount query parameter.
- **Code Location**: `foxxes.js:342-361`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 29. PATCH /foxxes/config?mount=...
- **Description**: Updates the configuration options for a specific Foxx service. Requires mount query parameter.
- **Code Location**: `foxxes.js:363-377`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 30. PATCH /foxxes/deps?mount=...
- **Description**: Updates the dependency options for a specific Foxx service. Requires mount query parameter.
- **Code Location**: `foxxes.js:379-393`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 31. POST /foxxes/tests?mount=...
- **Description**: Runs the tests for a specific Foxx service. Requires mount query parameter.
- **Code Location**: `foxxes.js:395-403`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 32. POST /foxxes/scripts/:name?mount=...
- **Description**: Executes a specific script for a Foxx service. Requires mount query parameter and script name path parameter.
- **Code Location**: `foxxes.js:405-419`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 33. PATCH /foxxes/devel?mount=...
- **Description**: Toggles development mode for a Foxx service (activates or deactivates). Requires mount query parameter.
- **Code Location**: `foxxes.js:421-430`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 34. GET /foxxes/fishbowl
- **Description**: Returns a list of all Foxx services available in the Foxx store (fishbowl). Updates the store from GitHub before returning results.
- **Code Location**: `foxxes.js:432-447`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 35. GET /foxxes/docs/standalone?mount=...
- **Description**: Documentation router for standalone Foxx service documentation. Requires mount query parameter. Supports swagger.json suffix.
- **Code Location**: `foxxes.js:450-457`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

#### 36. GET /foxxes/docs?mount=...
- **Description**: Documentation router for Foxx service documentation with index file. Requires mount query parameter. Supports swagger.json suffix.
- **Code Location**: `foxxes.js:459-467`
- **Action**: This whole section needs to move to the New BringYourOwnCode Service, or be deprecated and removed

---

## ⏳ Pending - To be deleted

### Main Application Routes

#### 3. POST /query/upload/:user
- **Description**: Uploads and imports user queries for a specific user. Adds queries to the user's extra.queries array if they don't already exist.
- **Code Location**: `aardvark.js:248-288`
- **Action**: Delete, not used anymore

#### 4. GET /query/download/:user
- **Description**: Downloads and exports all stored queries for a given username as a JSON file.
- **Code Location**: `aardvark.js:290-311`
- **Action**: Delete, not used anymore

#### 9. GET /replication/mode
- **Description**: Returns the replication mode of the server. Modes: 0 (no active replication), 1 (replication per database), 2 (replication per server). Also returns the role (leader/follower/null). Only accessible from _system database.
- **Code Location**: `aardvark.js:362-436`
- **Action**: Delete, part of Active Failover, removed with 3.12.0

#### 37. GET /whoAmI
- **Description**: Returns the current authenticated user or null if not authenticated. Returns false if authentication is disabled.
- **Code Location**: `aardvark.js:104-111`
- **Action**: Delete, read from JWT token instead

#### 38. POST /query/profile
- **Description**: Profiles a query with detailed execution information. Returns a user-friendly profiling result.
- **Code Location**: `aardvark.js:137-167`
- **Action**: Delete, replace with new API

#### 39. POST /query/explain
- **Description**: Explains a query execution plan in a more user-friendly way than the standard query_api/explain endpoint.
- **Code Location**: `aardvark.js:169-198`
- **Action**: Delete, replace with new API (`_api/explain`)

#### 40. GET /graph/:name
- **Description**: Returns vertices and edges for a specific graph in a format suitable for visualization. Supports various configuration options for node/edge labels, colors, sizes, and traversal depth. This is the legacy version.
- **Code Location**: `aardvark.js:438-917`
- **Action**: Delete (replaced by /graphs-v2/:name)

### Statistics Routes

#### 41. GET /statistics/coordshort
- **Description**: Returns short-term statistics history for all coordinators. Merges statistics from multiple coordinators. Returns enabled status and merged data.
- **Code Location**: `statistics.js:406-424`
- **Action**: This section will be deleted

#### 42. GET /statistics/short
- **Description**: Returns short-term statistics history for a specific server or all servers. Includes various metrics like request times, bytes sent/received, HTTP method counts, system metrics, and distribution data. Supports start time offset and DB server cluster ID query parameters.
- **Code Location**: `statistics.js:426-436`
- **Action**: This section will be deleted

---

## Summary

| Category | Total | Completed | Pending |
|----------|-------|-----------|---------|
| Needs to move to Core | 2 | 0 | 2 |
| Needs to move to Core (Swagger) | 1 | 0 | 1 |
| Needs to move to UI | 7 | 5 | 2 |
| Replaced by setup of the UI | 1 | 0 | 1 |
| BringYourOwnCode Service | 19 | 0 | 19 |
| To be deleted | 12 | 4 | 8 |
| **Total** | **42** | **9** | **33** |

### Progress: 9/42 routes completed (21%)

All routes require authentication (except where explicitly noted) when authentication is enabled in the system.
