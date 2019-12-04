
# Sass.js

  JavaScript implementation of Sass. Great for **node.js** and other
  frameworks supporting the CommonJS module system.
  
## Installation

npm:

      $ npm install sass

## Usage

    var sass = require('sass')
    sass.render('... string of sass ...')
    // => '... string of css ...'
    
    sass.collect('... string of sass ...')
    // => { selectors: [...], variables: { ... }, mixins: { ... }}
    
## Comments

    // foo
    body
      // bar
      a
        :color #fff
        
compiles to

    body a {
      color: #fff;}
      
## Variables

    !red = #ff0000
    body
      :color !red
      
and
    
    red: #ff0000
    body
      :color !red
     
compile to

    body {
      color: #ff0000;}

## Selector Continuations

    a
      :color #fff
      &:hover
        :color #000
      &.active
        :background #888
        &:hover
          :color #fff
          
compiles to

    a {
      color: #fff;}

    a:hover {
      color: #000;}

    a.active {
      background: #888;}

    a.active:hover {
      color: #fff;}
      
## Literal JavaScript

    type: "solid"
    size: 1
    input
      :border { parseInt(size) + 1 }px {type} #000
      
compiles to

    input {
      border: 2px "solid" #000;}
      
## Property Expansion

    div
      =border-radius 5px
      
compiles to

    div {
      -webkit-border-radius: 5px;
      -moz-border-radius: 5px;}
      
## Mixins

     +large
       :font-size 15px
     +striped
       tr
         :background #fff
         +large
         &:odd
           :background #000
     table
       +striped
       :border none
       
compiles to

    table {
      border: none;}
    table tr {
      background: #fff;}
    table tr {
      font-size: 15px;}
    table tr:odd {
      background: #000;}
    
    
## Testing

Update Git submodules and execute:
    $ make test
    
## More Information

* Featured in [Advanced JavaScript e-book](http://www.dev-mag.com/2010/02/18/advanced-javascript/) for only $4
  
## License 

(The MIT License)

Copyright (c) 2009 TJ Holowaychuk &lt;tj@vision-media.ca&gt;

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