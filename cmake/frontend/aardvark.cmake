################################################################################
## web interface build
################################################################################

find_package(Nodejs 16 REQUIRED)
find_package(Yarn REQUIRED)

set(FRONTEND_SRC
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/arango/templateEngine.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/arango/arango.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/_automaticRetryCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/_paginatedCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoClusterStatisticsCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoDatabase.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoLogs.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoReplication.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoStatisticsCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoStatisticsDescriptionCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/clusterCoordinators.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/clusterServers.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/coordinatorCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/foxxRepository.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/graphCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/notificationCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/queryManagementCollectionActive.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/queryManagementCollectionSlow.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoCollections.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoDocument.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoDocuments.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoMetrics.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoQueries.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/foxxCollection.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/collections/arangoUsers.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/config/dygraphConfig.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/documentation/documentation.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/bootstrap-min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/bootstrap-pagination.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/dygraph-combined.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/highlight.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/joi-browser.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/jquery-2.1.0.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/jquery-ui-1.9.2.custom.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/jquery.csv.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/jquery.form.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/jquery.uploadfile.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/moment.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/numeral.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/pretty-bytes.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/randomColor.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/raphael.icons.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/raphael.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.autoCurve.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.curve.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.dashed.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.dotted.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.labels.curve.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.labels.curvedArrow.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.labels.def.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.canvas.edges.tapered.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.exporters.image.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.layout.fruchtermanReingold.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.layout.noverlap.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.plugins.animate.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.plugins.dragNodes.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.plugins.filter.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.plugins.fullScreen.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.plugins.lasso.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/sigma.renderers.halo.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/tile.stamen.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/wheelnav.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/wheelnav.slicePath.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/select2.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/lib/leaflet.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoDatabase.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoDocument.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoQuery.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoReplication.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoSession.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoStatistics.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoStatisticsDescription.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/clusterCoordinator.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/clusterServer.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/coordinator.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/currentDatabase.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/metricModel.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/newArangoLog.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/notification.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/queryManagement.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/graph.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoUsers.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/userConfig.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/foxx.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/foxxRepoModel.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/models/arangoCollectionModel.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/routers/versionCheck.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/routers/router.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/routers/startApp.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/_paginationView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/dbSelectionView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/filterSelect.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/foxxActiveListView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/foxxRepoView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/helpUsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/installGitHubServiceView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/installNewServiceView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/installServiceView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/installUploadServiceView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/installUrlServiceView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/metricsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/nodeView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/scaleView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/statisticBarView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/storeDetailView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/tableView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/applierView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/computedValuesView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/infoView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/spotlightView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/supportView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/validationView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/graphSettingsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/userManagementView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/userPermissions.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/applicationDetailView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/applicationsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/dashboardView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/foxxActiveView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/loggerView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/maintenanceView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/navigationView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/nodesView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/notificationView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/queryManagementView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/rebalanceShardsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/shardDistributionView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/shardsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/showClusterView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/userView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/modalView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/progressView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/replicationView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/clusterView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/loginView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/graphViewer.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/documentView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/documentsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/collectionsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/databaseView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/graphManagementView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/settingsView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/userBarView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/collectionsItemView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/js/views/queryView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/frontend/ttf/arangofont/ie7/ie7.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/generator/index.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/ace.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/ace.min.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/ext-spellcheck.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/ext-static_highlight.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/ext-textarea.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/keybinding-emacs.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/keybinding-vim.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/mode-json.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/theme-jsoneditor.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/theme-textmate.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/worker-javascript.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/worker-json.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/public/assets/src/mode-aql.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/src/views/shards/ShardsReactView.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/src/serviceWorker.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/src/setupProxy.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/src/App.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/config-overrides.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/cluster.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/foxxes.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/index.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/statistics.js
  ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/aardvark.js
  )

add_custom_target(frontend ALL
  DEPENDS ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/build
)
add_custom_command(# frontend
  COMMENT "create frontend build"
  OUTPUT ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/build
  DEPENDS ${FRONTEND_SRC}
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react
  COMMAND yarn install
  COMMAND yarn build
)

add_custom_target(frontend_clean
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/js/apps/system/_admin/aardvark/APP/react/node_modules
  COMMENT "Removing frontend node modules"
  )

