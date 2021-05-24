#define BIT_HPOS(p) POSIT_SIZE-1-(p)

#include "posit.h"
#include <math.h>

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

int getFracSize(const Posit16Unpacked* unpacked, const Posit16Environment* env)
{
    int regsize = getRegimeSize(unpacked->regime);
    return POSIT_SIZE - regsize - getExpSize(env->es, regsize) - 2;
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

int bitSeriesCountRight(Posit16 src, int start)
{
     Posit16 m = 1 << start;
     char bit = bitGet(src, start);
     int c = 1;
     for (c = 1; c < start+1; ++c)
     {
         m >>= 1;
         if ( ((src & m) > 0) ^ bit )
             return c;
     }
     return c;
}

// returns a mask with continuous series of ones from low to high position inclusively
// e.g.: bitSeriesMask(3, 9) = 0000001111111000b
int bitSeriesMask(int low, int high)
{
    int m = (1 << (high-low+1))-1;
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

Posit16 bitCopyOffset(Posit16 dst, Posit16 src, int dstOffset, int srcOffset, int count)
{
    if (count <= 0)
        return dst;

    Posit16 clearMask = ~bitSeriesMask(dstOffset, dstOffset+count-1);
    Posit16 result = dst & clearMask;

    if (dstOffset >= srcOffset)
        src <<= dstOffset-srcOffset;
    else
        src >>= srcOffset-dstOffset;

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
    Posit16Unpacked unpacked;

    unpacked.sign = bitGet(packed, BIT_HPOS(0));

    // regime
    int regsize = bitSeriesCountRight(packed, BIT_HPOS(1));
    if (bitGet(packed, BIT_HPOS(1)))
        unpacked.regime = regsize-1;
    else
        unpacked.regime = -regsize;

    // exponent
    int expsize = getExpSize(env->es, regsize);
    unpacked.exponent = 0;
    unpacked.exponent = bitCopyOffset(0, packed, 0, BIT_HPOS(regsize+1+expsize), expsize);
    //unpacked.exponent = bitCopy(0, packed >> fracsize, 0, expsize);

    // fraction
    int fracsize = POSIT_SIZE-1-regsize-1-expsize;
    unpacked.fraction = 0;
    unpacked.fraction = bitCopy(0, packed, 0, fracsize);

    return unpacked;
}

void setFullExponent(int fullExp, Posit16Unpacked* unp, const Posit16Environment* env)
{
    int twoPowES = 1 << env->es;
    unp->regime = 0;
    if (fullExp > 0)
        unp->regime = fullExp / twoPowES;
    else if (fullExp < 0)
        unp->regime = -(1 + (-fullExp - 1) / twoPowES);//-(-fullExp + 1) / twoPowES;

    unp->exponent = fullExp - unp->regime * twoPowES;
}

int getFullExponent(const Posit16Unpacked* unp, const Posit16Environment* env)
{
    return unp->exponent + unp->regime * (1 << env->es);
}

Posit16 Posit_fromFloat(float value, const Posit16Environment* env)
{
    Posit16_UF32 uf;
    uf.f32 = value;

    // unpacking IEEE-754 single float
    const int fbias = -127;
    const int ffracsize = 23;
    int fsign = (uf.u32 >> 31);
    int fexp = (uf.u32 >> ffracsize) & 0xFF;
    int ffrac = (uf.u32 & 0x7FFFFF);

    // special cases
    if (ffrac == 0 && fexp == 0)
        return POSIT16_ZERO;
    if (fexp == 0xFF)
        return POSIT16_NAR;

    Posit16Unpacked unp;
    unp.sign = fsign;

    if (fexp > 0)
    {
        setFullExponent(fexp+fbias, &unp, env);

        int fracSize = getFracSize(&unp, env);
        int resize = fracSize - ffracsize;
        if (resize < 0)
            ffrac >>= (-resize);
        else if (resize > 0)
            ffrac <<= resize;

        unp.fraction = ffrac;
    }
    else // denormalized numbers
    {
        Posit16 normfrac = ffrac;
        int clz = bitSeriesCountRight(normfrac, POSIT_SIZE-1);
        int normexp = fexp - fbias - clz;
        normfrac <<= (clz + 1);
        setFullExponent(normexp, &unp, env);

        int fracSize = getFracSize(&unp, env);
        int resize = fracSize - ffracsize;
        if (resize < 0)
            normfrac >>= (-resize);
        else if (resize > 0)
            normfrac <<= resize;

        unp.fraction = normfrac;
    }

    //return unp;
    return Posit_pack(unp, env);
}

float Posit_toFloat(Posit16 value, const Posit16Environment* env)
{
    if (value == POSIT16_ZERO)
        return 0.0;
    if (value == POSIT16_NAR)
        return INFINITY;

    Posit16Unpacked unp = Posit_unpack(value, env);

    const int fbias = -127;
    const int ffracsize = 23;
    int fsign = unp.sign ? 1 : 0;
    int fexp = getFullExponent(&unp, env) - fbias;
    int ffrac = unp.fraction;

    int fracSize = getFracSize(&unp, env);
    int resize = ffracsize - fracSize;
    if (resize < 0)
        ffrac >>= (-resize);
    else if (resize > 0)
        ffrac <<= resize;

    Posit16_UF32 uf;
    uf.u32 = (fsign << 31) |
             ((fexp << 23) & bitSeriesMask(23, 30)) |
             (ffrac & bitSeriesMask(0, 22));

    return uf.f32;
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






