import os
from enum import Enum
from time import time
from typing import List
from string import Template
from subprocess import Popen, STDOUT, PIPE, run
from dataclasses import dataclass


@dataclass
class Tool:
    name: str
    cmd: Template
    validate: bool


@dataclass
class Benchmark:
    path: str
    relative: str


class BenchmarkResult(Enum):
    SAFE = 0
    UNSAFE = 1
    UNKNOWN = 2
    TIMEOUT = 3
    FAILURE = 4  # Non-zero return value
    INVALID = 5  # Invalid witness if checking enabled


def validate(bench: Benchmark, witness: str, aigsim_path: str) -> BenchmarkResult:
    cmd = f"{aigsim_path} -w {bench.path}"
    res = run(cmd, stdout=PIPE, stderr=STDOUT, text=True, shell=True, input=witness)
    out = res.stdout

    # The last line will either be
    #   Trace is a witness for: { b0 }
    # indicating a correct trace, or a
    #   Trace is a witness for: { }
    # indicating a bad trace

    if out.splitlines()[-1].removeprefix("Trace is a witness for: { ").startswith("b0"):
        return BenchmarkResult.UNSAFE
    else:
        return BenchmarkResult.INVALID


def check_output(tool: Tool, bench: Benchmark, aigsim_path: str, output) -> BenchmarkResult:
    assert output is not None

    lines = []

    for line in output:
        if line.startswith(('u', 'c')) or line.strip() == "":
            continue

        lines.append(line.strip())

    assert len(lines) > 0

    match lines[0]:
        case "0":
            return BenchmarkResult.SAFE
        case "1":
            if tool.validate:
                return validate(bench, "\n".join(lines), aigsim_path)
            else:
                return BenchmarkResult.UNSAFE
        case "2":
            return BenchmarkResult.UNKNOWN


def benchmark(tools: List[Tool], bench: Benchmark, *, timeout_secs, aigsim_path) -> None:
    max_tool_length = len(max(tools, key=lambda t: len(t.name)).name)

    def print_result(tool: Tool, result: BenchmarkResult) -> None:
        prefix = f"{tool.name}:".ljust(max_tool_length)

        match result:
            case BenchmarkResult.SAFE:
                res = "safe"
            case BenchmarkResult.UNSAFE:
                res = "unsafe"
            case BenchmarkResult.UNKNOWN:
                res = "unknown"
            case BenchmarkResult.TIMEOUT:
                res = f"TIMEOUT after {timeout_secs} s"
            case BenchmarkResult.FAILURE:
                res = "FAILED with non-zero exit code"
            case BenchmarkResult.INVALID:
                res = "claims unsafe, but with INCORRECT WITNESS"
            case _:
                res = "<?>"

        print(f"\t{prefix} {res}")

    def start(cmd: Template) -> Popen:
        real_cmd = cmd.substitute(aiger=bench.path)
        return Popen(real_cmd, stderr=STDOUT, stdout=PIPE, text=True, shell=True)

    procs = [start(tool.cmd) for tool in tools]

    all_stopped = False
    start_time = time()

    while not all_stopped and time() - start_time < timeout_secs:
        all_stopped = True

        for proc in procs:
            if proc.poll() is None:
                all_stopped = False

    print(bench.relative)

    for tool, proc in zip(tools, procs):
        if proc.poll() is None:
            proc.kill()
            res = BenchmarkResult.TIMEOUT
        elif proc.returncode != 0:
            res = BenchmarkResult.FAILURE
        else:
            res = check_output(tool, bench, aigsim_path, proc.stdout)

        print_result(tool, res)


def benchmarks(tools: List[Tool], root_dir: str, *, timeout_secs, aigsim_path=None) -> None:
    def locate() -> List[Benchmark]:
        bs = []

        for dirname, _, files in os.walk(root_dir):
            for filename in files:
                if filename.lower().endswith(('.aig', '.aag')):
                    path = str(os.path.join(dirname, filename))
                    relative = os.path.relpath(path, root_dir)

                    bs.append(Benchmark(path, relative))

        return bs

    for bench in locate():
        benchmark(tools, bench, timeout_secs=timeout_secs, aigsim_path=aigsim_path)
