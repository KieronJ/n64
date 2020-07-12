#ifndef N64_RSP_H
#define N64_RSP_H

#include <stdbool.h>
#include "../common/util.h"
#include "../common/log.h"
#include "rsp_types.h"
#include "../mem/addresses.h"
#include "../system/n64system.h"
#include "rsp_interface.h"
#include "../rdp/rdp.h"

#define RSP_CP0_DMA_CACHE        0
#define RSP_CP0_DMA_DRAM         1
#define RSP_CP0_DMA_READ_LENGTH  2
#define RSP_CP0_DMA_WRITE_LENGTH 3
#define RSP_CP0_SP_STATUS        4
#define RSP_CP0_DMA_FULL         5
#define RSP_CP0_DMA_BUSY         6
#define RSP_CP0_DMA_RESERVED     7
#define RSP_CP0_CMD_START        8
#define RSP_CP0_CMD_END          9
#define RSP_CP0_CMD_CURRENT      10
#define RSP_CP0_CMD_STATUS       11
#define RSP_CP0_CMD_CLOCK        12
#define RSP_CP0_CMD_BUSY         13
#define RSP_CP0_CMD_PIPE_BUSY    14
#define RSP_CP0_CMD_TMEM_BUSY    15

#define RSP_OPC_LWC2 0b110010
#define RSP_OPC_SWC2 0b111010

#define LWC2_LBV 0b00000
#define LWC2_LDV 0b00011
#define LWC2_LFV 0b01001
#define LWC2_LHV 0b01000
#define LWC2_LLV 0b00010
#define LWC2_LPV 0b00110
#define LWC2_LQV 0b00100
#define LWC2_LRV 0b00101
#define LWC2_LSV 0b00001
#define LWC2_LTV 0b01011
#define LWC2_LUV 0b00111

#define FUNCT_RSP_VEC_VABS  0b010011
#define FUNCT_RSP_VEC_VADD  0b010000
#define FUNCT_RSP_VEC_VADDC 0b010100
#define FUNCT_RSP_VEC_VAND  0b101000
#define FUNCT_RSP_VEC_VCH   0b100101
#define FUNCT_RSP_VEC_VCL   0b100100
#define FUNCT_RSP_VEC_VCR   0b100110
#define FUNCT_RSP_VEC_VEQ   0b100001
#define FUNCT_RSP_VEC_VGE   0b100011
#define FUNCT_RSP_VEC_VLT   0b100000
#define FUNCT_RSP_VEC_VMACF 0b001000
#define FUNCT_RSP_VEC_VMACQ 0b001011
#define FUNCT_RSP_VEC_VMACU 0b001001
#define FUNCT_RSP_VEC_VMADH 0b001111
#define FUNCT_RSP_VEC_VMADL 0b001100
#define FUNCT_RSP_VEC_VMADM 0b001101
#define FUNCT_RSP_VEC_VMADN 0b001110
#define FUNCT_RSP_VEC_VMOV  0b110011
#define FUNCT_RSP_VEC_VMRG  0b100111
#define FUNCT_RSP_VEC_VMUDH 0b000111
#define FUNCT_RSP_VEC_VMUDL 0b000100
#define FUNCT_RSP_VEC_VMUDM 0b000101
#define FUNCT_RSP_VEC_VMUDN 0b000110
#define FUNCT_RSP_VEC_VMULF 0b000000
#define FUNCT_RSP_VEC_VMULQ 0b000011
#define FUNCT_RSP_VEC_VMULU 0b000001
#define FUNCT_RSP_VEC_VNAND 0b101001
#define FUNCT_RSP_VEC_VNE   0b100010
#define FUNCT_RSP_VEC_VNOP  0b110111
#define FUNCT_RSP_VEC_VNOR  0b101011
#define FUNCT_RSP_VEC_VNXOR 0b101101
#define FUNCT_RSP_VEC_VOR   0b101010
#define FUNCT_RSP_VEC_VRCP  0b110000
#define FUNCT_RSP_VEC_VRCPH 0b110010
#define FUNCT_RSP_VEC_VRCPL 0b110001
#define FUNCT_RSP_VEC_VRNDN 0b001010
#define FUNCT_RSP_VEC_VRNDP 0b000010
#define FUNCT_RSP_VEC_VRSQ  0b110100
#define FUNCT_RSP_VEC_VRSQH 0b110110
#define FUNCT_RSP_VEC_VRSQL 0b110101
#define FUNCT_RSP_VEC_VSAR  0b011101
#define FUNCT_RSP_VEC_VSUB  0b010001
#define FUNCT_RSP_VEC_VSUBC 0b010101
#define FUNCT_RSP_VEC_VXOR  0b101100

INLINE void rsp_dma_read(rsp_t* rsp) {
    word length = (rsp->io.dma_read.length | 7) + 1;
    for (int i = 0; i < rsp->io.dma_read.count + 1; i++) {
        word mem_addr = rsp->io.mem_addr.address + (rsp->io.mem_addr.imem ? SREGION_SP_IMEM : SREGION_SP_DMEM);
        for (int j = 0; j < length; j++) {
            byte val = rsp->read_physical_byte(rsp->io.dram_addr.address + j);
            logtrace("SP DMA: Copying 0x%02X from 0x%08X to 0x%08X", val, rsp->io.dram_addr.address + j, mem_addr + j)
            rsp->write_physical_byte(mem_addr + j, val);
        }

        rsp->io.dram_addr.address += length + rsp->io.dma_read.skip;
        rsp->io.mem_addr.address += length;
    }
}

