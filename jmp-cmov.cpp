/*
	require http://homepage1.nifty.com/herumi/soft/xbyak_e.html

	benchmark of jmp/cmov/setg
	g++ -O3 -fomit-frame-pointer -fno-operator-names jmp-cmov.cpp

Core i7-2600 CPU 3.40GHz + Linux 2.6.35 + gcc 4.4.5
--- test1 ---
name  rand  first    inc   inc2
STL  2.752  2.718  2.752  2.759
jmp  1.920  1.973  1.966  3.214
cmov 2.773  2.823  2.763  2.817
--- test2 ---
        0.00   0.25   0.50   0.75   1.00
STL    2.713  2.794  2.764  2.761  2.738
jmp    1.841  6.385 11.304  7.969  1.913
setg   2.184  2.183  2.183  2.181  2.205
sbb    1.923  1.878  1.912  1.886  1.917
setg2  1.934  1.942  1.895  1.917  1.927
*/
#include <stdio.h>
#include <vector>
#include <algorithm>
#include "xbyak/xbyak.h"
#include "xbyak/xbyak_util.h"
#include "util.hpp"

typedef AlignedArray<int> IntVec;
typedef std::vector<double>DoubleVec;

const int MaxCount = 10000;

/*
	x[i] is always 1
	y[i] is 0 or 2
*/
void Init(IntVec& x, IntVec& y, size_t n, int rate)
{
	XorShift128 r;
	x.resize(n);
	y.resize(n);
	for (size_t i = 0; i < n; i++) {
		x[i] = 1;
		y[i] = int(r.get() % 1000) >= rate ? 0 : 2;
	}
}

void InitRandom(IntVec& x, size_t n)
{
	XorShift128 r;
	x.resize(n);
	for (size_t i = 0; i < n; i++) {
		x[i] = int(r.get() % 65537);
	}
}

int getMaxBySTL(const int *x, size_t n)
{
	return *std::max_element(x, x + n);
}

size_t countMax_C(const int *x, const int *y, size_t n)
{
	size_t ret = 0;
	for (size_t i = 0; i < n; i++) {
		if (x[i] > y[i]) ret++;
	}
	return ret;
}

void Test1(DoubleVec& dv, const IntVec& x, int f(const int*, size_t n))
{
	Xbyak::util::Clock clk;
	const size_t n = x.size();
	const int *p = &x[0];
	int ret = 0;
	for (int i = 0; i < MaxCount; i++) {
		clk.begin();
		ret += f(p, n);
		clk.end();
	}
	double c = clk.getClock() / ((double)MaxCount * n);
	dv.push_back(c);
	printf("ret=%d, %fKclk\n", ret / MaxCount, c);
}

void Test2(DoubleVec& dv, const IntVec& a, const IntVec& b, size_t f(const int*, const int *, size_t n))
{
	Xbyak::util::Clock clk;
	const size_t n = a.size();
	const int *p = &a[0];
	const int *q = &b[0];
	int ret = 0;
	for (int i = 0; i < MaxCount; i++) {
		clk.begin();
		ret += f(p, q, n);
		clk.end();
	}
	double c = clk.getClock() / ((double)MaxCount * n);
	dv.push_back(c);
	printf("ret=%d, %fKclk\n", ret / MaxCount, c);
}

static struct Func1Info {
	const char *name;
	int (*f)(const int*, size_t);
} func1Tbl[] = {
	{ "STL ", getMaxBySTL },
	{ "jmp ", 0 },
	{ "cmov", 0 },
};

static struct Func2Info {
	const char *name;
	size_t (*f)(const int *, const int*, size_t);
} func2Tbl[] = {
	{ "STL  ", countMax_C },
	{ "jmp  ", 0 },
	{ "setg ", 0 },
	{ "sbb  ", 0 },
	{ "setg2", 0 },
};

