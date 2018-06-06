ircodes =\
{
    "jmp"  :0,
    "cmprc":1,
    "neqrc":2,
    "set"  :3,
    "cpy"  :4,
    "inc"  :5,
    "dec"  :6,
    "drwr" :7,
    "drwc" :8,
    "ersr" :9,
    "ersc":10,
    "bcpy":11,
    "cmpbc":12,
    "neqbc":13,
    "cmprr":14,
    "neqrr":15,
    "idle":31,
}
ircodesSet = set(ircodes.keys())

irsignatures =\
{
    "jmp"  :('c','c'),
    "cmprc":('r','c'),
    "neqrc":('r','c'),
    "set"  :('r','c'),
    "cpy"  :('r','r'),
    "inc"  :('r', ''),
    "dec"  :('r', ''),
    "drwr" :('r','r'),
    "drwc" :('c','c'),
    "ersr" :('r','r'),
    "ersc" :('c','c'),
    "bcpy" :('b','r'),
    "cmpbc":('b','c'),
    "neqbc":('b','c'),
    "cmprr":('r','r'),
    "neqrr":('r','r'),
    "idle" :('', ''),
}

regsNumber = 32
linesOnPageNumber = 32
robsNumber = 4
wbits = 5
jmpCode = 0
regcodes = {"r"+str(i):i for i in range(regsNumber)}
robcodes = {"b"+str(i):i for i in range(robsNumber)}

program = []
aliases = {}
entries = {}
source = []

def loadSource(filename):
    print("Loading source...")
    fs = open(filename, 'r')
    lines = fs.readlines()
    irLineCounter = 0
    for line in lines:
        words = line.split()
        cntr = irLineCounter
        if (len(words) == 0 or words[0][0] == '/'):
            continue
        if (words[0] not in ircodesSet and aliases.get(words[0], "") not in ircodesSet):
            #cntr = -1
            pass
        else:
            irLineCounter += 1

        source.append( (cntr, words) )
    print(str(irLineCounter) + " instructions will be compiled")
    fs.close()

def readAliases():
    for line in source:
        if (line[1][0] == "alias"):
            if (len(line[1]) != 3):
                print("ERROR: wrong '"+line[1][0]+"' alias declaration syntax.")
                return -1
            aliases[line[1][2]] = line[1][1]
            
def updateAliases(line):
    if (line[1][0] == "alias"):
        if (len(line[1]) != 3):
            print("ERROR: wrong '"+line[1][0]+"' alias declaration syntax.")
            return -1
        aliases[line[1][2]] = line[1][1]

def readEntries():
    for line in source:
        if (line[1][0] == "entry"):
            if (len(line[1]) != 2 and line[1][1][-1] != ":"):
                print("ERROR: wrong '"+line[1][0]+"' entry declaration syntax.")
                return -1
            entries[line[1][1][:-1]] = line[0]

def dealias():
    for line in source:
        if (line[0] >= 0):
            for i in range(len(line[1])):
                line[1][i] = aliases.get(line[1][i], line[1][i])
                
def dealias(line):
    dline = (line[0], [])
    if (line[0] >= 0):
        for i in range(len(line[1])):
            dline[1].append(aliases.get(line[1][i], line[1][i]))
    return dline
    
def dealias_p(line):
    dline = [aliases.get(line[i], line[i]) for i in range(len(line))]
    return dline

def buildProgram():
    aliases = {}
    for line in source:
        updateAliases(line)
        dline = dealias(line)
        ircode = ircodes.get(dline[1][0], -1)
        if (ircode >= 0):
            ir = dline[1][0]
            arg1 = ""
            arg2 = ""
            if len(dline[1]) >= 2:
                arg1 = dline[1][1]
            if len(dline[1]) >= 3:
                arg2 = dline[1][2]
            if (ircode == jmpCode):
                jmpPos = entries.get(dline[1][1], -1)
                if (jmpPos < 0):
                    print("ERROR @"+str(dline[0])+": entry '" + dline[1][1] + "' is not declared.")
                    return -1
                jmpL = jmpPos % linesOnPageNumber
                jmpP = jmpPos // linesOnPageNumber
                arg1 = str(jmpL)
                arg2 = str(jmpP)

            if (arg2 != ""):
                program.append((ir, arg1, arg2))
            elif (arg1 != ""):
                program.append((ir, arg1))
            else:
                program.append((ir,))
                
        else:
            if dline[1][0] != "alias" and dline[1][0] != "entry":
                print("ERROR @"+str(dline[0])+": unknown operator '" + dline[1][0] + "'.")
                return -1

