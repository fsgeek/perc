import os
import sys
import argparse
import json


def main():
    parser = argparse.ArgumentParser(description="Browse Test Results")
    parser.add_argument('--noaction', dest='echo_args', action='store_true', default=False)
    parser.add_argument('--test', dest='test_name', default = None)
    parser.add_argument('--run', dest='test_run', default = None)
    parser.add_argument('--subtest', dest='subtest', default=None)
    parser.add_argument('--instance', dest='instance', default=None)
    parser.add_argument('inputfile', nargs='?', type=argparse.FileType('r'), default=sys.stdin)
    args = parser.parse_args()
    data = json.load(args.inputfile)
    if args.test_name is None or args.test_name not in data:
        print("test options are:")
        for i in data: print("\t{}".format(i))
        return
    if args.test_run is None or args.test_run not in data[args.test_name]:
        print("run data options:")
        for i in data[args.test_name]: print("\t{}".format(i))
        return
    if args.subtest is None or args.subtest not in data[args.test_name][args.test_run]:
        print("subtest options:")
        for i in data[args.test_name][args.test_run]: print("\t{}".format(i))
        return
    if args.instance is None or args.instance not in data[args.test_name][args.test_run][args.subtest]:
        print("instance options:")
        for i in data[args.test_name][args.test_run][args.subtest]: print("\t{}".format(i))
        return
    for i in data[args.test_name][args.test_run][args.subtest][args.instance]: print("\t{}".format(i))
    print("done")

if __name__ == "__main__":
  main()
