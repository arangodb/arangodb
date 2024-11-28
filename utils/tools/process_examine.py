#!/usr/bin/python3
import json
from pathlib import Path
import collections
#import pandas as pd
import polars as pd
from datetime import datetime

LOADS = pd.DataFrame()
MEMORY = {}
TREE_TEXTS = []
PARSED_LINES = []
DATE_FORMAT = '%Y-%m-%d %H:%M:%S.%f'

def get_parent_name(processes, ppid):
    key = f"p{ppid}"
    if key in processes:
        # print(json.dumps(processes[key]['process'][0], indent=4))
        return f"{processes[key]['process'][0]['name']} - {processes[key]['process'][0]['cpu_times'][0]} {processes[key]['process'][0]['cpu_times'][1]}"
    return "?"

def get_process_tree_recursive(processes, parent, tree, indent=""):
    """get an ascii representation of the process tree"""
    text = ""
    name = get_parent_name(processes, parent)
    skip = False
    text += f"{parent} {name}\n"
    if parent not in tree:
        return text
    children = tree[parent][:-1]
    for child in children:
        text += indent + "|- "
        text += get_process_tree_recursive(processes, child, tree, indent + "| ")
    child = tree[parent][-1]
    text += indent + "`_ "
    text += get_process_tree_recursive(processes, child, tree, indent + "  ")
    return text

def build_process_tree(parsed_slice, start_pid):
    tree = collections.defaultdict(list)
    for process_id in parsed_slice[1]:
        if process_id == "sys":
            pass
        else:
            tree[one_process['ppid']].append(one_process['pid'])
            # print(struct)
    tree_dump = get_process_tree_recursive(parsed_slice[1], start_pid, tree)
    #TREE_TEXTS.append(tree_dump)
    print(tree_dump)
    return tree_dump

def load_testing_js_mappings(main_log_file):
    arangosh_lookup_file = main_log_file.parents[0] / "job_to_pids.jsonl"
    pid_to_fn = {}
    if arangosh_lookup_file.exists():
        with open(arangosh_lookup_file, "r", encoding="utf-8") as jsonl_file:
            while True:
                line = jsonl_file.readline()
                if len(line) == 0:
                    break
                parsed_slice = json.loads(line)
                pid_to_fn[f"p{parsed_slice['pid']}"] = {'n': Path(parsed_slice["logfile"]).name}
    return pid_to_fn

def load_file(main_log_file, pid_to_fn, filter_str):
    global LOADS
    collect_loads = []
    last_netio = None
    with open(main_log_file, "r", encoding="utf-8") as jsonl_file:
        while True:
            line = jsonl_file.readline()
            if len(line) == 0:
                break
            parsed_slice = json.loads(line)
            PARSED_LINES.append(parsed_slice)
            this_date = datetime.strptime(parsed_slice[0], DATE_FORMAT)
            # tree = collections.defaultdict(list)
            if filter_str and parsed_slice[0].find(filter_str) < 0:
                continue
            #print(parsed_slice[0])
            #print(json.dumps(parsed_slice, indent=4))
            for process_id in parsed_slice[1]:
                if process_id == "sys":
                    sys_stat = parsed_slice[1][process_id]
                    if not last_netio:
                        netio = 0
                    else:
                        netio = sys_stat['netio']['lo'][0] - last_netio
                    last_netio = sys_stat['netio']['lo'][0]
                    collect_loads.append({
                            "Date": this_date,
                            "load": float(sys_stat['load'][0]),
                            "netio": float(netio)
                    })
                    MEMORY[this_date] = sys_stat['netio']['lo'][0]
                    # print(f"L {sys_stat['load'][0]:.1f} - {sys_stat['netio']['lo'][0]:,.3f}")
                    #print(json.dumps(parsed_slice[1][process_id], indent=4))
                    continue
                one_process = parsed_slice[1][process_id]['process'][0]
                #print(json.dumps(one_process, indent=4))
                name = one_process['name']
                if name == 'arangod':
                    #print(json.dumps(one_process, indent=4))
                    try:
                        n = one_process['cmdline'].index('--cluster.my-role')
                        one_process['name'] = f"{name} - {one_process['cmdline'][n+1]}"
                    except ValueError:
                        try:
                            n = one_process['cmdline'].index('--agency.my-address')
                            one_process['name'] = f"{name} - AGENT"
                        except ValueError:
                            one_process['name'] = f"{name} - SINGLE"
                elif name == 'arangosh':
                    # print(json.dumps(one_process, indent=4))
                    pidstr = f"p{one_process['pid']}"
                    if pidstr in pid_to_fn:
                        if not "Task" in pid_to_fn[pidstr]:
                            pid_to_fn[pidstr]["Task"] = pid_to_fn[pidstr]['n']
                            pid_to_fn[pidstr]["Start"] = this_date
                        pid_to_fn[pidstr]["End"] = this_date
                        one_process['name'] = pid_to_fn[pidstr]['n']
                    else:
                        try:
                            n = one_process['cmdline'].index('--')
                            one_process['name'] = f"{name} - {one_process['cmdline'][n+1]}"
                        except ValueError:
                            try:
                                n = one_process['cmdline'].index('--javascript.unit-tests')
                                one_process['name'] = f"{name} - {one_process['cmdline'][n+1]}"
                            except ValueError as ex:
                                #print(ex)
                                # print(json.dumps(one_process, indent=4))
                                pass
                            pass
                elif name == 'python3':
                    try:
                        n = one_process['cmdline'].index('launch')
                        one_process['name'] = f"{name} - launch controller"
                    except ValueError as ex:
                        print(ex)
    LOADS = pd.DataFrame(data=collect_loads)
    # print(pd.to_datetime(LOADS['Date']))

def convert_pids_to_gantt(PID_TO_FN):
    jobs = []
    for one_pid in PID_TO_FN:
        tsk = PID_TO_FN[one_pid]
        if "Task" in tsk:
            # print(f"DOING {one_pid} => {tsk}")
            jobs.append({
                "Task": tsk["Task"],
                "Start": tsk["Start"],
                "Finish": tsk["End"]
            })
        #else:
        #    print(f"skipping {one_pid} => {PID_TO_FN[one_pid]}")
    return jobs

def get_load():
    return LOADS

def append_dict_to_df(df, dict_to_append):
    df = pd.concat([df, pd.DataFrame.from_records([dict_to_append])])
    return df

# Creating an empty dataframe
#df = pd.DataFrame(columns=['a', 'b'])

# Appending a row
#df = append_dict_to_df(df,{ 'a': 1, 'b': 2 })
