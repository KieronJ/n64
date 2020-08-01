#include "debugger.h"
#include "../mem/n64bus.h"

#define GDBSTUB_IMPLEMENTATION
#include <gdbstub.h>

const char* target_xml =
        "<?xml version=\"1.0\"?>"
        "<!DOCTYPE feature SYSTEM \"gdb-target.dtd\">"
        "<target version=\"1.0\">"
        "<architecture>mips:4000</architecture>"
        "<osabi>none</osabi>"
        "<feature name=\"org.gnu.gdb.mips.cpu\">"
        "        <reg name=\"r0\" bitsize=\"64\" regnum=\"0\"/>"
        "        <reg name=\"r1\" bitsize=\"64\"/>"
        "        <reg name=\"r2\" bitsize=\"64\"/>"
        "        <reg name=\"r3\" bitsize=\"64\"/>"
        "        <reg name=\"r4\" bitsize=\"64\"/>"
        "        <reg name=\"r5\" bitsize=\"64\"/>"
        "        <reg name=\"r6\" bitsize=\"64\"/>"
        "        <reg name=\"r7\" bitsize=\"64\"/>"
        "        <reg name=\"r8\" bitsize=\"64\"/>"
        "        <reg name=\"r9\" bitsize=\"64\"/>"
        "        <reg name=\"r10\" bitsize=\"64\"/>"
        "        <reg name=\"r11\" bitsize=\"64\"/>"
        "        <reg name=\"r12\" bitsize=\"64\"/>"
        "        <reg name=\"r13\" bitsize=\"64\"/>"
        "        <reg name=\"r14\" bitsize=\"64\"/>"
        "        <reg name=\"r15\" bitsize=\"64\"/>"
        "        <reg name=\"r16\" bitsize=\"64\"/>"
        "        <reg name=\"r17\" bitsize=\"64\"/>"
        "        <reg name=\"r18\" bitsize=\"64\"/>"
        "        <reg name=\"r19\" bitsize=\"64\"/>"
        "        <reg name=\"r20\" bitsize=\"64\"/>"
        "        <reg name=\"r21\" bitsize=\"64\"/>"
        "        <reg name=\"r22\" bitsize=\"64\"/>"
        "        <reg name=\"r23\" bitsize=\"64\"/>"
        "        <reg name=\"r24\" bitsize=\"64\"/>"
        "        <reg name=\"r25\" bitsize=\"64\"/>"
        "        <reg name=\"r26\" bitsize=\"64\"/>"
        "        <reg name=\"r27\" bitsize=\"64\"/>"
        "        <reg name=\"r28\" bitsize=\"64\"/>"
        "        <reg name=\"r29\" bitsize=\"64\"/>"
        "        <reg name=\"r30\" bitsize=\"64\"/>"
        "        <reg name=\"r31\" bitsize=\"64\"/>"
        "        <reg name=\"lo\" bitsize=\"64\" regnum=\"33\"/>"
        "        <reg name=\"hi\" bitsize=\"64\" regnum=\"34\"/>"
        "        <reg name=\"pc\" bitsize=\"32\" regnum=\"37\"/>"
        "        </feature>"
        "<feature name=\"org.gnu.gdb.mips.cp0\">"
        "        <reg name=\"status\" bitsize=\"32\" regnum=\"32\"/>"
        "        <reg name=\"badvaddr\" bitsize=\"32\" regnum=\"35\"/>"
        "        <reg name=\"cause\" bitsize=\"32\" regnum=\"36\"/>"
        "        </feature>"
        "<!-- TODO fix the sizes here. How do we deal with configurable sizes? -->"
        "<feature name=\"org.gnu.gdb.mips.fpu\">"
        "        <reg name=\"f0\" bitsize=\"32\" type=\"ieee_single\" regnum=\"38\"/>"
        "        <reg name=\"f1\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f2\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f3\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f4\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f5\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f6\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f7\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f8\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f9\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f10\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f11\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f12\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f13\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f14\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f15\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f16\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f17\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f18\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f19\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f20\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f21\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f22\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f23\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f24\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f25\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f26\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f27\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f28\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f29\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f30\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"f31\" bitsize=\"32\" type=\"ieee_single\"/>"
        "        <reg name=\"fcsr\" bitsize=\"32\" group=\"float\"/>"
        "        <reg name=\"fir\" bitsize=\"32\" group=\"float\"/>"
        "        </feature>"
        "</target>";

const char* memory_map =
"<?xml version=\"1.0\"?>"
"<memory-map>"
    "<!-- KUSEG - TLB mapped, treat it as a giant block of RAM. Not ideal, but not sure how else to deal with it -->"
    "<memory type=\"ram\" start=\"0x0000000000000000\" length=\"0x80000000\"/>"

    "<!-- KSEG0 hardware mapped, full copy of the memory map goes here -->" // TODO finish
    "<memory type=\"ram\" start=\"0xffffffff80000000\" length=\"0x800000\"/>" // RDRAM
    "<memory type=\"ram\" start=\"0xffffffff84000000\" length=\"0x1000\"/>" // RSP DMEM
    "<memory type=\"ram\" start=\"0xffffffff84001000\" length=\"0x1000\"/>" // RSP IMEM

    "<!-- KSEG1 hardware mapped, full copy of the memory map goes here -->" // TODO finish
    "<memory type=\"ram\" start=\"0xffffffffa0000000\" length=\"0x800000\"/>" // RDRAM
    "<memory type=\"ram\" start=\"0xffffffffa4000000\" length=\"0x1000\"/>" // RSP DMEM
    "<memory type=\"ram\" start=\"0xffffffffa4001000\" length=\"0x1000\"/>" // RSP IMEM
"</memory-map>";

void n64_debug_start(n64_system_t* system) {
    system->debugger_state.broken = false;
}

