#!/usr/bin/python3
from pathlib import Path
import sys

from tools.process_examine import (
    load_file,
    load_testing_js_mappings,
    build_process_tree,
    convert_pids_to_gantt,
    get_load,
    append_dict_to_df,
)


filter_str = None
if len(sys.argv) > 2:
    filter_str = sys.argv[2]
if len(sys.argv) > 3:
    start_pid = int(sys.argv[3])
main_log_file = Path(sys.argv[1])
PID_TO_FN = load_testing_js_mappings(main_log_file)
load_file(main_log_file, PID_TO_FN, filter_str)

