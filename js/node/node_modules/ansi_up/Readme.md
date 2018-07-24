# ansi_up.js

__ansi_up__ is a simple, easy to use library that provides a streaming API to
transform text containing
[ANSI color escape codes](http://en.wikipedia.org/wiki/ANSI_escape_code#Colors) into proper HTML.
It can also transform any text that looks like a URL into an HTML anchor tag.

This module is a single Javascript file with no dependencies. It is a UMD style module so it
can be utilized in a browser, in node.js (CommonJS), or with AMD (require.js). The source code
was compiled from TypeScript and its type description ships with the NPM. This code has been used in production since 2011 and is actively maintained.

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

## Installation

    $ npm install ansi_up

## Versions

Version 2.x is the latest stateful, streaming version of the API. It is simpler and more correct.
Version 1.3.0 was the last of the older, deprecated API.

## Quick Start

1. Use whatever module system to import the _ansi_up_ module.
2. Instantiate the object.
3. For every piece of input that arrives, call **ansi_to_html**.
4. Append the emitted HTML to the previous HTML already emitted.


## API Methods

In order to use _ansi_up_, you must Instantiate an object using your given module
system.

#### ansi_to_html (txt)

This replaces ANSI terminal escape codes with SPAN tags that wrap the content. See the example output above.

This function only interprets ANSI SGR (Select Graphic Rendition) codes that can be represented in HTML. For example, cursor movement codes are ignored and hidden from output.

The default style uses colors that are very close to the prescribed standard. The standard assumes that the text will have a black background. These colors are set as inline styles on the SPAN tags. Another option is to set 'use_classes: true' in the options argument. This will instead set classes on the spans so the colors can be set via CSS. The class names used are of the format ````ansi-*-fg/bg```` and ````ansi-bright-*-fg/bg```` where * is the colour name, i.e black/red/green/yellow/blue/magenta/cyan/white. See the examples directory for a complete CSS theme for these classes.

#### ansi_to_text (txt)

This simply removes the ANSI escape codes from the stream.
No escaping is done.

#### linkify(txt)

This replaces any links in the text with anchor tags that display the link.
Only strings starting with 'http' or 'https', and surrounded by whitespace are
considered valid patterns.
You should only call this method if you can guarantee that the full URL
will be passed into ansi_to_html(). If the URL is split along a buffer
boundary, then the wrong URL will be 'linkified'.

## Properties

#### escape_for_html
(default: true)

This does the minimum escaping of text to make it compliant with HTML.
In particular, the '&','<', and '>' characters are escaped.


#### use_classes
(default: false)

This causes the SPAN tags to use class names for the color style instead
of specified RGB values.

## API Overview

On a high level, _ansi_up_ takes a stream of text and transforms it proper HTML with colors.
It does this by buffering the data and performing multiple passes over the
stream. Each time it consumes data, it may or may not emit HTML. This HTML will always be
proper HTML.

Because this process requires buffering (ie. stateful), you must insantiate an _ansi_up_ object
in order to begin. Also, text may be received later that is styled by a previous.

The first pass converts characters that are unsafe for HTML into their equivalents. It will only
convert '&', '<', and '>' characters. This pass is optional, and is on by default.

The second pass converts any ANSI color sequences to HTML spans. It does this by recognizing
what is termed as ANSI **SGR** codes. All ANSI sequences (SGR and non-SGR) are removed from the
output stream. The SGR codes create HTML **SPAN** tags to surround text that is styled by those
codes. If the ANSI sequence is incomplete, it will be held in _ansi_up_'s internal buffer
until new data is received to complete it.

The third and final pass transforms URLs to HTML anchors. This will also buffer output until a non URL
character is received. This pass is optional, and is off by default.


### Recommended Style of Use

There are two ways to stream this data to a web page. A push model or a pull model.

I have personally used a pull model to 'tail' a file.

In my 'pull' model, I had a process generating a log file on a remote machine.
I had a web server running on the same machine. I developed a simple page
that used AJAX to poll the web server periodically. Specifically I used an
HTTP/1.1 GET request with RFC 7233 Range query. The server would return
either range response.

I would then process each chunk received with _ansi_up_, and append the new
spans to the innerHTML of a PRE tag.


### UTF8 note

One last important note, _ansi_up_ takes its input in the form of a Javascript string.
These strings are UTF8. When you take the output of some program and send it to
Javascript, there will be buffering. Be sure to not send incomplete UTF8 sequences or
Javascript will ignore or drop the sequence from the stream when it converts it to a
string.


_ansi_up_ should be called via the functions defined on the module. It is recommended that the HTML is rendered with a monospace font and black background. See the examples, for a basic theme as a CSS definition.
At the same, it also properly escapes HTML unsafe characters (&,<,>,etc.) into their proper HTML representation.


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
