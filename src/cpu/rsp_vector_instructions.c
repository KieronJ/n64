#include "rsp_vector_instructions.h"

#ifdef N64_USE_SIMD
#include <emmintrin.h>
#endif

#include <log.h>
#ifdef N64_USE_SIMD
#include <immintrin.h>
#include <n64_rsp_bus.h>

#endif
#include "rsp.h"
#include "rsp_rom.h"
#include "n64_rsp_bus.h"

#define defvs vu_reg_t* vs = &N64RSP.vu_regs[instruction.cp2_vec.vs]
#define defvt vu_reg_t* vt = &N64RSP.vu_regs[instruction.cp2_vec.vt]
#define defvd vu_reg_t* vd = &N64RSP.vu_regs[instruction.cp2_vec.vd]
#define defvte vu_reg_t vte = get_vte(&N64RSP.vu_regs[instruction.cp2_vec.vt], instruction.cp2_vec.e)

#define MIN(x, y) ((x) < (y) ? (x) : (y))

INLINE shalf clamp_signed(sdword value) {
    if (value < -32768) return -32768;
    if (value > 32767) return 32767;
    return value;
}

#define clamp_unsigned(x) ((x) < 0 ? 0 : ((x) > 32767 ? 65535 : x))

INLINE bool is_sign_extension(shalf high, shalf low) {
    if (high == 0) {
        return (low & 0x8000) == 0;
    } else if (high == -1) {
        return (low & 0x8000) == 0x8000;
    }
    return false;
}

INLINE int CLZ(word value) {
//#if __has_builtin (__builtin_clz)
    return __builtin_clz(value);
    /*
#else
    int leading_zeroes = 0;
    for (int i = 0; i < 32 && (value & 0x80000000) == 0; i++) {
        leading_zeroes++;
        value <<= 1;
    }
    return leading_zeroes;
#endif
     */
}

#ifndef N64_USE_SIMD
INLINE vu_reg_t broadcast(vu_reg_t* vt, int lane0, int lane1, int lane2, int lane3, int lane4, int lane5, int lane6, int lane7) {
    vu_reg_t vte;
    vte.elements[VU_ELEM_INDEX(0)] = vt->elements[VU_ELEM_INDEX(lane0)];
    vte.elements[VU_ELEM_INDEX(1)] = vt->elements[VU_ELEM_INDEX(lane1)];
    vte.elements[VU_ELEM_INDEX(2)] = vt->elements[VU_ELEM_INDEX(lane2)];
    vte.elements[VU_ELEM_INDEX(3)] = vt->elements[VU_ELEM_INDEX(lane3)];
    vte.elements[VU_ELEM_INDEX(4)] = vt->elements[VU_ELEM_INDEX(lane4)];
    vte.elements[VU_ELEM_INDEX(5)] = vt->elements[VU_ELEM_INDEX(lane5)];
    vte.elements[VU_ELEM_INDEX(6)] = vt->elements[VU_ELEM_INDEX(lane6)];
    vte.elements[VU_ELEM_INDEX(7)] = vt->elements[VU_ELEM_INDEX(lane7)];
    return vte;
}
#endif

INLINE vu_reg_t get_vte(vu_reg_t* vt, byte e) {
    vu_reg_t vte;
    switch(e) {
        case 0 ... 1:
            return *vt;
        case 2:
#ifdef N64_USE_SIMD
            vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt->single, 0b11110101), 0b11110101);
#else
            vte = broadcast(vt, 0, 0, 2, 2, 4, 4, 6, 6);
#endif
            break;
        case 3:
#ifdef N64_USE_SIMD
            vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt->single, 0b10100000), 0b10100000);
#else
            vte = broadcast(vt, 1, 1, 3, 3, 5, 5, 7, 7);
#endif
            break;
        case 4:
#ifdef N64_USE_SIMD
            vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt->single, 0b11111111), 0b11111111);
#else
            vte = broadcast(vt, 0, 0, 0, 0, 4, 4, 4, 4);
#endif
            break;
        case 5:
#ifdef N64_USE_SIMD
            vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt->single, 0b10101010), 0b10101010);
#else
            vte = broadcast(vt, 1, 1, 1, 1, 5, 5, 5, 5);
#endif
            break;
        case 6:
#ifdef N64_USE_SIMD
            vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt->single, 0b01010101), 0b01010101);
#else
            vte = broadcast(vt, 2, 2, 2, 2, 6, 6, 6, 6);
#endif
            break;
        case 7:
#ifdef N64_USE_SIMD
            vte.single = _mm_shufflehi_epi16(_mm_shufflelo_epi16(vt->single, 0b00000000), 0b00000000);
#else
            vte = broadcast(vt, 3, 3, 3, 3, 7, 7, 7, 7);
#endif
            break;
        case 8 ... 15: {
            int index = VU_ELEM_INDEX(e - 8);
#ifdef N64_USE_SIMD
            vte.single = _mm_set1_epi16(vt->elements[index]);
#else
            for (int i = 0; i < 8; i++) {
                half val = vt->elements[index];
                vte.elements[i] = val;
            }
#endif
            break;
        }
        default:
            logfatal("vte where e > 15");
    }

    return vte;
}


vu_reg_t ext_get_vte(vu_reg_t* vt, byte e) {
    return get_vte(vt, e);
}


#define SHIFT_AMOUNT_LBV_SBV 0
#define SHIFT_AMOUNT_LSV_SSV 1
#define SHIFT_AMOUNT_LLV_SLV 2
#define SHIFT_AMOUNT_LDV_SDV 3
#define SHIFT_AMOUNT_LQV_SQV 4
#define SHIFT_AMOUNT_LRV_SRV 4
#define SHIFT_AMOUNT_LPV_SPV 3
#define SHIFT_AMOUNT_LUV_SUV 3
#define SHIFT_AMOUNT_LHV_SHV 4
#define SHIFT_AMOUNT_LFV_SFV 4
#define SHIFT_AMOUNT_LTV_STV 4

INLINE int sign_extend_7bit_offset(byte offset, int shift_amount) {
    sbyte soffset = ((offset << 1) & 0x80) | offset;

    sword ofs = soffset;
    word uofs = ofs;
    return uofs << shift_amount;
}

