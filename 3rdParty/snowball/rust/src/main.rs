use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::path::Path;
use std::env;
use std::borrow::Cow;

pub mod snowball;

use snowball::SnowballEnv;


fn usage(name: &str) {
    println!("{} -l <language> [-i <input file>] [-o <output file>]
The input file consists of a list of words to be stemmed, one per
line. Words should be in lower case, but (for English) A-Z letters
are mapped to their a-z equivalents anyway. If omitted, stdin is
used.", name);
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        usage(&args[0]);
    } else {
        let mut language = None;
        let mut input_arg = None;
        let mut output_arg = None;
        let mut i = 1;
        while i < args.len() {
            match args[i].as_str() {
                "-l" => {
                    language = Some(args[i+1].clone());
                    i += 2;
                },
                "-i" => {
                    input_arg = Some(args[i+1].clone());
                    i += 2;
                },
                "-o" => {
                    output_arg = Some(args[i+1].clone());
                    i += 2;
                },
                x => {
                    println!("Unrecognized option '{}'", x);
                    usage(&args[0]);
                    return
                }
            }
        }
        if language.is_none() {
            println!("Please specify a language!");
            usage(&args[0]);
            return;
        }
        let stemmer = Stemmer::create(language.unwrap());
        

        let mut output = if let Some(output_file) = output_arg {
            Box::new(File::create(Path::new(&output_file)).unwrap()) as Box<Write>
        } else {
            Box::new(std::io::stdout()) as Box<Write>
        };

        if let Some(input_file) = input_arg {
            for line in BufReader::new(File::open(Path::new(&input_file)).unwrap()).lines() {
                writeln!(&mut output, "{}", stemmer.stem(&line.unwrap())).unwrap();
            }
        } else {
            let stdin = std::io::stdin();
            for line in stdin.lock().lines() {
                writeln!(&mut output, "{}", stemmer.stem(&line.unwrap())).unwrap();
            }
        }        
    }
}


/// Wraps a usable interface around the actual stemmer implementation
pub struct Stemmer {
    stemmer: Box<Fn(&mut SnowballEnv) -> bool>,
}

impl Stemmer {
    /// Create a new stemmer from an algorithm
    pub fn create(lang: String) -> Self {
        // Have a look at ../build.rs
        // There we generate a file that is rust code for a closure that returns a stemmer.
        // We match against all the algorithms in src/snowball/algoritms/ folder.
        // Alas, this cannot be included as a match statement or function because of Rust's
        // hygenic macros.
        let match_language = include!(concat!(env!("OUT_DIR"), "/lang_matches.rs"));
        match_language(lang)
    }

    /// Stem a single word
    /// Please note, that the input is expected to be all lowercase (if that is applicable).
    pub fn stem<'a>(&self, input: &'a str) -> Cow<'a, str> {
        let mut env = SnowballEnv::create(input);
        (self.stemmer)(&mut env);
        env.get_current()
    }
}
