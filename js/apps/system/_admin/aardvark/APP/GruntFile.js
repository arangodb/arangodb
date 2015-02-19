(function() {
  "use strict";

  module.exports = function(grunt) {

    grunt.initConfig({
      pkg: grunt.file.readJSON('package.json'),

      project: {
        shared: {
          js: [

          ],
          lib: [
            "frontend/js/lib/jquery-2.1.0.min.js",
            "frontend/js/lib/underscore.js",
            "frontend/js/lib/backbone.js",
            "frontend/js/lib/bootstrap.js"
          ]
        },
        standalone: {
          css: [
            "frontend/scss/style.scss"
          ],
          lib: [
            "frontend/js/lib/jquery-ui-1.9.2.custom.js",
            "frontend/js/lib/jquery.snippet.js",
            "frontend/js/lib/jquery.slideto.min.js",
            "frontend/js/lib/jquery.wiggle.min.js",
            "frontend/js/lib/jquery.contextmenu.js",
            "frontend/js/lib/jquery.hotkeys.js",
            "frontend/js/lib/jquery.form.js",
            "frontend/js/lib/jquery.uploadfile.js",
            "frontend/js/lib/select2.min.js",
            "frontend/js/lib/handlebars-1.0.rc.1.js",
            "frontend/js/lib/jsoneditor-min.js",
            "frontend/js/lib/d3.v3.min.js",
            "frontend/js/lib/nv.d3.js",
            "frontend/js/lib/strftime-min.js",
            "frontend/js/lib/dygraph-combined.js",
            "frontend/js/lib/d3.fisheye.js",
            "frontend/js/lib/bootstrap-pagination.js",
            "frontend/js/lib/jqconsole.min.js",
            "frontend/js/lib/swagger.js",
            "frontend/js/lib/swagger-ui.js",
            "frontend/js/lib/highlight.7.3.pack.js",
            "frontend/js/lib/joi.browser.js",
            "frontend/js/lib/md5.js",
            "frontend/src/ace.js",
            "frontend/src/theme-textmate.js",
            "frontend/src/mode-json.js",
            "frontend/src/mode-aql.js"

          ],
          graphViewer: [
            "frontend/js/graphViewer/graph/*",
            "frontend/js/graphViewer/ui/*",
            "frontend/js/graphViewer/graphViewer.js"
          ],
          modules: [
            "frontend/js/arango/arango.js",
            "frontend/js/arango/templateEngine.js",
            "frontend/js/shell/browser.js",
            "frontend/js/config/dygraphConfig.js",
            "frontend/js/modules/underscore.js",
            "frontend/js/modules/org/arangodb/aql/functions.js",
            "frontend/js/modules/org/arangodb/graph/traversal.js",
            "frontend/js/modules/org/arangodb/arango-collection-common.js",
            "frontend/js/modules/org/arangodb/arango-collection.js",
            "frontend/js/modules/org/arangodb/arango-database.js",
            "frontend/js/modules/org/arangodb/arango-query-cursor.js",
            "frontend/js/modules/org/arangodb/arango-statement-common.js",
            "frontend/js/modules/org/arangodb/arango-statement.js",
            "frontend/js/modules/org/arangodb/arangosh.js",
            "frontend/js/modules/org/arangodb/general-graph.js",
            "frontend/js/modules/org/arangodb/graph-blueprint.js",
            "frontend/js/modules/org/arangodb/graph-common.js",
            "frontend/js/modules/org/arangodb/graph.js",
            "frontend/js/modules/org/arangodb/is.js",
            "frontend/js/modules/org/arangodb/mimetypes.js",
            "frontend/js/modules/org/arangodb/replication.js",
            "frontend/js/modules/org/arangodb/simple-query-common.js",
            "frontend/js/modules/org/arangodb/simple-query.js",
            "frontend/js/modules/org/arangodb/tutorial.js",
            "frontend/js/modules/org/arangodb-common.js",
            "frontend/js/modules/org/arangodb.js",
            "frontend/js/bootstrap/errors.js",
            "frontend/js/bootstrap/monkeypatches.js",
            "frontend/js/bootstrap/module-internal.js",
            "frontend/js/client/bootstrap/module-internal.js",
            "frontend/js/client/client.js",
            "frontend/js/bootstrap/module-console.js"
          ],
          js: [
            "frontend/js/models/*",
            "frontend/js/collections/*",
            "frontend/js/views/*",
            "frontend/js/routers/router.js",
            "frontend/js/routers/versionCheck.js",
            "frontend/js/routers/startApp.js"
          ]
        },
        cluster: {
          css: [
            "frontend/scss/cluster.scss"
          ],
          js: [
            "clusterFrontend/js/*"
          ]
        }
      },

      sass: {
        dev: {
          options: {
            style: 'nested'
          },
          files: {
            'frontend/build/style.css': '<%= project.standalone.css %>',
            'clusterFrontend/build/style.css': '<%= project.cluster.css %>'
          }
        },
        dist: {
          options: {
            style: 'compressed'
          },
          files: {
            'frontend/build/style.css': '<%= project.standalone.css %>',
            'clusterFrontend/build/style.css': '<%= project.cluster.css %>'
          }
        }
      },

      
      concat_in_order: {
        default: {
          files: {
            'frontend/build/app.js': [
              '<%=project.shared.lib %>',
              '<%=project.standalone.lib %>',
              '<%=project.standalone.graphViewer %>',
              '<%=project.standalone.modules %>',
              '<%=project.standalone.js %>'
            ]
          },
          options: {
            extractRequired: function () {
              return [];
            },
            extractDeclared: function () {
              return [];
            }
          }
        },
        coverage: {
          files: {
            'frontend/build/lib.test.js': [
              '<%=project.shared.lib %>',
              '<%=project.standalone.lib %>',
              '<%=project.standalone.modules %>'
            ],
            'frontend/build/app.test.js': [
              '<%=project.standalone.graphViewer %>',
              '<%=project.standalone.js %>'
            ]
          },
          options: {
            extractRequired: function () {
              return [];
            },
            extractDeclared: function () {
              return [];
            }
          }
        }
      },

      jshint: {
        options: {
          laxbreak: true
        },
        default: [
          '<%=project.standalone.js %>'
        ]
      },
      
      uglify: {
        dist: {
          files: {
            'frontend/build/app.min.js': 'frontend/build/app.js'
          }
        }
      },

      watch: {
        sass: {
          files: [
            'frontend/scss/{,*/}*.{scss,sass}', 
            'clusterFrontend/scss/{,*/}*.{scss,sass}', 
          ],
          tasks: ['sass:dev']
        },
        concat_in_order: {
          files: [
            'frontend/js/{,*/}*.js'
          ],
          tasks: ['concat_in_order:default']
        }
      }
    });

    require('matchdep').filterDev('grunt-*').forEach(grunt.loadNpmTasks);

    grunt.registerTask('default', [
      'sass:dev',
      'concat_in_order:default',
      'watch'
    ]);

    grunt.registerTask('deploy', [
      'sass:dist',
      'concat_in_order:default',
      'uglify:dist'
    ]);

    grunt.registerTask('coverage', [
      'concat_in_order:coverage'
    ]);

    grunt.registerTask('peter', [
      'concat_in_order:default',
    ]);

  };
}());