void n64_debug_stop(n64_system_t* system) {
    system->debugger_state.broken = true;
}

void n64_debug_step(n64_system_t* system) {
    bool old_broken = system->debugger_state.broken;
    system->debugger_state.broken = false;
    n64_system_step(system);
    system->debugger_state.broken = old_broken;
    system->debugger_state.steps += 2;
}

void n64_debug_set_breakpoint(n64_system_t* system, word address) {
    n64_breakpoint_t* breakpoint = malloc(sizeof(n64_breakpoint_t));
    breakpoint->address = address;
    breakpoint->next = NULL;

    // Special case for this being the first breakpoint
    if (system->debugger_state.breakpoints == NULL) {
        system->debugger_state.breakpoints = breakpoint;
    } else {
        // Find end of the list
        n64_breakpoint_t* tail = system->debugger_state.breakpoints;
        while (tail->next != NULL) {
            tail = tail->next;
        }

        tail->next = breakpoint;
    }
}

void n64_debug_clear_breakpoint(n64_system_t* system, word address) {
    if (system->debugger_state.breakpoints == NULL) {
        return; // No breakpoints set at all
    } else if (system->debugger_state.breakpoints->address == address) {
        // Special case for the first breakpoint being the one we want to clear
        n64_breakpoint_t* next = system->debugger_state.breakpoints->next;
        free(system->debugger_state.breakpoints);
        system->debugger_state.breakpoints = next;
    } else {
        // Find the breakpoint somewhere in the list and free it
        n64_breakpoint_t* iter = system->debugger_state.breakpoints;
        while (iter->next != NULL) {
            if (iter->next->address == address) {
                n64_breakpoint_t* next = iter->next->next;
                free(iter->next);
                iter->next = next;
            }
        }
    }
}

ssize_t n64_debug_get_memory(n64_system_t* system, char* buffer, size_t length, word address, size_t bytes) {
    printf("Checking memory at address 0x%08X\n", address);
    int printed = 0;
    for (int i = 0; i < bytes; i++) {
        byte value = n64_read_byte(system, vatopa(address + i, &system->cpu.cp0));
        printed += snprintf(buffer + (i*2), length, "%02X", value);
    }
    printf("Get memory: %ld bytes from 0x%08X: %d\n", bytes, address, printed);
    return printed + 1;
}

ssize_t n64_debug_get_register_value(n64_system_t* system, char * buffer, size_t buffer_length, int reg) {
    switch (reg) {
        case 0 ... 31:
            return snprintf(buffer, buffer_length, "%016lx", system->cpu.gpr[reg]);
        case 32:
            return snprintf(buffer, buffer_length, "%08x", system->cpu.cp0.status.raw);
        case 33:
            return snprintf(buffer, buffer_length, "%016lx", system->cpu.mult_lo);
        case 34:
            return snprintf(buffer, buffer_length, "%016lx", system->cpu.mult_hi);
        case 35:
            return snprintf(buffer, buffer_length, "%08x", system->cpu.cp0.bad_vaddr);
        case 36:
            return snprintf(buffer, buffer_length, "%08x", system->cpu.cp0.cause.raw);
        case 37:
            printf("Sending PC\n");
            return snprintf(buffer, buffer_length, "%08x", system->cpu.pc);
        case 38 ... 71: // TODO FPU stuff
            return snprintf(buffer, buffer_length, "%08x", 0);
        default:
            logfatal("debug get register %d value", reg)
    }
}

ssize_t n64_debug_get_general_registers(n64_system_t* system, char * buffer, size_t buffer_length) {
    ssize_t printed = 0;
    for (int i = 0; i < 32; i++) {
        int ofs = i * 16; // 64 bit regs take up 16 ascii chars to print in hex
        if (ofs + 16 > buffer_length) {
            logfatal("Too big!")
        }
        dword reg = system->cpu.gpr[i];
        printed += snprintf(buffer + ofs, buffer_length - ofs, "%016lx", reg);
    }
    return printed;
}

void debugger_init(n64_system_t* system) {
    gdbstub_config_t config;
    config.port                  = GDB_CPU_PORT;
    config.user_data             = system;
    config.start                 = (gdbstub_start_t) n64_debug_start;
    config.stop                  = (gdbstub_stop_t) n64_debug_stop;
    config.step                  = (gdbstub_step_t) n64_debug_step;
    config.set_breakpoint        = (gdbstub_set_breakpoint_t) n64_debug_set_breakpoint;
    config.clear_breakpoint      = (gdbstub_clear_breakpoint_t) n64_debug_clear_breakpoint;
    config.get_memory            = (gdbstub_get_memory_t) n64_debug_get_memory;
    config.get_register_value    = (gdbstub_get_register_value_t) n64_debug_get_register_value;
    config.get_general_registers = (gdbstub_get_general_registers_t) n64_debug_get_general_registers;

    config.target_config = target_xml;
    config.target_config_length = strlen(target_xml);

    printf("Sizeof target: %ld\n", config.target_config_length);

    config.memory_map = memory_map;
    config.memory_map_length = strlen(memory_map);

    printf("Sizeof memory map: %ld\n", config.memory_map_length);

    system->debugger_state.gdb = gdbstub_init(config);
    if (!system->debugger_state.gdb) {
        logfatal("Failed to initialize GDB stub!")
    }
}

void debugger_tick(n64_system_t* system) {
    gdbstub_tick(system->debugger_state.gdb);
}

void debugger_breakpoint_hit(n64_system_t* system) {
    system->debugger_state.broken = true;
    gdbstub_breakpoint_hit(system->debugger_state.gdb);
}

void debugger_cleanup(n64_system_t* system) {
    if (system->debugger_state.enabled) {
        gdbstub_term(system->debugger_state.gdb);
    }
}