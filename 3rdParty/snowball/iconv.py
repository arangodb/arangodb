#!env python
# Simple (but slow) iconv replacement in Python.
import sys

in_cs = out_cs = in_file = out_file = pending = None
for arg in sys.argv[1:]:
    if pending != None:
        arg = pending + arg
        pending = None
    if arg.startswith('-'):
        if arg[1] in ('f', 't', 'o'):
            if len(arg) == 2:
                pending = arg
                continue
        if arg[1] == 'f':
            in_cs = arg[2:]
            continue
        if arg[1] == 't':
            out_cs = arg[2:]
            continue
        if arg[1] == 'o':
            out_file = open(arg[2:], 'wb')
            continue
        print("Unknown option: '%s'" % arg)
        sys.exit(1)
    if in_file == None:
        in_file = open(arg, 'rb')
        continue
    print("Too many arguments")
    sys.exit(1)

if in_cs == None:
    print("Need to specify input cs with -f")
    sys.exit(1)
if out_cs == None:
    print("Need to specify output cs with -t")
    sys.exit(1)

if in_file == None:
    if hasattr(sys.stdin, 'buffer'):
        in_file = sys.stdin.buffer
    else:
        in_file = sys.stdin
if out_file == None:
    if hasattr(sys.stdout, 'buffer'):
        out_file = sys.stdout.buffer
    else:
        out_file = sys.stdout

out_file.write(in_file.read().decode(in_cs).encode(out_cs))
