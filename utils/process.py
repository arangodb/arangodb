#!/usr/bin/python3
import json
from pathlib import Path
import sys
import collections
import pyqtgraph as pg

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

PID_TO_FN = {}
LOADS = []
MEMORY = []
TREE_TEXTS = []
PARSED_LINES = []
def load_file(main_log_file):
    with open(main_log_file, "r", encoding="utf-8") as jsonl_file:
        while True:
            line = jsonl_file.readline()
            if len(line) == 0:
                break
            parsed_slice = json.loads(line)
            PARSED_LINES.append(parsed_slice)
            this_date = parsed_slice[0];
            tree = collections.defaultdict(list)
            if have_filter and parsed_slice[0].find(sys.argv[2]) < 0:
                continue
            #print(parsed_slice[0])
            #print(json.dumps(parsed_slice, indent=4))
            for process_id in parsed_slice[1]:
                if process_id == "sys":
                    sys_stat = parsed_slice[1][process_id]
                    LOADS.append(sys_stat['load'][0])
                    MEMORY.append(sys_stat['netio']['lo'][0])
                    # print(f"L {sys_stat['load'][0]:.1f} - {sys_stat['netio']['lo'][0]:,.3f}")
                    print(json.dumps(parsed_slice[1][process_id], indent=4))
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
                    if pidstr in PID_TO_FN:
                        one_process['name'] = PID_TO_FN[pidstr]
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
            tree_dump = get_process_tree_recursive(parsed_slice[1], start_pid, tree)
            TREE_TEXTS.append(tree_dump)
            # print(tree_dump)

have_filter = len(sys.argv) > 2
start_pid = 1
if len(sys.argv) > 3:
    start_pid = int(sys.argv[3])
print(have_filter)
main_log_file = Path(sys.argv[1])
PID_TO_FN = load_testing_js_mappings(main_log_file)
load_file(main_log_file)

#generate layout
app = pg.mkQApp("Crosshair Example")
win = pg.GraphicsLayoutWidget(show=True)
win.setWindowTitle('pyqtgraph example: crosshair')
label = pg.LabelItem(justify='right')
win.addItem(label)
p1 = win.addPlot(row=1, col=0)
# customize the averaged curve that can be activated from the context menu:
p1.avgPen = pg.mkPen('#FFFFFF')
p1.avgShadowPen = pg.mkPen('#8080DD', width=10)

p2 = win.addPlot(row=2, col=0)

region = pg.LinearRegionItem()
region.setZValue(10)
# Add the LinearRegionItem to the ViewBox, but tell the ViewBox to exclude this
# item when doing auto-range calculations.
p2.addItem(region, ignoreBounds=True)

#pg.dbg()
p1.setAutoVisible(y=True)


#create numpy arrays
#make the numbers large to show that the range shows data from 10000 to all the way 0

p1.plot(LOADS, pen="r")
p1.plot(MEMORY, pen="g")

p2d = p2.plot(LOADS, pen="w")
# bound the LinearRegionItem to the plotted data
region.setClipItem(p2d)

def update():
    region.setZValue(10)
    minX, maxX = region.getRegion()
    p1.setXRange(minX, maxX, padding=0)

region.sigRegionChanged.connect(update)

def updateRegion(window, viewRange):
    rgn = viewRange[0]
    region.setRegion(rgn)

p1.sigRangeChanged.connect(updateRegion)

region.setRegion([1000, 2000])

#cross hair
vLine = pg.InfiniteLine(angle=90, movable=False)
hLine = pg.InfiniteLine(angle=0, movable=False)
p1.addItem(vLine, ignoreBounds=True)
p1.addItem(hLine, ignoreBounds=True)


vb = p1.vb

def mouseMoved(evt):
    pos = evt
    if p1.sceneBoundingRect().contains(pos):
        mousePoint = vb.mapSceneToView(pos)
        index = int(mousePoint.x())
        if index > 0 and index < len(LOADS):
            label.setText("<span style='font-size: 12pt'>x=%0.1f,   <span style='color: red'>y1=%0.1f</span>,   <span style='color: green'>y2=%0.1f</span>" % (mousePoint.x(), LOADS[index], LOADS[index]))
        vLine.setPos(mousePoint.x())
        hLine.setPos(mousePoint.y())



p1.scene().sigMouseMoved.connect(mouseMoved)


pg.exec()
