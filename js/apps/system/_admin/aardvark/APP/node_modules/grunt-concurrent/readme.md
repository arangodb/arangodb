# grunt-concurrent [![Build Status](https://travis-ci.org/sindresorhus/grunt-concurrent.svg?branch=master)](https://travis-ci.org/sindresorhus/grunt-concurrent)

> Run grunt tasks concurrently

---

<p align="center"><b>🔥 Want to strengthen your core JavaScript skills and master ES6?</b><br>I would personally recommend this awesome <a href="https://ES6.io/friend/AWESOME">ES6 course</a> by Wes Bos.</p>

---

<img src="screenshot.png" width="439">

Running slow tasks like Coffee and Sass concurrently can potentially improve your build time significantly. This task is also useful if you need to run [multiple blocking tasks](#logconcurrentoutput) like `nodemon` and `watch` at once.


## Install

```
$ npm install --save-dev grunt-concurrent
```


## Usage

```js
require('load-grunt-tasks')(grunt); // npm install --save-dev load-grunt-tasks

grunt.initConfig({
	concurrent: {
		target1: ['coffee', 'sass'],
		target2: ['jshint', 'mocha']
	}
});

// tasks of target1 run concurrently, after they all finished, tasks of target2 run concurrently,
// instead of target1 and target2 run concurrently.
grunt.registerTask('default', ['concurrent:target1', 'concurrent:target2']);
```

## Sequential tasks in concurrent target

```js
grunt.initConfig({
	concurrent: {
		target: [['jshint', 'coffee'], 'sass']
	}
});
```
Now `jshint` will always be done before `coffee` and `sass` runs independent of both of them.


## Options

### limit

Type: `number`<br>
Default: Twice the number of CPU cores with a minimum of 2

Limit how many tasks that are run concurrently.

### logConcurrentOutput

Type: `boolean`<br>
Default: `false`

You can optionally log the output of your concurrent tasks by specifying the `logConcurrentOutput` option. Here is an example config which runs [grunt-nodemon](https://github.com/ChrisWren/grunt-nodemon) to launch and monitor a node server and [grunt-contrib-watch](https://github.com/gruntjs/grunt-contrib-watch) to watch for asset changes all in one terminal tab:

```js
grunt.initConfig({
	concurrent: {
		target: {
			tasks: ['nodemon', 'watch'],
			options: {
				logConcurrentOutput: true
			}
		}
	}
});

grunt.loadNpmTasks('grunt-concurrent');
grunt.registerTask('default', ['concurrent:target']);
```

*The output will be messy when combining certain tasks. This option is best used with tasks that don't exit like `watch` and `nodemon` to monitor the output of long-running concurrent tasks.*


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
