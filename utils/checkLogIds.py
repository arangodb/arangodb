#!/usr/bin/python3

import sys
import os

import re
g_log_topic_pattern = re.compile( \
    r'(?P<macro>LOG_(TOPIC(_IF)?|TRX|CTX(_IF)?|QUERY|PREGEL))\((?P<param>[^),]*),(?P<rest>.*)')

# a LogId is exactly 5 hexadecimal digits
g_log_id_pattern = re.compile(r'"[0-9a-f]{5}"')

def check_file(fullpath, id_database, invalid_logid_database):
    """
    Collects LogIDs and invalid LogID attempts from file given in fullpath

        :param fullpath: full path to the file to open
        :param id_database: dict containing all occurrences of LogIDs
        :param invalid_logid_database: locations in which invalid LogID attempts have been found
    """
    with open(fullpath, encoding="UTF-8") as source_file_handle:
        for cnt, line in enumerate(source_file_handle):
            match = g_log_topic_pattern.search(line)

            if match:
                location_string = f"{fullpath}:{cnt+1}"
                param = match.group('param').strip()

                if g_log_id_pattern.match(param):
                    # we need to store / check the id
                    uid = param[1:-1]

                    if uid in id_database:
                        id_database[uid]=id_database[uid] + [location_string]
                    else:
                        id_database[uid]=[location_string]

                elif param == "logId":
                    # This is made to pass over macro definitions that
                    # look like invocations to our primitive regex
                    pass
                else:
                    invalid_logid_database[location_string] = param

def check_log_ids_main(root_directory, directories_to_include, directories_or_files_to_exclude):
    """
    Function to check arangodb project for valid unique log ids.

        :param root_directory: path to project_root
        :param directories_to_include: list of directories to include
        :param directories_or_files_to_exclude: list of directories and files to exclude

        :return: dict of LogIds with locations of use
        :rtype: dict
    """

    id_database = {}
    invalid_logid_database = {}

    include = [ root_directory + os.path.sep + d for d in directories_to_include ]
    exclude = [ root_directory + os.path.sep + d for d in directories_or_files_to_exclude ]

    for root, dirs, files in os.walk(root_directory):
        dirs[:] = [ d for d in dirs if (root + os.path.sep + d).startswith(tuple(include)) ]
        if root in exclude:
            continue

        for filename in files:
            fullpath = root + os.path.sep + filename
            if fullpath in exclude:
                continue
            if filename.endswith((".cpp", ".h", ".tpp")):
                check_file(fullpath, id_database, invalid_logid_database)

    return [id_database, invalid_logid_database]

if __name__ == "__main__":
    my_root = os.path.dirname(__file__)
    my_root = os.path.abspath(os.path.join(my_root, os.pardir))

    my_id_database, my_invalid_logid_database = check_log_ids_main(my_root,
                       ["arangod", "client-tools", "enterprise", "lib"],
                       [".", "lib/Logger/LogMacros.h",
                        "arangod/StorageEngine/TransactionState.h",
                        "arangod/Replication2/LoggerContext.h"])

    return_value = 0

    for k, v in my_id_database.items():
        if len(v) > 1:
            return_value = 1
            print(f"LogId {k} is used more than once:")
            for loc in v:
                print(f"  {loc}")

    if my_invalid_logid_database:
        return_value = 1
        print("Found the following invalid LogIds: ")
        for loc, logId in my_invalid_logid_database.items():
            print(f" {logId} at {loc}")

    sys.exit(return_value)
