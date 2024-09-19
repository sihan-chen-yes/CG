import subprocess
import argparse
from os import path
import sys
import re



def _colored_str(msg: str, success: bool):
    code = "92" if success else "91"
    # While newer win32 support ANSI escape
    # codes, it is not very clean to check
    if sys.platform == "win32":
        return msg
    return f"\033[{code}m{msg}\033[0m"


def cerr(msg):
    print(_colored_str(msg, success=False))


def run_command(command):
    try:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        cerr(f"Error: {e.stderr.strip()}")
        raise


def check_repo_size():
    commits = run_command(["git", "rev-list", '--since="6 month ago"', "HEAD"])
    commits = commits.split("\n")

    threshold = 1 * (1024**2)  # 1 MB
    exceeding_commit = None

    for commit_hash in commits:
        files = run_command(["git", "ls-tree", "-r", "-l", commit_hash])
        files = re.findall(
            r"^\d+\s+blob\s+\S+\s+(\d+)\s+(.*)$", files, flags=re.MULTILINE
        )

        for size, filename in files:
            if int(size) > threshold:
                cerr(
                    f"File `{filename}` at commit `{commit_hash}` is exceeding 1MB blob size limit"
                )
                exceeding_commit = commit_hash

    if exceeding_commit is not None:
        raise RuntimeError(
            _colored_str(
                f"""
There are files in the git tree that exceed the limit of 1MB.
Make sure to use GIT LFS for any object files.
In order to fix it, you need to rewrite the git history starting 
commit {exceeding_commit}. See the Nori page for more information.
If you believe that this error message is wrong or you run into difficulties
fixing the issue, please contact the TA team.
""",
                success=False,
            )
        )


def check_print():
    # 1. Repo checks
    # Check if the current directory is a git repository
    if not path.isdir(".git"):
        cerr("Error: Not a git repository.")
        return
    
    # List all the remotes, and choose the one with namespace COURSE-CG23 as the working repo for pushing
    remotes = [r for r in run_command(["git", "remote"]).split("\n") if r.strip()]
    if len(remotes) == 0:
        cerr("Error: No remote repo found.")
        return
    
    working_remote = None
    working_remote_url = None
    for remote in remotes:
        url = run_command(["git", "remote", "get-url", remote])
        if "COURSE-CG" in url.upper():
            if working_remote is not None:
                cerr("Error: More than one working remote repo with namespace COURSE-CG* is found.")
                return
            working_remote = remote
            working_remote_url = url.lower()
    
    if working_remote is None:
        cerr("Error: No working remote repo with namespace COURSE-CG* is found.")
        return
    
    matches = re.search(r"(course-cg\d+/[\w\d]+)\.git", working_remote_url)
    if matches:
        url_extension = matches.group(1)
    else:
        cerr(f"Error: Cannot parse the ETH username from the working remote {working_remote_url}.")
        return
    
    # Raise a warning if we are on a branch other than master/main
    current_branch = run_command(["git", "rev-parse", "--abbrev-ref", "HEAD"])
    if current_branch not in ["master", "main"]:
        print(f"Warning: you are on branch {current_branch}. Make sure you are on the correct branch before submitting.")

    remote_branches = run_command(["git", "branch", "-r"])


    # 2. Warn if there are uncommitted changes
    unstaged_changes = run_command(["git", "diff", "--name-only"]).strip()
    num_unstaged_changes = len([file for file in unstaged_changes.split('\n') if file.strip()])
    if num_unstaged_changes > 0:
        print(f"Warning: you still have {num_unstaged_changes} unstaged changes. Make sure they do not contain anything you want to submit.")

    staged_changes = run_command(["git", "diff", "--cached", "--name-only"]).strip()
    num_staged_changes = len([file for file in staged_changes.split('\n') if file.strip()])
    if num_staged_changes > 0:
        print(f"Warning: you still have {num_staged_changes} staged changes. Make sure you commit them before submitting.")


    # 3. Check if the latest local commit is up to date with the remote branch
    remote_branch = f"{working_remote}/{current_branch}"
    if remote_branch not in remote_branches:
        cerr(f"You have not pushed your branch {current_branch} to {working_remote} yet.")
        return

    local_head = run_command(["git", "rev-parse", "HEAD"])
    remote_head = run_command(["git", "rev-parse", remote_branch])
    if local_head == remote_head:
        # Print the hash of the working_remote's current commit
        print(_colored_str(f"Your branch is up to date with {working_remote}.", True))
        print(f"Commit hash for the submission on Moodle: {local_head}")
        url = f'https://gitlab.inf.ethz.ch/{url_extension}/-/commit/{local_head}'
        print(f"URL: {url}")
    else:
        cerr(f"You have not pushed your latest commit to {working_remote} yet. Please push all your changes before submitting.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--no-remote-check", action="store_false", dest="remote_check")
    parser.add_argument("--no-size-check", action="store_false", dest="size_check")
    args = parser.parse_args()

    if args.remote_check:
        check_print()

    if args.size_check:
        check_repo_size()
