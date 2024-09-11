/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/x64/a32_jitstate.h"

#include <mcl/assert.hpp>
#include <mcl/stdint.hpp>
#include <emmintrin.h>
#include "dynarmic/backend/x64/block_of_code.h"
#include "dynarmic/backend/x64/nzcv_util.h"
#include "dynarmic/frontend/A32/a32_location_descriptor.h"

namespace Dynarmic::Backend::X64 {

/**
 * CPSR Bits
 * =========
 *
 * ARM CPSR flags
 * --------------
 * N        bit 31       Negative flag
 * Z        bit 30       Zero flag
 * C        bit 29       Carry flag
 * V        bit 28       oVerflow flag
 * Q        bit 27       Saturation flag
 * IT[1:0]  bits 25-26   If-Then execution state (lower 2 bits)
 * J        bit 24       Jazelle instruction set flag
 * GE       bits 16-19   Greater than or Equal flags
 * IT[7:2]  bits 10-15   If-Then execution state (upper 6 bits)
 * E        bit 9        Data Endianness flag
 * A        bit 8        Disable imprecise Aborts
 * I        bit 7        Disable IRQ interrupts
 * F        bit 6        Disable FIQ interrupts
 * T        bit 5        Thumb instruction set flag
 * M        bits 0-4     Processor Mode bits
 *
 * x64 LAHF+SETO flags
 * -------------------
 * SF   bit 15       Sign flag
 * ZF   bit 14       Zero flag
 * AF   bit 12       Auxiliary flag
 * PF   bit 10       Parity flag
 * CF   bit 8        Carry flag
 * OF   bit 0        Overflow flag
 */

u32 A32JitState::Cpsr() const {
    DEBUG_ASSERT((cpsr_q & ~1) == 0);
    DEBUG_ASSERT((cpsr_jaifm & ~0x010001DF) == 0);

    // Use SIMD instructions on x64
    __m128i vCPSR = _mm_set_epi32(0, 0, 0, 0);

    // NZCV flags
    vCPSR = _mm_or_si128(vCPSR, _mm_set_epi32(0, 0, 0, NZCV::FromX64(cpsr_nzcv)));

    // Q flag
    __m128i vQFlag = _mm_and_si128(_mm_set_epi32(0, 0, 0, cpsr_q), _mm_set1_epi32(1 << 27));
    vCPSR = _mm_or_si128(vCPSR, vQFlag);

    // GE flags
    __m128i vGEFlags = _mm_set_epi32(
        (cpsr_ge >> 31) & 1 ? 1 << 19 : 0,
        (cpsr_ge >> 23) & 1 ? 1 << 18 : 0,
        (cpsr_ge >> 15) & 1 ? 1 << 17 : 0,
        (cpsr_ge >> 7) & 1 ? 1 << 16 : 0
    );
    vCPSR = _mm_or_si128(vCPSR, vGEFlags);

    // E flag, T flag
    __m128i vETFlags = _mm_set_epi32(
        0,
        0,
        (upper_location_descriptor >> 1) & 1 ? 1 << 9 : 0,
        (upper_location_descriptor >> 0) & 1 ? 1 << 5 : 0
    );
    vCPSR = _mm_or_si128(vCPSR, vETFlags);

    // IT state
    __m128i vITState = _mm_set_epi32(
        0,
        static_cast<u32>(upper_location_descriptor & 0b00000011'00000000) << 17,
        static_cast<u32>(upper_location_descriptor & 0b11111100'00000000),
        0
    );
    vCPSR = _mm_or_si128(vCPSR, vITState);

    // Other flags
    vCPSR = _mm_or_si128(vCPSR, _mm_set_epi32(0, 0, 0, cpsr_jaifm));

    return _mm_cvtsi128_si32(vCPSR);
    
}

void A32JitState::SetCpsr(u32 cpsr) {

    // Use SIMD instructions on x64
    __m128i vCPSR = _mm_set_epi32(0, 0, 0, cpsr);

    // NZCV flags
    cpsr_nzcv = NZCV::ToX64(cpsr);

    // Q flag
    __m128i vQFlag = _mm_and_si128(_mm_srli_epi32(vCPSR, 27), _mm_set1_epi32(1));
    cpsr_q = _mm_cvtsi128_si32(vQFlag);

    // GE flags
    __m128i vGEFlags = _mm_or_si128(
        _mm_or_si128(
            _mm_or_si128(
                _mm_and_si128(_mm_srli_epi32(vCPSR, 16), _mm_set1_epi32(0xFF)),
                _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(vCPSR, 17), _mm_set1_epi32(0xFF)), 8)
            ),
            _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(vCPSR, 18), _mm_set1_epi32(0xFF)), 16)
        ),
        _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(vCPSR, 19), _mm_set1_epi32(0xFF)), 24)
    );
    cpsr_ge = _mm_cvtsi128_si32(vGEFlags);

    upper_location_descriptor &= 0xFFFF0000;
    // E flag, T flag
    upper_location_descriptor |= _mm_cvtsi128_si32(_mm_and_si128(_mm_srli_epi32(vCPSR, 5), _mm_set1_epi32(1)));
    upper_location_descriptor |= _mm_cvtsi128_si32(_mm_and_si128(_mm_srli_epi32(vCPSR, 9), _mm_set1_epi32(2)));
    // IT state
    upper_location_descriptor |= _mm_cvtsi128_si32(_mm_and_si128(vCPSR, _mm_set1_epi32(0b11111100'00000000)));
    upper_location_descriptor |= _mm_cvtsi128_si32(_mm_and_si128(_mm_slli_epi32(_mm_and_si128(vCPSR, _mm_set1_epi32(0b00000011'00000000)), 17), _mm_set1_epi32(0xFF00)));

    // Other flags
    cpsr_jaifm = cpsr & 0x010001DF;
}

void A32JitState::ResetRSB() {
    rsb_location_descriptors.fill(0xFFFFFFFFFFFFFFFFull);
    rsb_codeptrs.fill(0);
}

/**
 * Comparing MXCSR and FPSCR
 * =========================
 *
 * SSE MXCSR exception flags
 * -------------------------
 * PE   bit 5   Precision Flag
 * UE   bit 4   Underflow Flag
 * OE   bit 3   Overflow Flag
 * ZE   bit 2   Divide By Zero Flag
 * DE   bit 1   Denormal Flag                                 // Appears to only be set when MXCSR.DAZ = 0
 * IE   bit 0   Invalid Operation Flag
 *
 * VFP FPSCR cumulative exception bits
 * -----------------------------------
 * IDC  bit 7   Input Denormal cumulative exception bit       // Only ever set when FPSCR.FTZ = 1
 * IXC  bit 4   Inexact cumulative exception bit
 * UFC  bit 3   Underflow cumulative exception bit
 * OFC  bit 2   Overflow cumulative exception bit
 * DZC  bit 1   Division by Zero cumulative exception bit
 * IOC  bit 0   Invalid Operation cumulative exception bit
 *
 * SSE MSCSR exception masks
 * -------------------------
 * PM   bit 12  Precision Mask
 * UM   bit 11  Underflow Mask
 * OM   bit 10  Overflow Mask
 * ZM   bit 9   Divide By Zero Mask
 * DM   bit 8   Denormal Mask
 * IM   bit 7   Invalid Operation Mask
 *
 * VFP FPSCR exception trap enables
 * --------------------------------
 * IDE  bit 15  Input Denormal exception trap enable
 * IXE  bit 12  Inexact exception trap enable
 * UFE  bit 11  Underflow exception trap enable
 * OFE  bit 10  Overflow exception trap enable
 * DZE  bit 9   Division by Zero exception trap enable
 * IOE  bit 8   Invalid Operation exception trap enable
 *
 * SSE MXCSR mode bits
 * -------------------
 * FZ   bit 15      Flush To Zero
 * DAZ  bit 6       Denormals Are Zero
 * RN   bits 13-14  Round to {0 = Nearest, 1 = Negative, 2 = Positive, 3 = Zero}
 *
 * VFP FPSCR mode bits
 * -------------------
 * AHP      bit 26      Alternate half-precision
 * DN       bit 25      Default NaN
 * FZ       bit 24      Flush to Zero
 * RMode    bits 22-23  Round to {0 = Nearest, 1 = Positive, 2 = Negative, 3 = Zero}
 * Stride   bits 20-21  Vector stride
 * Len      bits 16-18  Vector length
 */

// NZCV; QC (ASIMD only), AHP; DN, FZ, RMode, Stride; SBZP; Len; trap enables; cumulative bits
constexpr u32 FPSCR_MODE_MASK = A32::LocationDescriptor::FPSCR_MODE_MASK;
constexpr u32 FPSCR_NZCV_MASK = 0xF0000000;

u32 A32JitState::Fpscr() const {
    DEBUG_ASSERT((fpsr_nzcv & ~FPSCR_NZCV_MASK) == 0);

    const u32 fpcr_mode = static_cast<u32>(upper_location_descriptor) & FPSCR_MODE_MASK;
    const u32 mxcsr = guest_MXCSR | asimd_MXCSR;

    u32 FPSCR = fpcr_mode | fpsr_nzcv;

    // Use SIMD instructions on x64
    __m128i vMXCSR = _mm_set_epi32(0, 0, 0, mxcsr);
    __m128i vIOC = _mm_and_si128(_mm_set1_epi32(0b0000000000001), vMXCSR);
    __m128i vIXCUFCOFCDZC = _mm_and_si128(_mm_set1_epi32(0b0000000111100), vMXCSR);
    FPSCR |= _mm_cvtsi128_si32(vIOC);
    FPSCR |= _mm_cvtsi128_si32(_mm_srli_epi32(vIXCUFCOFCDZC, 1));

    FPSCR |= fpsr_exc;
    FPSCR |= fpsr_qc != 0 ? 1 << 27 : 0;

    return FPSCR;
}

void A32JitState::SetFpscr(u32 FPSCR) {
    // Ensure that only upper half of upper_location_descriptor is used for FPSCR bits.
    static_assert((FPSCR_MODE_MASK & 0xFFFF0000) == FPSCR_MODE_MASK);

    upper_location_descriptor &= 0x0000FFFF;
    upper_location_descriptor |= FPSCR & FPSCR_MODE_MASK;

    fpsr_nzcv = FPSCR & FPSCR_NZCV_MASK;

    // Use SIMD instructions on x64
    __m128i vFPSCR = _mm_set_epi32(0, 0, 0, FPSCR);
    __m128i vQFlag = _mm_and_si128(_mm_srli_epi32(vFPSCR, 27), _mm_set1_epi32(1));
    fpsr_qc = _mm_cvtsi128_si32(vQFlag);

    __m128i vRMode = _mm_set_epi32(0, 0, 0, FPSCR);
    vRMode = _mm_and_si128(_mm_srli_epi32(vRMode, 22), _mm_set1_epi32(0x3));
    const __m128i vMXCSR_RMode = _mm_set_epi32(0x6000, 0x2000, 0x4000, 0x0);
    guest_MXCSR |= _mm_cvtsi128_si32(_mm_shuffle_epi32(_mm_and_si128(vRMode, vMXCSR_RMode), _MM_SHUFFLE(0, 0, 0, 0)));

    __m128i vFlushToZero = _mm_and_si128(_mm_srli_epi32(vFPSCR, 24), _mm_set1_epi32(1));
    if (_mm_cvtsi128_si32(vFlushToZero)) {
        guest_MXCSR |= (1 << 15);  // SSE Flush to Zero
        guest_MXCSR |= (1 << 6);   // SSE Denormals are Zero
    }

    guest_MXCSR = 0x00001f80;
    asimd_MXCSR = 0x00009fc0;

    // Cumulative flags IDC, IOC, IXC, UFC, OFC, DZC
    fpsr_exc = FPSCR & 0x9F;
}

}  // namespace Dynarmic::Backend::X64
