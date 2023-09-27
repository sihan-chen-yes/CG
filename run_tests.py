"""
-------------------------------------------------------------------------------
DO NOT MODIFY THIS FILE

This file is used for automatic testing. Any modifications could disrupt the
test process and lead to inaccurate results.

If you believe changes are necessary, please contact the TAs.
-------------------------------------------------------------------------------
"""
import argparse
import pathlib
import glob
import subprocess
import sys


BASE_DIR = pathlib.Path(__file__).absolute().parent
SCENES_DIR = BASE_DIR / "scenes"


WARP_TESTS = [
    ("square", None),
    ("disk", None),
    ("uniform_sphere", None),
    ("uniform_sphere_cap", 0.1),
    ("uniform_sphere_cap", 0.4),
    ("uniform_sphere_cap", 0.95),
    ("uniform_hemisphere", None),
    ("cosine_hemisphere", None),
    ("beckmann", 0.05),
    ("beckmann", 0.10),
    ("beckmann", 0.30),
    ("beckmann", 0.75),
]


def _colored_str(msg: str, success: bool):
    code = "92" if success else "91"
    # While newer win32 support ANSI escape
    # codes, it is not very clean to check
    if sys.platform == "win32":
        return msg
    return f"\033[{code}m{msg}\033[0m"


def execute_test(cmd, verbose: bool):
    output = subprocess.run(cmd, capture_output=True)

    if verbose:
        print("Nori Outputs:")
        print(output.stdout.decode())
        print(output.stderr.decode())

    passed = output.returncode == 0

    if passed:
        print(_colored_str("-- Passed", success=True))
    else:
        print(_colored_str("-- Failed", success=False))

    return passed


def run_warptests(warptest_exe, verbose: bool):
    failed_tests = []
    num_tests = len(WARP_TESTS)

    for test in WARP_TESTS:
        print(f"Running {test}")

        warp_type, param = test
        if not isinstance(param, (list, tuple)):
            param = [param]
        param = [str(p) for p in param if p is not None]

        passed = execute_test([str(warptest_exe), warp_type, *param], verbose=verbose)

        if not passed:
            failed_tests.append(f"PA2: {test}")

    num_passed = num_tests - len(failed_tests)
    success = len(failed_tests) == 0

    print(_colored_str(f"PA2 - Passed {num_passed}/{num_tests}", success=success))
    print("")

    return failed_tests


def run_tests(build_dir: pathlib.Path, pa_id: int, verbose: bool):
    ext = ".exe" if isinstance(build_dir, pathlib.WindowsPath) else ""
    nori_exe = build_dir / f"nori{ext}"
    warptest_exe = build_dir / f"warptest{ext}"

    if pa_id == 2:
        if not warptest_exe.exists():
            raise RuntimeError(f"{warptest_exe} does not exist")
        return run_warptests(warptest_exe, verbose)

    if not nori_exe.exists():
        raise RuntimeError(f"{nori_exe} does not exist")

    if not SCENES_DIR.exists():
        raise RuntimeError(f"{SCENES_DIR} does not exist")

    test_files = glob.glob(str(SCENES_DIR / f"pa{pa_id}/**/*test*.xml"), recursive=True)

    failed_tests = []
    num_tests = len(test_files)

    for test_file in test_files:
        print(f"Running {test_file}")
        passed = execute_test([str(nori_exe), "-b", str(test_file)], verbose=verbose)

        if not passed:
            failed_tests.append(f"PA{pa_id}: {test_file}")

    num_passed = num_tests - len(failed_tests)
    success = len(failed_tests) == 0

    print(
        _colored_str(f"PA{pa_id}  - Passed {num_passed}/{num_tests}", success=success)
    )
    print("")

    return failed_tests


def main():
    PA = [1, 2, 3, 4]
    DEFAULT_BUILD_DIR = BASE_DIR / "build"
    parser = argparse.ArgumentParser(description="Run Nori tests")
    parser.add_argument(
        "--build_dir",
        help=f"Path to Nori build directory (containing `nori` and `warptest` executables). Default: {DEFAULT_BUILD_DIR}",
        default=DEFAULT_BUILD_DIR,
    )
    parser.add_argument("--verbose", action="store_true", help="Print Nori outputs")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--pa", type=int, help="Assigment number", choices=PA, nargs="+")
    group.add_argument(
        "--all", action="store_true", help="Run tests for all assigments"
    )
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir)
    if not build_dir.exists():
        raise RuntimeError(f"Build directory {build_dir} does not exist")

    assigments = PA if args.all else args.pa

    failed_tests = []
    for pa in assigments:
        failed_tests.extend(
            run_tests(build_dir=build_dir, pa_id=pa, verbose=args.verbose)
        )

    if args.verbose:
        if len(failed_tests) > 0:
            print(_colored_str(f"Failed {len(failed_tests)} tests:", False))
            for test in failed_tests:
                print(_colored_str(test, False))

        else:
            print(_colored_str("All tests passed!", True))

    if len(failed_tests):
        exit(1)


if __name__ == "__main__":
    main()
