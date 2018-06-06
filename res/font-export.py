import png
fn = "font2-modified"
r = png.Reader(fn + ".png")
img = r.read()

w = img[0]
h = img[1]
pixels = list(img[2])
params = img[3]
print(w, h, params)

charw = 6
charh = 8

tex = [ [0 for x in range(w)] for y in range(h) ]
x = 0
y = 0
for row in pixels:   
    for p in row:
        tex[y][x] = p
        x += 1
    x = 0
    y += 1

fb = open(fn + ".vhd", 'w')
fb.write("(\n")
charsHor = w/charw
charsVer = h/charh
for charJ in range(charsVer):
    for charI in range(charsHor):    
        fb.write("(");
        charX = charI * charw
        charY = charJ * charh
        for px in range(charw):
            charColData = 0            
            for py in range(charh):
                charColData <<= 1
                charColData += 1 if tex[charY + py][charX + px] > 0 else 0
                #fb.write("1" if tex[charY + py][charX + px] > 0 else "0")
            fb.write('X"%0.2X"' % charColData)
            #fb.write("\n")
            if px < charw-1:
                fb.write(", ")
        fb.write(")")
        if (charI < charsHor-1 or charJ < charsVer-1):
            fb.write(", ")
        fb.write("\n")
    fb.write("\n")
                
fb.write(");")
fb.close()

##fb = open(fn + ".vhd", 'w')
##fb.write("(\n")
##col = 0
##counter = 0
##for row in pixels:
##    for p in row:
##        if (p > 0):
##            fb.write("'1'")
##        else:
##            fb.write("'0'")
##        if (counter < w*h-1):
##            fb.write(",")
##        counter += 1
##    fb.write("\n")
##fb.write(");")
##fb.close()
