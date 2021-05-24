#define POSIT_TEST

#include <QtTest>

extern "C"
{
#include "fridgemulib.h"
#include "posit.h"
}

// add necessary includes here

class fridgemulib_tests : public QObject
{
    Q_OBJECT

public:
    fridgemulib_tests();
    ~fridgemulib_tests();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void test_case1();

    void posit_bitOps();
    void posit_pack();
    void posit_float_conversion();
    void posit_add();
};

fridgemulib_tests::fridgemulib_tests()
{

}

fridgemulib_tests::~fridgemulib_tests()
{

}

void fridgemulib_tests::initTestCase()
{

}

void fridgemulib_tests::cleanupTestCase()
{

}

void fridgemulib_tests::test_case1()
{

}

void fridgemulib_tests::posit_bitOps()
{
    for (int i = 0; i < POSIT_SIZE; ++i)
    {
        for (int j = i; j < POSIT_SIZE; ++j)
        {
            Posit16 series = bitSeriesMask(i,j);
            for (int k = 0; k < POSIT_SIZE; ++k)
            {
                char bit = (k >= i && k <= j) ? 1 : 0;
                QVERIFY(bitGet(series, k) == bit);
            }
        }
    }

    Posit16 x1 = 127-16;
    Posit16 x2 = 0;
    QVERIFY(bitCopy(x2, x1, 3, 5) == 120);

    Posit16 n1 = 13579;
    Posit16 n2 = ~n1;

    for (int i = 0; i < POSIT_SIZE; ++i)
    {
        for (int j = i; j < POSIT_SIZE; ++j)
        {
            Posit16 m = bitCopy(n1, n2, i, j-i+1);
            for (int k = 0; k < POSIT_SIZE; ++k)
            {
                char bit = (k >= i && k <= j) ? bitGet(n2, k-i) : bitGet(n1, k);
                QVERIFY(bitGet(m, k) == bit);
            }
        }
    }

    for (int i = 0; i < POSIT_SIZE; ++i)
    {
        for (int j = i; j < POSIT_SIZE; ++j)
        {
            Posit16 s1 = bitSeriesMask(i, j);
            Posit16 s2 = ~s1;

            QVERIFY(bitSeriesCountRight(s1, j) == (j-i+1));
            QVERIFY(bitSeriesCountRight(s2, j) == (j-i+1));
        }
    }
}

void fridgemulib_tests::posit_pack()
{
    Posit16Environment env = Posit_env(3);
    Posit16Unpacked unp;
    unp.sign = 0;
    unp.regime = -1;
    unp.exponent = 5;
    unp.fraction = 789;
    Posit16 pck = Posit_pack(unp, &env);
    qDebug() << pck;
    Posit16Unpacked unp2 = Posit_unpack(pck, &env);
    qDebug() << "sign =" << unp2.sign << "reg =" << unp2.regime << "exp =" << unp2.exponent << "frac =" << unp2.fraction;
    QVERIFY(pck == 14101);

    env.es = 5;
    unp.sign = 1;
    unp.regime = -3;
    unp.exponent = 14;
    unp.fraction = 27;
    pck = Posit_pack(unp, &env);
    qDebug() << pck;
    unp2 = Posit_unpack(pck, &env);
    qDebug() << "sign =" << unp2.sign << "reg =" << unp2.regime << "exp =" << unp2.exponent << "frac =" << unp2.fraction;
    QVERIFY(pck == 35739);

}

void fridgemulib_tests::posit_float_conversion()
{
    Posit16Environment env = Posit_env(3);
    float x = 1.2345;

    Posit16 p = Posit_fromFloat(x, &env);
    Posit16Unpacked unp = Posit_unpack(p, &env);
    Posit16 p2 = Posit_pack(unp, &env);
    //Posit16Unpacked unp = Posit_fromFloat(x, &env);
    //Posit16 p = Posit_pack(unp, &env);

    qDebug() << "sign =" << unp.sign << "reg =" << unp.regime << "exp =" << unp.exponent << "frac =" << unp.fraction;

    float x2 = Posit_toFloat(p2, &env);
    qDebug() << x << "->" << x2;

    QVERIFY(qAbs(x-x2) < 0.001);

    QVERIFY(Posit_toFloat(Posit_fromFloat(0.0, &env), &env) == 0.0);
}

void fridgemulib_tests::posit_add()
{
    Posit16Environment env = Posit_env(3);

    float f1 = 1.2345;
    float f2 = 4.3156;

    for (int i = 0; i < 10; ++i)
    {
        Posit16 p1 = rand();//Posit_fromFloat(f1, &env);
        Posit16 p2 = -rand();//Posit_fromFloat(f2, &env);
        Posit16 pa = Posit_add(p1, p2, &env);

        float fa = f1+f2;
        float fpa = Posit_toFloat(pa, &env);
        float fp1 = Posit_toFloat(p1, &env);
        float fp2 = Posit_toFloat(p2, &env);

        //qDebug() << "float:" << f1 << "+" << f2 << "=" << fa;
        qDebug() << "posit:" << fp1 << "+" << fp2 << "=" << fpa;
        qDebug() << "float:" << fp1 << "+" << fp2 << "=" << fp1+fp2;
        qDebug() << " ";
        //qDebug() << f1 << "+" << f2 << "=" << fa << ";" << fp1 << "+" << fp2 << "=" << fpa;

        /*double mse = 0.0;
        for (Posit16 p1 = 0; p1 < 65535; ++p1)
        {
            for (Posit16 p2 = 0; p2 < 65535; ++p2)
            {
                Posit16 pa = Posit_add(p1, p2, &env);
                float f1 = Posit_toFloat(p1, &env);
                float f2 = Posit_toFloat(p2, &env);
                float fa = Posit_toFloat(pa, &env);
                float ffa = f1 + f2;
                mse += (fa-ffa)*(fa-ffa);
            }
        }*/
    }
}

QTEST_APPLESS_MAIN(fridgemulib_tests)

#include "tst_fridgemulib_tests.moc"
