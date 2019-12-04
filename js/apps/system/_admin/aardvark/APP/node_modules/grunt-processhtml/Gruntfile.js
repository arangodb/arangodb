/*
 * grunt-processhtml
 * https://github.com/dciccale/grunt-processhtml
 *
 * Copyright (c) 2013-2015 Denis Ciccale (@tdecs)
 * Licensed under the MIT license.
 * https://github.com/dciccale/grunt-processhtml/blob/master/LICENSE-MIT
 */

'use strict';

module.exports = function (grunt) {
  grunt.initConfig({
    jshint: {
      all: ['tasks/**/*.js', '<%= nodeunit.tests %>'],
      options: {
        jshintrc: '.jshintrc'
      }
    },
    processhtml: {
      dev: {
        options: {
          data: {
            message: 'This is dev target'
          }
        },
        files: {
          'test/fixtures/dev/index.processed.html': ['test/fixtures/index.html']
        }
      },
      dist: {
        options: {
          process: true,
          data: {
            title: 'My app',
            message: 'This is dist target'
          }
        },
        files: {
          'test/fixtures/dist/index.processed.html': ['test/fixtures/index.html']
        }
      },
      custom: {
        options: {
          templateSettings: {
            interpolate: /{{([\s\S]+?)}}/g
          },
          data: {
            message: 'This has custom delimiters for the template'
          }
        },
        files: {
          'test/fixtures/custom/custom.processed.html': ['test/fixtures/custom.html']
        }
      },
      marker: {
        options: {
          commentMarker: 'process',
          data: {
            message: 'This uses a custom comment marker'
          }
        },
        files: {
          'test/fixtures/commentMarker/commentMarker.processed.html': ['test/fixtures/commentMarker.html']
        }
      },
      strip: {
        options: {
          strip: true
        },
        files: {
          'test/fixtures/strip/strip.processed.html': ['test/fixtures/strip.html']
        }
      },
      inline: {
        options: {
          environment: null
        },
        files: {
          'test/fixtures/inline/inline.processed.html': ['test/fixtures/inline.html']
        }
      },

      /*
      The following three tests are for describing multiple targets
       */
      mult_one: {
        files: {
          'test/fixtures/multiple/mult_one.processed.html': ['test/fixtures/multiple.html']
        }
      },
      mult_two: {
        files: {
          'test/fixtures/multiple/mult_two.processed.html': ['test/fixtures/multiple.html']
        }
      },
      mult_three: {
        files: {
          'test/fixtures/multiple/mult_three.processed.html': ['test/fixtures/multiple.html']
        }
      },
      include_js: {
        files: {
          'test/fixtures/include/include.processed.html': ['test/fixtures/include.html']
        }
      },
      conditional_ie: {
        files: {
          'test/fixtures/conditional_ie/conditional_ie.processed.html': ['test/fixtures/conditional_ie.html']
        }
      },
      recursive: {
        options: {
          recursive: true
        },
        files: {
          'test/fixtures/recursive/recursive.processed.html': ['test/fixtures/recursive.html']
        }
      },
      custom_blocktype: {
        options: {
          customBlockTypes: ['test/fixtures/custom_blocktype.js']
        },
        files: {
          'test/fixtures/custom_blocktype/custom_blocktype.processed.html': ['test/fixtures/custom_blocktype.html']
        }
      },
      template: {
        options: {
          data: {
            msg: 'hey',
            test: 'text_$&_text'
          }
        },
        files: {
          'test/fixtures/template/template.processed.html': ['test/fixtures/template.html']
        }
      }
    },
    nodeunit: {
      tests: ['test/*_test.js']
    }
  });
  grunt.loadTasks('tasks');
  grunt.loadNpmTasks('grunt-contrib-jshint');
  grunt.loadNpmTasks('grunt-contrib-nodeunit');
  grunt.loadNpmTasks('grunt-release');
  grunt.registerTask('test', ['processhtml', 'nodeunit']);
  grunt.registerTask('default', ['jshint', 'test']);
};
