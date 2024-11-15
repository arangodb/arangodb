#!/usr/bin/python3

import json
import sys
import collections


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
print(have_filter)
print(sys.argv[2])
with open(sys.argv[1], "r", encoding="utf-8") as jsonl_file:
    while True:
        line = jsonl_file.readline()
        if len(line) == 0:
            break
        parsed_slice = json.loads(line)
        this_date = parsed_slice[0];
        tree = collections.defaultdict(list)
        if have_filter and parsed_slice[0].find(sys.argv[2]) < 0:
            continue
        print(parsed_slice[0])
        for process_id in parsed_slice[1]:
            #print(process)
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
                #print(json.dumps(one_process, indent=4))
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
        print(get_process_tree_recursive(parsed_slice[1], min(tree), tree))
