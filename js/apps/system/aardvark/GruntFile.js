(function() {
  "use strict";

  module.exports = function(grunt) {

    grunt.initConfig({
      pkg: grunt.file.readJSON('package.json'),

      project: {
        standalone: {
          css: [
            "frontend/scss/style.scss"
          ],
          js: [
            "frontend/js/*"
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

      watch: {
        sass: {
          files: [
            'frontend/scss/{,*/}*.{scss,sass}', 
            'clusterFrontend/scss/{,*/}*.{scss,sass}', 
          ],
          tasks: ['sass:dev']
        }
      }
    });

    require('matchdep').filterDev('grunt-*').forEach(grunt.loadNpmTasks);

    grunt.registerTask('default', [
      'sass:dev',
      'watch'
    ]);

  };
}());