INLINE void rsp_dma_write(rsp_t* rsp) {
    word length = (rsp->io.dma_read.length | 7) + 1;
    for (int i = 0; i < rsp->io.dma_read.count + 1; i++) {
        word mem_addr = rsp->io.mem_addr.address + (rsp->io.mem_addr.imem ? SREGION_SP_IMEM : SREGION_SP_DMEM);
        for (int j = 0; j < length; j++) {
            byte val = rsp->read_physical_byte(mem_addr + j);
            logtrace("SP DMA: Copying 0x%02X from 0x%08X to 0x%08X", val, rsp->io.dram_addr.address + j, mem_addr + j)
            rsp->write_physical_byte(rsp->io.dram_addr.address + j, val);
        }

        rsp->io.dram_addr.address += length + rsp->io.dma_read.skip;
        rsp->io.mem_addr.address += length;
    }
}

INLINE void set_rsp_register(rsp_t* rsp, byte r, word value) {
    logtrace("Setting RSP r%d to [0x%08X]", r, value)
    if (r != 0) {
        if (r < 64) {
            rsp->gpr[r] = value;
        } else {
            logfatal("Write to invalid RSP register: %d", r)
        }
    }
}

INLINE word get_rsp_register(rsp_t* rsp, byte r) {
    if (r < 64) {
        word value = rsp->gpr[r];
        logtrace("Reading RSP r%d: 0x%08X", r, value)
        return value;
    } else {
        logfatal("Attempted to read invalid RSP register: %d", r)
    }
}

bool rsp_acquire_semaphore(n64_system_t* system);

INLINE word get_rsp_cp0_register(n64_system_t* system, byte r) {
    switch (r) {
        case RSP_CP0_DMA_CACHE:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_DMA_CACHE", r)
        case RSP_CP0_DMA_DRAM:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_DMA_DRAM", r)
        case RSP_CP0_DMA_READ_LENGTH:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_DMA_READ_LENGTH", r)
        case RSP_CP0_DMA_WRITE_LENGTH:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_DMA_WRITE_LENGTH", r)
        case RSP_CP0_SP_STATUS: return system->rsp.status.raw;
        case RSP_CP0_DMA_FULL:  return system->rsp.status.dma_full;
        case RSP_CP0_DMA_BUSY:  return system->rsp.status.dma_busy;
        case RSP_CP0_DMA_RESERVED: return rsp_acquire_semaphore(system);
        case RSP_CP0_CMD_START:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_CMD_START", r)
        case RSP_CP0_CMD_END:     return system->dpc.end;
        case RSP_CP0_CMD_CURRENT: return system->dpc.current;
        case RSP_CP0_CMD_STATUS:  return system->dpc.status.raw;
        case RSP_CP0_CMD_CLOCK:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_CMD_CLOCK", r)
        case RSP_CP0_CMD_BUSY:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_CMD_BUSY", r)
        case RSP_CP0_CMD_PIPE_BUSY:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_CMD_PIPE_BUSY", r)
        case RSP_CP0_CMD_TMEM_BUSY:
            logfatal("Read from unknown RSP CP0 register $c%d: RSP_CP0_CMD_TMEM_BUSY", r)
        default:
            logfatal("Unsupported RSP CP0 $c%d read", r)
    }
}

INLINE void set_rsp_cp0_register(n64_system_t* system, byte r, word value) {
    switch (r) {
        case RSP_CP0_DMA_CACHE: system->rsp.io.mem_addr.raw = value; break;
        case RSP_CP0_DMA_DRAM:  system->rsp.io.dram_addr.raw = value; break;
        case RSP_CP0_DMA_READ_LENGTH:
            system->rsp.io.dma_read.raw = value;
            rsp_dma_read(&system->rsp);
            break;
        case RSP_CP0_DMA_WRITE_LENGTH:
            system->rsp.io.dma_write.raw = value;
            rsp_dma_write(&system->rsp);
            break;
        case RSP_CP0_SP_STATUS:
            rsp_status_reg_write(system, value);
            break;
        case RSP_CP0_DMA_FULL:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_DMA_FULL", r)
        case RSP_CP0_DMA_BUSY:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_DMA_BUSY", r)
        case RSP_CP0_DMA_RESERVED: {
            if (value == 0) {
                system->rsp.semaphore_held = 0;
            } else {
                logfatal("Wrote non-zero value 0x%08X to $c7 RSP_CP0_DMA_RESERVED", value)
            }
            break;
        }
        case RSP_CP0_CMD_START:
            system->dpc.start = value & 0xFFFFFF;
            system->dpc.current = system->dpc.start;
            printf("DPC_START = 0x%08X\n", system->dpc.start);
            break;
        case RSP_CP0_CMD_END:
            printf("DPC_END = 0x%08X\n", system->dpc.start);
            system->dpc.end = value & 0xFFFFFF;
            rdp_run_command();
            break;
        case RSP_CP0_CMD_CURRENT:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_CMD_CURRENT", r)
        case RSP_CP0_CMD_STATUS:
            rdp_status_reg_write(system, value);
            break;
        case RSP_CP0_CMD_CLOCK:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_CMD_CLOCK", r)
        case RSP_CP0_CMD_BUSY:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_CMD_BUSY", r)
        case RSP_CP0_CMD_PIPE_BUSY:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_CMD_PIPE_BUSY", r)
        case RSP_CP0_CMD_TMEM_BUSY:
            logfatal("Write to unknown RSP CP0 register $c%d: RSP_CP0_CMD_TMEM_BUSY", r)
        default:
            logfatal("Unsupported RSP CP0 $c%d written to", r)
    }
}

void rsp_step(n64_system_t* system);

#endif //N64_RSP_H
