var gulp = require('gulp-param')(require('gulp'), process.argv),
  mocha = require("gulp-mocha"),
  eslint = require('gulp-eslint'),
  istanbul = require('gulp-istanbul'),
  bench = require('gulp-bench'),
  uglify = require('gulp-uglify'),
  rimraf = require('gulp-rimraf'),
  bump = require('gulp-bump'),
  replace = require('gulp-replace'),
  rename = require('gulp-rename'),
  browserify = require('gulp-browserify'),
  header = require('gulp-header'),
  SRC = 'index.js',
  DEST = 'dist',
  SRC_COMPILED = 'underscore.string.js',
  MIN_FILE = 'underscore.string.min.js',
  VERSION_FILES = ['./package.json', './component.json', './bower.json'],
  VERSION_FILES_JS = [SRC, 'package.js'],
  package = require('./package.json'),
  headerText = '/* ' + package.name + ' ' + package.version + ' | ' +
               package.license + ' licensed | ' + package.homepage + ' */\n\n';

gulp.task('test', ['lint', 'browserify'], function(cov) {
  var reporters = ['html'];

  if (cov) {
    reporters.push('text');
  } else {
    reporters.push('text-summary');
  }

  return gulp.src(['*.js', 'helper/*.js'])
    .pipe(istanbul())
    .pipe(istanbul.hookRequire())
    .on('finish', function () {
      return gulp.src(['tests/*.js'])
        .pipe(mocha({
          ui: 'qunit',
          reporter: 'dot'
        }))
        .pipe(istanbul.writeReports({
          reporters: reporters
        }));
    });
});

gulp.task('lint', function() {
  var toLint = [
    '**/*.js',
    '!gulpfile.js',
    '!meteor-*.js',
    '!package.js',
    '!dist/**',
    '!coverage/**',
    '!node_modules/**'
  ];
  return gulp.src(toLint)
        .pipe(eslint({ fix: true }))
        .pipe(eslint.format())
        .pipe(eslint.failAfterError());
});

gulp.task('bench', ['browserify'], function(func) {
  func = func || '*';
  return gulp.src('bench/'+ func + '.js')
    .pipe(bench());
});

gulp.task('browserify', function() {
  return gulp.src(SRC)
    .pipe(browserify({
      detectGlobals: true,
      standalone: 's'
    }))
    .pipe(header(headerText))
    .pipe(rename('underscore.string.js'))
    .pipe(gulp.dest(DEST));
});

gulp.task('clean', function() {
  return gulp.src(DEST)
    .pipe(rimraf());
});

gulp.task('bump-in-js', function(semver) {
  return gulp.src(VERSION_FILES_JS)
    .pipe(replace(/(version?\s?=?\:?\s\')([\d\.]*)\'/gi, '$1' + semver + "'"))
    .pipe(gulp.dest('./'));
});

// usage: gulp bump -s <% Version %>
// usage: gulp bump --semver <% Version %>
gulp.task('bump', ['bump-in-js'], function(semver) {
  if (typeof semver !== 'string' || semver.length <= 0) {
    console.error('pass a new version `gulp bump --semver 2.4.1`');
    process.exit(1);
  }

  return gulp.src(VERSION_FILES)
    .pipe(bump({
      version: semver
    }))
    .pipe(gulp.dest('./'));
});

gulp.task('build', ['test', 'clean'], function() {
  gulp.src(DEST + '/' + SRC_COMPILED)
    .pipe(uglify())
    .pipe(header(headerText))
    .pipe(rename(MIN_FILE))
    .pipe(gulp.dest(DEST));
});
