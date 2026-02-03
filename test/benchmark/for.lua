local start = os.clock()
for i = 0, 9 do
  local list = {}
  for j = 0, 1999999 do
    list[j] = j
  end

  local sum = 0
  for k, j in pairs(list) do
    sum = sum + j
  end
  io.write(sum .. "\n")
end
io.write(string.format("elapsed: %.8f\n", os.clock() - start))
