

def fact(n):
    ret = 1
    for q in range(1, n+1):
        ret *= q
    return ret

s = 0
start = 1
stop = 20
for q in range(start, stop):
    s += 1/fact(q)
print(s)