def extractBits(c):
    bits = []
    n = c
    for i in range(wbits):
        bits.append(n % 2)
        n = n // 2
    return bits

def codesToHex15(c1, c2, c3):
    bitArray = []
    bitArray.extend(extractBits(c1))
    bitArray.extend(extractBits(c2))
    bitArray.extend(extractBits(c3))
    hexStr = ""
    hexDigits = "0123456789abcdef"

    hexStr += hexDigits[bitArray[12] + 2*bitArray[13] + 4*bitArray[14]]
    hexStr += hexDigits[bitArray[8] + 2*bitArray[9] + 4*bitArray[10] + 8*bitArray[11]]
    hexStr += hexDigits[bitArray[4] + 2*bitArray[5] + 4*bitArray[6] + 8*bitArray[7]]
    hexStr += hexDigits[bitArray[0] + 2*bitArray[1] + 4*bitArray[2] + 8*bitArray[3]]
    return hexStr

def programToBinary(outputFileName, intermediateOutput):
    fb = open(outputFileName, 'w')
    if (len(intermediateOutput) > 0):
        fi = open(intermediateOutput, 'w')
    else:
        fi = None
    
    fb.write("v2.0 raw\r\n")
    
    if (fi):
        fi.write("// Generated by XCM assembler compiler\n// Source: "+outputFileName+"\n")
    
    aliases = {}
    
    for line in program:
        #updateAliases(line)
        dline = dealias_p(line)
        
        ir = ircodes[dline[0]]

        arg1type = ''
        arg2type = ''
        arg1r = -1
        arg1b = -1
        arg1c = -1
        arg2r = -1
        arg2b = -1
        arg2c = -1        

        if len(dline) >= 2:
            arg1r = regcodes.get(dline[1], -1)
            arg1b = robcodes.get(dline[1], -1)
            arg1c = (int(dline[1]) if dline[1].isdigit() else -1)   
            arg1type = ('r' if arg1r >= 0 else ('b' if arg1b >= 0 else ('c' if arg1c >= 0 else '?')))
            if (arg1type == '?'):
                print("ERROR: Unknown identifier '" + dline[1] + "'.")
                fb.close()
                return -1

        if len(dline) >= 3:
            arg2r = regcodes.get(dline[2], -1)
            arg2b = robcodes.get(dline[2], -1)
            arg2c = (int(dline[2]) if dline[2].isdigit() else -1)
            arg2type = ('r' if arg2r >= 0 else ('b' if arg2b >= 0 else ('c' if arg2c >= 0 else '?')))
            if (arg2type == '?'):
                print("ERROR: Unknown identifier '" + dline[2] + "'.")
                fb.close()
                return -1
        
        if (irsignatures[dline[0]][0] != arg1type or irsignatures[dline[0]][1] != arg2type):
            print("ERROR: Instruction '" + dline[0] + "' cannot accept arguments of types " + arg1type + arg2type + ".")
            print(dline)
            fb.close()
            return -1

        arg1val = (arg1r if arg1r >= 0 else (arg1b if arg1b >= 0 else (arg1c if arg1c >= 0 else 0)))
        arg2val = (arg2r if arg2r >= 0 else (arg2b if arg2b >= 0 else (arg2c if arg2c >= 0 else 0)))
            
        fb.write(codesToHex15(ir, arg1val, arg2val))
        fb.write(" ")
        if (fi):
            fi.write(dline[0] + " " + str(arg1val) + " " + str(arg2val) + "\n")

    print("Program compiled successfully and saved to " + outputFileName)
    fb.close()

    if (fi):
        fi.close()
        
