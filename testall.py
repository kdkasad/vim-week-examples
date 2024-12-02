#!/usr/bin/env python3


# this testing script is essentially the same as the one provided with
# a few improvements:
#
#  1. multithreading
#  2. less-cluttered output
#  3. fuzzy-match of pattern to execute only a subset of tests
#  4. more robust error checking
#  5. colors if supported by terminal
#  6. Automatic dependency installation (`thefuzz` for fuzzy match, `termcolor` for colors)
#
# the multithreading is not a problem w.r.t. the G.I.L. since
# we are not bound by computation in python

import os
import signal
import subprocess
import sys
from threading import Thread

dependency_installed = False
commit = True
try:
    from thefuzz import fuzz
except ModuleNotFoundError:
    try:
        ignored = input(
            "Required Library `thefuzz` not found. Press [ENTER] to install or Ctrl-C to exit."
        )
        ret = subprocess.run("python3 -m pip install thefuzz", shell=True)
        if ret.returncode != 0:
            print(f"Installation unsuccessful (return code {ret.returncode})")
            print(f"Exiting Abnormally!")
            sys.exit(1)
        else:
            print(f"Installation of `thefuzz` was successful")
            dependency_installed = True
    except KeyboardInterrupt:
        print("\nInstallation Not allowed. Exiting...")
        exit(1)


try:
    from termcolor import *
except ModuleNotFoundError:
    try:
        ignored = input(
            "Required Library `termcolor` not found. Press [ENTER] to install or Ctrl-C to exit."
        )
        ret = subprocess.run("python3 -m pip install termcolor", shell=True)
        if ret.returncode != 0:
            print(f"Installation unsuccessful (return code {ret.returncode})")
            print(f"Exiting Abnormally!")
            sys.exit(1)
        else:
            print(f"Installation of `termcolor` was successful")
            dependency_installed = True
    except KeyboardInterrupt:
        print("\nInstallation Not allowed. Exiting...")
        exit(1)


if dependency_installed:
    print("All required dependencies installed; execute the script again to run tests.")
    sys.exit(1)


SCC_COMMAND = "../scc"
GCC_COMMAND = "gcc"
MAXTIME = 10


# function to handle command error
def system(
    cmd_name, desc, timeout=None, cannotFail=False, combineStreams=False, shell=True
) -> [any, bool, bool]:
    try:
        stderrLocus = subprocess.STDOUT if combineStreams else subprocess.PIPE
        cmd = subprocess.run(
            cmd_name,
            shell=shell,
            stdout=subprocess.PIPE,
            stderr=stderrLocus,
            stdin=subprocess.PIPE,
            timeout=timeout,
        )
        if (cannotFail) and (cmd.returncode != 0):
            cprint(
                f"Error: Could not execute `{cmd_name}` command. "
                + f"(Error Code: {cmd.returncode}) ",
                "red",
                attrs=["bold"],
            )
            cprint("STDOUT:", attrs=["bold"])
            print(cmd.stdout.decode("UTF-8"))
            cprint("STDERR:", attrs=["bold"])
            print(cmd.stderr.decode("UTF-8"))

            sys.exit(1)
        return [cmd, (cmd.returncode == 0), True]
    except subprocess.TimeoutExpired:
        if cannotFail:
            cprint(f"Error:Command `{cmd_name}` timed out.", "red", attrs=["bold"])
            sys.exit(1)
        return [None, False, False]


def runtest_thread(test_object, index, db):
    db[index] = runtest(test_object[0], test_object[2])


