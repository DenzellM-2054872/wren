class Fib {
  static get(n, a, b) {
    if (n == 0) return a
    if (n == 1) return b

    return get(n - 1, b, a + b)
  }
}

var start = System.clock
var n = 60
System.print(Fib.get(n, 0, 1))
for (i in 1..400000) {
  Fib.get(n, 0, 1)
}
System.print("elapsed: %(System.clock - start)")