def programToList():
   
    aliases = {}
    prg = []
    for line in program:
        #updateAliases(line)
        dline = dealias_p(line)
        
        ir = ircodes[dline[0]]

        arg1type = ''
        arg2type = ''
        arg1r = -1
        arg1b = -1
        arg1c = -1
        arg2r = -1
        arg2b = -1
        arg2c = -1        

        if len(dline) >= 2:
            arg1r = regcodes.get(dline[1], -1)
            arg1b = robcodes.get(dline[1], -1)
            arg1c = (int(dline[1]) if dline[1].isdigit() else -1)   
            arg1type = ('r' if arg1r >= 0 else ('b' if arg1b >= 0 else ('c' if arg1c >= 0 else '?')))
            if (arg1type == '?'):
                print("ERROR: Unknown identifier '" + dline[1] + "'.")
                fb.close()
                return -1

        if len(dline) >= 3:
            arg2r = regcodes.get(dline[2], -1)
            arg2b = robcodes.get(dline[2], -1)
            arg2c = (int(dline[2]) if dline[2].isdigit() else -1)
            arg2type = ('r' if arg2r >= 0 else ('b' if arg2b >= 0 else ('c' if arg2c >= 0 else '?')))
            if (arg2type == '?'):
                print("ERROR: Unknown identifier '" + dline[2] + "'.")
                fb.close()
                return -1
        
        if (irsignatures[dline[0]][0] != arg1type or irsignatures[dline[0]][1] != arg2type):
            print("ERROR: Instruction '" + dline[0] + "' cannot accept arguments of types " + arg1type + arg2type + ".")
            print(dline)
            fb.close()
            return -1

        arg1val = (arg1r if arg1r >= 0 else (arg1b if arg1b >= 0 else (arg1c if arg1c >= 0 else 0)))
        arg2val = (arg2r if arg2r >= 0 else (arg2b if arg2b >= 0 else (arg2c if arg2c >= 0 else 0)))
            
        prg.append(ir)
        prg.append(arg1val)
        prg.append(arg2val)
        
    return prg
    
    
def programToVHDL(outputFileName):
    fb = open(outputFileName, 'w')
    name = outputFileName.split(".")[0]
    fb.write("-- XCM1 ROM image generated by XCM assembler compiler\r\n")
    fb.write("library ieee;\nuse ieee.std_logic_1164.all;\nuse ieee.numeric_std.all;\nuse work.XCMGlobals.all;\nuse work.XCMIRCodes.all;\npackage "+name+"ROMImage is\nconstant "+name+"Data : XCMROMData := (\n")
       
    wordsInLine = 0;
    wordsTotal = 0;
    
    prg = programToList()
    romSize = 1024*3
    
    for pos in range(romSize):
            
        word = 0
        if (pos < len(prg)):
            word = prg[pos]
            
        fb.write(str(word))
        if (pos < romSize-1):
            fb.write(",")
        wordsInLine += 1
        wordsTotal += 1
        if (wordsInLine == 48):
            fb.write("\n")
            wordsInLine = 0        

    print("Program compiled successfully and saved to " + outputFileName)
    fb.write("\n); end "+name+"ROMImage;")
    fb.close()


def run(sourceFileName, outputFileName, outType, intermediateOutput = ""):
    loadSource(sourceFileName)
    #if readAliases() == -1:
    #    return -1
    if readEntries() == -1:
        return -1
    #if dealias() == -1:
    #    return -1
    if buildProgram() == -1:
        return -1
    if (outType == "raw2"):
        if programToBinary(outputFileName, intermediateOutput) == -1:
            return -1
    elif (outType == "vhdl"):
        if programToVHDL(outputFileName) == -1:
            return -1
    else:
        print("Unknown output type: " + outType)
        return -1

import sys
print("XCM assembler compiler v1.0")
print("Copyright (c) Plus&Minus Inc. 2008\n")

if len(sys.argv) == 1:
    print("Command line usage:")
    print("   <XCM source file name>\n   <Output file name>\n   <Output type: raw2, vhdl>\n    [XCM intermediate output]\n")
    if run(input("XCM source: "), input("BIN output: ")) == -1:
        print("Compilation failed due to errors.")
    input("Press ENTER to close this window.")
else:
    #print(sys.argv[1:])
    if run(sys.argv[1], sys.argv[2], sys.argv[3], (sys.argv[4] if len(sys.argv) == 5 else "")) == -1:
        print("Compilation failed due to errors.")

