/*
 * vector_cpu.c
 *
 * EC528: x86 CPUID feature detection with proper OS-enabled-state (XGETBV)
 * checks. SPTAG's InstructionUtils omits the XCR0 check; this file does it
 * correctly - AVX/AVX-512 registers are usable only when the OS saves them.
 */
#include "vector/vector_cpu.h"

#if defined(__x86_64__) || defined(_M_X64)

#include <cpuid.h>
#include <stddef.h>
#include <stdint.h>

// CPUID leaf 1 ecx.
#define CPU1_ECX_SSE41 (1u << 19)
#define CPU1_ECX_OSXSAVE (1u << 27)
#define CPU1_ECX_AVX (1u << 28)

// CPUID leaf 7 subleaf 0 ebx.
#define CPU7_EBX_AVX2 (1u << 5)
#define CPU7_EBX_AVX512F (1u << 16)
#define CPU7_EBX_AVX512BW (1u << 30)

// XCR0 state-enable bits.
#define XCR0_XMM (1u << 1)
#define XCR0_YMM (1u << 2)
#define XCR0_OPMASK (1u << 5)
#define XCR0_ZMM_LO (1u << 6)
#define XCR0_ZMM_HI (1u << 7)

static uint64_t
xgetbv0(void)
{
	uint32_t eax, edx;

	// xgetbv encoded as bytes so no -mxsave target flag is needed in this
	// baseline-compiled TU.
	__asm__ volatile(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) :
			"c"(0));

	return ((uint64_t)edx << 32) | eax;
}

typedef struct cpu_features_s {
	bool probed;
	bool sse41;
	bool avx2;
	bool avx512bw;
} cpu_features;

static cpu_features g_cpu = { 0 };

static void
probe(void)
{
	if (g_cpu.probed) {
		return;
	}

	uint32_t a = 0, b = 0, c = 0, d = 0;

	if (__get_cpuid(1, &a, &b, &c, &d) == 0) {
		g_cpu.probed = true; // no CPUID - everything stays false
		return;
	}

	uint32_t leaf1_ecx = c;

	g_cpu.sse41 = (leaf1_ecx & CPU1_ECX_SSE41) != 0;

	bool osxsave = (leaf1_ecx & CPU1_ECX_OSXSAVE) != 0;
	bool avx = (leaf1_ecx & CPU1_ECX_AVX) != 0;

	uint32_t max_leaf = __get_cpuid_max(0, NULL);
	uint32_t l7_ebx = 0;

	if (max_leaf >= 7) {
		uint32_t a7, b7, c7, d7;

		__cpuid_count(7, 0, a7, b7, c7, d7);
		l7_ebx = b7;
	}

	if (osxsave && avx) {
		uint64_t xcr0 = xgetbv0();
		bool ymm_ok = (xcr0 & (XCR0_XMM | XCR0_YMM)) ==
				(XCR0_XMM | XCR0_YMM);
		bool zmm_ok = ymm_ok && (xcr0 &
				(XCR0_OPMASK | XCR0_ZMM_LO | XCR0_ZMM_HI)) ==
						(XCR0_OPMASK | XCR0_ZMM_LO | XCR0_ZMM_HI);

		g_cpu.avx2 = ymm_ok && (l7_ebx & CPU7_EBX_AVX2) != 0;
		g_cpu.avx512bw = zmm_ok && (l7_ebx & CPU7_EBX_AVX512F) != 0 &&
				(l7_ebx & CPU7_EBX_AVX512BW) != 0;
	}

	g_cpu.probed = true;
}

bool
as_vector_cpu_has_sse41(void)
{
	probe();
	return g_cpu.sse41;
}

bool
as_vector_cpu_has_avx2(void)
{
	probe();
	return g_cpu.avx2;
}

bool
as_vector_cpu_has_avx512bw(void)
{
	probe();
	return g_cpu.avx512bw;
}

#else // non-x86

bool
as_vector_cpu_has_sse41(void)
{
	return false;
}

bool
as_vector_cpu_has_avx2(void)
{
	return false;
}

bool
as_vector_cpu_has_avx512bw(void)
{
	return false;
}

#endif
