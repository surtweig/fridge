#ifndef POSIT_H
#define POSIT_H

typedef unsigned short Posit16;

#define POSIT16_ZERO 0x0000
#define POSIT16_NAR 0x8000
#define POSIT_SIZE 16

typedef struct Posit16Unpacked
{
    unsigned char sign;
    signed char regime;
    unsigned short exponent;
    unsigned short fraction;
} Posit16Unpacked;

typedef union Posit16_UF32
{
    float f32;
    unsigned int u32;
} Posit16_UF32;


typedef struct Posit16Environment
{
    unsigned short loss;
    unsigned char es;
    //unsigned long quire; TODO!
} Posit16Environment;

Posit16Environment Posit_env(unsigned char es);
Posit16 Posit_pack(Posit16Unpacked unpacked, const Posit16Environment* env);
Posit16Unpacked Posit_unpack(Posit16 packed, const Posit16Environment* env);
Posit16 Posit_fromFloat(float value, const Posit16Environment* env);
float Posit_toFloat(Posit16 value, const Posit16Environment* env);
Posit16 Posit_add(Posit16 a, Posit16 b, const Posit16Environment* env);
Posit16 Posit_sub(Posit16 a, Posit16 b, const Posit16Environment* env);
Posit16 Posit_mul(Posit16 a, Posit16 b, const Posit16Environment* env);
Posit16 Posit_div(Posit16 a, Posit16 b, const Posit16Environment* env);
Posit16 Posit_fmadd(Posit16 a, Posit16 b, const Posit16Environment* env);

#ifdef POSIT_TEST
Posit16 bitSeriesMask(int low, int high);
int bitSeriesCountRight(Posit16 src, int start);
Posit16 bitSet(Posit16 dst, int position, char bit);
char bitGet(Posit16 src, int position);
Posit16 bitCopy(Posit16 dst, Posit16 src, int offset, int count);
#endif

#endif
