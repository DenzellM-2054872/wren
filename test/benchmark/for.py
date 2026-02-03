from __future__ import print_function

import time

# Map "range" to an efficient range in both Python 2 and 3.
try:
    range = xrange
except NameError:
    pass

start = time.process_time()
for i in range(0, 10):
  list = []
  for j in range(0, 2000000):
    list.append(j)

  sum = 0
  for j in list:
    sum += j
  print(sum)
print("elapsed: " + str(time.process_time() - start))