word rcp(sword sinput) {
    // One's complement absolute value, xor with the sign bit to invert all bits if the sign bit is set
    sword mask = sinput >> 31;
    sword input = sinput ^ mask;
    if (input > INT16_MIN) {
        input -= mask;
    }
    if (input == 0) {
        return 0x7FFFFFFF;
    } else if (sinput == INT16_MIN) {
        return 0xFFFF0000;
    }

    word shift = CLZ(input);
    dword dinput = (dword)input;
    word index = ((dinput << shift) & 0x7FC00000) >> 22;

    sword result = rcp_rom[index];
    result = (0x10000 | result) << 14;
    result = (result >> (31 - shift)) ^ mask;
    return result;
}

word rsq(sword sinput) {
    if (sinput == 0) {
        return 0x7FFFFFFF;
    } else if (sinput == 0xFFFF8000) { // Only for RSQ special case
        return 0xFFFF0000;
    }

    word input = abs(sinput);
    int lshift = CLZ(input) + 1;
    int rshift = (32 - lshift) >> 1; // Shifted by 1 instead of 0
    int index = (input << lshift) >> 24; // Shifted by 24 instead of 23

    word rom = rsq_rom[(index | ((lshift & 1) << 8))];
    word result = ((0x10000 | rom) << 14) >> rshift;

    if (input != sinput) {
        return ~result;
    } else {
        return result;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lbv) {
    logdebug("rsp_lwc2_lbv");
    vu_reg_t* vt = &N64RSP.vu_regs[instruction.cp2_vec.vt];
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LBV_SBV);

    vt->bytes[VU_BYTE_INDEX(instruction.v.element)] = n64_rsp_read_byte(address);
}

