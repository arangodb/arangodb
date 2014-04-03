#!/usr/bin/env python

#
# Copyright 2013 MongoDB, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Performance tests comparing cbson and bson Python modules."""

import bson
import cbson
from datetime import datetime
import sys
import threading
import timeit

N_THREADS = 12
EMPTY_BSON = '\x05\x00\x00\x00\x00'
o = {}
oo = o
for i in range(25):
    oo['o'] = {}
    oo = oo['o']
_25X_BSON = bson.BSON.encode(o)
DATETIME_BSON = bson.BSON.encode(dict((str(k),datetime.utcnow()) for k in [0,1,2,3,4,5,6,7,8,9]))

def timediff(a, b):
    delta = b - a
    if hasattr(delta, 'total_seconds'):
        return delta.total_seconds()
    ret = abs(delta.days * 86400)
    ret += abs(delta.seconds) 
    ret += abs(delta.microseconds / 1000000.0)
    if b < a:
        return -ret
    return ret

def compare_modules(name, bson_func, cbson_func, number=1, thread_count=None):
    """
    Runs two performance tests. One using the bson module and one
    using the cbson module. The resulting times are output for
    comparison.
    """
    sys.stdout.write('%-42s : %6d passes : ' % (name, number))
    sys.stdout.flush()

    bson_wrapper = lambda v: v.append(timeit.timeit(bson_func, number=number))
    cbson_wrapper = lambda v: v.append(timeit.timeit(cbson_func, number=number))

    if thread_count is not None:
        results = []
        start = datetime.utcnow()
        threads = [threading.Thread(target=bson_wrapper, args=[results]) for i in range(thread_count)]
        [t.start() for t in threads]
        [t.join() for t in threads]
        end = datetime.utcnow()
        bson_time = timediff(start, end)

        results = []
        start = datetime.utcnow()
        threads = [threading.Thread(target=cbson_wrapper, args=[results]) for i in range(thread_count)]
        [t.start() for t in threads]
        [t.join() for t in threads]
        end = datetime.utcnow()
        cbson_time = timediff(start, end)
    else:
        results = []
        bson_wrapper(results)
        bson_time = sum(results)

        results = []
        cbson_wrapper(results)
        cbson_time = sum(results)

    speedup = bson_time / cbson_time
    print >> sys.stdout, 'bson %0.4lf : cbson %0.4lf : %06.3lfx faster' % (bson_time, cbson_time, speedup)

def bson_generate_object_id():
    bson.ObjectId()

def cbson_generate_object_id():
    cbson.ObjectId()

def bson_decode_empty():
    bson.BSON(EMPTY_BSON).decode()

def cbson_decode_empty():
    cbson.loads(EMPTY_BSON)

def bson_decode_25x_level():
    bson.BSON(_25X_BSON).decode()

def cbson_decode_25x_level():
    cbson.loads(_25X_BSON)

def bson_decode_datetime_bson():
    bson.BSON(DATETIME_BSON).decode()

def cbson_decode_datetime_bson():
    cbson.loads(DATETIME_BSON)


DOC_WITH_ID = bson.BSON.encode({'_id': bson.ObjectId()})

def bson_decode_with_id():
    bson.BSON(DOC_WITH_ID).decode()

def cbson_decode_with_id():
    cbson.loads(DOC_WITH_ID)


import bson.json_util

def bson_to_json():
    bson.json_util.dumps(bson.BSON(_25X_BSON).decode())

def cbson_to_json():
    cbson.as_json(_25X_BSON)

_from_json_doc = bson.json_util.dumps(bson.BSON(_25X_BSON).decode())

def bson_from_json():
    return bson.json_util.loads(_from_json_doc)

def cbson_from_json():
    return cbson.from_json(_from_json_doc)

if __name__ == '__main__':
    compare_modules('Generate ObjectId', bson_generate_object_id, cbson_generate_object_id, number=10000)
    compare_modules('Generate ObjectId (threaded)', bson_generate_object_id, cbson_generate_object_id, number=10000, thread_count=N_THREADS)
    compare_modules('Decode Empty BSON', bson_decode_empty, cbson_decode_empty, number=10000)
    compare_modules('Decode Empty BSON (threaded)', bson_decode_empty, cbson_decode_empty, number=10000, thread_count=N_THREADS)
    compare_modules('Decode 25x level BSON', bson_decode_empty, cbson_decode_empty, number=10000)
    compare_modules('Decode 25x level BSON (threaded)', bson_decode_empty, cbson_decode_empty, number=10000, thread_count=N_THREADS)
    compare_modules('Decode Datetime BSON', bson_decode_datetime_bson, cbson_decode_datetime_bson, number=10000)
    compare_modules('Decode Datetime BSON (threaded)', bson_decode_datetime_bson, cbson_decode_datetime_bson, number=10000, thread_count=N_THREADS)
    compare_modules('Decode BSON w/ _id', bson_decode_with_id, cbson_decode_with_id, number=10000)
    compare_modules('to_json', bson_to_json, cbson_to_json, number=10000)
    compare_modules('from_json', bson_from_json, cbson_from_json, number=10000)
