(function() {
  "use strict";

  var vName = Date.now();
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
          ],
          css: [
            "frontend/css/swagger/hightlight.default.css",
            "frontend/css/bootstrap.css",
            "frontend/css/jquery-ui-1.9.2.custom.css",
            "frontend/css/jquery.contextmenu.css",
            "frontend/css/select2.css",
            "frontend/css/jquery.dataTables.css",
            "frontend/css/nv.d3.css",
            "frontend/css/swaggerView.css",
            "frontend/css/ansi.css",
            "frontend/css/highlightjs.css",
            "frontend/css/jsoneditor.css",
            "frontend/ttf/arangofont/style.css"
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
            "frontend/js/lib/jquery.textfill.min.js",
            "frontend/js/lib/cytoscape.min.js",
            "frontend/js/lib/cola.min.js",
            "frontend/js/lib/select2.min.js",
            "frontend/js/lib/handlebars-1.0.rc.1.js",
            "frontend/js/lib/jsoneditor-min.js",
            "frontend/js/lib/d3.v3.min.js",
            "frontend/js/lib/nv.d3.js",
            "frontend/js/lib/strftime-min.js",
            "frontend/js/lib/dygraph-combined.js",
            "frontend/js/lib/vivagraph.js",
            "frontend/js/lib/d3.fisheye.js",
            "frontend/js/lib/bootstrap-pagination.js",
            "frontend/js/lib/jqconsole.min.js",
            "frontend/js/lib/swagger.js",
            "frontend/js/lib/swagger-ui.js",
            "frontend/js/lib/highlight.7.3.pack.js",
            "frontend/js/lib/joi.browser.js",
            "frontend/js/lib/md5.js",
            "frontend/js/lib/lunr.min.js",
            "frontend/js/lib/ammap/ammap.js",
            "frontend/js/lib/ammap/maps/js/usa2High.js",
            "frontend/js/lib/ammap/themes/light.js",
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
            "frontend/js/config/dygraphConfig.js" 
            /*
            "frontend/js/modules/underscore.js",
            "frontend/js/modules/org/arangodb/aql/explainer.js",
            "frontend/js/modules/org/arangodb/aql/functions.js",
            "frontend/js/modules/org/arangodb/aql/queries.js",
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
            "frontend/js/bootstrap/modules/internal.js", // deps: -
            "frontend/js/bootstrap/errors.js", // deps: internal
            "frontend/js/bootstrap/modules/console.js", // deps: internal
            "frontend/js/bootstrap/monkeypatches.js", 
            "frontend/js/client/bootstrap/modules/internal.js", // deps: internal
            "frontend/js/client/client.js"
            */
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
            "clusterFrontend/js/**"
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

      htmlmin: {
        dist: {
          options: {
            minifyJS: true,
            removeComments: true,
            collapseWhitespace: true
          },
          files: {
            'frontend/build/standalone-min.html' : 'frontend/build/standalone.html',
            'clusterFrontend/build/cluster-min.html' : 'clusterFrontend/build/cluster.html'
          }
        }
      },

      compress: {
        main: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/build/*min*.html'],
            dest: '.',
            ext: '.html.gz'
          }]
        },
        standaloneCSS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/build/*.css'],
            dest: '.',
            ext: '.css.gz'
          }]
        },
        clusterCSS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['clusterFrontend/build/*.css'],
            dest: '.',
            ext: '.css.gz'
          }]
        },
        sharedCSS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['build/*.css'],
            dest: '.',
            ext: '.css.gz'
          }]
        },
        standaloneJSMinified: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/build/app.min.js', 'frontend/build/arangoes5.min.js'],
            dest: '.',
            ext: '.min.js.gz'
          }]
        },
        standaloneJS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/build/app.js', 'frontend/build/arangoes5.js'],
            dest: '.',
            ext: '.js.gz'
          }]
        },
        clusterJS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['clusterFrontend/build/cluster.js'],
            dest: '.',
            ext: '.js.gz'
          }]
        },
        clusterMinJS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['clusterFrontend/build/cluster.min.js'],
            dest: '.',
            ext: '.min.js.gz'
          }]
        },
        aceJS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/src/ace.js'],
            dest: '.',
            ext: '.js.gz'
          }]
        },
        workerJSON: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/src/worker-json.js'],
            dest: '.',
            ext: '.js.gz'
          }]
        },
        sharedJS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['build/sharedLibs.js'],
            dest: '.',
            ext: '.js.gz'
          }]
        },
        sharedMinJS: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['build/sharedLibs.min.js'],
            dest: '.',
            ext: '.min.js.gz'
          }]
        }
      },

      cssmin: {
        dist: {
          options: {
            banner: '/* arangodb */'
          },
          files: {
            'frontend/build/style-minified.css': ['frontend/build/style.css'],
            'build/extra-minified.css': ['build/extra.css'],
            'clusterFrontend/build/cluster-minified.css': ['frontend/scss/cluster.css'],
            'clusterFrontend/build/style-minified.css': ['clusterFrontend/build/style.css']
          }
        }
      },

      imagemin: {
        dist: {
          options: {
            optimizationLevel: 7
          },
          files: [{
             expand: true,
             cwd: 'frontend/img',
             src: ['**/*.{png,jpg,gif}'],
             dest: 'frontend/compressed-img/'
          }]
        }
      },

      concat: {
        css: {
          src: ['<%=project.shared.css %>'],
          dest: 'build/extra.css'
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
        sharedLibs: {
          src: [
            "frontend/js/lib/jquery-2.1.0.min.js",
            "frontend/js/lib/underscore.js",
            "frontend/js/lib/backbone.js",
            "frontend/js/lib/bootstrap.js"
          ],
          dest: 'build/sharedLibs.js',
          options: {
            extractRequired: function () {
              return [];
            },
            extractDeclared: function () {
              return [];
            }
          }
        },
        sharedES: {
          src: [
            "frontend/js/modules/**/*.js",
            "frontend/js/bootstrap/modules/internal.js", 
            "frontend/js/bootstrap/errors.js",
            "frontend/js/bootstrap/modules/console.js",
            "frontend/js/client/bootstrap/modules/internal.js", 
            "frontend/js/bootstrap/monkeypatches.js", 
            "frontend/js/client/client.js"
          ],
          dest: 'frontend/build/arangoes6.js',
          options: {
            extractRequired: function () {
              return [];
            },
            extractDeclared: function () {
              return [];
            }
          }
        },
        jsCluster: {
          src: [
            "frontend/js/lib/dygraph-combined.js",
            "frontend/js/config/dygraphConfig.js",
            "frontend/js/lib/d3.v3.min.js",
            "frontend/js/lib/nv.d3.js",
            "frontend/js/arango/arango.js",
            "clusterFrontend/js/models/*",
            "clusterFrontend/js/collections/*",
            "frontend/js/models/arangoDocument.js",
            "frontend/js/models/arangoStatistics.js",
            "frontend/js/models/arangoStatisticsDescription.js",
            "frontend/js/collections/_paginatedCollection.js",
            "frontend/js/collections/arangoStatisticsCollection.js",
            "frontend/js/collections/arangoDocuments.js",
            "frontend/js/arango/templateEngine.js",
            "frontend/js/views/footerView.js",
            "frontend/js/views/dashboardView.js",
            "frontend/js/views/modalView.js",
            "frontend/js/collections/arangoStatisticsDescriptionCollection.js",
            "clusterFrontend/js/views/*",
            "clusterFrontend/js/routers/*"
          ],
          dest: 'clusterFrontend/build/cluster.js'
        },
        htmlCluster: {
          src: [ 
            "frontend/html/start.html.part",
            "clusterFrontend/html/head.html.part",
            "frontend/js/templates/dashboardView.ejs",
            "frontend/js/templates/modalBase.ejs",
            "frontend/js/templates/footerView.ejs",
            "frontend/js/templates/modalGraph.ejs",
            "frontend/js/templates/lineChartDetailView.ejs",
            "clusterFrontend/js/templates/*.ejs",
            "frontend/html/body.html.part",
            "clusterFrontend/html/scripts.html.part",
            "frontend/html/end.html.part"
          ],
          dest: 'clusterFrontend/build/cluster.html'
        },
        htmlStandalone: {
          src: [
            "frontend/html/start.html.part",
            "frontend/html/head.html.part",
            "frontend/js/templates/*.ejs",
            "frontend/html/body.html.part",
            "frontend/build/scripts.html.part",
            "frontend/html/end.html.part"
          ],
          dest: 'frontend/build/standalone.html' 
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

      replace: {
        scripts: {
          src: "frontend/html/scripts.html.part",
          dest: "frontend/build/scripts.html.part",
          replacements: [{
            from: "__VERSION",
            to: vName
          }]
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

      babel: {
        options: {
          sourceMap: false
        },
        dist: {
          files: {
            'frontend/build/arangoes5.js': 'frontend/build/arangoes6.js'
          }
        }
      },

      uglify: {
        dist: {
          files: {
            'frontend/build/app.min.js': 'frontend/build/app.js',
            'clusterFrontend/build/cluster.min.js': 'clusterFrontend/build/cluster.js',
            'frontend/src/ace.min.js': 'frontend/src/ace.js',
            'build/sharedLibs.min.js': 'build/sharedLibs.js',
            'frontend/build/arangoes5.min.js': 'frontend/build/arangoes5.js'
          }
        }
      },

      watch: {
        sass: {
          files: [
            'frontend/scss/{,*/}*.{scss,sass}',
            'clusterFrontend/scss/{,*/}*.{scss,sass}',
          ],
          tasks: ['sass:dev', 'cssmin', 'compress']
        },
        imagemin: {
          files: [
            'frontend/img/*.gif',
            'frontend/img/*.png',
            'frontend/img/*.jpg',
            'frontend/img/*.jpeg'
          ],
          tasks: [
            'imagemin'
          ]
        },
        concat_in_order: {
          files: [
            '!frontend/js/modules/**/*.js',
            'frontend/js/{,*/}*.js',
            'frontend/js/graphViewer/**/*.js',
            'clusterFrontend/js/{,*/}*.js'
          ],
          tasks: [
            'concat_in_order:sharedES',
            'concat_in_order:sharedLibs',
            'concat_in_order:default',
            'concat_in_order:jsCluster',
            'compress',
          ]
        },
        html: {
          files: [
            'frontend/html/*',
            'frontend/js/templates/*.ejs',
            'clusterFrontend/html/*'
          ],
          tasks: [
            'concat_in_order:htmlCluster',
            'concat_in_order:htmlStandalone',
            'compress',
            'htmlmin'
          ]
        }
      }
    });

    grunt.loadNpmTasks("grunt-sass");
    grunt.loadNpmTasks("grunt-contrib-imagemin");
    grunt.loadNpmTasks('grunt-contrib-cssmin');
    grunt.loadNpmTasks('grunt-contrib-compress');
    grunt.loadNpmTasks("grunt-contrib-concat");
    grunt.loadNpmTasks('grunt-contrib-htmlmin');
    grunt.loadNpmTasks('grunt-contrib-uglify');
    require('load-grunt-tasks')(grunt);

    require('matchdep').filterDev('grunt-*').forEach(grunt.loadNpmTasks);
    grunt.loadNpmTasks('grunt-text-replace');

    grunt.registerTask('default', [
      'sass:dev',
      'jshint:default',
      'replace',
      'imagemin',
      'concat',
      'concat_in_order:sharedES',
      'concat_in_order:sharedLibs',
      'concat_in_order:default',
      'concat_in_order:jsCluster',
      'concat_in_order:htmlCluster',
      'concat_in_order:htmlStandalone',
      'babel',
      'cssmin',
      'uglify',
      'htmlmin',
      'compress',
      'watch'
    ]);


    grunt.registerTask('devel', [
      'sass:dev',
      'replace',
      'concat',
      'concat_in_order:sharedES',
      'concat_in_order:sharedLibs',
      'concat_in_order:default',
      'concat_in_order:jsCluster',
      'concat_in_order:htmlCluster',
      'concat_in_order:htmlStandalone',
      'watch'
    ]);

    grunt.registerTask('deploy', [
      'sass:dist',
      'concat_in_order:sharedES',
      'concat_in_order:sharedLibs',
      'concat_in_order:default',
      'concat_in_order:jsCluster',
      'concat_in_order:htmlCluster',
      'concat_in_order:htmlStandalone',
      'uglify:dist'
    ]);

    grunt.registerTask('coverage', [
      'concat_in_order:coverage'
    ]);

  };
}());
