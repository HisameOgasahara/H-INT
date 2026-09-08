#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/* RGB LED Registers */
#define PORT2_BASE_ADDRESS     (0xF003A200)
#define PORT2_IOCR4            (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x14))
#define PORT2_OUTPUT           (*(volatile unsigned int *)(PORT2_BASE_ADDRESS))

#define PORT10_BASE_ADDRESS    (0xF003B000)
#define PORT10_IOCR0           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_IOCR4           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x14))
#define PORT10_OUTPUT          (*(volatile unsigned int *)(PORT10_BASE_ADDRESS))

/* Field bit positions of RGB LED Registers */
#define PC3                    27
#define PC5                    11
#define PC7                    27
#define P3                     3
#define P5                     5
#define P7                     7

/* Versatile Analog-to-Digital Converter Registers */
#define VADC_BASE_ADDRESS      (0xF0020000)

#define VADC_CLC               (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x000))
#define VADC_GLOBCFG           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x080))
#define VADC_G4ARBCFG          (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1480))
#define VADC_G4ARBPR           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1484))
#define VADC_G4ICLASS0         (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x14A0))
#define VADC_G4QMR0            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1504))
#define VADC_G4QINR0           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1510))
#define VADC_G4CHCTR7          (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x161C))
#define VADC_G4RES1            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1704))

/* Field bit positions of VADC Registers */
#define DISS                   1
#define DISR                   0

#define ANONC                  0
#define ASEN0                  24
#define CSM0                   3
#define PRIO0                  0

#define CMS                    8
#define STCS                   0

#define FLUSH                  10
#define TREV                   9
#define ENGT                   0
#define RF                     5
#define REQCHNR                0

#define RESPOS                 21
#define RESREG                 16
#define ICLSEL                 0

#define VF                     31
#define RESULT                 0

/* System Control Unit Registers */
#define SCU_BASE_ADDRESS       (0xF0036000)
#define SCU_WDTCPU0CON0        (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100))

/* Field bit positions of SCU Registers */
#define LCK                    1
#define ENDINIT                0

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_RGBLED(void);
void init_VADC(void);
void VADC_start_conversion(void);
unsigned int VADC_read_result(void);

void core0_main(void)
{
    unsigned int adc_result;
    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_RGBLED();
    init_VADC();

    while(1)
    {
        /* Start a single VADC conversion. */
        VADC_start_conversion();

        /* Read the converted ADC value. */
        adc_result = VADC_read_result();

        if (adc_result >= 3096)
        {
            PORT2_OUTPUT |= 0x1 << P7;     /* Turn on the red LED. */
            PORT10_OUTPUT |= 0x1 << P5;    /* Turn on the green LED. */
            PORT10_OUTPUT |= 0x1 << P3;    /* Turn on the blue LED. */
        }
        else if (adc_result >= 2048)
        {
            PORT2_OUTPUT &= 0x0 << P7;     /* Turn off the red LED. */
            PORT10_OUTPUT |= 0x1 << P5;    /* Turn on the green LED. */
            PORT10_OUTPUT |= 0x1 << P3;    /* Turn off the blue LED. */
        }
        else if (adc_result >= 1024)
        {
            PORT2_OUTPUT &= 0x0 << P7;     /* Turn off the red LED. */
            PORT10_OUTPUT &= 0x0 << P5;    /* Turn off the green LED. */
            PORT10_OUTPUT |= 0x1 << P3;    /* Turn off the blue LED. */
        }
        else
        {
            PORT2_OUTPUT &= 0x0 << P7;     /* Turn off the red LED. */
            PORT10_OUTPUT &= 0x0 << P5;    /* Turn off the green LED. */
            PORT10_OUTPUT &= 0x0 << P3;    /* Turn off the blue LED. */
        }
    }
}