RSP_VECTOR_INSTR(rsp_lwc2_ldv) {
    logdebug("rsp_lwc2_ldv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LDV_SDV);

    int start = instruction.v.element;
    int end = MIN(start + 8, 16);

    for (int i = start; i < end; i++) {
        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(i)] = n64_rsp_read_byte(address);
        address++;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lfv) {
    logdebug("rsp_lwc2_lfv");
    logwarn("LFV executed, this instruction is known to be buggy!");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LFV_SFV);
    int e = instruction.v.element;
    int start = e >> 1;
    int start_ofs = e & 1;
    int end = (start + 4);

    for (int i = start; i < end; i++) {
        half val = n64_rsp_read_byte(address);
        int shift_amount = e & 7;
        if (shift_amount == 0) {
            shift_amount = 7;
        }
        val <<= shift_amount;
        byte low = val & 0xFF;
        byte high = (val >> 8) & 0xFF;

        int elem_byte = i * 2 + start_ofs;

        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((elem_byte + 0) & 15)] = high;
        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((elem_byte + 1) & 15)] = low;
        address += 4;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lhv) {
    logdebug("rsp_lwc2_lhv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LHV_SHV);

    word in_addr_offset = address & 0x7;
    address &= ~0x7;

    int e = instruction.v.element;

    for (int i = 0; i < 8; i++) {
        int ofs = ((16 - e) + (i * 2) + in_addr_offset) & 0xF;
        half val = n64_rsp_read_byte(address + ofs);
        val <<= 7;
        N64RSP.vu_regs[instruction.v.vt].elements[VU_ELEM_INDEX(i)] = val;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_llv) {
    logdebug("rsp_lwc2_llv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LLV_SLV);

    for (int i = 0; i < 4; i++) {
        int element = i + e;
        if (element > 15) {
            break;
        }
        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(element)] = n64_rsp_read_byte(address + i);
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lpv) {
    logdebug("rsp_lwc2_lpv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LPV_SPV);

    // Take into account how much the address is misaligned
    // since the accesses still wrap on the 8 byte boundary
    int address_offset = address & 7;
    address &= ~7;

    for(int elem = 0; elem < 8; elem++) {
        int element_offset = (16 - e + (elem + address_offset)) & 0xF;

        half value = n64_rsp_read_byte(address + element_offset);
        value <<= 8;
        N64RSP.vu_regs[instruction.v.vt].elements[VU_ELEM_INDEX(elem)] = value;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lqv) {
    logdebug("rsp_lwc2_lqv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LQV_SQV);
    word end_address = ((address & ~15) + 15);

    for (int i = 0; address + i <= end_address && i + e < 16; i++) {
        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(i + e)] = n64_rsp_read_byte(address + i);
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lrv) {
    logdebug("rsp_lwc2_lrv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LRV_SRV);
    int start = 16 - ((address & 0xF) - e);
    address &= 0xFFFFFFF0;

    for (int i = start; i < 16; i++) {
        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(i & 0xF)] = n64_rsp_read_byte(address++);
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_lsv) {
    logdebug("rsp_lwc2_lsv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LSV_SSV);
    half val = n64_rsp_read_half(address);
    byte lo = val & 0xFF;
    byte hi = (val >> 8) & 0xFF;
    N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(e + 0)] = hi;
    if (e < 15) {
        N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(e + 1)] = lo;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_ltv) {
    logdebug("rsp_lwc2_ltv");
    word base = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LTV_STV);
    base &= ~7;
    byte e = instruction.v.element;

    for (int i = 0; i < 8; i++) {
        word address = base;

        word offset = (i * 2) + e + ((base) & 8);

        half hi = n64_rsp_read_byte(address + ((offset + 0) & 0xF));
        half lo = n64_rsp_read_byte(address + ((offset + 1) & 0xF));

        int reg = (instruction.v.vt & 0x18) | ((i + (e >> 1)) & 0x7);

        N64RSP.vu_regs[reg].elements[VU_ELEM_INDEX(i & 0x7)] = (hi << 8) | lo;
    }
}

RSP_VECTOR_INSTR(rsp_lwc2_luv) {
    logdebug("rsp_lwc2_luv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LUV_SUV);

    int e = instruction.v.element;

    // Take into account how much the address is misaligned
    // since the accesses still wrap on the 8 byte boundary
    int address_offset = address & 7;
    address &= ~7;

    for (int elem = 0; elem < 8; elem++) {
        int element_offset = (16 - e + (elem + address_offset)) & 0xF;

        half value = n64_rsp_read_byte(address + element_offset);
        value <<= 7;
        N64RSP.vu_regs[instruction.v.vt].elements[VU_ELEM_INDEX(elem)] = value;
    }
}

RSP_VECTOR_INSTR(rsp_swc2_sbv) {
    logdebug("rsp_swc2_sbv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LBV_SBV);

    int element = instruction.v.element;
    n64_rsp_write_byte(address, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(element)]);
}

RSP_VECTOR_INSTR(rsp_swc2_sdv) {
    logdebug("rsp_swc2_sdv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LDV_SDV);

    for (int i = 0; i < 8; i++) {
        int element = i + instruction.v.element;
        n64_rsp_write_byte(address + i, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(element & 0xF)]);
    }
}

RSP_VECTOR_INSTR(rsp_swc2_sfv) {
    logdebug("rsp_swc2_sfv");
    logwarn("SFV executed, this instruction is known to be buggy!");
    defvt;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LFV_SFV);
    int e = instruction.v.element;

    int start = e >> 1;
    int end = start + 4;
    int base = address & 15;

    address &= ~15;
    for(int i = start; i < end; i++) {
        n64_rsp_write_byte(address + (base & 15), vt->elements[VU_ELEM_INDEX(i & 7)] >> 7);
        base += 4;
    }
}

RSP_VECTOR_INSTR(rsp_swc2_shv) {
    logdebug("rsp_swc2_shv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LHV_SHV);

    word in_addr_offset = address & 0x7;
    address &= ~0x7;

    int e = instruction.v.element;

    for (int i = 0; i < 8; i++) {
        int byte_index = (i * 2) + e;
        half val = N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(byte_index & 15)] << 1;
        val |= N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((byte_index + 1) & 15)] >> 7;
        byte b = val & 0xFF;

        int ofs = in_addr_offset + (i * 2);
        n64_rsp_write_byte(address + (ofs & 0xF), b);
    }
}

RSP_VECTOR_INSTR(rsp_swc2_slv) {
    logdebug("rsp_swc2_slv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LLV_SLV);

    for (int i = 0; i < 4; i++) {
        int element = i + e;
        n64_rsp_write_byte(address + i, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX(element & 0xF)]);
    }
}

RSP_VECTOR_INSTR(rsp_swc2_spv) {
    logdebug("rsp_swc2_spv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LPV_SPV);

    int start = instruction.v.element;
    int end = start + 8;

    for (int offset = start; offset < end; offset++) {
        if((offset & 15) < 8) {
            n64_rsp_write_byte(address++, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((offset & 7) << 1)]);
        } else {
            n64_rsp_write_byte(address++, N64RSP.vu_regs[instruction.v.vt].elements[VU_ELEM_INDEX(offset & 7)] >> 7);
        }
    }
}

RSP_VECTOR_INSTR(rsp_swc2_sqv) {
    logdebug("rsp_swc2_sqv");
    int e = instruction.v.element;
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LQV_SQV);
    word end_address = ((address & ~15) + 15);

    for (int i = 0; address + i <= end_address; i++) {
        n64_rsp_write_byte(address + i, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((i + e) & 15)]);
    }
}

RSP_VECTOR_INSTR(rsp_swc2_srv) {
    logdebug("rsp_swc2_srv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LRV_SRV);
    int start = instruction.v.element;
    int end = start + (address & 15);
    int base = 16 - (address & 15);
    address &= ~15;
    for(int i = start; i < end; i++) {
        n64_rsp_write_byte(address++, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((i + base) & 0xF)]);
    }
}

RSP_VECTOR_INSTR(rsp_swc2_ssv) {
    logdebug("rsp_swc2_ssv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LSV_SSV);

    int element = instruction.v.element;

    byte hi = N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((element + 0) & 15)];
    byte lo = N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((element + 1) & 15)];
    half value = (half)hi << 8 | lo;

    n64_rsp_write_half(address, value);
}

RSP_VECTOR_INSTR(rsp_swc2_stv) {
    logdebug("rsp_swc2_stv");
    word base = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LTV_STV);
    word in_addr_offset = base & 0x7;
    base &= ~0x7;

    byte e = instruction.v.element >> 1;

    for (int i = 0; i < 8; i++) {
        word address = base;

        word offset = (i * 2) + in_addr_offset;

        int reg = (instruction.v.vt & 0x18) | ((i + e) & 0x7);

        half val = N64RSP.vu_regs[reg].elements[VU_ELEM_INDEX(i & 0x7)];
        half hi = (val >> 8) & 0xFF;
        half lo = (val >> 0) & 0xFF;

        n64_rsp_write_byte(address + ((offset + 0) & 0xF), hi);
        n64_rsp_write_byte(address + ((offset + 1) & 0xF), lo);
    }
}

RSP_VECTOR_INSTR(rsp_swc2_suv) {
    logdebug("rsp_swc2_suv");
    word address = get_rsp_register(instruction.v.base) + sign_extend_7bit_offset(instruction.v.offset, SHIFT_AMOUNT_LUV_SUV);

    int start = instruction.v.element;
    int end = start + 8;
    for(int offset = start; offset < end; offset++) {
        if((offset & 15) < 8) {
            n64_rsp_write_byte(address++, N64RSP.vu_regs[instruction.v.vt].elements[VU_ELEM_INDEX(offset & 7)] >> 7);
        } else {
            n64_rsp_write_byte(address++, N64RSP.vu_regs[instruction.v.vt].bytes[VU_BYTE_INDEX((offset & 7) << 1)]);
        }
    }
}

RSP_VECTOR_INSTR(rsp_cfc2) {
    logdebug("rsp_cfc2");
    shalf value = 0;
    switch (instruction.r.rd & 3) {
        case 0: { // VCO
            value = rsp_get_vco();
            break;
        }
        case 1: { // VCC
            value = rsp_get_vcc();
            break;
        }
        case 2:
        case 3: { // VCE
            value = rsp_get_vce();
            break;
        }
    }

    set_rsp_register(instruction.r.rt, (sword)value);
}

RSP_VECTOR_INSTR(rsp_ctc2) {
    logdebug("rsp_ctc2");
    half value = get_rsp_register(instruction.r.rt) & 0xFFFF;
    switch (instruction.r.rd & 3) {
        case 0: { // VCO
            for (int i = 0; i < 8; i++) {
                N64RSP.vco.h.elements[VU_ELEM_INDEX(i)] = FLAGREG_BOOL(((value >> (i + 8)) & 1) == 1);
                N64RSP.vco.l.elements[VU_ELEM_INDEX(i)] = FLAGREG_BOOL(((value >> i) & 1) == 1);
            }
            break;
        }
        case 1: { // VCC
            for (int i = 0; i < 8; i++) {
                N64RSP.vcc.h.elements[VU_ELEM_INDEX(i)] = FLAGREG_BOOL(((value >> (i + 8)) & 1) == 1);
                N64RSP.vcc.l.elements[VU_ELEM_INDEX(i)] = FLAGREG_BOOL(((value >> i) & 1) == 1);
            }
            break;
        }
        case 2:
        case 3: { // VCE
            for (int i = 0; i < 8; i++) {
                N64RSP.vce.elements[VU_ELEM_INDEX(i)] = FLAGREG_BOOL(((value >> i) & 1) == 1);
            }
            break;
        }
    }
}

RSP_VECTOR_INSTR(rsp_mfc2) {
    logdebug("rsp_mfc2");
    byte hi = N64RSP.vu_regs[instruction.cp2_regmove.rd].bytes[VU_BYTE_INDEX(instruction.cp2_regmove.e)];
    byte lo = N64RSP.vu_regs[instruction.cp2_regmove.rd].bytes[VU_BYTE_INDEX((instruction.cp2_regmove.e + 1) & 0xF)];
    shalf element = hi << 8 | lo;
    set_rsp_register(instruction.cp2_regmove.rt, (sword)element);
}

RSP_VECTOR_INSTR(rsp_mtc2) {
    logdebug("rsp_mtc2");
    half element = get_rsp_register(instruction.cp2_regmove.rt);
    byte lo = element & 0xFF;
    byte hi = (element >> 8) & 0xFF;
    N64RSP.vu_regs[instruction.cp2_regmove.rd].bytes[VU_BYTE_INDEX(instruction.cp2_regmove.e + 0)] = hi;
    if (instruction.cp2_regmove.e < 0xF) {
        N64RSP.vu_regs[instruction.cp2_regmove.rd].bytes[VU_BYTE_INDEX(instruction.cp2_regmove.e + 1)] = lo;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vabs) {
    logdebug("rsp_vec_vabs");
    defvs;
    defvd;
    defvte;

#ifdef N64_USE_SIMD
    // check if each element is zero
    __m128i vs_is_zero = _mm_cmpeq_epi16(vs->single, N64RSP.zero);

    // arithmetic shift right by 15 (effectively sets each bit of the number to the sign bit)
    __m128i vs_is_negative = _mm_srai_epi16(vs->single, 15);
    // (!vs_is_zero) & vte
    // for each vte element, outputs zero if the corresponding vs element is zero, or else outputs the regular vte element.
    __m128i temp = _mm_andnot_si128(vs_is_zero, vte.single);

    // xor each element of the temp var with whether the vs element is negative
    // If the vs element is negative, flip all the bits. Otherwise, do nothing.
    temp = _mm_xor_si128(temp, vs_is_negative);
    // acc.l = temp[element] - (is_negative[element] ? 0xFFFF : 0)
    N64RSP.acc.l.single = _mm_sub_epi16(temp, vs_is_negative);

    // only difference here is that this a _signed_ subtraction
    // handle the special case where vte.elements[i] == 0x8000
    // acc.l = temp[element] - (is_negative[element] ? -1 : 0)
    vd->single = _mm_subs_epi16(temp, vs_is_negative);
#else
    for (int i = 0; i < 8; i++) {
        if (vs->signed_elements[i] < 0) {
            if (unlikely(vte.elements[i] == 0x8000)) {
                vd->elements[i] = 0x7FFF;
                N64RSP.acc.l.elements[i] = 0x8000;
            } else {
                vd->elements[i] = -vte.signed_elements[i];
                N64RSP.acc.l.elements[i] = -vte.signed_elements[i];
            }
        } else if (vs->elements[i] == 0) {
            vd->elements[i] = 0x0000;
            N64RSP.acc.l.elements[i] = 0x0000;
        } else {
            vd->elements[i] = vte.elements[i];
            N64RSP.acc.l.elements[i] = vte.elements[i];
        }
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vadd) {
    logdebug("rsp_vec_vadd");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        shalf vs_element = vs->signed_elements[i];
        shalf vte_element = vte.signed_elements[i];
        sword result = vs_element + vte_element + (N64RSP.vco.l.elements[i] != 0);
        N64RSP.acc.l.elements[i] = result;
        vd->elements[i] = clamp_signed(result);
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vaddc) {
    logdebug("rsp_vec_vaddc");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        half vs_element = vs->elements[i];
        half vte_element = vte.elements[i];
        word result = vs_element + vte_element;
        N64RSP.acc.l.elements[i] = result & 0xFFFF;
        vd->elements[i] = result & 0xFFFF;
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL((result >> 16) & 1);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vand) {
    logdebug("rsp_vec_vand");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        half result = vte.elements[i] & vs->elements[i];
        vd->elements[i] = result;
        N64RSP.acc.l.elements[i] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vch) {
    logdebug("rsp_vec_vch");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        shalf vs_element = vs->signed_elements[i];
        shalf vte_element = vte.signed_elements[i];

        if ((vs_element ^ vte_element) < 0) {
            shalf result = vs_element + vte_element;

            N64RSP.acc.l.signed_elements[i] = (result <= 0 ? -vte_element : vs_element);
            N64RSP.vcc.l.elements[i] = FLAGREG_BOOL(result <= 0);
            N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(vte_element < 0);
            N64RSP.vco.l.elements[i] = FLAGREG_BOOL(1);
            N64RSP.vco.h.elements[i] = FLAGREG_BOOL(result != 0 && (half)vs_element != ((half)vte_element ^ 0xFFFF));
            N64RSP.vce.elements[i]   = FLAGREG_BOOL(result == -1);
        } else {
            shalf result = vs_element - vte_element;

            N64RSP.acc.l.elements[i] = (result >= 0 ? vte_element : vs_element);
            N64RSP.vcc.l.elements[i] = FLAGREG_BOOL(vte_element < 0);
            N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(result >= 0);
            N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
            N64RSP.vco.h.elements[i] = FLAGREG_BOOL(result != 0 && (half)vs_element != ((half)vte_element ^ 0xFFFF));
            N64RSP.vce.elements[i]   = FLAGREG_BOOL(0);
        }

        vd->elements[i] = N64RSP.acc.l.elements[i];
    }
}

RSP_VECTOR_INSTR(rsp_vec_vcl) {
    logdebug("rsp_vec_vcl");
    defvs;
    defvd;
    defvte;
    for (int i = 0; i < 8; i++) {
        half vs_element = vs->elements[i];
        half vte_element = vte.elements[i];
        if (N64RSP.vco.l.elements[i] == 0 && N64RSP.vco.h.elements[i] == 0) {
            N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(vs_element >= vte_element);
        }

        if (N64RSP.vco.l.elements[i] != 0 && N64RSP.vco.h.elements[i] == 0) {
            bool lte = vs_element + vte_element <= 0xFFFF;
            bool eql = (shalf)vs_element == -(shalf)vte_element;
            N64RSP.vcc.l.elements[i] = FLAGREG_BOOL(N64RSP.vce.elements[i] != 0 ? lte : eql);
        }
        bool clip = N64RSP.vco.l.elements[i] != 0 ? N64RSP.vcc.l.elements[i] : N64RSP.vcc.h.elements[i];
        half vtabs = N64RSP.vco.l.elements[i] != 0 ? -(half)vte_element : (half)vte_element;
        half acc = clip ? vtabs : vs_element;
        N64RSP.acc.l.elements[i] = acc;
        vd->elements[i] = acc;

    }
    for (int i = 0; i < 8; i++) {
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);

        N64RSP.vce.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vcr) {
    logdebug("rsp_vec_vcr");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        half vs_element = vs->elements[i];
        half vte_element = vte.elements[i];

        bool sign_different = (0x8000 & (vs_element ^ vte_element)) == 0x8000;

        // If vte and vs have different signs, make this negative vte
        half vt_abs = sign_different ? ~vte_element : vte_element;

        // Compare using one's complement
        bool gte = (shalf)vte_element <= (shalf)(sign_different ? 0xFFFF : vs_element);
        bool lte = (((sign_different ? vs_element : 0) + vte_element) & 0x8000) == 0x8000;

        // If the sign is different, check LTE, otherwise, check GTE.
        bool check = sign_different ? lte : gte;
        half result = check ? vt_abs : vs_element;

        N64RSP.acc.l.elements[i] = result;
        vd->elements[i] = result;

        N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(gte);
        N64RSP.vcc.l.elements[i] = FLAGREG_BOOL(lte);

        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vce.elements[i] = FLAGREG_BOOL(0);

    }
}

RSP_VECTOR_INSTR(rsp_vec_veq) {
    logdebug("rsp_vec_veq");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        N64RSP.vcc.l.elements[i] = FLAGREG_BOOL((N64RSP.vco.h.elements[i] == 0) && (vs->elements[i] == vte.elements[i]));
        N64RSP.acc.l.elements[i] = N64RSP.vcc.l.elements[i] != 0 ? vs->elements[i] : vte.elements[i];
        vd->elements[i] = N64RSP.acc.l.elements[i];

        N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vge) {
    logdebug("rsp_vec_vge");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        bool eql = vs->signed_elements[i] == vte.signed_elements[i];
        bool neg = !(N64RSP.vco.l.elements[i] != 0 && N64RSP.vco.h.elements[i] != 0) && eql;

        N64RSP.vcc.l.elements[i] = FLAGREG_BOOL(neg || (vs->signed_elements[i] > vte.signed_elements[i]));
        N64RSP.acc.l.elements[i] = N64RSP.vcc.l.elements[i] != 0 ? vs->elements[i] : vte.elements[i];
        vd->elements[i] = N64RSP.acc.l.elements[i];
        N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);

    }
}

