import runner
from os import path
from string import Template

GEYSER_PATH = path.expanduser("~/workbench/geyser/cmake-build-release/run-geyser")
AIGSIM_PATH = path.expanduser("~/workbench/aiger/aigsim")

HWMCC2010 = path.expanduser("~/workbench/geyser/benchmarks/hwmcc/2010")
TIMEOUT_SECS = 60

GEYSER_PDR_CMD = f"{GEYSER_PATH} -e=pdr $aiger"
GEYSER_CAR_CMD = f"{GEYSER_PATH} -e=car --repush --no-predecessor-muc $aiger"

TOOLS = [
    runner.Tool("geyser-pdr", Template(GEYSER_PDR_CMD), validate=True),
    runner.Tool("geyser-car", Template(GEYSER_CAR_CMD), validate=True)
]


if __name__ == "__main__":
    runner.benchmarks(TOOLS, HWMCC2010, timeout_secs=TIMEOUT_SECS, aigsim_path=AIGSIM_PATH)
