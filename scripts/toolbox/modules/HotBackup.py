#!/usr/bin/env python

import os
import subprocess
from contextlib import contextmanager


@contextmanager
def cwd(path):
    oldPwd = os.getcwd()
    os.chdir(path)
    try:
        yield
    finally:
        os.chdir(oldPwd)


def checkBinary():
    with cwd('../../'):
        if not os.path.exists("./build/bin/arangobackup"):
            print("ERROR: arangobackup binary not found. Please build it first.")
            exit(1)


def parseEndpoint(endpoint):
    if "http://" in endpoint:
        endpoint = endpoint.replace("http://", "tcp://")
    return endpoint


def getHotBackupID(endpoint, label):
    checkBinary()
    endpoint = parseEndpoint(endpoint)
    result = None

    # main ArangoDB repository
    with cwd('../../'):
        parameters = [
            "./build/bin/arangobackup", "list", "--server.endpoint", endpoint]
        process = subprocess.Popen(parameters, stdout=subprocess.PIPE)
        output = process.communicate()[0]
        output = output.decode("utf-8")
        print(output)
        lines = output.split("\n")
        for line in lines:
            if label in line:
                result = line.split(" ")[7]
        return result


def createHotBackup(endpoint, label):
    checkBinary()
    endpoint = parseEndpoint(endpoint)

    # main ArangoDB repository
    with cwd('../../'):
        parameters = [
            "./build/bin/arangobackup", "create", "--label", label, "--max-wait-for-lock", "180",
            "--server.endpoint", endpoint]
        subprocess.Popen(parameters).wait()

    return getHotBackupID(endpoint, label)


def restoreHotBackup(endpoint, identifier):
    checkBinary()
    endpoint = parseEndpoint(endpoint)

    # main ArangoDB repository
    with cwd('../../'):
        parameters = [
            "./build/bin/arangobackup", "restore", "--identifier", identifier, "--max-wait-for-lock", "180",
            "--server.endpoint", endpoint]
        subprocess.Popen(parameters).wait()
