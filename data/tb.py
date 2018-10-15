import os
import sys
import argparse
import json

def size_to_pages(size):
    count = -1
    size_to_page ={ '4KB': 1, '8KB': 2, '12KB': 3, 
      '16KB': 4, '24KB': 6, '32KB': 8, '48KB': 12, '64KB': 16, '128KB' : 32, '256KB': 64}
    for s in size_to_page:
        if s in size and size_to_page[s] > count: count = size_to_page[s]
    if count < 0: raise KeyError
    #print('{} -> {}'.format(size, count))
    return count


def main():
    parser = argparse.ArgumentParser(description="Browse Test Results")
    parser.add_argument('--noaction', dest='echo_args', action='store_true', default=False)
    parser.add_argument('--test', dest='test_name', default = 'backing file test')
    parser.add_argument('--run', dest='test_run', default = 'None')
    parser.add_argument('--subtest', dest='subtest', default=None)
    parser.add_argument('--instance', dest='instance', default=None)
    parser.add_argument('inputfile', nargs='?', type=argparse.FileType('r'), default=sys.stdin)
    args = parser.parse_args()
    data = json.load(args.inputfile)


    tests = [i for i in data]
    if 0 is len(tests):
        print('no valid tests present')
        return
    if args.test_name is None or args.test_name not in data:
        print("test options are:")
        for i in data: print("\t{}".format(i))
        args.test_name = tests[0]
        print('defaulting to test {}'.format(args.test_name))
    runs=[]
    if args.test_run is None or args.test_run not in data[args.test_name]:
        for i in data[args.test_name]:
            if 'size' in i: runs.append(i)
    #print(runs)
    subtests = []
    for r in runs:
        for i in data[args.test_name][r]:
            if 'test_' in i: subtests.append(i)
    #print(subtests)

    instances = []
    for r in runs:
        for s in subtests:
            if s not in data[args.test_name][r]: continue
            for i in data[args.test_name][r][s]: 
                if i not in instances: instances.append(i)
    #print(instances)

    # for s in subtests:
    s = 'test_cache_behavior_9'
    once = False
    for i in instances:
        if 'stride' in i: continue
        if 'results' in i: continue
        if 'runs' in i: continue
        if 'pagecount' in i: continue
        if 'summary' in i: continue
        for r in runs:
            if s not in data[args.test_name][r]: continue
            if i not in data[args.test_name][r][s]: continue
            if s != 'test_cache_behavior_9':
                # print('("{}", "{}", "{}", "{}")'.format(i, s, r, data[args.test_name][r][s][i]))
                continue
            # for test 9: field 4 is a compound field that consists of the period and then the
            # specific results
            frequency=[]
            for f in data[args.test_name][r][s][i]:
                for p in data[args.test_name][r][s][i][f]:
                    # if 'set' not in p: print(p)
                    if p not in frequency: frequency.append(p)
            for p in frequency:
                for f in data[args.test_name][r][s][i]:
                    """
                    print('("{}", "{}", "{}", "{}, {}")'.format(i, s, r, 
                    data[args.test_name][r][s][i],
                    data[args.test_name][r][s][i][f]))
                    """
                    for j in data[args.test_name][r][s][i][f]:
                        if 'cache' in j:
                            #continue
                            #print(r)
                            for c in data[args.test_name][r][s][i][f]:
                                if not once:
                                    once = True
                                    print('Page Count\tTest\tcache\tTime (100 Runs)')
                                print('{}\t{}\t{}\t{}'.format(size_to_pages(r), f, c, data[args.test_name][r][s][i][f][c]))
                        else:
                            continue
                            for c in data[args.test_name][r][s][i][f]:
                                for period in data[args.test_name][r][s][i][f][c]:
                                    if not once:
                                        once = True
                                        print('Page Count\tTest\tFence Period\tcache\tTime (100 Runs)')
                                        #print('{}, {}, {}, {}, {}, {}, {}\n'.format(r, s, i, f, p, period, 
                                        #data[args.test_name][r][s][i][f]))
                                    print('{}\t{}\t{}\t{}\t{}'.format(size_to_pages(r),
                                    f, c, period, 
                                    data[args.test_name][r][s][i][f][c][period]))
                                    #print('{}, "{}", {}", {}'.format(c, f, period, data[args.test_name][r]))
                    #print('({}, {})\n'.format(p, data[args.test_name][r][s][i][f]))

    #print("done")

if __name__ == "__main__":
  main()
