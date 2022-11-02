process.env.NODE_ENV = process.env.NODE_ENV || 'development';

module.exports = function(grunt) {

    grunt.initConfig({
        pkg: grunt.file.readJSON('package.json'),
        
        jshint: {
            all: {
                src: ['index.js', 'lib/*.js'],
                options: {
                    globalstrict: true,
                    globals: {
                        require: false,
                        __dirname: false,
                        exports: false,
                        module: false,
                        setTimeout: false
                    }
                }
            }
        },
        
        mochaTest: {
            options: {
                reporter: 'spec'
            },
            src: ['test/*.js']
        }
    });

    if (process.env.NODE_ENV === 'development') {
        grunt.loadNpmTasks('grunt-mocha-test');
        grunt.loadNpmTasks('grunt-contrib-jshint');

        grunt.registerTask('test', ['mochaTest']);
        grunt.registerTask('default', ['jshint', 'test']);
    }
};
