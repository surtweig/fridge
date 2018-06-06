n = 16#7639 #8010072064
buf = bytearray([1 for i in range(1024*1024)])

with open("img2.iso", "wb") as f:
    i = n
    while i > 0:
        print(i)
        f.write(buf)
        i -= 1