struct Code : public Xbyak::CodeGenerator {
	// int getMax(const int *x, size_t n); // n > 0
	void genGetMax(bool useCmov)
	{
		using namespace Xbyak;
		inLocalLabel();
		const Reg32& a = eax;
#if defined(XBYAK64_WIN)
		const Reg64& x = rcx;
		const Reg64& n = rdx;
		xor(rax, rax);
#elif defined(XBYAK64_GCC)
		const Reg64& x = rdi;
		const Reg64& n = rsi;
		xor(rax, rax);
#else
		const Reg32& x = ecx;
		const Reg32& n = edx;
		mov(x, ptr [esp + 4]);
		mov(n, ptr [esp + 8]);
#endif
		mov(a, ptr [x]);
		cmp(n, 1);
		je(".exit");
		lea(x, ptr [x + n * 4]);
		neg(n);
		add(n, 1);
	L("@@");
		if (useCmov) {
			cmp(a, ptr [x + n * 4]);
			cmovl(a, ptr [x + n * 4]);
		} else {
			cmp(a, ptr [x + n * 4]);
			jge(".skip");
			mov(a, ptr [x + n * 4]);
		L(".skip");
		}
		add(n, 1);
		jne("@b");
	L(".exit");
		ret();
		outLocalLabel();
	}
	/*
		size_t getCountMax(const int *x, const int *y, size_t n); // n > 0
		x[i] > y[i] �Ȥʤ�Ŀ����֤�
		mode = 0 : use jmp
		       1 : use setg
		       2 : use sbb
		       3 : use setg(wo. movzx)
	*/
	void genCountMax(int mode)
	{
		using namespace Xbyak;
		inLocalLabel();
#if defined(XBYAK64_WIN)
		const Reg64& x = rcx;
		const Reg64& y = r9;
		const Reg64& n = r8;
		const Reg32& t = edx;
		const Reg32& t2 = r10d;
		const Reg64& a = rax;
		mov(r9, rdx); // to use lower 8bit of t
		xor(rdx, rdx);
#elif defined(XBYAK64_GCC)
		const Reg64& x = rdi;
		const Reg64& y = rsi;
		const Reg64& n = rdx;
		const Reg32& t = ecx;
		const Reg32& t2 = r8d;
		const Reg64& a = rax;
		xor(rcx, rcx);
#else
		const Reg32& x = esi;
		const Reg32& y = edx;
		const Reg32& n = ecx;
		const Reg32& t = ebx;
		const Reg32& t2 = edi;
		const Reg32& a = eax;
		push(ebx);
		push(esi);
		int P = 4 * 2;
		if (mode == 3) {
			P = 4 * 3;
			push(edi);
			xor(ebx, ebx);
		}
		mov(x, ptr [esp + P + 4]);
		mov(y, ptr [esp + P + 8]);
		mov(n, ptr [esp + P + 12]);
#endif
		const Reg8& low8 = Reg8(t.getIdx());
		lea(x, ptr [x + n * 4]);
		lea(y, ptr [y + n * 4]);
		neg(n);
		xor(a, a);

	L(".lp");
		switch (mode) {
		case 0:
			mov(t, ptr [x + n * 4]);
			cmp(t, ptr [y + n * 4]);
			jle(".skip");
			add(a, 1);
		L(".skip");
			break;
		case 1:
			mov(t, ptr [x + n * 4]);
			cmp(t, ptr [y + n * 4]);
			setg(low8);
#ifdef XBYAK64
			movzx(Reg64(t.getIdx()), low8);
#else
			movzx(t, low8);
#endif
			add(a, t);
			break;
		case 2:
			mov(t, ptr [x + n * 4]);
			cmp(t, ptr [y + n * 4]);
			sbb(a, -1);
			break;
		case 3:
			mov(t2, ptr [x + n * 4]);
			cmp(t2, ptr [y + n * 4]);
			setg(low8);
			add(a, t);
			break;
		}
		add(n, 1);
		jne(".lp");
	L(".exit");
#ifdef XBYAK32
		if (mode == 3) {
			pop(edi);
		}
		pop(esi);
		pop(ebx);
#endif
		ret();
		outLocalLabel();
	}
};

void Test1All(std::vector<DoubleVec>& ret1, const IntVec& a)
{
	for (size_t i = 0; i < NUM_OF_ARRAY(func1Tbl); i++) {
		Test1(ret1[i], a, func1Tbl[i].f);
	}
}

int main()
{
	try {
		IntVec a;
		Code code;
		std::vector<DoubleVec> ret1(NUM_OF_ARRAY(func1Tbl));
		func1Tbl[1].f = (int (*)(const int*, size_t))code.getCurr();
		code.genGetMax(0);

		code.align(16);
		func1Tbl[2].f = (int (*)(const int*, size_t))code.getCurr();
		code.genGetMax(1);

		/* ��� */
		InitRandom(a, 8192);
		printf("rand max pos=%d\n", int(std::max_element(a.begin(), a.end()) - a.begin()));
		Test1All(ret1, a);
		/* �ǽ餬�����礭�� */
		puts("fst is max");
		a[0] = 100000;
		Test1All(ret1, a);
		puts("inc");
		/* ñĴ���� */
		for (size_t i = 0; i < a.size(); i++) {
			a[i] = i;
		}
		Test1All(ret1, a);

		/* ���ñĴ���� */
		puts("inc2");
		XorShift128 r;
		for (size_t i = 0; i < a.size(); i += 4) {
			a[i] = (r.get() % 1) ? i : i - 2;
			a[i + 1] = i - 1;
			a[i + 2] = i - 1;
			a[i + 3] = i - 1;
		}
		Test1All(ret1, a);

		///
		for (int i = 1; i <= 4; i++) {
			code.align(16);
			func2Tbl[i].f = (size_t (*)(const int*, const int*, size_t))code.getCurr();
			code.genCountMax(i - 1);
		}

		std::vector<DoubleVec> ret2(5);
		for (int i = 0; i < 5; i++) {
			IntVec b;
			int rate = i * 250;
			Init(a, b, 8192, rate);

			printf("rate=%d\n", rate);
			for (size_t j = 0; j < NUM_OF_ARRAY(func2Tbl); j++) {
				Test2(ret2[j], a, b, func2Tbl[j].f);
			}
		}

		puts("--- test1 ---");
		// print ret1
		// STL/jmp/cmov
		printf("name  rand  first    inc   inc2\n");
		for (size_t i = 0; i < NUM_OF_ARRAY(func1Tbl); i++) {
			printf("%s ", func1Tbl[i].name);
			for (size_t j = 0; j < ret1[i].size(); j++) {
				printf("%.3f  ", ret1[i][j]);
			}
			printf("\n");
		}
		puts("--- test2 ---");
		printf("        0.00   0.25   0.50   0.75   1.00\n");
		// print ret2
		for (size_t i = 0; i < NUM_OF_ARRAY(func2Tbl); i++) {
			printf("%s ", func2Tbl[i].name);
			for (size_t j = 0; j < ret2[i].size(); j++) {
				printf("%6.3f ", ret2[i][j]);
			}
			printf("\n");
		}

	} catch (Xbyak::Error err) {
		printf("ERR:%s(%d)\n", Xbyak::ConvertErrorToString(err), err);
	} catch (...) {
		printf("unknown error\n");
	}
}
