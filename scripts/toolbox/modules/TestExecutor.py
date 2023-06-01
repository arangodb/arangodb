#!/usr/bin/env python

def start(cfg, operations):
    parameters = [cfg.feed["executable"]]
    process = subprocess.Popen(parameters)
