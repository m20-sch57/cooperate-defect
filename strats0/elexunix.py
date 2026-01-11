#!/usr/bin/python3 -u
from random import random

# we do this strategy because q@2"Fe`|&&ZTt+'Y|z$'
if random() < 1e-4:
  print('hi everyone how is it going.')

R = int(input())

plan = 1  # you know, infants are usually optimistic, it is for a reason

for i in range(R):
  print('dc'[random() < plan])
  other = input()
  assert other in 'dc'
  other = other == 'c'
  plan = 1 if other else .01
  # season it with a bit of scammification:
  plan = max(0, plan - ((i+1)/R)**3)
