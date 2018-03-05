// Stemmer library sample for JSX

import "stemmer.jsx";

import "danish-stemmer.jsx";
import "dutch-stemmer.jsx";
import "english-stemmer.jsx";
import "finnish-stemmer.jsx";
import "french-stemmer.jsx";
import "german-stemmer.jsx";
import "hungarian-stemmer.jsx";
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
        log "$ jsx --run testapp.jsx <algorithm> \"sentence\"...";
    }

    static function main (args : string[]) : void
    {
	if (args.length < 1) {
            _Main.usage();
            return;
        }
        var algorithm = "English";
        if (args.length > 1)
        {
            algorithm = args.shift();
        }
        var stemmer = _Main.create(algorithm);
        var words = args[0].split(/[\s\.-]/);
        for (var i = 0; i < words.length; i++)
        {
            if (words[i] == '')
            {
                continue;
            }
            var original = words[i].toLowerCase();
            log original + " -> " + stemmer.stemWord(original);
        }
    }

    static function create (algorithm : string) : Stemmer
    {
        var stemmer : Stemmer;
        switch (algorithm.toLowerCase())
        {
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
