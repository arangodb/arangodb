#!/usr/bin/python3

import sys
import os

from enum import Enum

import re
g_log_topic_pattern = re.compile(r'(?P<macro>LOG_(TOPIC(_IF)?|TRX|CTX(_IF)?|QUERY|PREGEL))\((?P<param>[^),]*),(?P<rest>.*)')

# a LogId is exactly 5 hexadecimal digits
g_log_id_pattern = re.compile(r'"[0-9a-f]{5}"')

def check_file(fullpath, project_path, id_database, invalid_logid_database):
    with open(fullpath) as source_file_handle:
        for cnt, line in enumerate(source_file_handle):
            match = g_log_topic_pattern.search(line)

            if match:
                location_string = "{}:{}".format(project_path, cnt)
                macro = match.group('macro')
                rest = match.group('rest')
                param = match.group('param').strip()

                if g_log_id_pattern.match(param):
                    # we need to store / check the id
                    uid = param[1:-1]

                    if uid in id_database:
                        id_database[uid]=id_database[uid] + [location_string]
                    else:
                        id_database[uid]=[location_string]

                elif param == "logId":
                    # TODO: warn?
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

    id_database = dict()
    invalid_logid_database = dict()

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
                check_file(fullpath, root_directory, id_database, invalid_logid_database)

    return [id_database, invalid_logid_database]

if __name__ == "__main__":
    """
    Returns non zero status if there is a duplicate id!

    Note:
    `check_only` must be set to true for normal operation mode.
    """

    root = os.path.dirname(__file__)
    root = os.path.abspath(root + os.path.sep + os.pardir)

    id_database, invalid_logid_database = check_log_ids_main(root,
                       ["arangod", "client-tools", "enterprise", "lib"],
                       [".", "lib/Logger/LogMacros.h",
                        "arangod/StorageEngine/TransactionState.h",
                        "arangod/Replication2/LoggerContext.h"])

    return_value = 0

    for k, v in id_database.items():
        if len(v) > 1:
            return_value = 1
            print("LogId {} is used more than once:".format(k))
            for loc in v:
                print("  {}".format(loc))

    if invalid_logid_database:
        return_value = 1
        print("Found the following invalid LogIds: ")
        for loc, logId in invalid_logid_database.items():
            print(" {} at {}".format(logId, loc))

    sys.exit(return_value)
