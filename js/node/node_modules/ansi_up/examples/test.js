


//
// PRIVATE FUNCTIONS
//

// ES5 template string transformer
function rgx(tmplObj, ...subst) {
    // Use the 'raw' value so we don't have to double backslash in a template string
    let regexText = tmplObj.raw[0];

    // Remove white-space and comments
    let wsrgx = /^\s+|\s+\n|\s*#[\s\S]*?\n|\n/gm;
    let txt2 = regexText.replace(wsrgx, '');
    console.log(txt2);
    return new RegExp(txt2);
}

// ES5 template string transformer
// Multi-Line On
function rgxM(tmplObj, ...subst) {
    // Use the 'raw' value so we don't have to double backslash in a template string
    let regexText = tmplObj.raw[0];

    // Remove white-space and comments
    let wsrgx = /^\s+|\s+\n|\s+#[\s\S]+?\n/gm;
    let txt2 = regexText.replace(wsrgx, '');
    return new RegExp(txt2, 'm');
}

// MATCH A CSI
let csi_regex = rgx`
    ^                           # beginning of line
                                #
                                # First attempt
    (?:                         # begin non-capture block
      \x1b\[
      ([\x3c-\x3f]?)              # a private-mode char (<=>?)
      ([\d;]*)                    # any digits or semicolons
      ([\x20-\x2f]?               # an intermediate modifier
      [\x40-\x7e])                # the command
    )
    |                           # alternate
                                # Second  attempt
    (?:                         # begin non-capture block
      \x1b\[
      [\x20-\x7e]*
      ([\x00-\x1f:])
    )
`;

function show_match(txt, rgx)
{
    console.log(JSON.stringify(txt));
    let res = txt.match(rgx);
    if (res == null)
        return console.log("Still possible...");
    if (res[4])
        return console.log("BAD MATCH: ", JSON.stringify(res[0]));
    console.log(res.slice(1,4));
}
show_match("\x1b[\n",       csi_regex);
show_match("\x1b[34\nm",    csi_regex);
show_match("\x1b[34;44m",   csi_regex);
show_match("\x1b[33:44m",   csi_regex);
show_match("\x1b[>33;44m",  csi_regex);
show_match("\x1b[33;44",    csi_regex);
show_match("\x1b[",         csi_regex);

