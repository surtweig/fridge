cols = 40
rowSize = cols*2
rows = 20
title = "XCM2 Fridge"
corp = "\xc4\xc4\xc4\xc4 Plus&Minus Inc. 2008 \xc4\xc4\xc4\xc4"
corp2 = "Plus&Minus Inc."

# 12x6
snowflake = \
"  __/  \__  "\
"   _\/\/_   "\
" \_\_\/_/_/ "\
" / /_/\_\ \ "\
"  __/\/\__  "\
"    \  /    "

# 14x8
snowflake2 = \
"      /\      "\
" __   \/   __ "\
" \_\_\/\/_/_/ "\
"   _\_\/_/_   "\
"  __/_/\_\__  "\
" /_/ /\/\ \_\ "\
"      /\      "\
"      \/      "

color_bg = 0
color_fg = 11

vmem = []

for y in range(rows):
    for x in range(cols):
        #vmem.append(ord(s[x % len(s)]) / 16)
        #vmem.append(ord(s[x % len(s)]) % 16)
        #vmem.append(15 - y % 8)
        #vmem.append(y % 8)
        vmem.append(0)
        vmem.append(color_fg*16+color_bg)

def printText(px, py, s, fg = color_fg, bg = color_bg):
    for i in range(len(s)):
        addr = py*rowSize + (px+i)*2
        vmem[addr] = ord(s[i])
        vmem[addr + 1] = fg*16+bg

def printArt(px, py, w, h, s):
    for y in range(h):
        for x in range(w):
            vmem[(py+y)*rowSize + (px+x)*2] = ord(s[y*w+x])
            

#printText(3, 2, title, 15)
#printText(5, 19, corp, 8, 7)
#printArt(26, 2, 12, 6, snowflake)

printText(3, 2, title, 15, 0)
printText(21, 17, corp2, 15, 0)
printArt(14, 5, 14, 8, snowflake2)

fb = open("vmem.vhd", 'w')
fb.write("(\n")

for i in range(len(vmem)):
    fb.write('X"%0.2X"' % vmem[i])
    fb.write(", ")
    if i % 16 == 15:
        fb.write("\n")

fb.write("others => 0);")
fb.close()

