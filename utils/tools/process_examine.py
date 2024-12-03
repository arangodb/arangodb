#!/usr/bin/python3
import json
from pathlib import Path
import collections
import pandas as pd
#import polars as pd
from datetime import datetime

from arango import ArangoClient, exceptions

LOADS_COL_NAME="load"
JOBS_COL_NAME="jobs"
CLIENT = ArangoClient(hosts="http://localhost:8529")
SYS_DB = CLIENT.db("_system", username="root", password="")
try:
    LOADS_COL = SYS_DB.create_collection(LOADS_COL_NAME)
except exceptions.CollectionCreateError:
    LOADS_COL = SYS_DB[LOADS_COL_NAME]
LOADS_COL.add_index({'type': 'persistent', 'fields': ['timestamp'], 'unique': False})
try:
    JOBS_COL = SYS_DB.create_collection(JOBS_COL_NAME)
except exceptions.CollectionCreateError:
    JOBS_COL = SYS_DB[JOBS_COL_NAME]


LOADS = pd.DataFrame()
MEMORY = {}
TREE_TEXTS = []
PARSED_LINES = pd.DataFrame()
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

def aggregate_sub_metrics(testing_pid, processes, full_slice):
    testing = processes[testing_pid]
    name = testing['process'][0]['name']
    if not 'children' in testing:
        print(f"skipping {name}")
        full_slice[len(full_slice) - 1][name] = {
            'raw': [],
            'sum': []
        }
        return
    children = testing['children']
    i = 0
    while i < len(children):
        if 'children' in processes[children[i]]:
            children += processes[children[i]]['children']
        i += 1
    sums = {
        'percent': [],
        'iocounters': [],
        'ctxSwitches': [],
        'numfds': [],
        'cpu_times': [],
        'meminfo': [],
        'netcons': [],
        'no_threads': [],
    }
    testing['subsums_raw'] = sums.copy()
    testing['subsums'] = sums.copy()
    testing['subsums']['no_threads'] = [0]
    for child in children:
        for key in testing['subsums'].keys():
            if key == 'no_threads':
                no_threads = len(processes[child].keys()) -1
                testing['subsums_raw'][key].append(no_threads)
                testing['subsums'][key][0] += no_threads
            else:
                testing['subsums_raw'][key].append(processes[child]['process'][0][key])
                if hasattr(processes[child]['process'][0][key], '__len__'):
                    for i, _ in enumerate(processes[child]['process'][0][key]):
                        if len(testing['subsums'][key]) < i:
                            testing['subsums'][key][i] = processes[child]['process'][0][key][i]
                        else:
                            testing['subsums'][key].append(processes[child]['process'][0][key][i])
                else:
                    if len(testing['subsums'][key]) == 0:
                        testing['subsums'][key] = [processes[child]['process'][0][key]]
                    else:
                        testing['subsums'][key][0] += processes[child]['process'][0][key]
    full_slice[len(full_slice) - 1][name] = {
        'raw': testing['subsums_raw'],
        'sum': testing['subsums']
    }

def jobs_db_overview():
    cursor = SYS_DB.aql.execute(
        "FOR doc IN @@col RETURN doc",
        bind_vars={"@col": JOBS_COL_NAME},
        batch_size=9999)
    print(cursor.empty())
    return [
        {
            "Task": doc["Task"],
            "Start": pd.to_datetime(doc["Start"]),
            "Finish": pd.to_datetime(doc["Finish"]),
        } for doc in cursor]

def load_db_overview():
    cursor = SYS_DB.aql.execute(
        """
        FOR doc IN @@col RETURN {
          'Date': doc.date,
          'load': doc.sys_stat.load[0],
          'netio': doc.sys_stat.last_netio
        }
        """,
        bind_vars={"@col": LOADS_COL_NAME})
    return [doc for doc in cursor]

def load_file(main_log_file, pid_to_fn, filter_str):
    global LOADS
    LOADS_COL.truncate()
    JOBS_COL.truncate()
    LOADS_COL_cache = []
    collect_loads = []
    last_netio = None
    line_count = 1;
    #print(1)
    #pd.read_ndjson(main_log_file)
    #print(2)
    with open(main_log_file, "r", encoding="utf-8") as jsonl_file:
        while True:
            line = jsonl_file.readline()
            if len(line) == 0:
                break
            parsed_slice = json.loads(line)
            this_date_str = parsed_slice[0]
            this_date = datetime.strptime(this_date_str, DATE_FORMAT)
            # tree = collections.defaultdict(list)
            if filter_str and parsed_slice[0].find(filter_str) < 0:
                continue
            #print(parsed_slice[0])
            #print(json.dumps(parsed_slice, indent=4))
            testing_tasks = []
            sys_stat = None
            sys_id = None
            for process_id in parsed_slice[1]:
                if process_id == "sys":
                    sys_stat = parsed_slice[1][process_id]
                    sys_id = process_id
                    if not last_netio:
                        netio = 0
                    else:
                        netio = sys_stat['netio']['lo'][0] - last_netio
                    last_netio = sys_stat['netio']['lo'][0]
                    sys_stat['last_netio'] = last_netio
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
                if 'ppid' in one_process and one_process['ppid'] > 1:
                    parent = parsed_slice[1][f"p{one_process['ppid']}"]
                    if not 'children' in parent:
                        parent['children'] = []
                    parent['children'].append(process_id)
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
                            pid_to_fn[pidstr]["Start"] = this_date_str
                        pid_to_fn[pidstr]["End"] = this_date_str
                        one_process['name'] = pid_to_fn[pidstr]['n']
                        testing_tasks.append(pidstr)
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
            #print(dir(PARSED_LINES))
            if sys_id:
                del parsed_slice[1][sys_id]
            parsed_slice.append({})
            for testing_pid in testing_tasks:
                aggregate_sub_metrics(testing_pid, parsed_slice[1], parsed_slice)
            LOADS_COL_cache.append({
                "date": this_date_str,
                "sys_stat": sys_stat,
                "processes": parsed_slice,
                "testing_pids": testing_tasks,
                "testing_summary": parsed_slice[len(parsed_slice)-1]
                })
            if len(LOADS_COL_cache) == 100:
                LOADS_COL.insert(LOADS_COL_cache)
                LOADS_COL_cache = []
    LOADS_COL.insert(LOADS_COL_cache)
    LOADS = pd.DataFrame(data=collect_loads)
    JOBS_COL.insert(convert_pids_to_gantt(pid_to_fn))
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
    return load_db_overview()

def append_dict_to_df(df, dict_to_append):
    df = pd.concat([df, pd.DataFrame.from_records([dict_to_append])])
    return df

# Creating an empty dataframe
#df = pd.DataFrame(columns=['a', 'b'])

# Appending a row
#df = append_dict_to_df(df,{ 'a': 1, 'b': 2 })
