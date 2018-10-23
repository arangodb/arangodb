/* ansi_up.js
 * author : Dru Nelson
 * license : MIT
 * http://github.com/drudru/ansi_up
 */

"use strict";

interface AU_Color
{
    rgb:number[];
    class_name:string;
}

// Represents the output of process_ansi(): a snapshot of the AnsiUp state machine
// at a given point in time, which wraps a fragment of text. This wouuld allow deferred
// processing of text fragments and colors, if ever needed.
interface TextWithData {
    fg:AU_Color;
    bg:AU_Color;
    bright:boolean;
    text:string;
}

// Represents an object that is responsible for generating output from parsed ANSI color
// metadata and text content.
interface Formatter {
    // Invoked for each generated TextWithData fragment outputted by process_ansi().
    // this function is responsible for generating output for a single TextWithData
    // fragment. The result of transform() will be collected into an array that will
    // be provided to compose().
    transform(fragment:TextWithData, instance:AnsiUp):any;

    // Invoked on the set of outputs from transform; the return value of this function
    // will be the final output of ANSI processing.
    compose(segments:any[], instance:AnsiUp):any;
}

// ES5 template string transformer
// NOTE: default is multiline (workaround for now til I can
// determine flags inline)
function rgx(tmplObj, ...subst) {
    // Use the 'raw' value so we don't have to double backslash in a template string
    let regexText:string = tmplObj.raw[0];

    // Remove white-space and comments
    let wsrgx = /^\s+|\s+\n|\s+#[\s\S]+?\n/gm;
    let txt2 = regexText.replace(wsrgx, '');
    return new RegExp(txt2, 'm');
}

class AnsiUp
{
    VERSION = "2.0.2";

    ansi_colors =
    [
        // Normal colors
        [
        { rgb: [  0,   0,   0],  class_name: "ansi-black"   },
        { rgb: [187,   0,   0],  class_name: "ansi-red"     },
        { rgb: [  0, 187,   0],  class_name: "ansi-green"   },
        { rgb: [187, 187,   0],  class_name: "ansi-yellow"  },
        { rgb: [  0,   0, 187],  class_name: "ansi-blue"    },
        { rgb: [187,   0, 187],  class_name: "ansi-magenta" },
        { rgb: [  0, 187, 187],  class_name: "ansi-cyan"    },
        { rgb: [255, 255, 255],  class_name: "ansi-white"   }
        ],

        // Bright colors
        [
        { rgb: [ 85,  85,  85],  class_name: "ansi-bright-black"   },
        { rgb: [255,  85,  85],  class_name: "ansi-bright-red"     },
        { rgb: [  0, 255,   0],  class_name: "ansi-bright-green"   },
        { rgb: [255, 255,  85],  class_name: "ansi-bright-yellow"  },
        { rgb: [ 85,  85, 255],  class_name: "ansi-bright-blue"    },
        { rgb: [255,  85, 255],  class_name: "ansi-bright-magenta" },
        { rgb: [ 85, 255, 255],  class_name: "ansi-bright-cyan"    },
        { rgb: [255, 255, 255],  class_name: "ansi-bright-white"   }
        ]
    ];

    htmlFormatter:Formatter = {
        transform(fragment:TextWithData, instance:AnsiUp):string {
            let txt = fragment.text;

            if (txt.length === 0)
                return txt;

            if (instance._escape_for_html)
                txt = instance.old_escape_for_html(txt);

            // If colors not set, default style is used
            if (!fragment.bright && fragment.fg === null && fragment.bg === null)
                return txt;

            let styles:string[] = [];
            let classes:string[] = [];

            let fg = fragment.fg;
            let bg = fragment.bg;

            // Handle the case where we are told to be bright, but without a color
            if (fg === null && fragment.bright)
                fg = instance.ansi_colors[1][7];

            if (!instance._use_classes) {
                // USE INLINE STYLES
                if (fg)
                    styles.push(`color:rgb(${fg.rgb.join(',')})`);
                if (bg)
                    styles.push(`background-color:rgb(${bg.rgb})`);
            } else {
                // USE CLASSES
                if (fg) {
                    if (fg.class_name !== 'truecolor') {
                        classes.push(`${fg.class_name}-fg`);
                    } else {
                        styles.push(`color:rgb(${fg.rgb.join(',')})`);
                    }
                }
                if (bg) {
                    if (bg.class_name !== 'truecolor') {
                        classes.push(`${bg.class_name}-bg`);
                    } else {
                        styles.push(`background-color:rgb(${bg.rgb.join(',')})`);
                    }
                }
            }

            let class_string = '';
            let style_string = '';

            if (classes.length)
                class_string = ` class="${classes.join(' ')}"`;

            if (styles.length)
                style_string = ` style="${styles.join(';')}"`;

            return `<span${class_string}${style_string}>${txt}</span>`;
        },

        compose(segments:string[], instance:AnsiUp):string {
            return segments.join("");
        }
    };

    textFormatter:Formatter = {
        transform(fragment:TextWithData, instance:AnsiUp):string {
            return fragment.text;
        },

        compose(segments:string[], instance:AnsiUp):string {
            return segments.join("");
        }
    };

