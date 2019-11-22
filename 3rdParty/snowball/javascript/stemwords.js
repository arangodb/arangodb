const stemmer = require('base-stemmer.js');

const fs = require('fs');

function usage() {
    console.log("usage: stemwords.js [-l <language>] -i <input file> -o <output file> [-c <character encoding>] [-p[2]] [-h]\n");
    console.log("The input file consists of a list of words to be stemmed, one per");
    console.log("line. Words should be in lower case.\n");
    console.log("If -c is given, the argument is the character encoding of the input");
    console.log("and output files.  If it is omitted, the UTF-8 encoding is used.\n");
    console.log("If -p is given the output file consists of each word of the input");
    console.log("file followed by \"->\" followed by its stemmed equivalent.");
    console.log("If -p2 is given the output file is a two column layout containing");
    console.log("the input words in the first column and the stemmed eqivalents in");
    console.log("the second column.\n");
    console.log("Otherwise, the output file consists of the stemmed words, one per");
    console.log("line.\n");
    console.log("-h displays this help");
}

if (process.argv.length < 5)
{
    usage();
}
else
{
    var pretty = 0;
    var input = '';
    var output = '';
    var encoding = 'utf8';
    var language = 'English';
    var show_help = false;
    while (process.argv.length > 0)
    {
	var arg = process.argv.shift();
	switch (arg)
	{
	case "-h":
	    show_help = true;
	    process.argv.length = 0;
	    break;
	case "-p":
	    pretty = 1;
	    break;
	case "-p2":
	    pretty = 2;
	    break;
	case "-l":
	    if (process.argv.length == 0)
	    {
		show_help = true;
		break;
	    }
	    language = process.argv.shift();
	    break;
	case "-i":
	    if (process.argv.length == 0)
	    {
		show_help = true;
		break;
	    }
	    input = process.argv.shift();
	    break;
	case "-o":
	    if (process.argv.length == 0)
	    {
		show_help = true;
		break;
	    }
	    output = process.argv.shift();
	    break;
	case "-c":
	    if (process.argv.length == 0)
	    {
		show_help = true;
		break;
	    }
	    encoding = process.argv.shift();
	    break;
	}
    }
    if (show_help || input == '' || output == '')
    {
	usage();
    }
    else
    {
	stemming(language, input, output, encoding, pretty);
    }
}

// function stemming (lang : string, input : string, output : string, encoding : string, pretty : int) {
function stemming (lang, input, output, encoding, pretty) {
        var lines = fs.readFileSync(input, encoding).split("\n");
        if (lines[lines.length - 1] == "") {
            lines.pop();
        }
        var stemmer = create(lang);
        for (var i in lines)
        {
            var original = lines[i];
            var stemmed = stemmer.stemWord(original);
            switch (pretty)
            {
            case 0:
                lines[i] = stemmed;
                break;
            case 1:
                lines[i] += " -> " + stemmed;
                break;
            case 2:
                if (original.length < 30)
                {
                    for (var j = original.length; j < 30; j++)
                    {
                        lines[i] += " ";
                    }
                }
                else
                {
                    lines[i] += "\n";
                    for (var j = 0; j < 30; j++)
                    {
                        lines[i] += " ";
                    }
                }
                lines[i] += stemmed;
            }
        }
        lines.push("");
        fs.writeFileSync(output, lines.join('\n'), encoding);
}

function create (name) {
    var lc_name = name.toLowerCase();
    if (!lc_name.match('\\W') && lc_name != 'base') {
	var algo = lc_name.substr(0, 1).toUpperCase() + lc_name.substr(1);
	try {
	    const stemmer = require(lc_name + '-stemmer.js');
	    return Function('return new ' + algo + 'Stemmer()')();
	} catch (error) {
	}
    }
    console.log('Unknown stemming language: ' + name + '\n');
    usage();
    process.exit(1);
}