void init_RGBLED(void)
{
    // Reset PC7 in Port 2 IOCR4 register
    PORT2_IOCR4 &= ~((0x1F) << PC7);

    // Set PC7 to push-pull mode in Port 2 IOCR4 register
    PORT2_IOCR4 |= ((0x10) << PC7);

    // Reset PC5 in Port 10 IOCR4 register
    PORT10_IOCR4 &= ~((0x1F) << PC5);

    // Set PC5 to push-pull mode in Port 10 IOCR4 register
    PORT10_IOCR4 |= ((0x10) << PC5);

    // Reset PC3 in Port 10 IOCR0 register
    PORT10_IOCR0 &= ~((0x1F) << PC3);

    // Set PC3 to push-pull mode in Port 10 IOCR0 register
    PORT10_IOCR0 |= ((0x10) << PC3);
}

void init_VADC(void)
{
    // Password Access to unlock CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) & ~(1 << LCK)) | (1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) != 0);  // Wait until to unlock CPU0 WDT Control Register 0

    // Modify Access to clear the ENDINIT bit in CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) | (1 << LCK)) & ~(1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) == 0);  // Wait until to clear the ENDINIT bit in CPU0 WDT Control Register 0

    /* Enable the VADC module clock. */
    VADC_CLC &= ~(1 << DISR);

    // Password Access to unlock CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) & ~(1 << LCK)) | (1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) != 0);  // Wait until to unlock CPU0 WDT Control Register 0

    // Modify Access to set the ENDINIT bit in CPU0 WDT Control Register 0
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFC) | (1 << LCK)) | (1 << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1 << LCK)) == 0);  // Wait until to clear the ENDINIT bit in CPU0 WDT Control Register 0

    /* Wait until the VADC module is enabled. */
    while ((VADC_CLC & (1u << DISS)) != 0u)
    {
    }

    // Set Request Source 0 to the highest priority.
    VADC_G4ARBPR |= ((0x3) << PRIO0);

    // Select Wait-for-Start conversion mode.
    VADC_G4ARBPR &= ~(1 << CSM0);

    // Enable Arbitration Source Input 0.
    VADC_G4ARBPR |= (1 << ASEN0);

    /* Clear the Queue gate-control field. */
    VADC_G4QMR0 &= ~((0x3) << ENGT);
    /* Enable Queue conversion requests. */
    VADC_G4QMR0 |= ((0x1) << ENGT);

    /* Remove all existing queue entries. */
    VADC_G4QMR0 |= (1 << FLUSH);

    /* Set the Group 4 converter to normal operation. */
    VADC_G4ARBCFG |= ((0x3) << ANONC);

    /* Select 12-bit standard conversion mode. */
    VADC_G4ICLASS0 &= ~((0x7) << CMS);

    /* Store Channel 7 results in right-aligned format. */
    VADC_G4CHCTR7 |= (1 << RESPOS);

    /* Clear the result-register selection field. */
    VADC_G4CHCTR7 &= ~((0xF) << RESREG);

    /* Store Channel 7 results in Group Result Register 1. */
    VADC_G4CHCTR7 |= (1 << RESREG);

    /* Select Group-specific Input Class 0. */
    VADC_G4CHCTR7 &= ~((0x3) << ICLSEL);
}

void VADC_start_conversion(void)
{
    /* Reset the queue channel selection bits. */
    VADC_G4QINR0 &= ~0x1F;

    /* Select VADC Group 4 Channel 7 for conversion. */
    VADC_G4QINR0 |= 0x07;

    /* Configure the queue entry for a single conversion. */
    VADC_G4QINR0 &= ~((0x1 << RF));

    /* Issue a software trigger to execute the queue request. */
    VADC_G4QMR0 |= (0x1 << TREV);
}

unsigned int VADC_read_result(void)
{
    unsigned int result;

    /* Poll until the result-valid flag is set. */
    while ((VADC_G4RES1 & (0x1 << VF)) == 0u)
    {
    }

    /* Extract the conversion data from result register 1. */
    result = VADC_G4RES1 & (0xFFFF << RESULT);

    /* Return the converted ADC value to the caller. */
    return result;
}
