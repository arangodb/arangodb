# Copyright (C) 2018 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# Python 2/3 Compatibility (ICU-20299)
# TODO(ICU-20301): Remove this.
from __future__ import print_function

import argparse
import glob as pyglob
import json
import os
import sys

from . import *
from .comment_stripper import CommentStripper
from .request_types import CopyRequest
from .renderers import makefile, unix_exec, windows_exec
from . import filtration, utils
import BUILDRULES

flag_parser = argparse.ArgumentParser(
    description = """Generates rules for building ICU binary data files from text
and other input files in source control.

Use the --mode option to declare how to execute those rules, either exporting
the rules to a Makefile or spawning child processes to run them immediately:

  --mode=gnumake prints a Makefile to standard out.
  --mode=unix-exec spawns child processes in a Unix-like environment.
  --mode=windows-exec spawns child processes in a Windows-like environment.

Tips for --mode=unix-exec
=========================

Create two empty directories for out_dir and tmp_dir. They will get filled
with a lot of intermediate files.

Set LD_LIBRARY_PATH to include the lib directory. e.g., from icu4c/source:

  $ LD_LIBRARY_PATH=lib PYTHONPATH=data python3 -m buildtool ...

Once buildtool finishes, you have compiled the data, but you have not packaged
it into a .dat or .so file. This is done by the separate pkgdata tool in bin.
Read the docs of pkgdata:

  $ LD_LIBRARY_PATH=lib ./bin/pkgdata --help

Example command line to call pkgdata:

  $ LD_LIBRARY_PATH=lib ./bin/pkgdata -m common -p icudt63l -c \\
      -O data/icupkg.inc -s $OUTDIR -d $TMPDIR $TMPDIR/icudata.lst

where $OUTDIR and $TMPDIR are your out and tmp directories, respectively.
The above command will create icudt63l.dat in the tmpdir.

Command-Line Arguments
======================
""",
    formatter_class = argparse.RawDescriptionHelpFormatter
)

arg_group_required = flag_parser.add_argument_group("required arguments")
arg_group_required.add_argument(
    "--mode",
    help = "What to do with the generated rules.",
    choices = ["gnumake", "unix-exec", "windows-exec"],
    required = True
)

flag_parser.add_argument(
    "--src_dir",
    help = "Path to data source folder (icu4c/source/data).",
    default = "."
)
flag_parser.add_argument(
    "--filter_file",
    metavar = "PATH",
    help = "Path to an ICU data filter JSON file.",
    default = None
)
flag_parser.add_argument(
    "--include_uni_core_data",
    help = "Include the full Unicode core data in the dat file.",
    default = False,
    action = "store_true"
)
flag_parser.add_argument(
    "--seqmode",
    help = "Whether to optimize rules to be run sequentially (fewer threads) or in parallel (many threads). Defaults to 'sequential', which is better for unix-exec and windows-exec modes. 'parallel' is often better for massively parallel build systems.",
    choices = ["sequential", "parallel"],
    default = "sequential"
)

arg_group_exec = flag_parser.add_argument_group("arguments for unix-exec and windows-exec modes")
arg_group_exec.add_argument(
    "--out_dir",
    help = "Path to where to save output data files.",
    default = "icudata"
)
arg_group_exec.add_argument(
    "--tmp_dir",
    help = "Path to where to save temporary files.",
    default = "icutmp"
)
arg_group_exec.add_argument(
    "--tool_dir",
    help = "Path to where to find binary tools (genrb, etc).",
    default = "../bin"
)
arg_group_exec.add_argument(
    "--tool_cfg",
    help = "The build configuration of the tools. Used in 'windows-exec' mode only.",
    default = "x86/Debug"
)


