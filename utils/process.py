#!/usr/bin/python3

import json
from pathlib import Path
import sys
import collections
import pyqtgraph as pg

app = pg.mkQApp("Symbols Examples")
win = pg.GraphicsLayoutWidget(show=True, title="Scatter Plot Symbols")
win.resize(1000,600)

pg.setConfigOptions(antialias=True)

plot = win.addPlot(title="Plotting with symbols")
plot.addLegend()


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

have_filter = len(sys.argv) > 2
start_pid = 1
if len(sys.argv) > 3:
    start_pid = int(sys.argv[3])
print(have_filter)
main_log_file = Path(sys.argv[1])
arangosh_lookup_file = main_log_file.parents[0] / "job_to_pids.jsonl"
pid_to_fn = {}
loads = []
if arangosh_lookup_file.exists():
    with open(arangosh_lookup_file, "r", encoding="utf-8") as jsonl_file:
        while True:
            line = jsonl_file.readline()
            if len(line) == 0:
                break
            parsed_slice = json.loads(line)
            pid_to_fn[f"p{parsed_slice['pid']}"] = Path(parsed_slice["logfile"]).name
print(pid_to_fn)
with open(main_log_file, "r", encoding="utf-8") as jsonl_file:
    while True:
        line = jsonl_file.readline()
        if len(line) == 0:
            break
        parsed_slice = json.loads(line)
        this_date = parsed_slice[0];
        tree = collections.defaultdict(list)
        if have_filter and parsed_slice[0].find(sys.argv[2]) < 0:
            continue
        #print(parsed_slice[0])
        #print(json.dumps(parsed_slice, indent=4))
        for process_id in parsed_slice[1]:
            if process_id == "sys":
                sys_stat = parsed_slice[1][process_id]
                loads.append(sys_stat['load'][0])
                print(f"L {sys_stat['load'][0]:.1f} - {sys_stat['netio']['lo'][0]:,.3f}")
                # print(json.dumps(parsed_slice[1][process_id], indent=4))
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
                    one_process['name'] = pid_to_fn[pidstr]
                else:
                    try:
                        n = one_process['cmdline'].index('--')
                        one_process['name'] = f"{name} - {one_process['cmdline'][n+1]}"
                    except ValueError:
                        try:
                            n = one_process['cmdline'].index('--javascript.unit-tests')
                            one_process['name'] = f"{name} - {one_process['cmdline'][n+1]}"
                        except ValueError:
                            print(json.dumps(one_process, indent=4))
                            pass
                        pass
            elif name == 'python3':
                try:
                    n = one_process['cmdline'].index('launch')
                    one_process['name'] = f"{name} - launch controller"
                except ValueError:
                    print(json.dumps(one_process, indent=4))
            #print(json.dumps(one_process, indent=4))
            # print(f"{one_process['pid']} {one_process['ppid']} {one_process['name']}")
            tree[one_process['ppid']].append(one_process['pid'])
        # print(struct)
        print(get_process_tree_recursive(parsed_slice[1], start_pid, tree))

plot.plot(loads, pen=(0,0,200), symbolBrush=(0,0,200), symbolPen='w', symbol='o', symbolSize=1, name="symbol='o'")


plot.setXRange(0, len(loads))
pg.exec()
