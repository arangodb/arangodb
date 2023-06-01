#!/usr/bin/env python

import importlib.util


def module_from_file(module_name, file_path):
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


Feed = module_from_file("Feed", "../modules/Feed.py")


def start():
    Feed.start()
