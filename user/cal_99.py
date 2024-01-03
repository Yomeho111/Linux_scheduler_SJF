#!/bin/python3
import argparse
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='The filename')
    parser.add_argument('--filename', type=str, help='the filename')
    args = parser.parse_args()

    completion_time = []

    corresponding_runqueue = {}

    with open(args.filename, "r") as f:
        for line in f:
            con = line.split(",")
            rt = int(con[2])
            ct = int(con[3])
            corresponding_runqueue[ct] = rt
            completion_time.append(ct)

    completion_time.sort()
    p100 = completion_time[int(len(completion_time) * 0.99)]
    rt_100 = corresponding_runqueue[p100]
    p99 = completion_time[int(len(completion_time) * 0.99) - 1]
    rt_99 = corresponding_runqueue[p99]
    max_runqueue = max(corresponding_runqueue.values())
    print(f"The maximum completion time: {p100}")
    print(f"The corresponding runqueue time of maximum: {rt_100}")
    print(f"The 99% maximum completion time: {p99}")
    print(f"The corresponding runqueue time of 99%: {rt_99}")
    print(f"The maximum runqueue time: {max_runqueue}")

