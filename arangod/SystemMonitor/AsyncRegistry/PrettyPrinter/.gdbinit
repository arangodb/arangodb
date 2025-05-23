python
import sys
sys.path.insert(0, './arangod/SystemMonitor/AsyncRegistry/PrettyPrinter/src/')
end

source ./arangod/SystemMonitor/AsyncRegistry/PrettyPrinter/src/asyncregistry/gdb_printer.py

echo "asyncregistry pretty-printer loaded\n"
