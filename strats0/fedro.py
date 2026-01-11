#!/usr/bin/python3 -u

R = int(input())
last = 'c'
for i in range(R):
    print(last if i < R - 1 else 'd')
    last = input()