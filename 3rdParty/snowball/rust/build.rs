use std::env;
use std::fs;
use std::fs::{OpenOptions};
use std::io::Write;
use std::path::Path;


// This build script makes the code independent from the algorithms declared
// in the makefile.
// We check which stemmers were generated and then produce the corresponding
// includes for src/algorithms/mod.rs and a closure for src/main.rs to match
// strings to stemmers
fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    let lang_match_path = Path::new(&out_dir).join("lang_matches.rs");
    let lang_include_path = Path::new(&out_dir).join("lang_include.rs");
    let mut lang_match_file = OpenOptions::new().write(true).create(true).truncate(true).open(&lang_match_path).unwrap();
    let mut lang_include_file = OpenOptions::new().write(true).create(true).truncate(true).open(&lang_include_path).unwrap();

    let src_dir = Path::new(&env::var("CARGO_MANIFEST_DIR").unwrap()).join("src");
    let algo_dir = src_dir.join("snowball/algorithms");

    lang_match_file.write_all(b"
         move |lang:String|{
         match lang.as_str() {")
        .unwrap();

    for file in fs::read_dir(&algo_dir).unwrap() {
        let file = file.unwrap();
        let path = file.path();
        let filestem = path.file_stem().unwrap().to_str().unwrap();
        if path.is_file() && filestem != "mod" {
            //Also we need to copy all the stemmer files into OUT_DIR...
            fs::copy(&path, Path::new(&out_dir).join(file.file_name())).unwrap();
            let split = filestem.len() - 8;
            let langname = &filestem[..split];
            writeln!(&mut lang_match_file,
                     "\"{}\" => Stemmer {{ stemmer: Box::new(snowball::algorithms::{}_stemmer::stem)}},",
                     langname,
                     langname)
                .unwrap();

            writeln!(&mut lang_include_file, "pub mod {}_stemmer;", langname).unwrap();
                
        }
    }

    lang_match_file.write_all(b"
            x => panic!(\"Unknown algorithm '{}'\", x)
        }
      }
    ")
        .unwrap();

}
