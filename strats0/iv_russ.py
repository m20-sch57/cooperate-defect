#!/usr/bin/python3 -u
from random import randint
R = int(input())
arr = []

N = 10

pres_arr = ['c' if j!=6 else 'd' for j in range(N)]

flag_alien = True

for i in range(R):
    if i < N:
        print(pres_arr[i])
        arr.append(input())
    
    elif i==N:
        if arr == pres_arr:
            flag_alien = False
        if flag_alien:
            if randint(0, R//2) < R//2:
                print(max(set(arr[-5:]), key=arr.count))
            else:
                print('d')
            arr.append(input())
        else:
            print('c')
            move = input()
            arr.append(move)
            if move == 'd':
                flag_alien = True
    
    else:
        if flag_alien:
            if randint(0, R//2) < R//2:
                print(max(set(arr[-5:]), key=arr.count))
            else:
                print('d')
            arr.append(input())
        else:
            print('c')
            move = input()
            arr.append(move)
            if move == 'd':
                flag_alien = True