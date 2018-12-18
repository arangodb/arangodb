(function() {
  "use strict";

  const sass = require('node-sass');
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
            "frontend/js/lib/underscore-min.js",
            "frontend/js/lib/backbone-min.js",
            "frontend/js/lib/bootstrap-min.js",
            "frontend/js/lib/d3.min.js",
            "frontend/js/lib/nv.d3.min.js",
            "frontend/js/lib/dygraph-combined.min.js"
          ],
          css: [
            "frontend/css/swagger/hightlight.default.css",
            "frontend/css/bootstrap.css",
            "frontend/css/jquery-ui-1.9.2.custom.css",
            "frontend/css/jquery.contextmenu.css",
            "frontend/css/select2.css",
            "frontend/css/jquery.dataTables.css",
            "frontend/css/swaggerView.css",
            "frontend/css/ansi.css",
            "frontend/css/highlightjs.css",
            "frontend/css/jsoneditor.css",
            "frontend/css/grids-responsive-min.css",
            "frontend/css/tippy.css",
            "frontend/css/dygraph.css",
            "frontend/css/leaflet.css",
            "frontend/css/nv.d3.css",
            "frontend/ttf/arangofont/style.css"
          ]
        },
        standalone: {
          css: [
            "frontend/scss/style.scss"
          ],
          lib: [
            "frontend/js/lib/jquery-ui-1.9.2.custom.min.js",
            "frontend/js/lib/jquery.snippet.min.js",
            "frontend/js/lib/jquery.hotkeys.min.js",
            "frontend/js/lib/jquery.contextmenu.min.js",
            "frontend/js/lib/jquery.form.min.js",
            "frontend/js/lib/jquery.uploadfile.min.js",
            "frontend/js/lib/jquery.textfill.min.js",
            "frontend/js/lib/jquery.noty.packaged.min.js",
            "frontend/js/lib/select2.min.js",
            "frontend/js/lib/typeahead.bundle.min.js",
            "frontend/js/lib/numeral.min.js",
            "frontend/js/lib/moment.min.js",
            "frontend/js/lib/randomColor.js",
            "frontend/js/lib/tippy.js",
            // START SIGMA LIBRARIES
            "frontend/js/lib/sigma.min.js",
            "frontend/js/lib/sigma.renderers.halo.js",
            "frontend/js/lib/sigma.layout.noverlap.js",
            "frontend/js/lib/sigma.layout.fruchtermanReingold.js",
            "frontend/js/lib/sigma.exporters.image.js",
            "frontend/js/lib/sigma.canvas.edges.labels.curve.js",
            "frontend/js/lib/sigma.canvas.edges.labels.curvedArrow.js",
            "frontend/js/lib/sigma.canvas.edges.labels.def.js",
            "frontend/js/lib/sigma.canvas.edges.autoCurve.js",
            "frontend/js/lib/sigma.canvas.edges.tapered.js",
            "frontend/js/lib/sigma.canvas.edges.dotted.js",
            "frontend/js/lib/sigma.canvas.edges.dashed.js",
            "frontend/js/lib/sigma.plugins.animate.js",
            "frontend/js/lib/sigma.plugins.dragNodes.js",
            "frontend/js/lib/sigma.plugins.fullScreen.js",
            "frontend/js/lib/sigma.plugins.filter.js",
            "frontend/js/lib/sigma.plugins.lasso.js",
            "frontend/js/lib/jquery.csv.min.js",
            "frontend/js/lib/worker.js",
            "frontend/js/lib/supervisor.js",
            // END SIGMA LIBRARIES
            "frontend/js/lib/wheelnav.slicePath.js",
            "frontend/js/lib/wheelnav.min.js",
            "frontend/js/lib/raphael.min.js",
            "frontend/js/lib/raphael.icons.min.js",
            "frontend/js/lib/jsoneditor-min.js",
            "frontend/js/lib/strftime-min.js",
            "frontend/js/lib/d3.fisheye.min.js",
            "frontend/js/lib/bootstrap-pagination.min.js",
            "frontend/js/lib/jqconsole.min.js",
            "frontend/js/lib/highlight.js",
            "frontend/js/lib/joi-browser.min.js",
            "frontend/js/lib/md5.min.js",
            "frontend/js/lib/pretty-bytes.js",
            "frontend/js/lib/marked.min.js",
            "frontend/js/lib/leaflet.js",
            "frontend/js/lib/tile.stamen.js",
            "frontend/src/ace.js",
            "frontend/src/theme-textmate.js",
            "frontend/src/mode-json.js",
            "frontend/src/mode-aql.js"
          ],
          modules: [
            "frontend/js/arango/arango.js",
            "frontend/js/arango/templateEngine.js",
            "frontend/js/modules/underscore.js"
          ],
          js: [
            "frontend/js/config/dygraphConfig.js",
            "frontend/js/models/*",
            "frontend/js/collections/*",
            "frontend/js/views/*",
            "frontend/js/routers/router.js",
            "frontend/js/routers/versionCheck.js",
            "frontend/js/routers/startApp.js"
          ]
        }
      },

      sass: {
        dev: {
          options: {
            implementation: sass,
            style: 'nested'
          },
          files: {
            'frontend/build/style.css': '<%= project.standalone.css %>'
          }
        },
        dist: {
          options: {
            implementation: sass,
            // style: 'compressed'
          },
          files: {
            'frontend/build/style.css': '<%= project.standalone.css %>'
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
            'frontend/build/index-min.html' : 'frontend/build/index.html'
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
        standaloneJSMinified: {
          options: {
            mode: 'gzip'
          },
          files: [{
            expand: true,
            src: ['frontend/build/app.min.js', 'frontend/build/libs.min.js'],
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
            src: ['frontend/build/app.js', 'frontend/build/libs.min.js'],
            dest: '.',
            ext: '.js.gz'
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
      },

      cssmin: {
        dist: {
          options: {
            banner: '/* arangodb */'
          },
          files: {
            'frontend/build/style-minified.css': ['frontend/build/style.css'],
            'frontend/build/extra-minified.css': ['frontend/build/extra.css']
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
          dest: 'frontend/build/extra.css'
        }
      },

      concat_in_order: {
        default: {
          files: {
            'frontend/build/app.js': [
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
        libs: {
          files: {
            'frontend/build/libs.min.js': [
              '<%=project.shared.lib %>',
              '<%=project.standalone.lib %>'
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
        htmlStandalone: {
          src: [
            "frontend/html/start.html.part",
            "frontend/html/head.html.part",
            "frontend/js/templates/*.ejs",
            "frontend/html/body.html.part",
            "frontend/build/scripts.html.part",
            "frontend/html/end.html.part"
          ],
          dest: 'frontend/build/index.html'
        },
        coverage: {
          files: {
            'frontend/build/lib.test.js': [
              '<%=project.shared.lib %>',
              '<%=project.standalone.lib %>',
              '<%=project.standalone.modules %>'
            ],
            'frontend/build/app.test.js': [
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

      eslint: {
        options: {
          configFile: '../../../../../.eslintrc'
        },
        target: [
          'frontend/js/views/*.js',
          'frontend/js/arango/*.js',
          'frontend/js/models/*.js',
          'frontend/js/collections/*.js',
          'frontend/js/routers/*.js'
        ]
      },

      uglify: {
        default1: {
          files: {
            'frontend/build/app.min.js': 'frontend/build/app.js'
          }
        },
        libs2: {
          files: {
            'frontend/src/ace.min.js': 'frontend/src/ace.js'
          }
        }
      },

      concurrent: {
        uglifyFast: ['uglify:default1'],
        uglifyAll: ['uglify:default1', 'uglify:libs2']
      },

      watch: {
        sass: {
          files: [
            'frontend/scss/{,*/}*.{scss,sass}'
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
          ],
          tasks: [
            'concat_in_order:default',
            'compress',
          ]
        },
        html: {
          files: [
            'frontend/html/*',
            'frontend/js/templates/*.ejs'
          ],
          tasks: [
            'concat_in_order:htmlStandalone',
            'compress',
            'htmlmin'
          ]
        }
      }
    });

    grunt.loadNpmTasks("grunt-babel");
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
      'eslint',
      'sass:dev',
      'replace',
      'concat',
      'concat_in_order:default',
      'concat_in_order:htmlStandalone',
      'cssmin',
      'concurrent:uglifyFast',
      'htmlmin',
      'compress'
    ]);

    grunt.registerTask('devel', [
      'sass:dev',
      'replace',
      'concat',
      'concat_in_order:default',
      'concat_in_order:htmlStandalone',
      'concurrent:uglifyFast',
      'watch'
    ]);

    grunt.registerTask('deploy', [
      'sass:dev',
      'eslint',
      'replace',
      'imagemin',
      'concat',
      'concat_in_order:libs',
      'concat_in_order:default',
      'concat_in_order:htmlStandalone',
      'cssmin',
      'concurrent:uglifyAll',
      'htmlmin',
      'compress'
    ]);

    grunt.registerTask('coverage', [
      'concat_in_order:coverage'
    ]);

  };
}());
