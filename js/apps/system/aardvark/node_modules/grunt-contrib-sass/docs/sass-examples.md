# Examples

## Example config

```javascript
grunt.initConfig({
  sass: {                              // Task
    dist: {                            // Target
      options: {                       // Target options
        style: 'expanded'
      },
      files: {                         // Dictionary of files
        'main.css': 'main.scss',       // 'destination': 'source'
        'widgets.css': 'widgets.scss'
      }
    }
  }
});

grunt.loadNpmTasks('grunt-contrib-sass');

grunt.registerTask('default', ['sass']);
```

## Compile

```javascript
grunt.initConfig({
  sass: {
    dist: {
      files: {
        'main.css': 'main.scss'
      }
    }
  }
});
```

## Concat and compile

Instead of concatenating the files, just `@import` them into another `.sass` file eg. `main.scss`.


## Compile multiple files

You can specify multiple `destination: source` items in `files`.

```javascript
grunt.initConfig({
  sass: {
    dist: {
      files: {
        'main.css': 'main.scss',
        'widgets.css': 'widgets.scss'
      }
    }
  }
});
```

## Compile files in a directory

Instead of naming all files you want to compile, you can use the `expand` property allowing you to specify a directory. More information available in the [grunt docs](http://gruntjs.com/configuring-tasks) - `Building the files object dynamically`.

```javascript
grunt.initConfig({
  sass: {
    dist: {
      files: [{
        expand: true,
        cwd: 'styles',
        src: ['*.scss'],
        dest: '../public',
        ext: '.css'
      }]
    }
  }
});
```
