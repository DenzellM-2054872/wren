
var start = System.clock
for (i in 0...10) {
    var list = []
    for (j in 0...2000000) list.add(j)

    var sum = 0
    for (j in list) sum = sum + j

    System.print(sum)
}
System.print("elapsed: %(System.clock - start)")
