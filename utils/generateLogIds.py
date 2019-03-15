#!/usr/bin/python3

import sys
import os

from enum import Enum

class Operation(Enum):
    READ = 1
    MODIFY = 2
    READ_ONLY = 3

class Status(Enum):
    OK = 0
    OK_REPLACED = 0
    FAIL = 1

def is_good(status):
    return status in (Status.OK, Status.OK_REPLACED)

import re
g_log_topic_pattern = re.compile("(?P<macro>LOG_TOPIC(_(RAW|IF|EVERY_N))?)\((?P<params>[^)]*)\)")

import hashlib
g_hash_algorithm = hashlib.md5()


def generate_id(location_string, params, id_database):
    id_len = 5
    g_hash_algorithm.update((location_string + params).encode())
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

    match_dict = dict()
    match_dict["LOG_TOPIC"] = 2
    match_dict["LOG_TOPIC_RAW"] = 2
    match_dict["LOG_TOPIC_IF"] = 3
    match_dict["LOG_TOPIC_EVERY_N"] = 3

    if operation == Operation.MODIFY:
        target_file = fullpath + ".replacement"
        with open(target_file, 'w') as target_file_handle:
            status = do_operation(fullpath, project_path, target_file_handle, id_database, match_dict, operation)

        if status == Status.OK_REPLACED:
            os.rename(target_file, fullpath)
        else:
            os.unlink(target_file)

    else:
        #not modifying operation
        status = do_operation(fullpath, project_path, None, id_database, match_dict, operation)


    if is_good(status):
        return Status.OK # filter OK_REPLACED
    return status


def do_operation(fullpath, project_path, target_file_handle, id_database, match_dict, operation):
    status = Status.OK

    with open(fullpath) as source_file_handle:
        for cnt, line in enumerate(source_file_handle):
            match = g_log_topic_pattern.search(line)

            if match:
                location_string = "{}:{}".format(project_path, cnt)
                macro = match.group('macro')
                original_params = match.group('params')
                splitted = [ x.strip() for x in original_params.split(',') ]

                original_len = match_dict[macro]

                if operation == Operation.MODIFY:
                    if len(splitted) == original_len:
                        # replace with new id
                        uid = generate_id(location_string, original_params, id_database)
                        replacement = '{}({}, {})'.format(macro,uid, original_params)
                        #output = "{} -- {}".format(location_string, replacement)
                        #print("REPLACE")
                        status = Status.OK_REPLACED
                        newline = g_log_topic_pattern.sub(replacement, line)
                        target_file_handle.write(newline)


                    elif len(splitted) == original_len + 1:
                        # nothing to do - just copy
                        target_file_handle.write(line)

                    else:
                        print("BROKEN PARAMS IN:")
                        output = "{} -- {}".format(location_string, original_params)
                        print(output)
                        status = Status.FAIL

                if operation == Operation.READ:
                    if len(splitted) == original_len:
                        # still unmodified
                        continue

                    elif len(splitted) == original_len + 1:
                        # we need to store / check the id
                        uid = splitted[0].replace('"','')

                        if uid in id_database:
                            print("collision check failed - the following lines contain equal ids:")
                            print(location_string)
                            print(id_database[uid])
                            status = Status.FAIL
                        else:
                            id_database[uid]=location_string

                    else:
                        print("BROKEN PARAMS IN:")
                        output = "{} -- {}".format(location_string, original_params)
                        print(output)
                        status = Status.FAIL

                else:
                    #ignore operation
                    pass


            elif operation == Operation.MODIFY:
                #no match but we need to midify
                target_file_handle.write(line)

            if not is_good(status):
                return status

    return Status.OK


def generate_ids_main(check_only, project_root, directories_to_include, directories_or_files_to_exclude, id_database):
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
    root = os.path.dirname(__file__)
    root = os.path.abspath(root + os.path.sep + os.pardir)
    check_only = False
    sys.exit(generate_ids_main(check_only, root                             #project_root
            ,["arangod", "arangosh", "enterprise", "lib"]        #dirs to include
            ,[".", "lib/Logger/LogMacros.h"]                    #dirs and files to exclude (does not purge the dir)
            , dict()                                            #database
            ));
