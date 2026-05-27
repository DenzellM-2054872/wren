local start = clock()
for i = 0, 9 do
  local list = {}
  for j = 0, 1999999 do
    list[j] = j
  end

  local sum = 0
  for j = 0, getn(list) do
    sum = sum + list[j]
  end
  write(sum, "\n")
end
write(format("elapsed: %.8f\n", clock() - start))
