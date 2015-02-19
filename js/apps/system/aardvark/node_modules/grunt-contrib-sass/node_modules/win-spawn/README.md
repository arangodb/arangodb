# win-spawn

  Spawn for node.js but in a way that works regardless of which OS you're using.  Use this if you want to use spawn with a JavaScript file.  It works by explicitly invoking node on windows.  It also shims support for environment variable setting by attempting to parse the command with a regex.  Since all modification is wrapped in `if (os === 'Windows_NT')` it can be safely used on non-windows systems and will not break anything.

## Installation

  $ npm install win-spawn

## Usage

### Command Line

  All the following will work exactly as if the 'win-spawn ' prefix was ommitted when on unix.

    $ win-spawn foo
    $ win-spawn ./bin/foo
    $ win-spawn NODE_PATH=./lib foo
    $ win-spawn NODE_PATH=./lib foo arg1 arg2

  You can also transform all the line endings in a directory from `\r\n` to `\n` just by running:

    $ win-line-endings

  You can preview the changes by running:

    $ win-line-endings -p

  It will ignore `node_modules` and `.git` by default, but is not clever enough to recognise binary files yet.

### API

This will just pass through to `child_process.spawn` on unix systems, but will correctly parse the arguments on windows.

```javascript
spawn('foo', [], {stdio: 'inherit'});
spawn('./bin/foo', [], {stdio: 'inherit'});
spawn('NODE_PATH=./lib foo', [], {stdio: 'inherit'});
spawn('NODE_PATH=./lib foo', [arg1, arg2], {stdio: 'inherit'});
```

![viewcount](https://viewcount.jepso.com/count/ForbesLindesay/win-spawn.png)