RSP_VECTOR_INSTR(rsp_vec_vlt) {
    logdebug("rsp_vec_vlt");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        bool eql = vs->elements[i] == vte.elements[i];
        bool neg = N64RSP.vco.h.elements[i] != 0 && N64RSP.vco.l.elements[i] != 0 && eql;
        N64RSP.vcc.l.elements[i] = FLAGREG_BOOL(neg || (vs->signed_elements[i] < vte.signed_elements[i]));
        N64RSP.acc.l.elements[i] = N64RSP.vcc.l.elements[i] != 0 ? vs->elements[i] : vte.elements[i];
        vd->elements[i] = N64RSP.acc.l.elements[i];
        N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmacf) {
    logdebug("rsp_vec_vmacf");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc_delta = prod;
        acc_delta *= 2;
        sdword acc = get_rsp_accumulator(e) + acc_delta;
        set_rsp_accumulator(e, acc);
        acc = get_rsp_accumulator(e);

        shalf result = clamp_signed(acc >> 16);

        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmacq) {
    logdebug("rsp_vec_vmacq");
    logwarn("Unimplemented: rsp_vec_vmacq");
}

RSP_VECTOR_INSTR(rsp_vec_vmacu) {
    logdebug("rsp_vec_vmacu");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc_delta = prod;
        acc_delta *= 2;
        sdword acc = get_rsp_accumulator(e) + acc_delta;
        set_rsp_accumulator(e, acc);
        acc = get_rsp_accumulator(e);

        half result = clamp_unsigned(acc >> 16);

        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmadh) {
    logdebug("rsp_vec_vmadh");
    defvs;
    defvd;
    defvte;
#ifdef N64_USE_SIMD
    vecr lo, hi, omask;
    lo                 = _mm_mullo_epi16(vs->single, vte.single);
    hi                 = _mm_mulhi_epi16(vs->single, vte.single);
    omask              = _mm_adds_epu16(N64RSP.acc.m.single, lo);
    N64RSP.acc.m.single  = _mm_add_epi16(N64RSP.acc.m.single, lo);
    omask              = _mm_cmpeq_epi16(N64RSP.acc.m.single, omask);
    omask              = _mm_cmpeq_epi16(omask, N64RSP.zero);
    hi                 = _mm_sub_epi16(hi, omask);
    N64RSP.acc.h.single  = _mm_add_epi16(N64RSP.acc.h.single, hi);
    lo                 = _mm_unpacklo_epi16(N64RSP.acc.m.single, N64RSP.acc.h.single);
    hi                 = _mm_unpackhi_epi16(N64RSP.acc.m.single, N64RSP.acc.h.single);
    vd->single         = _mm_packs_epi32(lo, hi);
#else
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;
        word uprod = prod;

        dword acc_delta = (dword)uprod << 16;
        sdword acc = get_rsp_accumulator(e) + acc_delta;
        set_rsp_accumulator(e, acc);
        acc = get_rsp_accumulator(e);

        shalf result = clamp_signed(acc >> 16);

        vd->elements[e] = result;
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vmadl) {
    logdebug("rsp_vec_vmadl");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        dword multiplicand1 = vte.elements[e];
        dword multiplicand2 = vs->elements[e];
        dword prod = multiplicand1 * multiplicand2;

        dword acc_delta = prod >> 16;
        dword acc = get_rsp_accumulator(e) + acc_delta;

        set_rsp_accumulator(e, acc);
        half result;
        if (is_sign_extension(N64RSP.acc.h.signed_elements[e], N64RSP.acc.m.signed_elements[e])) {
            result = N64RSP.acc.l.elements[e];
        } else if (N64RSP.acc.h.signed_elements[e] < 0) {
            result = 0;
        } else {
            result = 0xFFFF;
        }

        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmadm) {
    logdebug("rsp_vec_vmadm");
    defvs;
    defvd;
    defvte;
#ifdef N64_USE_SIMD
    vecr lo, hi, sign, vta, omask;
    lo                 = _mm_mullo_epi16(vs->single, vte.single);
    hi                 = _mm_mulhi_epu16(vs->single, vte.single);
    sign               = _mm_srai_epi16(vs->single, 15);
    vta                = _mm_and_si128(vte.single, sign);
    hi                 = _mm_sub_epi16(hi, vta);
    omask              = _mm_adds_epu16(N64RSP.acc.l.single, lo);
    N64RSP.acc.l.single  = _mm_add_epi16(N64RSP.acc.l.single, lo);
    omask              = _mm_cmpeq_epi16(N64RSP.acc.l.single, omask);
    omask              = _mm_cmpeq_epi16(omask, N64RSP.zero);
    hi                 = _mm_sub_epi16(hi, omask);
    omask              = _mm_adds_epu16(N64RSP.acc.m.single, hi);
    N64RSP.acc.m.single  = _mm_add_epi16(N64RSP.acc.m.single, hi);
    omask              = _mm_cmpeq_epi16(N64RSP.acc.m.single, omask);
    omask              = _mm_cmpeq_epi16(omask, N64RSP.zero);
    hi                 = _mm_srai_epi16(hi, 15);
    N64RSP.acc.h.single  = _mm_add_epi16(N64RSP.acc.h.single, hi);
    N64RSP.acc.h.single  = _mm_sub_epi16(N64RSP.acc.h.single, omask);
    lo                 = _mm_unpacklo_epi16(N64RSP.acc.m.single, N64RSP.acc.h.single);
    hi                 = _mm_unpackhi_epi16(N64RSP.acc.m.single, N64RSP.acc.h.single);
    vd->single         = _mm_packs_epi32(lo, hi);
#else
    for (int e = 0; e < 8; e++) {
        half multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc_delta = prod;
        sdword acc = get_rsp_accumulator(e);
        acc += acc_delta;
        set_rsp_accumulator(e, acc);
        acc = get_rsp_accumulator(e);

        shalf result = clamp_signed(acc >> 16);

        vd->elements[e] = result;
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vmadn) {
    logdebug("rsp_vec_vmadn");
    defvs;
    defvd;
    defvte;
#ifdef N64_USE_SIMD
    vecr lo, hi, sign, vsa, omask, nhi, nmd, shi, smd, cmask, cval;
    lo                 = _mm_mullo_epi16(vs->single, vte.single);
    hi                 = _mm_mulhi_epu16(vs->single, vte.single);
    sign               = _mm_srai_epi16(vte.single, 15);
    vsa                = _mm_and_si128(vs->single, sign);
    hi                 = _mm_sub_epi16(hi, vsa);
    omask              = _mm_adds_epu16(N64RSP.acc.l.single, lo);
    N64RSP.acc.l.single  = _mm_add_epi16(N64RSP.acc.l.single, lo);
    omask              = _mm_cmpeq_epi16(N64RSP.acc.l.single, omask);
    omask              = _mm_cmpeq_epi16(omask, N64RSP.zero);
    hi                 = _mm_sub_epi16(hi, omask);
    omask              = _mm_adds_epu16(N64RSP.acc.m.single, hi);
    N64RSP.acc.m.single  = _mm_add_epi16(N64RSP.acc.m.single, hi);
    omask              = _mm_cmpeq_epi16(N64RSP.acc.m.single, omask);
    omask              = _mm_cmpeq_epi16(omask, N64RSP.zero);
    hi                 = _mm_srai_epi16(hi, 15);
    N64RSP.acc.h.single  = _mm_add_epi16(N64RSP.acc.h.single, hi);
    N64RSP.acc.h.single  = _mm_sub_epi16(N64RSP.acc.h.single, omask);
    nhi                = _mm_srai_epi16(N64RSP.acc.h.single, 15);
    nmd                = _mm_srai_epi16(N64RSP.acc.m.single, 15);
    shi                = _mm_cmpeq_epi16(nhi, N64RSP.acc.h.single);
    smd                = _mm_cmpeq_epi16(nhi, nmd);
    cmask              = _mm_and_si128(smd, shi);
    cval               = _mm_cmpeq_epi16(nhi, N64RSP.zero);
    vd->single         = _mm_blendv_epi8(cval, N64RSP.acc.l.single, cmask);
#else
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        half multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc_delta = prod;
        sdword acc = get_rsp_accumulator(e) + acc_delta;

        set_rsp_accumulator(e, acc);

        half result;

        if (is_sign_extension(N64RSP.acc.h.signed_elements[e], N64RSP.acc.m.signed_elements[e])) {
            result = N64RSP.acc.l.elements[e];
        } else if (N64RSP.acc.h.signed_elements[e] < 0) {
            result = 0;
        } else {
            result = 0xFFFF;
        }

        vd->elements[e] = result;
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vmov) {
    logdebug("rsp_vec_vmov");
    defvd;
    defvte;

    byte se;

    switch (instruction.cp2_vec.e) {
        case 0 ... 1:
            se = (instruction.cp2_vec.e & 0b000) | (instruction.cp2_vec.vs & 0b111);
            break;
        case 2 ... 3:
            se = (instruction.cp2_vec.e & 0b001) | (instruction.cp2_vec.vs & 0b110);
            break;
        case 4 ... 7:
            se = (instruction.cp2_vec.e & 0b011) | (instruction.cp2_vec.vs & 0b100);
            break;
        case 8 ... 15:
            se = (instruction.cp2_vec.e & 0b111) | (instruction.cp2_vec.vs & 0b000);
            break;
        default:
            logfatal("This should be unreachable!");
    }

    byte de = instruction.cp2_vec.vs & 7;

    half vte_elem = vte.elements[VU_ELEM_INDEX(se)];
#ifdef N64_USE_SIMD
    vd->elements[VU_ELEM_INDEX(de)] = vte_elem;
    N64RSP.acc.l = vte;
#else
    vd->elements[VU_ELEM_INDEX(de)] = vte_elem;
    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = vte.elements[i];
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vmrg) {
    logdebug("rsp_vec_vmrg");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = N64RSP.vcc.l.elements[i] != 0 ? vs->elements[i] : vte.elements[i];
        vd->elements[i] = N64RSP.acc.l.elements[i];

        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmudh) {
    logdebug("rsp_vec_vmudh");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc = (sdword)prod;

        shalf result = clamp_signed(acc);

        acc <<= 16;
        set_rsp_accumulator(e, acc);

        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmudl) {
    logdebug("rsp_vec_vmudl");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        dword multiplicand1 = vte.elements[e];
        dword multiplicand2 = vs->elements[e];
        dword prod = multiplicand1 * multiplicand2;

        dword acc = prod >> 16;

        set_rsp_accumulator(e, acc);
        half result;
        if (is_sign_extension(N64RSP.acc.h.signed_elements[e], N64RSP.acc.m.signed_elements[e])) {
            result = N64RSP.acc.l.elements[e];
        } else if (N64RSP.acc.h.signed_elements[e] < 0) {
            result = 0;
        } else {
            result = 0xFFFF;
        }

        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmudm) {
    logdebug("rsp_vec_vmudm");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        half multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc = prod;

        shalf result = clamp_signed(acc >> 16);

        set_rsp_accumulator(e, acc);
        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmudn) {
    logdebug("rsp_vec_vmudn");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        half multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc = prod;

        set_rsp_accumulator(e, acc);

        half result;
        if (is_sign_extension(N64RSP.acc.h.signed_elements[e], N64RSP.acc.m.signed_elements[e])) {
            result = N64RSP.acc.l.elements[e];
        } else if (N64RSP.acc.h.signed_elements[e] < 0) {
            result = 0;
        } else {
            result = 0xFFFF;
        }

        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmulf) {
    logdebug("rsp_vec_vmulf");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc = prod;
        acc = (acc * 2) + 0x8000;

        set_rsp_accumulator(e, acc);

        shalf result = clamp_signed(acc >> 16);
        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vmulq) {
    logdebug("rsp_vec_vmulq");
    logwarn("Unimplemented: rsp_vec_vmulq");
}

RSP_VECTOR_INSTR(rsp_vec_vmulu) {
    logdebug("rsp_vec_vmulu");
    defvs;
    defvd;
    defvte;
    for (int e = 0; e < 8; e++) {
        shalf multiplicand1 = vte.elements[e];
        shalf multiplicand2 = vs->elements[e];
        sword prod = multiplicand1 * multiplicand2;

        sdword acc = prod;
        acc = (acc * 2) + 0x8000;

        half result = clamp_unsigned(acc >> 16);

        set_rsp_accumulator(e, acc);
        vd->elements[e] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vnand) {
    logdebug("rsp_vec_vnand");
    defvs;
    defvd;
    defvte;
    for (int i = 0; i < 8; i++) {
        half result = ~(vte.elements[i] & vs->elements[i]);
        vd->elements[i] = result;
        N64RSP.acc.l.elements[i] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vne) {
    logdebug("rsp_vec_vne");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        N64RSP.vcc.l.elements[i] = FLAGREG_BOOL((N64RSP.vco.h.elements[i] != 0) || (vs->elements[i] != vte.elements[i]));
        N64RSP.acc.l.elements[i] = N64RSP.vcc.l.elements[i] != 0 ? vs->elements[i] : vte.elements[i];
        vd->elements[i] = N64RSP.acc.l.elements[i];

        N64RSP.vcc.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vnop) {}

RSP_VECTOR_INSTR(rsp_vec_vnor) {
    logdebug("rsp_vec_vnor");
    defvs;
    defvd;
    defvte;
    for (int i = 0; i < 8; i++) {
        half result = ~(vte.elements[i] | vs->elements[i]);
        vd->elements[i] = result;
        N64RSP.acc.l.elements[i] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vnxor) {
    logdebug("rsp_vec_vnxor");
    defvs;
    defvd;
    defvte;
    for (int i = 0; i < 8; i++) {
        half result = ~(vte.elements[i] ^ vs->elements[i]);
        vd->elements[i] = result;
        N64RSP.acc.l.elements[i] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vor) {
    logdebug("rsp_vec_vor");
    defvs;
    defvd;
    defvte;
    for (int i = 0; i < 8; i++) {
        half result =  vte.elements[i] | vs->elements[i];
        vd->elements[i] = result;
        N64RSP.acc.l.elements[i] = result;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vrcp) {
    logdebug("rsp_vec_vrcp");
    defvd;
    defvt;
    defvte;
    sword input;
    int e  = instruction.cp2_vec.e & 7;
    int de = instruction.cp2_vec.vs & 7;
    input = vt->signed_elements[VU_ELEM_INDEX(e)];
    sword result = rcp(input);
    vd->elements[VU_ELEM_INDEX(de)] = result & 0xFFFF;
    N64RSP.divout = (result >> 16) & 0xFFFF;
    N64RSP.divin_loaded = false;
#ifdef N64_USE_SIMD
    N64RSP.acc.l.single = vte.single;
#else
    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = vte.elements[i];
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vrcpl) {
    logdebug("rsp_vec_vrcpl");
    defvd;
    defvt;
    defvte;
    sword input;
    int e  = instruction.cp2_vec.e & 7;
    int de = instruction.cp2_vec.vs & 7;
    if (N64RSP.divin_loaded) {
        input = ((sword)N64RSP.divin << 16) | vt->elements[VU_ELEM_INDEX(e)];
    } else {
        input = vt->signed_elements[VU_ELEM_INDEX(e)];
    }
    sword result = rcp(input);
    vd->elements[VU_ELEM_INDEX(de)] = result & 0xFFFF;
    N64RSP.divout = (result >> 16) & 0xFFFF;
    N64RSP.divin = 0;
    N64RSP.divin_loaded = false;
#ifdef N64_USE_SIMD
    N64RSP.acc.l.single = vte.single;
#else
    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = vte.elements[i];
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vrndn) {
    logdebug("rsp_vec_vrndn");
    logwarn("Unimplemented: rsp_vec_vrndn");
}

RSP_VECTOR_INSTR(rsp_vec_vrndp) {
    logdebug("rsp_vec_vrndp");
    logwarn("Unimplemented: rsp_vec_vrndp");
}

RSP_VECTOR_INSTR(rsp_vec_vrsq) {
    logdebug("rsp_vec_vrsq");
    sword input;
    defvd;
    defvt;
    defvte;
    int e  = instruction.cp2_vec.e & 7;
    int de = instruction.cp2_vec.vs & 7;
    input = vt->signed_elements[VU_ELEM_INDEX(e)];
    word result = rsq(input);
    vd->elements[VU_ELEM_INDEX(de)] = result & 0xFFFF;
    N64RSP.divout = (result >> 16) & 0xFFFF;
    N64RSP.divin_loaded = false;

#ifdef N64_USE_SIMD
    N64RSP.acc.l.single = vte.single;
#else
    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = vte.elements[i];
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vrcph_vrsqh) {
    logdebug("rsp_vec_vrcph_vrsqh");
    defvt;
    defvd;
    defvte;
    byte de = instruction.cp2_vec.vs;

#ifdef N64_USE_SIMD
    N64RSP.acc.l.single = vte.single;
#else
    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = vte.elements[i];
    }
#endif
    N64RSP.divin_loaded = true;
    N64RSP.divin = vt->elements[VU_ELEM_INDEX(instruction.cp2_vec.e & 7)];
    vd->elements[VU_ELEM_INDEX(de & 7)] = N64RSP.divout;
}

RSP_VECTOR_INSTR(rsp_vec_vrsql) {
    logdebug("rsp_vec_vrsql");
    defvt;
    defvte;
    defvd;
    sword input;
    int e  = instruction.cp2_vec.e & 7;
    int de = instruction.cp2_vec.vs & 7;
    if (N64RSP.divin_loaded) {
        input = (N64RSP.divin << 16) | vt->elements[VU_ELEM_INDEX(e)];
    } else {
        input = vt->signed_elements[VU_ELEM_INDEX(e)];
    }
    word result = rsq(input);
    vd->elements[VU_ELEM_INDEX(de)] = result & 0xFFFF;
    N64RSP.divout = (result >> 16) & 0xFFFF;
    N64RSP.divin = 0;
    N64RSP.divin_loaded = false;

#ifdef N64_USE_SIMD
    N64RSP.acc.l.single = vte.single;
#else
    for (int i = 0; i < 8; i++) {
        N64RSP.acc.l.elements[i] = vte.elements[i];
    }
#endif
}

RSP_VECTOR_INSTR(rsp_vec_vsar) {
    logdebug("rsp_vec_vsar");
    defvd;
    switch (instruction.cp2_vec.e) {
        case 0x8:
#ifdef N64_USE_SIMD
            vd->single = N64RSP.acc.h.single;
#else
            for (int i = 0; i < 8; i++) {
                vd->elements[i] = N64RSP.acc.h.elements[i];
            }
#endif
            break;
        case 0x9:
#ifdef N64_USE_SIMD
            vd->single = N64RSP.acc.m.single;
#else
            for (int i = 0; i < 8; i++) {
                vd->elements[i] = N64RSP.acc.m.elements[i];
            }
#endif
            break;
        case 0xA:
#ifdef N64_USE_SIMD
            vd->single = N64RSP.acc.l.single;
#else
            for (int i = 0; i < 8; i++) {
                vd->elements[i] = N64RSP.acc.l.elements[i];
            }
#endif
            break;
        default: // Not actually sure what the default behavior is here
            for (int i = 0; i < 8; i++) {
                vd->elements[i] = 0x0000;
            }
            break;
    }
}

RSP_VECTOR_INSTR(rsp_vec_vsub) {
    logdebug("rsp_vec_vsub");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        sword result = vs->signed_elements[i] - vte.signed_elements[i] - (N64RSP.vco.l.elements[i] != 0);
        N64RSP.acc.l.signed_elements[i] = result;
        vd->signed_elements[i] = clamp_signed(result);
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(0);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(0);
    }
}

RSP_VECTOR_INSTR(rsp_vec_vsubc) {
    logdebug("rsp_vec_vsubc");
    defvs;
    defvd;
    defvte;

    for (int i = 0; i < 8; i++) {
        word result = vs->elements[i] - vte.elements[i];
        half hresult = result & 0xFFFF;
        bool carry = (result >> 16) & 1;

        vd->elements[i] = hresult;
        N64RSP.acc.l.elements[i] = hresult;
        N64RSP.vco.l.elements[i] = FLAGREG_BOOL(carry);
        N64RSP.vco.h.elements[i] = FLAGREG_BOOL(result != 0); // not hresult, but I bet that'd also work
    }
}

RSP_VECTOR_INSTR(rsp_vec_vxor) {
    logdebug("rsp_vec_vxor");
    defvs;
    defvte;
    defvd;
    for (int i = 0; i < 8; i++) {
        half result = vte.elements[i] ^ vs->elements[i];
        vd->elements[i] = result;
        N64RSP.acc.l.elements[i] = result;
    }
}