# returns [points_attained, description of why points may be lost]
def runtest(test_name, test_description) -> [bool, str]:
    # remove files associated with test to be run
    # ignore output of this
    rm_cmd = system(
        f"rm -f {test_name} {test_name}.s {test_name}.scc {test_name}.gcc* {test_name}.out*",
        f"remove build/test artifacts for test {test_name}",
        cannotFail=False,
    )
    if not rm_cmd[1]:
        return [False, "`rm` command failed"]

    gcc_cmd = system(
        f"{GCC_COMMAND} -g -static -o {test_name}.gcc {test_name}.c",
        f"gcc-compilation of {test_name}.c to {test_name}.gcc",
        combineStreams=True,
        cannotFail=False,
    )

    # write `gcc` output to file!
    with open(f"{test_name}.gcc.out", "wb") as file:
        file.write(gcc_cmd[0].stdout)

    if gcc_cmd[1] == False:
        return [False, "`gcc` failed to compile .c file"]

    # run scc on file
    scc_cmd = system(
        f"{SCC_COMMAND} {test_name}.c",
        f"scc-compilation of {test_name}.c to {test_name}.s",
        cannotFail=False,
        combineStreams=True,
        timeout=MAXTIME,
    )
    if not scc_cmd[2]:
        return [False, "`scc` command timed out"]

    # write scc output to file
    with open(f"{test_name}.scc.out", "wb") as file:
        file.write(scc_cmd[0].stdout)

    if not scc_cmd[1]:
        return [False, "`scc` failed while compiling file"]

    # compile '.s' file with 'gcc'
    gcc_scc_cmd = system(
        f"{GCC_COMMAND} -g -static -o {test_name}.scc {test_name}.s",
        "gcc-compilation of {test_name}.s to {test_name}.scc",
        cannotFail=False,
        combineStreams=True,
    )

    with open(f"{test_name}.scc.gcc.out", "wb") as file:
        file.write(gcc_scc_cmd[0].stdout)

    if not gcc_scc_cmd[1]:
        return [False, "`gcc` failed to parse `.s` file generated by `scc`"]

    exec_gcc = system(
        f"./{test_name}.gcc",
        "test executable compiled with `gcc`",
        cannotFail=False,
        timeout=MAXTIME,
    )

    if not exec_gcc[2]:
        return [False, "executable compiled with `gcc` timed out"]

    with open(f"{test_name}.out.gcc", "wb") as file:
        file.write(exec_gcc[0].stdout)

    exec_scc = system(
        f"./{test_name}.scc",
        "test executable compiled with `gcc`",
        cannotFail=False,
        timeout=MAXTIME,
    )

    if not exec_scc[2]:
        return [0, "executable compiled with `scc` timed out"]

    with open(f"{test_name}.out.scc", "wb") as file:
        file.write(exec_scc[0].stdout)

    if exec_scc[0].returncode == 139:  # 139 = 128 + 11
        return [False, f"executable compiled with `scc` segfaulted"]

    if exec_gcc[0].stdout != exec_scc[0].stdout:
        return [False, "standard outputs did not match"]

        # the main function is void, technically...
        # elif (exec_gcc[0].returncode != exec_scc[0].returncode):
        #     return [False, "return codes did not match"]
    else:
        return [True, ""]


all_tests_possible = [
    ["test1", 1, "Simple Hello Program", True],
    ["global", 3, "Simple Test for Global Variables", True],
    ["local", 4, "Simple Test for Local Variables", True],
    ["args", 4, "Simple Test for Arguments", True],
    ["return", 3, "Simple Test for Arguments", True],
    ["nested", 3, "Test for nested function", True],
    ["div", 3, "Test for division", True],
    ["expr", 3, "Test expression", True],
    ["expr2", 3, "Test expression and vars", True],
    ["rel", 3, "Test Relational Operators", True],
    ["equal", 3, "Test Equality Operators", True],
    ["and", 3, "Test And Operator", True],
    ["or", 3, "Test Or Operator", True],
    ["if", 3, "Test If Statement", True],
    ["while", 3, "Test While Statement", True],
    ["dowhile", 3, "Test Do/While Statement", True],
    ["for", 4, "Test For Statement", True],
    ["break", 3, "Test break Statement", True],
    ["continue", 3, "Test continue Statement", True],
    ["array", 3, "Test arrays 1", True],
    ["array2", 3, "Test arrays 2", True],
    ["sum", 3, "Test sum of an array", True],
    ["max", 3, "Test max of an array", True],
    ["bubblesort", 4, "Bubble Sort of longs", True],
    ["quicksort", 4, "QuickSort of longs", True],
    ["fact", 3, "Test factorial", True],
    ["ampersand", 3, "Test ampersand operator", True],
    ["char", 3, "Array char access", True],
    ["char2", 3, "Array char assignment", True],
    ["strlen", 3, "Compute strlen", True],
    ["quicksortstr", 4, "Quicksort with strings", True],
    ["queens", 3, "8 queens problem", True],
]


def add_user_defined_tests():
    if not os.path.isfile("additional_tests.txt"):
        return  # nothing to discover
    try:
        with open("additional_tests.txt", "r") as file:
            lines = [line.strip().split(":") for line in file]
            new_tests = [[x[0], 0, x[1] if len(x) > 1 else "", False] for x in lines]
            all_tests_possible.extend(new_tests)
    except:
        cprint("Error adding user defined tests!", "red", attrs=["bold"])
        raise
        sys.exit(1)