class Config(object):

    def __init__(self, args):
        # Process arguments
        self.max_parallel = (args.seqmode == "parallel")

        # Boolean: Whether to include core Unicode data files in the .dat file
        self.include_uni_core_data = args.include_uni_core_data

        # Default fields before processing filter file
        self.filters_json_data = {}

        # Process filter file
        if args.filter_file:
            try:
                with open(args.filter_file, "r") as f:
                    print("Note: Applying filters from %s." % args.filter_file, file=sys.stderr)
                    self._parse_filter_file(f)
            except IOError:
                print("Error: Could not read filter file %s." % args.filter_file, file=sys.stderr)
                exit(1)

        # Either "unihan" or "implicithan"
        self.coll_han_type = "unihan"
        if "collationUCAData" in self.filters_json_data:
            self.coll_han_type = self.filters_json_data["collationUCAData"]

    def _parse_filter_file(self, f):
        # Use the Hjson parser if it is available; otherwise, use vanilla JSON.
        try:
            import hjson
            self.filters_json_data = hjson.load(f)
        except ImportError:
            self.filters_json_data = json.load(CommentStripper(f))

        # Optionally pre-validate the JSON schema before further processing.
        # Some schema errors will be caught later, but this step ensures
        # maximal validity.
        try:
            import jsonschema
            schema_path = os.path.join(os.path.dirname(__file__), "filtration_schema.json")
            with open(schema_path) as schema_f:
                schema = json.load(CommentStripper(schema_f))
            validator = jsonschema.Draft4Validator(schema)
            for error in validator.iter_errors(self.filters_json_data, schema):
                print("WARNING: ICU data filter JSON file:", error.message,
                    "at", "".join(
                        "[%d]" % part if isinstance(part, int) else ".%s" % part
                        for part in error.absolute_path
                    ),
                    file=sys.stderr)
        except ImportError:
            print("Tip: to validate your filter file, install the Pip package 'jsonschema'", file=sys.stderr)
            pass


def add_copy_input_requests(requests, config, common_vars):
    files_to_copy = set()
    for request in requests:
        for f in request.all_input_files():
            if isinstance(f, InFile):
                files_to_copy.add(f)

    result = []
    id = 0

    json_data = config.filters_json_data["fileReplacements"]
    dirname = json_data["directory"]
    for directive in json_data["replacements"]:
        input_file = LocalFile(dirname, directive["src"])
        output_file = InFile(directive["dest"])
        result += [
            CopyRequest(
                name = "input_copy_%d" % id,
                input_file = input_file,
                output_file = output_file
            )
        ]
        files_to_copy.remove(output_file)
        id += 1

    for f in files_to_copy:
        result += [
            CopyRequest(
                name = "input_copy_%d" % id,
                input_file = SrcFile(f.filename),
                output_file = f
            )
        ]
        id += 1

    result += requests
    return result


def main():
    args = flag_parser.parse_args()
    config = Config(args)

    if args.mode == "gnumake":
        makefile_vars = {
            "SRC_DIR": "$(srcdir)",
            "IN_DIR": "$(srcdir)",
            "INDEX_NAME": "res_index"
        }
        makefile_env = ["ICUDATA_CHAR", "OUT_DIR", "TMP_DIR"]
        common = {
            key: "$(%s)" % key
            for key in list(makefile_vars.keys()) + makefile_env
        }
        common["GLOB_DIR"] = args.src_dir
    else:
        common = {
            # GLOB_DIR is used now, whereas IN_DIR is used during execution phase.
            # There is no useful distinction in unix-exec or windows-exec mode.
            "GLOB_DIR": args.src_dir,
            "SRC_DIR": args.src_dir,
            "IN_DIR": args.src_dir,
            "OUT_DIR": args.out_dir,
            "TMP_DIR": args.tmp_dir,
            "INDEX_NAME": "res_index",
            # TODO: Pull this from configure script:
            "ICUDATA_CHAR": "l"
        }

    def glob(pattern):
        result_paths = pyglob.glob("{GLOB_DIR}/{PATTERN}".format(
            GLOB_DIR = args.src_dir,
            PATTERN = pattern
        ))
        # For the purposes of buildtool, force Unix-style directory separators.
        return [v.replace("\\", "/")[len(args.src_dir)+1:] for v in sorted(result_paths)]

    requests = BUILDRULES.generate(config, glob, common)
    requests = filtration.apply_filters(requests, config)
    requests = utils.flatten_requests(requests, config, common)

    if "fileReplacements" in config.filters_json_data:
        tmp_in_dir = "{TMP_DIR}/in".format(**common)
        if makefile_vars:
            makefile_vars["IN_DIR"] = tmp_in_dir
        else:
            common["IN_DIR"] = tmp_in_dir
        requests = add_copy_input_requests(requests, config, common)

    build_dirs = utils.compute_directories(requests)

    if args.mode == "gnumake":
        print(makefile.get_gnumake_rules(
            build_dirs,
            requests,
            makefile_vars,
            common_vars = common
        ))
    elif args.mode == "windows-exec":
        return windows_exec.run(
            build_dirs = build_dirs,
            requests = requests,
            common_vars = common,
            tool_dir = args.tool_dir,
            tool_cfg = args.tool_cfg
        )
    elif args.mode == "unix-exec":
        return unix_exec.run(
            build_dirs = build_dirs,
            requests = requests,
            common_vars = common,
            tool_dir = args.tool_dir
        )
    else:
        print("Mode not supported: %s" % args.mode)
        return 1
    return 0

if __name__ == "__main__":
    exit(main())
