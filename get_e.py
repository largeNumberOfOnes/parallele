#!/bin/python3

import sys

with open('e_ref.txt', 'r') as file:
    for _ in range(19):
        file.readline()

    n = int(sys.argv[1])
    for q in range(n // 60):
        print(file.readline()[(q==0):-1], end='');
    print(file.readline()[(n <= 60):n % 60])