def main():
    outfile = "total.txt"
    all_tests = all_tests_possible
    if len(sys.argv) > 2:
        cprint("Too many Arguments!", "red", attrs=["bold"])
        cprint(f"\tUsage: {sys.argv[0]} [test_pattern (optional)]", attrs=["bold"])
        sys.exit(1)

    # change directory to script's containing directory
    os.chdir(os.path.dirname(__file__))

    # user-defined more tests
    add_user_defined_tests()
    # fuzzy match tests!
    if len(sys.argv) == 2:
        search_term = sys.argv[1].lower()
        all_tests = [
            test
            for test in all_tests_possible
            if (fuzz.partial_ratio(test[0].lower(), search_term) > 80)
            or (fuzz.partial_ratio(test[2].lower(), search_term) > 80)
        ]
        outfile = "/dev/null"
        if len(all_tests) == 0:
            cprint("No tests found matching: ", attrs=["bold"], end="")
            cprint(sys.argv[1], "cyan", attrs=["bold"])
            sys.exit(0)

    # execute make commands to clean up artifacts
    system("make clean", "make clean", cannotFail=True)
    system("make scc", "make all", cannotFail=True)

    # go to the test directory
    os.chdir("./tests")
    results = [None for _ in all_tests]
    total_max = sum([x[1] for x in all_tests])
    threads = [
        Thread(target=runtest_thread, args=(x, i, results))
        for (i, x) in enumerate(all_tests)
    ]
    # start all threads
    for thread in threads:
        thread.start()

    # join all threads
    for thread in threads:
        thread.join()

    # output time!!
    username = os.environ.get("USER")
    # commit (with score)
    total = sum(
        [test[1] if result[0] else 0 for (test, result) in zip(all_tests, results)]
    )
    if commit:
        commit_msg = "Commit. ["
        if len(all_tests) == len(all_tests_possible):
            commit_msg += "Full Test"
        else:
            commit_msg += f'Partial Test with Query "{sys.argv[1]}"'

        commit_msg += f". {total}/{total_max}]"
        checkout_cmd = system(
            "git checkout master",
            "checkout to master",
            cannotFail=True,
        )
        with open("../.local.git.out", "ab") as file:
            file.write(checkout_cmd[0].stdout)

        system(
            "git add ../.local.git.out",
            "add .local.git.out to git",
            cannotFail=True,
        )
        system(
            "git add ../*.l ../*.y ../Makefile ./total.txt",
            "add files to git",
            cannotFail=False,
        )
        system(
            ["git", "commit", "-a", "-m", commit_msg],
            f"Commit with message: {commit_msg}",
            shell=False,
            cannotFail=True,
        )
        system("git push origin master", "git push", cannotFail=True)

    hash = (
        system(
            "git rev-parse --short HEAD", "get commit hash from git", cannotFail=True
        )[0]
        .stdout.decode("UTF-8")
        .strip()
        if commit
        else "Uncommitted changes"
    )
    num_commits = (
        system("git rev-list --count HEAD", "get #commits from git", cannotFail=True)[0]
        .stdout.decode("UTF-8")
        .strip()
    )

    col1_width = max(max([len(x[0]) for x in all_tests]), 10)
    col2_width = max(max([len(x[2]) for x in all_tests]), 25)
    col3_width = 14
    padding = 3
    total_width = 3 + col1_width + padding + col2_width + padding + col3_width
    cprint("-" * total_width, attrs=["bold"])
    cprint(f" CS250: Lab 6. Compiler Project.   User:  {username}", attrs=["bold"])
    cprint(
        "\n"
        + f" Last Git Commit Hash: {hash}\n"
        + f"                       ({num_commits} commits made)",
        attrs=["bold"],
    )
    cprint("-" * total_width, attrs=["bold"])
    # print to both stdout and [outfile] at once!
    with open(outfile, "w") as file:
        for test, result in zip(all_tests, results):
            score = test[1] if result[0] else 0
            if test[3]:  # if this is a test provided by the class
                file.write(
                    test[0].ljust(15)
                    + ": "
                    + test[2].ljust(36)
                    + ": "
                    + str(score).ljust(3)
                    + " of "
                    + str(test[1]).ljust(3)
                    + "\n"
                )
            out = "   " + test[0].ljust(col1_width) + " : " + test[2].ljust(col2_width)
            out += " : " + str(score).rjust(3) + " of " + str(test[1]).rjust(3)
            out += "   "
            if not result[0]:
                cprint(out, "white", "on_red", attrs=["bold"], end="")
                cprint(
                    "    " + result[1],
                    "yellow",
                )
            else:
                cprint(
                    out,
                    "green",
                )

        # output total
        cprint("-" * total_width, attrs=["bold"])
        file.write(f"                              Total:  {total} of {total_max}\n")
        cprint(
            "   Total:".ljust(total_width - col3_width)
            + str(total).rjust(3)
            + " of "
            + str(total_max).rjust(3)
            + "   ",
            "black",
            "on_white",
            attrs=["bold"],
        )
        cprint("-" * total_width, attrs=["bold"])


if __name__ == "__main__":
    main()
