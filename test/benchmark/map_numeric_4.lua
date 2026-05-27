local start = clock()

local map = {}
for i = 1, 2000000 do
  map[i] = i
end

local sum = 0
for i = 1, 2000000 do
  sum = sum + map[i]
end
write(format("%.0f\n", sum))

for i = 1, 2000000 do
  map[i] = nil
end

write(format("elapsed: %.8f\n", clock() - start))
