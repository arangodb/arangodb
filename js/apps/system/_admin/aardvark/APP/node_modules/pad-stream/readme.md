# pad-stream [![Build Status](https://travis-ci.org/sindresorhus/pad-stream.svg?branch=master)](https://travis-ci.org/sindresorhus/pad-stream)

> Pad each line in a stream


## Install

```
$ npm install --save pad-stream
```


## Usage

```js
// pad.js
var padStream = require('pad-stream');

process.stdin.pipe(padStream('>', 2)).pipe(process.stdout);
```

```
$ echo 'foo\nbar' | node pad.js
>>foo
>>bar
```


## API

### padStream([indent], [count])

Returns a [transform stream](http://nodejs.org/api/stream.html#stream_class_stream_transform_1).

#### indent

Type: `string`  
Default: `' '`

String to use as indent.

#### count

Type: `number`  
Default: `1`

Number of times to repeat `indent`.


## CLI

```
$ npm install --global pad-stream
```

```
$ pad --help

  Usage
    pad <filename>
    echo <text> | pad

  Example
    echo 'foo\nbar' | pad --count=4
        foo
        bar

  Options
    --indent  Indent string
    --count   Times to repeat indent string
```


## License

MIT © [Sindre Sorhus](http://sindresorhus.com)
