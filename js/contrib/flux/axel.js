/**
 * @license
 * Copyright (c) 2014 Alistair G MacDonald
 * 
 * (The MIT License)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
 * documentation files (the 'Software'), to deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit 
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
(function() {

  var internal = require('internal');
  //const Writable = require('stream').Writable;
  class StdoutWritable {//extends Writable 
    constructor(options) {
      // Calls the stream.Writable() constructor
      //super(options);

      this._type = 'tty';
      this.isTTY = internal.COLOR_OUTPUT;
    }

    get rows() {
      var size = internal.terminalSize();
      return size[0];
    }

    get columns() {
      var size = internal.terminalSize();
      return size[1];
    }

    write(chunk, encoding, callback) {
      internal.output(chunk);
      if (callback) {callback();}
    }
  }

  // Requires
  var ansi = require('./ansi')
    , stdo = new StdoutWritable()
    , cursor = ansi(stdo)
    , cols = stdo.columns
    , rows = stdo.rows
    , defaultChar = ' '

    , PI2 = Math.PI*2
    
    , color = {
        fg:{ r: 255, g: 255, b: 255 },
        bg:{ r: 255, g: 255, b: 255 }
      }
    ;



  var axel = {

    // Clears a block
    scrub: function(x1, y1, w, h){
         
      // Turn off the color settings while we scrub
      var oldBrush = this.defaultChar;
      cursor.reset();
      this.defaultChar = ' ';

      this.box(x1, y1, w, h);

      // Put the colors back after
      cursor.fg.rgb(color.fg.r, color.fg.g, color.fg.b);
      cursor.bg.rgb(color.bg.r, color.bg.g, color.bg.b);
      this.defaultChar = oldBrush;
    },


    clear:function () {
      console.log('\033[2J');
    },


    // Changes the foreground character █ default is [space]
    set brush (character){
      defaultChar = character || ' ';
    },

    get brush (){
      return defaultChar;
    },


    cursorInterface: {
      on: function ()      { cursor.show();  },
      off: function ()     { cursor.hide();  },

      // Resets background & foreground colors
      reset: function ()   { cursor.reset(); },
      
      // Restores colors and places cursor after the graphics
      // so that the drawing does not get drawn over when the
      // program ends
      restore: function () {
        cursor.reset();
        cursor.goto(axel.cols, axel.rows-1);
      }
    },


    get cursor() {
      return this.cursorInterface;
    },

    get rows (){
      return stdo.rows;
    },

    get cols (){
      return stdo.columns;
    },

    goto: function(x, y){
      cursor.goto(parseInt(x), parseInt(y));
    },

    point: function (x, y, char){
      if(!(
          x < 0             || y < 0          ||
          x > stdo.columns  || y > stdo.rows  ||
          x < 0             || y < 0          ||
          x > stdo.columns  || y > stdo.rows
      )){
          cursor.goto(parseInt(x), parseInt(y)).write(char || defaultChar);
      }
    },


    // Get in interpolation point between two points at a given magnitude
    lerp: function (p1, p2, m) {
      return ((p2 - p1) * m) + p1;
    },


    circ: function (x, y, m) {
      var res = m*PI2
        , i
        ;
      
      for(i=0; i< res; i+=1){
        var loc = PI2/res * i;
        this.point(x+Math.sin(loc)*m,y+Math.cos(loc)*m/2);
      }
    },


    box: function (x1, y1, w, h) {
      var line = ''
        , x
        , y
        ;

      for (x=0; x< w; x+=1) {
        line+=this.brush;
      }

      for (y=0; y< h; y+=1) {
        cursor.goto(x1,y1+y).write(line);
      } 
    },


    // Get the distance between two points
    dist: function (x1, y1, x2, y2){
      return Math.sqrt(Math.pow(x2-x1,2) + Math.pow(y2-y1,2));
    },


    // Get all the points along a line and draw them
    line: function (x1, y1, x2, y2) {
      var D = this.dist(x1, y1, x2, y2)+1;

      // this.point(x1, y1);
      for(var i=0; i< D; i++){
        var m = 1 / D * i
          , x = this.lerp(x1, x2, m)
          , y = this.lerp(y1, y2, m)
          ;
        this.point(x, y);
      }
      // this.point(x2, y2);
    },

    color: function (hex, g, b, a) {
      // if 
    },

    text: function(x, y, text){
      cursor.goto(x,y).write(text);
    },


    // moveTo: function (x, y) {
    //   cursor.moveTo()
    // },


    // Changes foreground color
    fg: function (r, g, b) {
      cursor.fg.rgb(r,g,b);
    },

    // Changes background color
    bg: function (r, g, b) {
      cursor.bg.rgb(r,g,b);
    },

    draw: function(cb){
      with(this){
        cb();
      }
    }
  };
  
  module.exports = axel;

}());