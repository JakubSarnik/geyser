import runner
from os import path
from string import Template

GEYSER_PATH = path.expanduser("~/workbench/geyser/cmake-build-release/run-geyser")
AIGSIM_PATH = path.expanduser("~/workbench/aiger/aigsim")
AIGBMC_PATH = path.expanduser("~/workbench/aiger/aigbmc")

HWMCC2010 = path.expanduser("~/workbench/geyser/benchmarks/hwmcc/2010")

BOUND = 20
TIMEOUT_SECS = 60

# Remove -m from aiger command if running AIGER 1.9!
GEYSER_CMD = f"{GEYSER_PATH} -e=bmc -k={BOUND} $aiger"
AIGBMC_CMD = f"{AIGBMC_PATH} $aiger -m {BOUND}"

TOOLS = [
    runner.Tool("geyser", Template(GEYSER_CMD), validate=True),
    runner.Tool("aigbmc", Template(AIGBMC_CMD), validate=False)
]


if __name__ == "__main__":
    runner.benchmarks(TOOLS, HWMCC2010, timeout_secs=TIMEOUT_SECS, aigsim_path=AIGSIM_PATH)