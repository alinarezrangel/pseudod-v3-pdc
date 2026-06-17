import json
import sys
import shutil


with open(sys.argv[1], "r") as in_handle:
    in_json = json.load(in_handle)
try:
    with open(sys.argv[2], "r") as out_handle:
        out_json = json.load(out_handle)
except FileNotFoundError:
    out_json = None

if in_json != out_json:
    shutil.copyfile(sys.argv[1], sys.argv[2])
