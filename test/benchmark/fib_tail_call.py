from __future__ import print_function

import time

def fib(n, a, b):
  if n == 0: return a
  if n == 1: return b
  return fib(n - 1, b, a + b)

start = time.process_time()
times = 60
print(fib(times, 0, 1))

for i in range(0, 400000):
  fib(times, 0, 1)

print("elapsed: " + str(time.process_time() - start))