    // 256 Colors Palette
    // CSS RGB strings - ex. "255, 255, 255"
    private palette_256:AU_Color[];

    private fg:AU_Color;
    private bg:AU_Color;
    private bright:boolean;

    private _use_classes:boolean;
    private _escape_for_html;
    private _sgr_regex:RegExp;

    private _buffer:string;

    constructor()
    {
        this.setup_256_palette();
        this._use_classes = false;
        this._escape_for_html = true;

        this.bright = false;
        this.fg = this.bg = null;

        this._buffer = '';
    }

    set use_classes(arg:boolean)
    {
        this._use_classes = arg;
    }

    get use_classes():boolean
    {
        return this._use_classes;
    }

    set escape_for_html(arg:boolean)
    {
        this._escape_for_html = arg;
    }

    get escape_for_html():boolean
    {
        return this._escape_for_html;
    }

    private setup_256_palette():void
    {
        this.palette_256 = [];

        // Index 0..15 : Ansi-Colors
        this.ansi_colors.forEach( palette => {
            palette.forEach( rec => {
                this.palette_256.push(rec);
            });
        });

        // Index 16..231 : RGB 6x6x6
        // https://gist.github.com/jasonm23/2868981#file-xterm-256color-yaml
        let levels = [0, 95, 135, 175, 215, 255];
        for (let r = 0; r < 6; ++r) {
            for (let g = 0; g < 6; ++g) {
                for (let b = 0; b < 6; ++b) {
                    let col = {rgb:[levels[r], levels[g], levels[b]], class_name:'truecolor'};
                    this.palette_256.push(col);
                }
            }
        }

        // Index 232..255 : Grayscale
        let grey_level = 8;
        for (let i = 0; i < 24; ++i, grey_level += 10) {
            let gry = {rgb:[grey_level, grey_level, grey_level], class_name:'truecolor'};
            this.palette_256.push(gry);
        }
    }

    private old_escape_for_html(txt:string):string
    {
      return txt.replace(/[&<>]/gm, (str) => {
        if (str === "&") return "&amp;";
        if (str === "<") return "&lt;";
        if (str === ">") return "&gt;";
      });
    }

    private old_linkify(txt:string):string
    {
      return txt.replace(/(https?:\/\/[^\s]+)/gm, (str) => {
        return `<a href="${str}">${str}</a>`;
      });
    }

    private detect_incomplete_ansi(txt:string)
    {
        // Scan forwards for a potential command character
        // If one exists, we must assume we are good
        // [\x40-\x7e])               # the command

        return !(/.*?[\x40-\x7e]/.test(txt));
    }

    private detect_incomplete_link(txt:string)
    {
        // It would be nice if Javascript RegExp supported
        // a hitEnd() method

        // Scan backwards for first whitespace
        var found = false;
        for (var i = txt.length - 1; i > 0; i--) {
            if (/\s|\x1B/.test(txt[i])) {
                found = true;
                break;
            }
        }

        if (!found) {
            // Handle one other case
            // Maybe the whole string is a URL?
            if (/(https?:\/\/[^\s]+)/.test(txt))
                return 0;
            else
                return -1;
        }

        // Test if possible prefix
        var prefix = txt.substr(i + 1, 4);

        if (prefix.length === 0) return -1;

        if ("http".indexOf(prefix) === 0)
            return (i + 1);
    }

