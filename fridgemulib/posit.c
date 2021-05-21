#define BIT_HPOS(p) POSIT_SIZE-1-(p)

#include "posit.h"

int getRegimeSize(int regime)
{
    if (regime >= 0)
        return regime + 1;
    else
        return -regime;
}

int getExpSize(int es, int regSize)
{
    int expsize = POSIT_SIZE - regSize - 2; // -1 for sign, -1 for regime terminator
    if (expsize > es)
        expsize = es;
    return expsize;
}

Posit16Environment Posit_env(unsigned char es)
{
    Posit16Environment env;
    env.loss = 0;
    env.es = es;
    if (es == 0)
        env.es = 1;
    return env;
}

Posit16 bitSet(Posit16 dst, int position, char bit)
{
    Posit16 m = 1 << position;
    dst &= ~m;
    if (bit)
        dst |= m;
    return dst;
}

char bitGet(Posit16 src, int position)
{
    return ((src & (1 << position)) > 0) ? 1 : 0;
}

// returns a mask with continuous series of ones from low to high position inclusively
// e.g.: bitSeriesMask(3, 9) = 0000001111111000b
Posit16 bitSeriesMask(int low, int high)
{
    Posit16 m = (1 << (high-low+1))-1;
    return m << low;
}

// writes count bits from src starting from low bit to dest with offset
Posit16 bitCopy(Posit16 dst, Posit16 src, int offset, int count)
{
    if (count <= 0)
        return dst;

    Posit16 clearMask = ~bitSeriesMask(offset, offset+count-1);
    Posit16 result = dst & clearMask;

    src <<= offset;
    src &= ~clearMask;

    result |= src;

    return result;
}

Posit16 Posit_pack(Posit16Unpacked unpacked, const Posit16Environment* env)
{
    Posit16 r = POSIT16_ZERO;

    // sign
    r = unpacked.sign << (POSIT_SIZE-1);

    // regime
    int regsize = getRegimeSize(unpacked.regime);
    Posit16 reg = 0;
    if (unpacked.regime >= 0)
        reg = bitSeriesMask(BIT_HPOS(regsize), BIT_HPOS(1));//POSIT_SIZE-2-(unpacked.regime+1), POSIT_SIZE-2);
    else
        reg = bitSet(0, BIT_HPOS(regsize+1), 1);

    r |= reg;

    // exponent
    int expsize = getExpSize(env->es, regsize);
    if (unpacked.exponent > 0)
    {
        int expoffset = BIT_HPOS(regsize + 1 + expsize);
        if (expsize > 0)
        {
            r = bitCopy(r, unpacked.exponent, expoffset, expsize);
        }
    }

    // fraction
    if (unpacked.fraction > 0)
    {
        int fracsize = POSIT_SIZE-1-regsize-1-expsize;
        if (fracsize > 0)
        {
            r = bitCopy(r, unpacked.fraction, 0, fracsize);
        }
    }

    return r;
}

Posit16Unpacked Posit_unpack(Posit16 packed, const Posit16Environment* env)
{

}

Posit16 Posit_fromFloat(float value, const Posit16Environment* env)
{
    Posit16_UF32 uf;
    uf.f32 = value;

    // unpacking IEEE-754 single float
    int fbias = -127;
    int fsign = (uf.u32 >> 31);
    int fexp = (uf.u32 >> 23) & 0xFF;
    int ffrac = (uf.u32 & 0x7FFFFF);


}

float Posit_toFloat(Posit16 value, const Posit16Environment* env)
{

}

Posit16 Posit_add(Posit16 a, Posit16 b, const Posit16Environment* env)
{

}

Posit16 Posit_sub(Posit16 a, Posit16 b, const Posit16Environment* env)
{

}

Posit16 Posit_mul(Posit16 a, Posit16 b, const Posit16Environment* env)
{

}

Posit16 Posit_div(Posit16 a, Posit16 b, const Posit16Environment* env)
{

}

Posit16 Posit_fmadd(Posit16 a, Posit16 b, const Posit16Environment* env)
{

}






