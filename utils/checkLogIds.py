#!/usr/bin/python3

import sys
import os

from enum import Enum

class Operation(Enum):
    READ = 1
    MODIFY = 2

class Status(Enum):
    OK = 0
    OK_REPLACED = 1
    FAIL = 2

def is_good(status):
    return status in (Status.OK, Status.OK_REPLACED)

import re
g_log_topic_pattern = re.compile(r'(?P<macro>LOG_(TOPIC(_IF)?|TRX))\((?P<param>[^),]*),(?P<rest>.*)')

import hashlib
g_hash_algorithm = hashlib.md5()


def generate_id(location_string, params, rest, id_database):
    sys.exit(1) #disabled on purpose

    id_len = 5
    g_hash_algorithm.update((location_string + params + rest).encode())
    digest = g_hash_algorithm.hexdigest()
    uid = digest[0:id_len]

    while uid in id_database:
        print("collision on id {} \n{}\n{}\n".format(uid, location_string, id_database[uid]))
        g_hash_algorithm.update((location_string + digest).encode())
        digest = g_hash_algorithm.hexdigest()
        uid = digest[0:id_len]

    id_database[uid]=location_string

    return uid

def handle_file(fullpath, project_root, id_database, operation):
    project_path = os.path.relpath(fullpath, project_root)
    status = Status.OK

    levels = ( 'TRACE', 'INFO', 'WARN', 'DEBUG', 'ERR', 'FATAL')

    if operation == Operation.MODIFY:
        target_file = fullpath + ".replacement"
        with open(target_file, 'w') as target_file_handle:
            status = do_operation(fullpath, project_path, target_file_handle, id_database, levels, operation)

        if status == Status.OK_REPLACED:
            os.rename(target_file, fullpath)
        else:
            os.unlink(target_file)

    else:
        #not modifying operation
        status = do_operation(fullpath, project_path, None, id_database, levels, operation)


    if is_good(status):
        return Status.OK # filter OK_REPLACED
    return status


def do_operation(fullpath, project_path, target_file_handle, id_database, levels, operation):
    status = Status.OK
    with open(fullpath) as source_file_handle:
        for cnt, line in enumerate(source_file_handle):
            match = g_log_topic_pattern.search(line)

            if match:
                location_string = "{}:{}".format(project_path, cnt)
                macro = match.group('macro')
                rest = match.group('rest')
                param = match.group('param').strip()

                if operation == Operation.MODIFY:
                    if param in levels:
                        # replace with new id
                        uid = generate_id(location_string, param, rest, id_database)
                        replacement = '{}("{}", {},{}'.format(macro ,uid, param, rest.replace('\\', '\\\\' ))
                        output = "{} -- {}".format(location_string, replacement)
                        #print(output)
                        status = Status.OK_REPLACED
                        newline = g_log_topic_pattern.sub(replacement, line)
                        target_file_handle.write(newline)

                    elif len(param) == 5:
                        # nothing to do - just copy
                        target_file_handle.write(line)

                    else:
                        print("BROKEN PARAMS IN: " + project_path + ":" + str(cnt))
                        print(operation)
                        print(line)
                        print("macro: " + macro)
                        print("param: " +  param)
                        print("rest:" + rest)
                        status = Status.FAIL

                if operation == Operation.READ:
                    if param in levels:
                        # still unmodified
                        continue

                    elif len(param) == 7 and param.startswith('"') and param.endswith('"'):
                        # we need to store / check the id
                        uid = param[1:-1]

                        if uid in id_database:
                            print("collision check failed - the following lines contain equal ids:")
                            print(location_string)
                            print(id_database[uid])
                            status = Status.FAIL
                        else:
                            id_database[uid]=location_string

                    elif param == "logId":
                        # additional macro
                        pass 

                    else:
                        print("BROKEN PARAMS IN: " + project_path + ":" + str(cnt))
                        print(operation)
                        print(line)
                        print("macro: " + macro)
                        print("param: " +  param)
                        print("rest:" + rest)
                        status = Status.FAIL

                else:
                    #ignore operation
                    pass


            elif operation == Operation.MODIFY:
                #no match but we need to midify
                target_file_handle.write(line)

            if not is_good(status):
                return status

    return status


def generate_ids_main(check_only, project_root, directories_to_include, directories_or_files_to_exclude, id_database):
    """
    Library function to check arangodb project for valid unique log ids.

    If `check_only` is set to False it will also convert macros form old style
    logging to new the new style inserting unique ids.

        :param check_only: check for uniqueness but do not modify
        :param project_root: path to project_root
        :param directories_to_include: list of directories to include
        :param directories_or_files_to_exclude: list of directories and files to exclude

        :return: status
        :rtype: int
    """

    include = [ project_root + os.path.sep + d for d in directories_to_include ]
    exclude = [ project_root + os.path.sep + d for d in directories_or_files_to_exclude ]

    operations = [ Operation.READ ]
    if not check_only:
        operations.append(Operation.MODIFY)

    for root, dirs, files in os.walk(project_root):
        status = Status.OK

        dirs[:] = [ d for d in dirs if (root + os.path.sep + d).startswith(tuple(include)) ]
        if root in exclude:
            continue

        for operation in operations:
            for filename in files:
                fullpath = root + os.path.sep + filename
                if fullpath in exclude:
                    continue
                if (filename.endswith((".cpp",".h"))):
                    status = handle_file(fullpath, project_root, id_database, operation)

                if not is_good(status):
                    return status

    return 0

if __name__ == "__main__":
    """
    Returns non zero status if there is a duplicate id!

    Note:
    `check_only` must be set to true for normal operation mode.
    """

    root = os.path.dirname(__file__)
    root = os.path.abspath(root + os.path.sep + os.pardir)
    check_only = True
    sys.exit(generate_ids_main(check_only, root              #project_root
            ,["arangod", "arangosh", "enterprise", "lib"]    #dirs to include
            ,[".", "lib/Logger/LogMacros.h",
              "arangod/StorageEngine/TransactionState.h"]    #dirs and files to exclude (does not purge the dir)
            , dict()                                         #database
            ))
