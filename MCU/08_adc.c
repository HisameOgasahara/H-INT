#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/* RGB LED Registers
 * ShieldBuddy mapping:
 * D9  -> P02.7 (Red)
 * D10 -> P10.5 (Green)
 * D11 -> P10.3 (Blue)
 */
#define PORT2_BASE_ADDRESS     (0xF003A200)
#define PORT2_IOCR4            (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x14))
#define PORT2_OMR              (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x04))

#define PORT10_BASE_ADDRESS    (0xF003B000)
#define PORT10_IOCR0           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x10))
#define PORT10_IOCR4           (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x14))
#define PORT10_OMR             (*(volatile unsigned int *)(PORT10_BASE_ADDRESS + 0x04))

/* IOCR field positions */
#define PC3                    27
#define PC5                    11
#define PC7                    27

/* OMR pin-set / pin-clear positions */
#define PS3                    3
#define PS5                    5
#define PS7                    7
#define PCL3                   19
#define PCL5                   21
#define PCL7                   23

/* Versatile Analog-to-Digital Converter Registers */
#define VADC_BASE_ADDRESS      (0xF0020000)

#define VADC_CLC               (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x000))
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

#define FLUSH                  10
#define TREV                   9
#define ENGT                   0

#define RESPOS                 21
#define RESREG                 16
#define ICLSEL                 0

#define VF                     31

/* System Control Unit Registers */
#define SCU_BASE_ADDRESS       (0xF0036000)
#define SCU_WDTCPU0CON0        (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100))

/* Field bit positions of SCU Registers */
#define LCK                    1
#define ENDINIT                0

IfxCpu_syncEvent cpuSyncEvent = 0;

void init_RGBLED(void);
void set_RGBLED(unsigned int red, unsigned int green, unsigned int blue);
void init_VADC(void);
void VADC_start_conversion(void);
unsigned int VADC_read_result(void);

void core0_main(void)
{
    unsigned int adc_result;

    IfxCpu_enableInterrupts();

    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_RGBLED();
    init_VADC();

    while (1)
    {
        VADC_start_conversion();
        adc_result = VADC_read_result();

        if (adc_result >= 3096u)
        {
            /* Red + Green + Blue = White */
            set_RGBLED(1u, 1u, 1u);
        }
        else if (adc_result >= 2048u)
        {
            /* Green + Blue = Cyan */
            set_RGBLED(0u, 1u, 1u);
        }
        else if (adc_result >= 1024u)
        {
            /* Blue */
            set_RGBLED(0u, 0u, 1u);
        }
        else
        {
            /* All off */
            set_RGBLED(0u, 0u, 0u);
        }
    }
}

void init_RGBLED(void)
{
    /* P02.7: push-pull general-purpose output */
    PORT2_IOCR4 &= ~((0x1Fu) << PC7);
    PORT2_IOCR4 |=  ((0x10u) << PC7);

    /* P10.5: push-pull general-purpose output */
    PORT10_IOCR4 &= ~((0x1Fu) << PC5);
    PORT10_IOCR4 |=  ((0x10u) << PC5);

    /* P10.3: push-pull general-purpose output */
    PORT10_IOCR0 &= ~((0x1Fu) << PC3);
    PORT10_IOCR0 |=  ((0x10u) << PC3);

    set_RGBLED(0u, 0u, 0u);
}

void set_RGBLED(unsigned int red, unsigned int green, unsigned int blue)
{
    unsigned int port10_omr = 0u;

    /*
     * TC27x OMR semantics:
     * PSx=1,PCLx=0 -> set OUT.Px
     * PSx=0,PCLx=1 -> clear OUT.Px
     * This changes only the selected pin and does not overwrite the whole OUT register.
     */
    port10_omr |= green ? (1u << PS5) : (1u << PCL5);
    port10_omr |= blue  ? (1u << PS3) : (1u << PCL3);
    PORT10_OMR = port10_omr;

    PORT2_OMR = red ? (1u << PS7) : (1u << PCL7);
}

void init_VADC(void)
{
    /* Password Access to unlock CPU0 WDT Control Register 0 */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u);

    /* Modify Access to clear ENDINIT */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) & ~(1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u);

    /* Enable VADC module clock */
    VADC_CLC &= ~(1u << DISR);

    /* Password Access to unlock CPU0 WDT Control Register 0 */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) & ~(1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) != 0u);

    /* Modify Access to set ENDINIT */
    SCU_WDTCPU0CON0 = ((SCU_WDTCPU0CON0 ^ 0xFCu) | (1u << LCK)) | (1u << ENDINIT);
    while ((SCU_WDTCPU0CON0 & (1u << LCK)) == 0u);

    while ((VADC_CLC & (1u << DISS)) != 0u);

    /* Request Source 0: highest priority, wait-for-start mode, arbitration slot enabled */
    VADC_G4ARBPR |= ((0x3u) << PRIO0);
    VADC_G4ARBPR &= ~(1u << CSM0);
    VADC_G4ARBPR |= (1u << ASEN0);

    /* Queue Source 0: enable requests, clear old queue entries */
    VADC_G4QMR0 &= ~((0x3u) << ENGT);
    VADC_G4QMR0 |=  ((0x1u) << ENGT);
    VADC_G4QMR0 |=  (1u << FLUSH);

    /* Group 4 converter: normal operation */
    VADC_G4ARBCFG &= ~(0x3u << ANONC);
    VADC_G4ARBCFG |=  (0x3u << ANONC);

    /* Input Class 0: 12-bit standard conversion */
    VADC_G4ICLASS0 &= ~((0x7u) << CMS);

    /* Channel 7 -> Group Result Register 1, right-aligned, Input Class 0 */
    VADC_G4CHCTR7 &= ~((0xFu) << RESREG);
    VADC_G4CHCTR7 |=  (1u << RESREG);
    VADC_G4CHCTR7 |=  (1u << RESPOS);
    VADC_G4CHCTR7 &= ~((0x3u) << ICLSEL);
}

void VADC_start_conversion(void)
{
    /*
     * IMPORTANT: G4QINR0 is write-only from the software point of view at this
     * shared address; reads return G4QBUR0 status. Therefore do not use a
     * read-modify-write expression here.
     *
     * REQCHNR = 7, RF = 0, ENSI = 0, EXTR = 0 -> one conversion of G4CH7.
     */
    VADC_G4QINR0 = 0x07u;

    /* Generate software trigger */
    VADC_G4QMR0 |= (1u << TREV);
}

unsigned int VADC_read_result(void)
{
    unsigned int result_register;

    /* Wait for a new conversion result */
    do
    {
        result_register = VADC_G4RES1;
    }
    while ((result_register & (1u << VF)) == 0u);

    /* 12-bit, right-aligned result: bits [11:0] */
    return (result_register & 0x0FFFu);
}
