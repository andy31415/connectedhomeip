#!/usr/bin/env python3

import logging
import subprocess
import re
import io

import click
import coloredlogs
import alive_progress


def execute_compile(target) -> bool:

    progress_match = re.compile(r'.*\[\s*(?P<done>\d+)\s*/\s*(?P<total>\d+)\s*\].*')

    with subprocess.Popen(
        ["./scripts/build/build_examples.py", "--target", target, "build"],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0, # line buffering
        pipesize=0, # attempt line buffering
    ) as proc:
        assert proc.stdout is not None

        with alive_progress.alive_bar(title=f'Compiling {target}', manual=True) as bar:
            for out_line in io.TextIOWrapper(proc.stdout, encoding='utf-8'):
                m = progress_match.match(out_line)
                if not m:
                    continue
                progress = float(m.group('done'))/int(m.group('total'))
                bar(progress)

        return proc.wait() == 0


@click.command()
@click.option(
    "--size",
    default=128 * 1024,
    show_default=True,
    type=int,
    help="How many bytes to start the test with",
)
@click.argument("target")
def main(size, target):
    log_fmt = "%(asctime)s %(levelname)-7s %(message)s"
    coloredlogs.install(level=logging.INFO, fmt=log_fmt)

    current = size
    low = None
    high = None

    while current != high and current != low:
        logging.info("Checking %d bytes", current)
        if current > 10 * 1024 * 1024:
            logging.error("Size too large!")
            break

        if current < 10:
            logging.error("Size too small!")
            break

        subprocess.run(["./gen_file.py", "--size", str(current)], check=True)

        if execute_compile(target):
            logging.info("COMPILE OK")
            low = current
        else:
            logging.warning("COMPILE FAIL")
            high = current

        if low is None:
            if high is None:
                logging.error("LOGIC ERROR!")
                break
            current = high // 2
        elif high is None:
            current = low * 2
        else:
            current = (high + low) // 2

    print("RESULT:")
    print("  LOW:  %d" % low)
    print("  HIGH: %d" % high)


if __name__ == "__main__":
    main()
