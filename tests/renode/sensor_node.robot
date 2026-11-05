*** Comments ***
Firmware tests on an emulated NUCLEO-F446RE (Renode).
Tests tagged `todo` describe features that are not written yet; CI skips them.
When a feature lands, remove its `todo` tag so CI starts enforcing it.
I2C1 uses renode/STM32F4_I2C_AckModel.cs, a patched copy of Renode's STM32F1_I2C model
(see README Status); it is loaded with `include` before the platform file.
The expected UART strings are a proposed format; change them together with the firmware.

*** Settings ***
Test Setup        Create Machine
Test Teardown     Test Teardown

*** Variables ***
${ROOT}           ${CURDIR}/../..
${ELF}            ${ROOT}/build/firmware.elf
${REPL}           ${ROOT}/renode/stm32f446_nucleo.repl
${UART}           sysbus.usart2
${LED}            sysbus.gpioPortA.UserLED

*** Keywords ***
Create Machine
    Execute Command    using sysbus
    Execute Command    include @${ROOT}/renode/STM32F4_I2C_AckModel.cs
    Execute Command    mach create "nucleo-f446re"
    Execute Command    machine LoadPlatformDescription @${REPL}
    Execute Command    sysbus LoadELF @${ELF}

*** Test Cases ***
Vector Table Sets Initial SP And Reset Vector
    [Documentation]    At reset the CPU loads SP from 0x08000000 and PC from 0x08000004.
    ...                The reset vector must be Reset_Handler with the Thumb bit (bit 0) set.
    ${sp}=             Execute Command    sysbus ReadDoubleWord 0x08000000
    Should Be Equal As Integers    ${sp}    0x20020000
    ${vector}=         Execute Command    sysbus ReadDoubleWord 0x08000004
    ${reset}=          Execute Command    sysbus GetSymbolAddress "Reset_Handler"
    Should Be Equal As Integers    ${vector}    ${{ (int($reset.strip(), 0) & ~1) | 1 }}

User LED Blinks
    [Tags]             week1
    Create LED Tester  ${LED}    defaultTimeout=2
    Start Emulation
    Assert LED State   true
    Assert LED State   false

UART Prints Boot Banner
    [Tags]             week2
    Create Terminal Tester    ${UART}    timeout=2
    Start Emulation
    Wait For Line On Uart     sensor-node boot

BME280 Reading Reaches UART
    [Tags]             week3
    Execute Command    i2c1ack.bme280 Temperature 25.0
    Execute Command    i2c1ack.bme280 Humidity 50.0
    Execute Command    i2c1ack.bme280 Pressure 1013.25
    Create Terminal Tester    ${UART}    timeout=5
    Start Emulation
    Wait For Line On Uart     temperature: 25.00
    Execute Command    i2c1ack.bme280 Temperature 30.5
    Wait For Line On Uart     temperature: 30.50

UART Receive Interrupt Echoes A Line
    [Tags]             week2
    [Documentation]    Bytes arrive through the USART2 RXNE interrupt and a ring buffer.
    Create Terminal Tester    ${UART}    timeout=5
    Start Emulation
    Wait For Line On Uart     sensor-node boot
    # The firmware does not echo characters, so do not wait for an echo.
    Write Line To Uart        hello    waitForEcho=false
    Wait For Line On Uart     echo: hello

UART Temp Command Prints A Reading
    [Tags]             week2
    Execute Command    i2c1ack.bme280 Temperature 21.5
    Create Terminal Tester    ${UART}    timeout=5
    Start Emulation
    Wait For Line On Uart     temperature: 21.50
    Execute Command    i2c1ack.bme280 Temperature 22.75
    Write Line To Uart        temp    waitForEcho=false
    # The periodic reading comes once per second; 0.2 s of virtual time
    # means this line can only be the reply to the command.
    Wait For Line On Uart     temperature: 22.75    timeout=0.2

Missing Display Is Reported
    [Tags]             week4
    [Documentation]    The board file has no SSD1306 (Renode has no model for it), so the
    ...                address byte is NACKed and the firmware carries on without a display.
    Execute Command    i2c1ack.bme280 Temperature 25.0
    Create Terminal Tester    ${UART}    timeout=5
    Start Emulation
    Wait For Line On Uart     ssd1306 not found
    Wait For Line On Uart     temperature: 25.00
