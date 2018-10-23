import "js/nodejs.jsx";
import "stemmer.jsx";

import "arabic-stemmer.jsx";
import "danish-stemmer.jsx";
import "dutch-stemmer.jsx";
import "english-stemmer.jsx";
import "finnish-stemmer.jsx";
import "french-stemmer.jsx";
import "german-stemmer.jsx";
import "hungarian-stemmer.jsx";
import "irish-stemmer.jsx";
import "italian-stemmer.jsx";
import "norwegian-stemmer.jsx";
import "porter-stemmer.jsx";
import "portuguese-stemmer.jsx";
import "romanian-stemmer.jsx";
import "russian-stemmer.jsx";
import "spanish-stemmer.jsx";
import "swedish-stemmer.jsx";
import "turkish-stemmer.jsx";

class _Main
{
    static function usage () : void
    {
        log "usage: jsx_stemwords [-l <language>] [-i <input file>] [-o <output file>] [-c <character encoding>] [-p[2]] [-h]\n";
	log "The input file consists of a list of words to be stemmed, one per";
	log "line. Words should be in lower case, but (for English) A-Z letters";
	log "are mapped to their a-z equivalents anyway. If omitted, stdin is";
	log "used.\n";
	log "If -c is given, the argument is the character encoding of the input";
        log "and output files.  If it is omitted, the UTF-8 encoding is used.\n";
	log "If -p is given the output file consists of each word of the input";
	log "file followed by \"->\" followed by its stemmed equivalent.";
	log "If -p2 is given the output file is a two column layout containing";
	log  "the input words in the first column and the stemmed eqivalents in";
	log "the second column.\n";
	log "Otherwise, the output file consists of the stemmed words, one per";
	log "line.\n";
	log "-h displays this help";
    }

    static function main(args : string[]) : void
    {
        if (args.length < 5)
        {
            _Main.usage();
        }
        else
        {
            var pretty = 0;
            var input = '';
            var output = '';
            var encoding = 'utf8';
            var language = 'English';
            var show_help = false;
            while (args.length > 0)
            {
                var arg = args.shift();
                switch (arg)
                {
                case "-h":
                    show_help = true;
                    args.length = 0;
                    break;
                case "-p":
                    pretty = 1;
                    break;
                case "-p2":
                    pretty = 2;
                    break;
                case "-l":
                    if (args.length == 0)
                    {
                        show_help = true;
                        break;
                    }
                    language = args.shift();
                    break;
                case "-i":
                    if (args.length == 0)
                    {
                        show_help = true;
                        break;
                    }
                    input = args.shift();
                    break;
                case "-o":
                    if (args.length == 0)
                    {
                        show_help = true;
                        break;
                    }
                    output = args.shift();
                    break;
                case "-c":
                    if (args.length == 0)
                    {
                        show_help = true;
                        break;
                    }
                    encoding = args.shift();
                    break;
                }
            }
            if (show_help || input == '' || output == '')
            {
                _Main.usage();
            }
            else
            {
                _Main.stemming(language, input, output, encoding, pretty);
            }
        }
    }

    static function stemming (lang : string, input : string, output : string, encoding : string, pretty : int) : void
    {
        var lines = node.fs.readFileSync(input, encoding).split("\n");
        var stemmer = _Main.create(lang);
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
        node.fs.writeFileSync(output, lines.join('\n'), encoding);
    }

    static function create (algorithm : string) : Stemmer
    {
        var stemmer : Stemmer;
        switch (algorithm.toLowerCase())
        {
        case "arabic":
            stemmer = new ArabicStemmer();
            break;
        case "danish":
            stemmer = new DanishStemmer();
            break;
        case "dutch":
            stemmer = new DutchStemmer();
            break;
        case "english":
            stemmer = new EnglishStemmer();
            break;
        case "finnish":
            stemmer = new FinnishStemmer();
            break;
        case "french":
            stemmer = new FrenchStemmer();
            break;
        case "german":
            stemmer = new GermanStemmer();
            break;
        case "hungarian":
            stemmer = new HungarianStemmer();
            break;
        case "irish":
            stemmer = new IrishStemmer();
            break;
        case "italian":
            stemmer = new ItalianStemmer();
            break;
        case "norwegian":
            stemmer = new NorwegianStemmer();
            break;
        case "porter":
            stemmer = new PorterStemmer();
            break;
        case "portuguese":
            stemmer = new PortugueseStemmer();
            break;
        case "romanian":
            stemmer = new RomanianStemmer();
            break;
        case "russian":
            stemmer = new RussianStemmer();
            break;
        case "spanish":
            stemmer = new SpanishStemmer();
            break;
        case "swedish":
            stemmer = new SwedishStemmer();
            break;
        case "turkish":
            stemmer = new TurkishStemmer();
            break;
        default:
            stemmer = new EnglishStemmer();
            break;
        }
        return stemmer;
    }
}

