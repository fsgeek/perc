#!/usr/bin/python3

import sys
import json

def dict_raise_on_duplicates(ordered_pairs):
    """Reject duplicate keys."""
    d = {}
    for k, v in ordered_pairs:
        if k in d:
           raise ValueError("duplicate key: %r" % (k,))
        else:
           d[k] = v
    return d

def check(file):
    with open(file, 'r') as fd:
        data = json.load(fd, object_pairs_hook=dict_raise_on_duplicates)

for i in range(1,len(sys.argv)):
    print(sys.argv[i])
    try:
        check(sys.argv[i])
    except ValueError as ve:
        print("{} has duplicate keys or is not valid, {}".format(sys.argv[i], ve))