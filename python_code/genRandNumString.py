#!/usr/bin/python

import random

a = str()

# Generates a random string of numbers with "hello" at offset 88 and "WORLD" at offset 176
for i in range(88):
    a += str(random.randrange(10))

a += "hello"

for i in range(83):
    a += str(random.randrange(10))

a += "WORLD"

for i in range(100):
    a += str(random.randrange(10))

print a
