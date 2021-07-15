# ansi_up.js

__ansi_up__ is an easy to use library that transforms text containing
[ANSI color escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code#Colors) into HTML.

This module is a single Javascript file with no dependencies.
It is "isomorphic" javascript. This is just another way of saying that
the ansi_up.js file will work in both the browser or node.js.
The js library is compiled from TypeScript and its type description ships with the NPM.
This code has been used in production since 2011 and is actively maintained.

For example, turn this terminal output:

    ESC[1;Foreground
    [1;30m 30  [1;30m 30  [1;30m 30  [1;30m 30  [1;30m 30  [1;30m 30  [1;30m 30  [1;30m 30  [0m
    [1;31m 31  [1;31m 31  [1;31m 31  [1;31m 31  [1;31m 31  [1;31m 31  [1;31m 31  [1;31m 31  [0m
    [1;32m 32  [1;32m 32  [1;32m 32  [1;32m 32  [1;32m 32  [1;32m 32  [1;32m 32  [1;32m 32  [0m
    ...

...into this browser output:

![](https://raw.github.com/drudru/ansi_up/master/sample.png)


## Browser Example

```HTML
    <script src="ansi_up.js" type="text/javascript"></script>
    <script type="text/javascript">

    var txt  = "\n\n\033[1;33;40m 33;40  \033[1;33;41m 33;41  \033[1;33;42m 33;42  \033[1;33;43m 33;43  \033[1;33;44m 33;44  \033[1;33;45m 33;45  \033[1;33;46m 33;46  \033[1m\033[0\n\n\033[1;33;42m >> Tests OK\n\n"

    var ansi_up = new AnsiUp;

    var html = ansi_up.ansi_to_html(txt);

    var cdiv = document.getElementById("console");

    cdiv.innerHTML = html;

    </script>
```

## Node Example

```JavaScript
    var AU = require('ansi_up');
    var ansi_up = new AU.default;

    var txt  = "\n\n\033[1;33;40m 33;40  \033[1;33;41m 33;41  \033[1;33;42m 33;42  \033[1;33;43m 33;43  \033[1;33;44m 33;44  \033[1;33;45m 33;45  \033[1;33;46m 33;46  \033[1m\033[0\n\n\033[1;33;42m >> Tests OK\n\n"

    var html = ansi_up.ansi_to_html(txt);
```

More examples are in the 'examples' directory in the repo.

## Typescript Example

```TypeScript
    import {
        default as AnsiUp
    } from 'ansi_up';

    const ansi_up = new AnsiUp();

    const txt  = "\n\n\x1B[1;33;40m 33;40  \x1B[1;33;41m 33;41  \x1B[1;33;42m 33;42  \x1B[1;33;43m 33;43  \x1B[1;33;44m 33;44  \x1B[1;33;45m 33;45  \x1B[1;33;46m 33;46  \x1B[1m\x1B[0\n\n\x1B[1;33;42m >> Tests OK\n\n"

    let html = ansi_up.ansi_to_html(txt);
```

## Installation

    $ npm install ansi_up

## Versions

* Version 4.0 - Re-architect code to support [https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda](terminal URL codes).
* Version 3.0 - now treats ANSI bold sequences as CSS font-weight:bold
* Version 2.0 - moved to a stateful, streaming version of the API
* Version 1.3 - was the last of the older, deprecated API.

## Quick Start

1. Use whatever module system to import the _ansi_up_ module.
2. Instantiate the object.
3. For every piece of ansi escaped text string, call **ansi_to_html**.
4. Append the emitted HTML to the previous HTML already emitted.
5. DONE


## API Methods and Recommended Settings

You only need the ansi_to_html method. The other properties listed below allow you to
override some of the escaping behaviour. You probably don't need to change these
from their default values.

It is recommended that the HTML container that holds the span tags is styled with a monospace font.
A PRE tag would work just fine for this.
It is also recommended that the HTML container is styled with a black background.
See the examples, for more CSS theming.


#### ansi_to_html (txt)

This transforms ANSI terminal escape codes/sequences into SPAN tags that wrap and style the content.

This method only interprets ANSI SGR (Select Graphic Rendition) codes or escaped URL codes.
For example, cursor movement codes are ignored and hidden from output.

This method also safely escapes any unsafe HTML characters.

The default style uses colors that are very close to the prescribed standard.
The standard assumes that the text will have a black background.
These colors are set as inline styles on the SPAN tags.
Another option is to set the 'use_classes' property to true'.
This will instead set classes on the spans so the colors can be set via CSS.
The class names used are of the format ````ansi-*-fg/bg```` and ````ansi-bright-*-fg/bg```` where * is the colour name, i.e black/red/green/yellow/blue/magenta/cyan/white.
See the examples directory for a complete CSS theme for these classes.

## Properties

#### escape_for_html
(default: true)

This does the minimum escaping of text to make it compliant with HTML.
In particular, the '&','<', and '>' characters are escaped. It is
** highly ** recommended that you do not set this to false. It will open
the door security vulnerabilities.

#### use_classes
(default: false)

This causes the SPAN tags to use classes to style the SPAN tags instead
of specified RGB values.

#### url_whitelist
(default: { 'http':1, 'https':1 };

This mapping is a whitelist of URI schemes that will be allowed to render HTML anchor tags.

## Buffering

In general, the ansi_to_html *should* emit HTML when invoked with a non-empty string.
The only exceptions are an incomplete ESC sequence or an incomplete escaped URL.
For those cases, the library will buffer the escape or the sequence for the escaped URL.

The library is also stateful. If a color is set in a prior invocation, then it will
continue to emit that color in further invocations until the color/SGR attribute is changed.

### Example of a Use Case

I have used this library to 'tail' a file.

On a remote machine, I had process generating a log file.
I had a web server running on the same machine.
The server hosted a simple HTML page that used AJAX to poll an object with a range query.
Specifically I used an HTTP/1.1 GET request with RFC 7233 Range query.
The first range query would start at 0, but then progressively move forward after
new data was received.

For each new chunk of data received, I would transform the data with _ansi_up_, and append the new
spans to the innerHTML of a PRE tag.


### UTF8 note

One last important note, _ansi_up_ takes its input in the form of a Javascript string.
These strings are UTF8. When you take the output of some program and send it to
Javascript, there will be buffering. Be sure that you do not send incomplete UTF8 sequences.
Javascript will ignore or drop the sequence from the stream when it converts it to a
string.


## Building

To build, a simple Makefile handles it all.

```shell
    $ make
```

## Running tests

To run the tests for _ansi_up_, run `npm install` to install dev dependencies. Then:

```shell
    $ make test
```

## Credits

This code was developed by Dru Nelson (<https://github.com/drudru>).

Thanks goes to the following contributors for their patches:

- AIZAWA Hina (<https://github.com/fetus-hina>)
- James R. White (<https://github.com/jamesrwhite>)
- Aaron Stone (<https://github.com/sodabrew>)
- Maximilian Antoni (<https://github.com/mantoni>)
- Jim Bauwens (<https://github.com/jimbauwens>)
- Jacek Jędrzejewski (<https://github.com/eXtreme>)

## License

(The MIT License)

Copyright (c) 2011 Dru Nelson

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
'Software'), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
