import png

cols = 40
rowSize = cols*2
rows = 20
def printText(vmem, px, py, s, fg, bg):
    addr = py*rowSize + px*2
    for i in range(len(s)):
        vmem[addr + i*2] = ord(s[i])
        vmem[addr + 1 + i*2] = fg*16+bg
        fg+=1
        bg+=1
        if (fg == 16):
            fg = 0
        if (bg == 16):
            bg = 0

fn = "ega-test2"
r = png.Reader(fn + ".png")
img = r.read()

w = img[0]
h = img[1]
pixels = list(img[2])
params = img[3]
print(w, h, params)

#pal = [0, 8, 7, 6, 15, 4, 12, 2, 5, 9, 1, 3, 14, 13, 11]

tex = [ [0 for x in range(w)] for y in range(h) ]
x = 0
y = 0
for row in pixels:   
    for p in row:
        tex[y][x] = p
        x += 1
    x = 0
    y += 1

vmem = []
for y in range(h):
    for x in range(w//2):
        vmem.append(tex[y][x*2]*16 + tex[y][x*2+1])

s = "Fridge is a 8-bit computer based on extended Intel 8080 instruction set with graphics acceleration. "\
    "Current implementation consists of an emulator (Windows x64 + DirectX 11) and VHDL design for FPGA "\
    "development board Terasic DE0-CV (Altera Cyclone V), as well as various tools such as - assembly compiler, "\
    "custom simplistic language compiler and an IDE."\
    "System specs "\
    "CPU "\
    "Modified Big-Endian Intel 8080 "\
    "Graphical instructions "\
    "10 MHz clock frequency "\
    "RAM "\
    "64 KB (16-bit address) "\
    "Video "\
    "Display: 240x160 pixels "\
    "4-bit pallette (16 colors) from 4096 possible colors "\
    "Two framebuffers 240x160x4 "\
    "64 KB sprite memory (102 KB total video memory) "\
    "40x20 ASCII text mode (6x8 font) "\
    "ROM "\
    "SD card (16 MB maximum)"
printText(vmem, 0, 0, s, 2, 7)

fb = open(fn + ".vhd", 'w')
fb.write("(\n")

i = 0
#for y in range(h):
#    for x in range(w//2):
for i in range(len(vmem)):
    fb.write('X"%0.2X"' % vmem[i])#(tex[y][x*2]*16 + tex[y][x*2+1]))
    fb.write(", ")
    if i % 16 == 15:
        fb.write("\n")
        #i += 1

fb.write('others => X"00");')
fb.close()
