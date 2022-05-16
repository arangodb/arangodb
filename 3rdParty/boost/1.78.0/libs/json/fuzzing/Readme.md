# Fuzzing

Boost json has support for fuzzing. Clang/libFuzzer is used.

Fuzzing is made on pull requests, using a github action CI job.
The job does the following

 - compiles the fuzzers with address and undefined sanitizers turned on
 - runs all unit test inputs
 - reruns old crashing input from the old_crashes folder
 - downloads corpus from the last run (stored on bintray)
 - fuzzes for a short time (30 seconds per fuzzer)
 - minimizes the corpus and uploads it to bintray

The idea with storing crashes in the old_crashes folder is that once a bug is detected,
the offending input can be added so it prevents the CI job from succeeding until the bug
is fixed.

## Building and running the fuzzers locally
Execute the fuzzing/fuzz.sh script. You need clang++ installed. The fuzzer script will start fuzzing for a limited time, interrupt it if you wish.

There are several fuzzers, to exercise different parts of the api, following the usage examples in the documentation.

## Running a specific fuzzer manually
Either modify the fuzz.sh script, or run it first so the fuzzer is compiled with the proper flags.

Each fuzzer is a separate binary, an example with the basic_parser fuzzer is shown below:
```sh
mkdir -p out
./fuzzer_basic_parser out/
```

If you want to run a specific input (say an old crash), use
```sh
./fuzzer_basic_parser path/to/crash.json
```

## Minimizing a crash
If a crash is found, it is good to minimize and clean the crashing input.
This is makes it easier to understand what the problem is.

An example of minimizing and cleaning a real crash:
```sh
./fuzzer_basic_parser out 
# ...crashes and writes the crash-... file
# minimize it:
./fuzzer_basic_parser crash-1f8f27db1fcb30f32727472867633b7cee66d045 -minimize_crash=1 -exact_artifact_path=minimized_crash.json -max_total_time=100
# minimized_crash.json shrank from 38493 bytes to 4102

# replace irrelevant parts with space:
./fuzzer_basic_parser minimized_crash.json -cleanse_crash=1 -exact_artifact_path=cleaned_crash.json

# result is in cleaned_crash.json, commit it
cp cleaned_crash.json old_crashes/basic_parser/20200903.json
git add old_crashes/basic_parser/20200903.json
```

## Rerunning old crashes
Given a test case testcase.json, build the fuzzer and execute it with the test file:
```sh
./fuzzer_basic_parser testcase.json
```

