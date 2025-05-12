# Pretty Printer for ArangoDB's Task Monitoring Output

This python-package provides a pretty-printer for the hierarchical task monitoring JSON output produced by ArangoDB.

The pretty-printer groups tasks by their top-level (hierarchy 0, no parent) and by their state. The output is grouped and ordered as follows:
1. Running tasks
2. Finished tasks
3. Deleted tasks (optional, see below)

Each group displays the task hierarchy as an ASCII tree for improved readability.

## Usage

To pretty-print a monitoring output JSON file:

```sh
cat <monitoring_output.json> | ./src/pretty_printer.py [--show-deleted]
```

- By default, **Deleted** tasks are hidden.
- Use the `--show-deleted` flag to include Deleted tasks in the output.

## Run tests

Inside the src-folder run unittests via:

```sh
python3 -m unittest discover
```

## Project Structure

- `src/pretty_printer.py`: Main script for pretty-printing the monitoring output.
- `src/taskmonitoring/`: Python package with core logic for parsing and formatting the task monitoring data.
- `src/tests/`: Unit tests for the pretty printer. 