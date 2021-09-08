#!/usr/bin/python
import sys
if len(sys.argv) != 2:
    print "ERROR: Expecting one parameter, name of the file to open"
    sys.exit(1)
else:
    FILENAME = sys.argv[1]

contents = ""

inputFile = open(FILENAME, 'r')
for line in inputFile:
    print line
    contents += line

inputFile.close()

outputFile = open("/tmp/" + FILENAME + "-test", 'w')
outputFile.write(contents)
outputFile.close()
