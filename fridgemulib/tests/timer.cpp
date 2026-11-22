#include <cstdlib>
#include <iostream>

extern "C" {
#include "fridgemulib.h"
}

static void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

static FRIDGE_WORD io(FRIDGE_SYSTEM& sys, FRIDGE_WORD opcode, FRIDGE_WORD port)
{
    FRIDGE_CPU& cpu = *sys.cpu;
    cpu.PC = FRIDGE_EXECUTABLE_OFFSET;
    cpu.ram[cpu.PC] = opcode;
    cpu.ram[cpu.PC + 1] = port;
    FRIDGE_sys_tick(&sys);
    return cpu.rA;
}

static void command(FRIDGE_SYSTEM& sys, FRIDGE_WORD port, FRIDGE_TIMER_COMMAND op, unsigned arg = 0)
{
    sys.cpu->rA = static_cast<FRIDGE_WORD>((op << 4) | arg);
    io(sys, IOUT, port);
}

int main()
{
    FRIDGE_CPU cpu{};
    FRIDGE_TIMER timer{}, stopwatch{};
    FRIDGE_SYSTEM sys{};
    sys.cpu = &cpu;
    sys.timer = &timer;
    sys.stopwatch = &stopwatch;
    FRIDGE_cpu_reset(&cpu);

    cpu.inte = FRIDGE_CPU_INTERRUPTS_ENABLED;
    FRIDGE_sys_timer_tick(&sys, 1000);
    require(timer.counter == 0 && stopwatch.counter == 0, "Timers should start disabled");
    require(cpu.inte == FRIDGE_CPU_INTERRUPTS_ENABLED, "A disabled timer should leave interrupts alone");

    const FRIDGE_WORD timer_port = FRIDGE_DEV_TIMER_ID;
    const FRIDGE_WORD stopwatch_port = FRIDGE_DEV_STOPWATCH_ID;
    command(sys, stopwatch_port, FRIDGE_TIMER_SET_INTERVAL_NIB3, 4);
    command(sys, stopwatch_port, FRIDGE_TIMER_SET_INTERVAL_NIB2, 3);
    command(sys, stopwatch_port, FRIDGE_TIMER_SET_INTERVAL_NIB1, 2);
    command(sys, stopwatch_port, FRIDGE_TIMER_SET_INTERVAL_NIB0, 0xf);
    command(sys, stopwatch_port, FRIDGE_TIMER_SET_INTERVAL_NIB0, 1);
    require(stopwatch.interval == 0x4321 && timer.interval == 0, "Interval writes should replace only the requested nibble");

    FRIDGE_sys_timer_tick(&sys, 0x1234);
    command(sys, stopwatch_port, FRIDGE_TIMER_GET_COUNTER_HIGH, 7);
    require(io(sys, IIN, stopwatch_port) == 0x12, "Wrong high byte");
    command(sys, stopwatch_port, FRIDGE_TIMER_GET_COUNTER_LOW);
    require(io(sys, IIN, stopwatch_port) == 0x34, "Wrong low byte");
    command(sys, stopwatch_port, FRIDGE_TIMER_NOP, 0xf);
    FRIDGE_sys_timer_tick(&sys, 1);
    require(io(sys, IIN, stopwatch_port) == 0x35, "IN should read the live selected byte");

    command(sys, stopwatch_port, FRIDGE_TIMER_RESET);
    require(stopwatch.counter == 0 && stopwatch.interval == 0x4321, "Reset should only clear the counter");
    FRIDGE_sys_timer_tick(&sys, 0x4321 * 2u);
    require(stopwatch.counter == 0x4321, "Exact multiples should end at the interval");
    FRIDGE_sys_timer_tick(&sys, 3);
    require(stopwatch.counter == 3 && cpu.inte == FRIDGE_CPU_INTERRUPTS_ENABLED,
            "Stopwatch rollover should keep the remainder without issuing an IRQ");

    command(sys, timer_port, FRIDGE_TIMER_SET_INTERVAL_NIB0, 5);
    FRIDGE_sys_timer_tick(&sys, 5);
    require(timer.counter == 5 && stopwatch.counter == 8, "Each device should count from its own state");
    require(cpu.inte == FRIDGE_CPU_INTERRUPTS_ENABLED, "Equality should leave interrupts alone");

    cpu.state = FRIDGE_CPU_HALTED;
    FRIDGE_sys_timer_tick(&sys, 12);
    require(timer.counter == 2, "Timer should keep the remainder across several periods");
    require(cpu.PC == FRIDGE_IRQ_SYS_TIMER && cpu.state == FRIDGE_CPU_ACTIVE &&
            cpu.inte == FRIDGE_CPU_INTERRUPTS_DISABLED, "Rollover should wake the CPU at the timer IRQ vector");

    FRIDGE_sys_timer_tick(&sys, 5);
    require(timer.counter == 2 && cpu.PC == FRIDGE_IRQ_SYS_TIMER, "Timer should count with interrupts masked");
    command(sys, timer_port, FRIDGE_TIMER_SET_INTERVAL_NIB0, 0);
    FRIDGE_sys_timer_tick(&sys, 100);
    require(timer.counter == 2, "Setting interval to zero should stop counting");

    sys.timer = nullptr;
    command(sys, timer_port, FRIDGE_TIMER_RESET);
    require(io(sys, IIN, timer_port) == 0, "An absent timer should read as zero");
    FRIDGE_sys_timer_tick(&sys, 0xffffffffu);
    require(stopwatch.counter == (0xffffffffull + 125 - 1) % 0x4321 + 1,
            "Large elapsed times should retain the full remainder");
}
