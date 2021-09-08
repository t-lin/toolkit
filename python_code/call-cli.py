#!/usr/bin/python
import subprocess

# Execute command and re-direct stderr to stdout
cmdLine = "ls -l"
shell = subprocess.Popen(cmdLine, shell = True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)

stdout, stderr = shell.communicate()

if stdout:
    print stdout
