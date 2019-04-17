


//
// PRIVATE FUNCTIONS
//

// ES5 template string transformer
function rgxG(tmplObj, ...subst) {
    // Use the 'raw' value so we don't have to double backslash in a template string
    let regexText = tmplObj.raw[0];

    // Remove white-space and comments
    let wsrgx = /^\s+|\s+\n|\s*#[\s\S]*?\n|\n/gm;
    let txt2 = regexText.replace(wsrgx, '');
    console.log(txt2);
    //return new RegExp(txt2, 'y');
    return new RegExp(txt2, 'g');
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


osc_st = rgxG`                                                                                                           
(?:                         # legal sequence                                                                              
	(\x1b\\)                    # ESC \                                                                                     
	|                           # alternate                                                                                 
	(\x07)                      # BEL (what xterm did)                                                                      
)                                                                                                                         
	|                           # alternate (second attempt)                                                                  
(                           # illegal sequence                                                                            
	[\x00-\x06]                 # anything illegal                                                                          
	|                           # alternate                                                                                 
	[\x08-\x1a]                 # anything illegal                                                                          
	|                           # alternate                                                                                 
	[\x1c-\x1f]                 # anything illegal                                                                          
)                                                                                                                         
`;                                                                                                                            

function show_match(txt, rgx)
{
    console.log(JSON.stringify(txt));
    //rgx.lastIndex = 0;
    //let res = txt.match(rgx);
    let res = rgx.exec(txt);
    if (res == null)
    {
        rgx.lastIndex = 0;
        return console.log("Still possible...");
    }
    if (res[3])
    {
        rgx.lastIndex = 0;
        return console.log("BAD MATCH: ", JSON.stringify(res[0]));
    }
    console.log(res.slice(1,3));
    //console.log(res);
    console.log(rgx.lastIndex);
	rgx.lastIndex = 0;
}

function show_match_repeat(txt, rgx)
{
    console.log(JSON.stringify(txt));
    //rgx.lastIndex = 0;
    //let res = txt.match(rgx);
    let res = rgx.exec(txt);
    if (res == null)
        return console.log("RPT Still possible...");

    if (res[3])
        return console.log("RPT BAD MATCH: ", JSON.stringify(res[0]));

    console.log('RPT: ' + JSON.stringify(res.slice(1,3)) + " - " + rgx.lastIndex);
    //console.log(res);
}
show_match("\x1b]\n\x07",     osc_st);
show_match("\x1b]\x07",       osc_st);
show_match("\x1b]8;;\x07",    osc_st);
show_match("\x1b]8;;\x1b\\",  osc_st);
show_match("\x1b]8;;",        osc_st);
show_match("\x1b]8;;\b",      osc_st);

console.log("// Valid");
osc_st.lastIndex = 0;
var str = "\x1b]8;;http://example.com/\x07TEST\x1b]8;;\x07";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// Newline");
osc_st.lastIndex = 0;
var str = "\x1b]8;;http://example.com/\x07TEST\n\x1b]8;;\x07";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// Still Bad");
osc_st.lastIndex = 0;
var str = "\x1b]8;;http://example.com/\x1b\\TEST\n\x1b]8;;\x07";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// Standard St");
osc_st.lastIndex = 0;
var str = "\x1b]8;;http://example.com/\x1b\\TEST\x1b]8;;\x1b\\";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// Legal but bad");
osc_st.lastIndex = 0;
var str = "\x1b]8;;http://example.com/\x1b\x07TEST\x1b]8;;\x1b\\";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// Mixed St");
osc_st.lastIndex = 0;
var str = "\x1b]8;;http://example.com/\x07TEST\x1b]8;;\x1b\\";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// TEST ST");
osc_st.lastIndex = 0;
var str = "\x1b]\x07\x1b]8\x07";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

console.log("// TEST ST");
osc_st.lastIndex = 0;
var str = "\x1b]\x1b\\\x1b]8\x1b\\";
show_match_repeat(str, osc_st);
show_match_repeat(str, osc_st);
console.log(" ");

//show_match("\x1b[34\nm",    osc_st);
//show_match("\x1b[34;44m",   osc_st);
//show_match("\x1b[33:44m",   osc_st);
//show_match("\x1b[>33;44m",  osc_st);
//show_match("\x1b[33;44",    osc_st);
//show_match("\x1b[",         osc_st);

