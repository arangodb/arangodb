python
import sys
sys.path.insert(0, './arangod/AsyncRegistryServer/PrettyPrinter/src/')
end

source ./arangod/AsyncRegistryServer/PrettyPrinter/src/asyncregistry/gdb_printer.py

echo "asyncregistry pretty-printer loaded\n"
