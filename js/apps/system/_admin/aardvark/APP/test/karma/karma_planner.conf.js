// Karma configuration
// Generated on Thu Jul 04 2013 11:39:34 GMT+0200 (CEST)

module.exports = function(karma) {

  karma.set({

    // base path, that will be used to resolve files and exclude
    basePath: '../../',


    // frameworks to use
    //frameworks: ['jasmine', 'junit-reporter'],
    frameworks: ['jasmine'],

    // list of files / patterns to load in the browser
    files: [
      'test/lib/jasmine-1.3.1/jasmine-html.js',
      'test/lib/jshint.js',

      'frontend/js/lib/jquery-1.8.3.js',
      'frontend/js/lib/jquery-ui-1.9.2.custom.js',
      'frontend/js/lib/jquery.dataTables.js',
      'frontend/js/lib/jquery.autogrow.js',
      'frontend/js/lib/jquery.jeditable.js',
      'frontend/js/lib/jquery.jeditable.autogrow.js',
      'frontend/js/lib/jquery.snippet.js',
      'frontend/js/lib/jquery.gritter.js',
      'frontend/js/lib/jquery.slideto.min.js',
      'frontend/js/lib/jquery.wiggle.min.js',
      'frontend/js/lib/jquery.ba-bbq.min.js',
      'frontend/js/lib/jquery.contextmenu.js',
      'frontend/js/lib/handlebars-1.0.rc.1.js',
      'frontend/js/lib/underscore.js',
      'frontend/js/lib/backbone.js',
      'frontend/js/lib/d3.v3.min.js',
      'frontend/js/lib/nv.d3.js',
      'frontend/js/lib/d3.fisheye.js',
      'frontend/js/lib/ejs_0.9_alpha_1_production.js',
      'frontend/js/lib/ColVis.js',
      'frontend/js/lib/bootstrap.js',
      'frontend/js/lib/bootstrap-pagination.js',
      'frontend/src/ace.js',
      'frontend/js/lib/jqconsole.min.js',
      'frontend/js/lib/splitter.js',
      'frontend/js/lib/swagger.js',
      'frontend/js/lib/swagger-ui.js',
      'frontend/js/lib/highlight.7.3.pack.js',

      // arangodb
      'frontend/js/arango/arango.js',
      'frontend/js/arango/templateEngine.js',
      'frontend/js/shell/browser.js',
      'frontend/js/modules/@arangodb/arango-collection-common.js',
      'frontend/js/modules/@arangodb/arango-collection.js',
      'frontend/js/modules/@arangodb/arango-database.js',
      'frontend/js/modules/@arangodb/arango-query-cursor.js',
      'frontend/js/modules/@arangodb/arango-statement-common.js',
      'frontend/js/modules/@arangodb/arango-statement.js',
      'frontend/js/modules/@arangodb/arangosh.js',
      'frontend/js/modules/@arangodb/graph-common.js',
      'frontend/js/modules/@arangodb/graph.js',
      'frontend/js/modules/@arangodb/mimetypes.js',
      'frontend/js/modules/@arangodb/simple-query-common.js',
      'frontend/js/modules/@arangodb/simple-query.js',
      'frontend/js/modules/@arangodb/aql/functions.js',
      'frontend/js/modules/@arangodb/graph/traversal.js',
      'frontend/js/modules/@arangodb/common.js',
      'frontend/js/modules/@arangodb/index.js',
      'frontend/js/bootstrap/errors.js',
      'frontend/js/bootstrap/module-internal.js',
      'frontend/js/client/bootstrap/module-internal.js',
      'frontend/js/client/client.js',

      // Mocks
      'test/mocks/disableEJS.js',
      'test/specs/graphViewer/helper/eventHelper.js',
      'test/specs/graphViewer/helper/objectsHelper.js',
      'test/specs/graphViewer/helper/mocks.js',
      'test/specs/graphViewer/helper/commMock.js',
      'test/specs/graphViewer/helper/uiMatchers.js',



      // GraphViewer
      // Core Modules
      'frontend/js/graphViewer/graphViewer.js',
      'frontend/js/graphViewer/graph/domObserverFactory.js',
      'frontend/js/graphViewer/graph/colourMapper.js',
      'frontend/js/graphViewer/graph/communityNode.js',
      'frontend/js/graphViewer/graph/webWorkerWrapper.js',
      'frontend/js/graphViewer/graph/nodeShaper.js',
      'frontend/js/graphViewer/graph/abstractAdapter.js',
      'frontend/js/graphViewer/graph/JSONAdapter.js',
      'frontend/js/graphViewer/graph/arangoAdapter.js',
      'frontend/js/graphViewer/graph/foxxAdapter.js',
      'frontend/js/graphViewer/graph/previewAdapter.js',
      'frontend/js/graphViewer/graph/edgeShaper.js',
      'frontend/js/graphViewer/graph/forceLayouter.js',
      'frontend/js/graphViewer/graph/eventDispatcher.js',
      'frontend/js/graphViewer/graph/eventLibrary.js',
      'frontend/js/graphViewer/graph/zoomManager.js',
      'frontend/js/graphViewer/graph/nodeReducer.js',
      'frontend/js/graphViewer/graph/modularityJoiner.js',

      // UI Modules
      'frontend/js/graphViewer/ui/modalDialogHelper.js',
      'frontend/js/graphViewer/ui/contextMenuHelper.js',
      'frontend/js/graphViewer/ui/nodeShaperControls.js',
      'frontend/js/graphViewer/ui/edgeShaperControls.js',
      'frontend/js/graphViewer/ui/arangoAdapterControls.js',
      'frontend/js/graphViewer/ui/layouterControls.js',
      'frontend/js/graphViewer/ui/uiComponentsHelper.js',
      'frontend/js/graphViewer/ui/eventDispatcherControls.js',
      'frontend/js/graphViewer/ui/graphViewerUI.js',
      'frontend/js/graphViewer/ui/graphViewerWidget.js',
      'frontend/js/graphViewer/ui/graphViewerPreview.js',

      // Models
      'frontend/js/models/currentDatabase.js',
      '../../frontend/js/models/arangoCollectionModel.js',
      'frontend/js/models/arangoDatabase.js',
      'frontend/js/models/arangoDocument.js',
      'frontend/js/models/arangoStatistics.js',
      'frontend/js/models/arangoStatisticsDescription.js',
      'frontend/js/models/foxx.js',
      'frontend/js/models/graph.js',
      'frontend/js/models/clusterServer.js',
      'frontend/js/models/clusterCoordinator.js',
      'frontend/js/models/clusterDatabase.js',
      'frontend/js/models/clusterCollection.js',
      'frontend/js/models/clusterShard.js',

      // Collections
      'frontend/js/collections/arangoCollections.js',
      'frontend/js/collections/arangoDocuments.js',
      'frontend/js/collections/arangoDocument.js',
      'frontend/js/collections/arangoDatabase.js',
      'frontend/js/collections/arangoSession.js',
      'frontend/js/collections/arangoStatisticsCollection.js',
      'frontend/js/collections/arangoStatisticsDescriptionCollection.js',
      'frontend/js/collections/foxxCollection.js',
      'frontend/js/collections/graphCollection.js',
      'frontend/js/collections/clusterServers.js',
      'frontend/js/collections/clusterCoordinators.js',
      'frontend/js/collections/clusterDatabases.js',
      'frontend/js/collections/clusterCollections.js',
      'frontend/js/collections/clusterShards.js',

      // Views
      'frontend/js/views/navigationView.js',
      'frontend/js/views/apiView.js',
      'frontend/js/views/footerView.js',
      'frontend/js/views/queryView.js',
      'frontend/js/views/shellView.js',
      'frontend/js/views/dashboardView.js',
      'frontend/js/views/collectionsView.js',
      'frontend/js/views/collectionsItemView.js',
      'frontend/js/views/documentsView.js',
      'frontend/js/views/documentView.js',
      'frontend/js/views/documentSourceView.js',
      'frontend/js/views/applicationsView.js',
      'frontend/js/views/foxxActiveView.js',
      'frontend/js/views/foxxActiveListView.js',
      'frontend/js/views/foxxInstalledListView.js',
      'frontend/js/views/foxxEditView.js',
      'frontend/js/views/foxxMountView.js',
      'frontend/js/views/appDocumentationView.js',
      'frontend/js/views/graphManagementView.js',
      'frontend/js/views/addNewGraphView.js',
      'frontend/js/views/deleteGraphView.js',
      'frontend/js/views/dbSelectionView.js',
      'frontend/js/views/editListEntryView.js',
      'frontend/js/views/loginView.js',
      'frontend/js/views/clusterDashboardView.js',
      'frontend/js/views/clusterOverviewView.js',
      'frontend/js/views/clusterServerView.js',
      'frontend/js/views/clusterCoordinatorView.js',
      'frontend/js/views/clusterDatabaseView.js',
      'frontend/js/views/clusterCollectionView.js',
      'frontend/js/views/clusterShardsView.js',

      //Views Planner
      'clusterFrontend/js/views/planScenarioSelectorView.js',
      'clusterFrontend/js/views/planSymmetricView.js',
      'clusterFrontend/js/views/planTestView.js',


      // Router
      'frontend/js/routers/router.js',
      'plannerFrontend/js/routers/plannerRouter.js',

      //Templates
      {pattern: 'frontend/js/templates/*.ejs', served:true, included:false, watched: true},
      {pattern: 'plannerFrontend/js/templates/*.ejs', served:true, included:false, watched: true},
      // Specs
      // GraphViewer
      //Router
      'test/specs/planner/router/routerSpec.js',
      //View
      'test/specs/planner/views/planSymmetricViewSpec.js',
      'test/specs/planner/views/planTestViewSpec.js',
      'test/specs/planner/views/planScenarioSelectorViewSpec.js'

    ],


    // list of files to exclude
    exclude: [

    ],

    // test results reporter to use
    // possible values: 'dots', 'progress', 'junit', 'growl', 'coverage'
    reporters: ['dots'],

    // web server port
    port: 9876,


    // cli runner port
    runnerPort: 9100,

    // enable / disable colors in the output (reporters and logs)
    colors: false,


    // level of logging
    // possible values: karma.LOG_DISABLE || karma.LOG_ERROR || karma.LOG_WARN || karma.LOG_INFO || karma.LOG_DEBUG
    logLevel: karma.LOG_INFO,


    // enable / disable watching file and executing tests whenever any file changes
    autoWatch: false,


    // Start these browsers, currently available:
    // - Chrome
    // - ChromeCanary
    // - Firefox
    // - Opera
    // - Safari (only Mac)
    // - PhantomJS
    // - IE (only Windows)
    browsers: ["PhantomJS", "Firefox"],

    // If browser does not capture in given timeout [ms], kill it
    captureTimeout: 60000,


    // Continuous Integration mode
    // if true, it capture browsers, run tests and exit
    singleRun: true
  });
};
