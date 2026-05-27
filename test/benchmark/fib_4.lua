function fib(n)
  if n < 2 then return n end
  return fib(n - 2) + fib(n - 1)
end

local start = clock()
for i = 1, 10 do
  write(fib(31), "\n")
end
write(format("elapsed: %.8f\n", clock() - start))