    ansi_to(txt:string, formatter:Formatter):any {
        var pkt = this._buffer + txt;
        this._buffer = '';

        var raw_text_pkts = pkt.split(/\x1B\[/);

        if (raw_text_pkts.length === 1)
            raw_text_pkts.push('');

        this.handle_incomplete_sequences(raw_text_pkts);

        let first_chunk = this.with_state(raw_text_pkts.shift()); // the first pkt is not the result of the split

        let blocks = new Array(raw_text_pkts.length);
        for (let i = 0, len = raw_text_pkts.length; i < len; ++i) {
            blocks[i] = (formatter.transform(this.process_ansi(raw_text_pkts[i]), this));
        }

        if (first_chunk.text.length > 0)
            blocks.unshift(formatter.transform(first_chunk, this));

        return formatter.compose(blocks, this);
    }

    ansi_to_html(txt:string):string
    {
        return this.ansi_to(txt, this.htmlFormatter);
    }

    ansi_to_text(txt:string):string
    {
        return this.ansi_to(txt, this.textFormatter);
    }

    private with_state(text:string):TextWithData {
        return { bright: this.bright, fg: this.fg, bg: this.bg, text: text };
    }

    private handle_incomplete_sequences(chunks:string[]):void {
        // COMPLEX - BEGIN

        // Validate the last chunks for:
        // - incomplete ANSI sequence
        // - incomplete ESC
        // If any of these occur, we may have to buffer
        var last_chunk = chunks[chunks.length - 1];

        // - incomplete ANSI sequence
        if ((last_chunk.length > 0) && this.detect_incomplete_ansi(last_chunk)) {
            this._buffer = "\x1B[" + last_chunk;
            chunks.pop();
            chunks.push('');
        } else {
            // - incomplete ESC
            if (last_chunk.slice(-1) === "\x1B") {
                this._buffer = "\x1B";
                console.log("raw", chunks);
                chunks.pop();
                chunks.push(last_chunk.substr(0, last_chunk.length - 1));
                console.log(chunks);
                console.log(last_chunk);
            }
            // - Incomplete ESC, only one packet
            if (chunks.length === 2 &&
                chunks[1] === "" &&
                chunks[0].slice(-1) === "\x1B") {
                this._buffer = "\x1B";
                last_chunk = chunks.shift();
                chunks.unshift(last_chunk.substr(0, last_chunk.length - 1));
            }
        }

        // COMPLEX - END
    }

    private process_ansi(block:string):TextWithData
    {
      // This must only be called with a string that started with a CSI (the string split above)
      // The CSI must not be in the string. We consider this string to be a 'block'.
      // It has an ANSI command at the front that affects the text that follows it.
      //
      // All ansi codes are typically in the following format. We parse it and focus
      // specifically on the graphics commands (SGR)
      //
      // CONTROL-SEQUENCE-INTRODUCER CSI             (ESC, '[')
      // PRIVATE-MODE-CHAR                           (!, <, >, ?)
      // Numeric parameters separated by semicolons  ('0' - '9', ';')
      // Intermediate-modifiers                      (0x20 - 0x2f)
      // COMMAND-CHAR                                (0x40 - 0x7e)
      //
      // We use a regex to parse into capture groups the PRIVATE-MODE-CHAR to the COMMAND
      // and the following text

      if (!this._sgr_regex) {
          // This regex is designed to parse an ANSI terminal CSI command. To be more specific,
          // we follow the XTERM conventions vs. the various other "standards".
          // http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
          //
          this._sgr_regex = rgx`
            ^                           # beginning of line
            ([!\x3c-\x3f]?)             # a private-mode char (!, <, =, >, ?)
            ([\d;]*)                    # any digits or semicolons
            ([\x20-\x2f]?               # an intermediate modifier
            [\x40-\x7e])                # the command
            ([\s\S]*)                   # any text following this CSI sequence
          `;
      }

      let matches = block.match(this._sgr_regex);

      // The regex should have handled all cases!
      if (!matches) {
          return this.with_state(block);
      }

      let orig_txt = matches[4];

      if (matches[1] !== '' || matches[3] !== 'm') {
          return this.with_state(orig_txt);
      }

      // Ok - we have a valid "SGR" (Select Graphic Rendition)

      let sgr_cmds = matches[2].split(';');

      // Each of these params affects the SGR state

      // Why do we shift through the array instead of a forEach??
      // ... because some commands consume the params that follow !
      while (sgr_cmds.length > 0) {
          let sgr_cmd_str = sgr_cmds.shift();
          let num = parseInt(sgr_cmd_str, 10);

          if (isNaN(num) || num === 0) {
              this.fg = this.bg = null;
              this.bright = false;
          } else if (num === 1) {
              this.bright = true;
          } else if (num === 22) {
              this.bright = false;
          } else if (num === 39) {
              this.fg = null;
          } else if (num === 49) {
              this.bg = null;
          } else if ((num >= 30) && (num < 38)) {
              let bidx = this.bright ? 1 : 0;
              this.fg = this.ansi_colors[bidx][(num - 30)];
          } else if ((num >= 90) && (num < 98)) {
              this.fg = this.ansi_colors[1][(num - 90)];
          } else if ((num >= 40) && (num < 48)) {
              this.bg = this.ansi_colors[0][(num - 40)];
          } else if ((num >= 100) && (num < 108)) {
              this.bg = this.ansi_colors[1][(num - 100)];
          } else if (num === 38 || num === 48) {

              // extended set foreground/background color

              // validate that param exists
              if (sgr_cmds.length > 0) {
                  // extend color (38=fg, 48=bg)
                  let is_foreground = (num === 38);

                  let mode_cmd = sgr_cmds.shift();

                  // MODE '5' - 256 color palette
                  if (mode_cmd === '5' && sgr_cmds.length > 0) {
                      let palette_index = parseInt(sgr_cmds.shift(), 10);
                      if (palette_index >= 0 && palette_index <= 255) {
                          if (is_foreground)
                              this.fg = this.palette_256[palette_index];
                          else
                              this.bg = this.palette_256[palette_index];
                      }
                  }

                  // MODE '2' - True Color
                  if (mode_cmd === '2' && sgr_cmds.length > 2) {
                      let r = parseInt(sgr_cmds.shift(), 10);
                      let g = parseInt(sgr_cmds.shift(), 10);
                      let b = parseInt(sgr_cmds.shift(), 10);

                      if ((r >= 0 && r <= 255) && (g >= 0 && g <= 255) && (b >= 0 && b <= 255)) {
                          let c = { rgb: [r,g,b], class_name: 'truecolor'};
                          if (is_foreground)
                              this.fg = c;
                          else
                              this.bg = c;
                      }
                  }
              }
          }
      }

      return this.with_state(orig_txt);
    }
}
