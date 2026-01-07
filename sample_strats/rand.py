#!/usr/bin/python3 -u

import random

R = int(input())
for _ in range(R):
    print(random.choice(['c', 'd']))
    other = input()
