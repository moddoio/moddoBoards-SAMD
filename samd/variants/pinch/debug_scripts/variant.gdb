#
#  pinch board GDB debug script
#  MCU: ATSAMD11D14A (Cortex-M0+)
#
#  Copyright (c) 2014-2015 Arduino LLC. All right reserved.
#
#  Modified 2026 by moddo, https://github.com/moddoio
#

# Define 'reset' command

define reset

info reg

break main

# End of 'reset' command
end

target remote | openocd -c "interface cmsis-dap" -c "set CHIPNAME at91samd11d14" -f target/at91samdXX.cfg -c "gdb_port pipe; log_output openocd.log"
