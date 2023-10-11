# Copyright 2020 Texas Instruments Incorporated

"""A wrapper to run the SDK makefiles for Platform driver generation
Args:
1. [TI Simplelink SDK Root]
2. [Matter repository Root]
"""

import argparse
import os
import shutil
import subprocess
import sys


parser = argparse.ArgumentParser()
parser.add_argument('--sdk', help="TI SDK root")
parser.add_argument('--chip-root', help="CHIP Root")
parser.add_argument('--lib', help="Library file to copy to output", nargs="+")
parser.add_argument('--out-dir', help="Output directory")

args = parser.parse_args()

ret = False

if os.getenv('_PW_ACTUAL_ENVIRONMENT_ROOT'):
    CHIP_ENV_ROOT = os.getenv('_PW_ACTUAL_ENVIRONMENT_ROOT')
else:
    CHIP_ENV_ROOT = os.path.join(args.chip_root, ".environment")

GCC_ARMCOMPILER_PATH = os.path.join(CHIP_ENV_ROOT, "cipd", "packages", "arm")

if not os.path.isdir(GCC_ARMCOMPILER_PATH):
    print("Compiler Path is invalid: " + GCC_ARMCOMPILER_PATH)
    sys.exit(2)

make_command = ["make", "-C", args.sdk, "CMAKE=cmake", "GCC_ARMCOMPILER=" +
                GCC_ARMCOMPILER_PATH, "IAR_ARMCOMPILER=", "TICLANG_ARMCOMPILER=", "GENERATOR=Ninja"]

res = subprocess.run(make_command, capture_output=True, encoding="utf8")
if res.returncode != 0:
    print("!!!!!!!!!!!! EXEC FAILED !!!!!!!!!!!!!!!!")
    print("!!!!!!!!!!!!!!! STDOUT !!!!!!!!!!!!!!!!!!")
    print("%s" % res.stdout)
    print("!!!!!!!!!!!!!!! STDERR !!!!!!!!!!!!!!!!!!")
    print("%s" % res.stderr)
    sys.exit(1)

for name in args.lib:
  shutil.copyfile(os.path.join(args.sdk, name), os.path.join(args.out_dir, os.path.basename(name)))

