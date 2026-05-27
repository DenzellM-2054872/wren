function fib(n, a, b)
  if n == 0 then return a end
  if n == 1 then return b end
  return fib(n - 1, b, a + b)
end

local start = clock()
local n = 60
write(fib(n, 0, 1), "\n")
for i = 1, 400000 do
  fib(n, 0, 1)
end
write(format("elapsed: %.8f\n", clock() - start))
