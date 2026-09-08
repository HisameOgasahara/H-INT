#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"

/*
 * 10 - Do-Re-Mi-Fa-Sol-La-Ti-Do buzzer exercise
 *
 * Hardware mapping (Easy Module Shield V1 + ShieldBuddy TC275):
 *   SW1 : D2 -> P02.0
 *   SW2 : D3 -> P02.1 -> SCU REQ14 -> ERS2 / ETL2
 *   Buzzer : D5 -> P02.3 -> GTM TOUT3 -> TOM0 Channel 11
 *
 * Operation:
 *   1) Press SW1: play C4-D4-E4-F4-G4-A4-B4-C5 in order.
 *   2) Press SW2 at any time: ERU interrupt immediately silences the buzzer
 *      and aborts the remaining notes.
 *   3) Release SW1 and press it again to start the scale again.
 *
 * The PWM path follows the preceding PWM/buzzer exercise in this repository.
 * TOM0_CH11 uses CMU_FXCLK1 = 6.25 MHz.  The note period is therefore
 *     period_count = 6,250,000 / note_frequency_hz
 * with approximately 50% duty cycle.
 */

/* ============================ PORT 2 ============================ */
#define PORT2_BASE_ADDRESS          (0xF003A200U)
#define REG_PORT2_IOCR0             (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10U))
#define REG_PORT2_INPUT             (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24U))

#define PC0_SHIFT                   3U   /* P02.0 = SW1 */
#define PC1_SHIFT                   11U  /* P02.1 = SW2 */
#define PC3_SHIFT                   27U  /* P02.3 = buzzer */
#define SW1_PIN                     0U

/* ============================== SCU ============================= */
#define SCU_BASE_ADDRESS            (0xF0036000U)
#define REG_SCU_WDTCPU0CON0         (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100U))
#define REG_SCU_EICR1               (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x214U))
#define REG_SCU_IGCR0               (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22CU))

#define LCK                         1U
#define ENDINIT                     0U

/*
 * SW2: P02.1 -> REQ14 -> ERS2 input In22.
 * EICR1 lower half controls input channel 2 (ERS2 / ETL2).
 */
#define EXIS0_SHIFT                 4U
#define FEN0_BIT                    9U
#define EIEN0_BIT                   11U
#define INP0_SHIFT                  12U
#define IGP0_SHIFT                  14U

/* ======================= Interrupt Router ======================= */
#define SRC_BASE_ADDRESS            (0xF0038000U)
#define REG_SRC_SCU_ERU0            (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0xCD4U))

#define SRE_BIT                     10U
#define TOS_SHIFT                   11U
#define SW2_ISR_PRIORITY            0x10U

/* ============================== STM ============================= */
#define STM0_BASE_ADDRESS           (0xF0000000U)
#define REG_STM0_TIM0               (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))
#define STM0_FREQUENCY_HZ           100000000U

/* ============================== GTM ============================= */
#define GTM_BASE_ADDRESS            (0xF0100000U)
#define REG_GTM_CLC                 (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00U))
#define REG_GTM_TOUTSEL0            (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD30U))

#define GTM_DISS                    1U
#define GTM_DISR                    0U
#define SEL3_SHIFT                  6U

/* GTM CMU */
#define REG_GTM_CMU_CLK_EN          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300U))
#define REG_GTM_CMU_FXCLK_CTRL      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344U))

#define EN_FXCLK_SHIFT              22U
#define FXCLK_SEL_SHIFT             0U
#define BUZZER_CLOCK_HZ             6250000U

/* TOM0 Channel 11 -> TGC1 local channel 3 */
#define REG_GTM_TOM0_TGC1_GLB_CTRL   (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08230U))
#define REG_GTM_TOM0_TGC1_FUPD_CTRL  (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08238U))
#define REG_GTM_TOM0_TGC1_ENDIS_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08270U))
#define REG_GTM_TOM0_TGC1_OUTEN_CTRL (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x08278U))

#define REG_GTM_TOM0_CH11_CTRL        (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C0U))
#define REG_GTM_TOM0_CH11_SR0         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C4U))
#define REG_GTM_TOM0_CH11_SR1         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082C8U))
#define REG_GTM_TOM0_CH11_CM0         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082CCU))
#define REG_GTM_TOM0_CH11_CM1         (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x082D0U))

#define UPEN_CTRL3_SHIFT            22U
#define HOST_TRIG_BIT               0U
#define ENDIS_CTRL3_SHIFT           6U
#define OUTEN_CTRL3_SHIFT           6U
#define FUPD_CTRL3_SHIFT            6U
#define RSTCN0_CH3_SHIFT            22U
#define CLK_SRC_SR_SHIFT            12U
#define SL_BIT                      11U

/* ======================== Melody settings ======================= */
#define NOTE_DURATION_MS            350U
#define NOTE_GAP_MS                 70U
#define NOTE_COUNT                  8U

/* Equal-tempered note frequencies rounded to integer hertz. */
static const unsigned int g_note_hz[NOTE_COUNT] =
{
    262U,  /* C4 : Do  */
    294U,  /* D4 : Re  */
    330U,  /* E4 : Mi  */
    349U,  /* F4 : Fa  */
    392U,  /* G4 : Sol */
    440U,  /* A4 : La  */
    494U,  /* B4 : Ti  */
    523U   /* C5 : Do  */
};

IfxCpu_syncEvent cpuSyncEvent = 0;

/* Set by SW2 ISR, read by the melody loop. */
volatile unsigned int g_stop_requested = 0U;

static void init_ports(void);
static void init_sw2_interrupt(void);
static void init_buzzer_pwm(void);
static unsigned int read_sw1(void);
static void buzzer_set_frequency(unsigned int frequency_hz);
static void buzzer_off(void);
static unsigned int wait_ms_abortable(unsigned int milliseconds);
static void play_scale(void);

/*
 * SW2 interrupt: silence immediately and request melody abort.
 * Priority 0x10 is programmed into SRC_SCU_ERU0 below.
 */
__interrupt(SW2_ISR_PRIORITY) __vector_table(0)
void ISR_SW2_STOP(void)
{
    g_stop_requested = 1U;

    /* Immediate 0% duty. Keep shadow/action duty registers identical. */
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOM0_CH11_CM1 = 0U;
}

int core0_main(void)
{
    unsigned int previous_sw1 = 1U;
    unsigned int current_sw1;

    IfxCpu_enableInterrupts();

    /*
     * !!WATCHDOG AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable and service them periodically when required by the application.
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());

    IfxCpu_emitEvent(&cpuSyncEvent);
    IfxCpu_waitEvent(&cpuSyncEvent, 1);

    init_ports();
    init_buzzer_pwm();
    init_sw2_interrupt();
    buzzer_off();

    while (1)
    {
        current_sw1 = read_sw1();

        /* Active-low SW1: start only on a released(1) -> pressed(0) edge. */
        if ((previous_sw1 != 0U) && (current_sw1 == 0U))
        {
            g_stop_requested = 0U;
            play_scale();
            buzzer_off();

            /* Do not retrigger while SW1 is still held down. */
            while (read_sw1() == 0U)
            {
                if (g_stop_requested != 0U)
                {
                    buzzer_off();
                }
            }

            previous_sw1 = 1U;
        }
        else
        {
            previous_sw1 = current_sw1;
        }
    }

    return 1;
}

static void init_ports(void)
{
    /* SW1 P02.0 and SW2 P02.1: general-purpose input with pull-up. */
    REG_PORT2_IOCR0 &= ~((0x1FU << PC0_SHIFT) | (0x1FU << PC1_SHIFT));
    REG_PORT2_IOCR0 |=  ((0x02U << PC0_SHIFT) | (0x02U << PC1_SHIFT));

    /* Buzzer P02.3: alternate output function 1 = GTM TOUT3. */
    REG_PORT2_IOCR0 &= ~(0x1FU << PC3_SHIFT);
    REG_PORT2_IOCR0 |=  (0x11U << PC3_SHIFT);
}

static unsigned int read_sw1(void)
{
    return ((REG_PORT2_INPUT >> SW1_PIN) & 0x1U);
}

static void init_sw2_interrupt(void)
{
    /*
     * Official TC27x ERU path:
     * P02.1 -> REQ14 -> ERS2 input In22.
     * Therefore EXIS0 (channel 2) selects input number 2 = 010b.
     */
    REG_SCU_EICR1 &= ~(0x7U << EXIS0_SHIFT);
    REG_SCU_EICR1 |=  (0x2U << EXIS0_SHIFT);

    /* Pull-up switch press is a high -> low transition: falling-edge detect. */
    REG_SCU_EICR1 |= (1U << FEN0_BIT);

    /* Enable trigger event generation for ETL2. */
    REG_SCU_EICR1 |= (1U << EIEN0_BIT);

    /* Route ETL2 trigger to OGU0. */
    REG_SCU_EICR1 &= ~(0x7U << INP0_SHIFT);

    /* OGU0 generates IOUT0 whenever its trigger event arrives. */
    REG_SCU_IGCR0 &= ~(0x3U << IGP0_SHIFT);
    REG_SCU_IGCR0 |=  (0x1U << IGP0_SHIFT);

    /* Interrupt priority and CPU0 routing. */
    REG_SRC_SCU_ERU0 &= ~0xFFU;
    REG_SRC_SCU_ERU0 |= SW2_ISR_PRIORITY;
    REG_SRC_SCU_ERU0 |= (1U << SRE_BIT);
    REG_SRC_SCU_ERU0 &= ~(0x3U << TOS_SHIFT);
}

static void buzzer_set_frequency(unsigned int frequency_hz)
{
    unsigned int period;
    unsigned int duty;

    if (frequency_hz == 0U)
    {
        buzzer_off();
        return;
    }

    /* Rounded integer period count for CMU_FXCLK1 = 6.25 MHz. */
    period = (BUZZER_CLOCK_HZ + (frequency_hz / 2U)) / frequency_hz;
    duty = period / 2U;

    /*
     * Keep both shadow and action registers identical.
     * This changes the note immediately while preserving the next shadow update.
     */
    REG_GTM_TOM0_CH11_SR0 = period;
    REG_GTM_TOM0_CH11_SR1 = duty;
    REG_GTM_TOM0_CH11_CM0 = period;
    REG_GTM_TOM0_CH11_CM1 = duty;
}

static void buzzer_off(void)
{
    /* TC27x TOM: CM1 = 0 gives 0% duty cycle. */
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOM0_CH11_CM1 = 0U;
}

static unsigned int wait_ms_abortable(unsigned int milliseconds)
{
    unsigned int start;
    unsigned int ticks;

    start = REG_STM0_TIM0;
    ticks = (STM0_FREQUENCY_HZ / 1000U) * milliseconds;

    while ((unsigned int)(REG_STM0_TIM0 - start) < ticks)
    {
        if (g_stop_requested != 0U)
        {
            buzzer_off();
            return 0U;
        }
    }

    return 1U;
}

static void play_scale(void)
{
    unsigned int i;

    for (i = 0U; i < NOTE_COUNT; i++)
    {
        if (g_stop_requested != 0U)
        {
            break;
        }

        buzzer_set_frequency(g_note_hz[i]);

        if (wait_ms_abortable(NOTE_DURATION_MS) == 0U)
        {
            break;
        }

        buzzer_off();

        if (wait_ms_abortable(NOTE_GAP_MS) == 0U)
        {
            break;
        }
    }

    buzzer_off();
}

static void init_buzzer_pwm(void)
{
    /* Password access: unlock CPU0 WDT Control Register 0. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U)
    {
    }

    /* Modify access: clear ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) & ~(1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U)
    {
    }

    /* Enable GTM module. */
    REG_GTM_CLC &= ~(1U << GTM_DISR);

    /* Password access: unlock CPU0 WDT Control Register 0. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U)
    {
    }

    /* Modify access: set ENDINIT. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U)
    {
    }

    while ((REG_GTM_CLC & (1U << GTM_DISS)) != 0U)
    {
    }

    /* Enable fixed clocks and select CMU_GCLK_EN as FXCLK input. */
    REG_GTM_CMU_FXCLK_CTRL &= ~(0xFU << FXCLK_SEL_SHIFT);
    REG_GTM_CMU_CLK_EN |= (0x2U << EN_FXCLK_SHIFT);

    /* TOM0 channel 11 = local channel 3 in TGC1. */
    REG_GTM_TOM0_TGC1_GLB_CTRL |= (0x2U << UPEN_CTRL3_SHIFT);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= (0x2U << FUPD_CTRL3_SHIFT);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= (0x2U << RSTCN0_CH3_SHIFT);

    REG_GTM_TOM0_TGC1_ENDIS_CTRL |= (0x2U << ENDIS_CTRL3_SHIFT);
    REG_GTM_TOM0_TGC1_OUTEN_CTRL |= (0x2U << OUTEN_CTRL3_SHIFT);

    /* Active level high and CMU_FXCLK1 as channel clock. */
    REG_GTM_TOM0_CH11_CTRL |= (1U << SL_BIT);
    REG_GTM_TOM0_CH11_CTRL &= ~(0x7U << CLK_SRC_SR_SHIFT);
    REG_GTM_TOM0_CH11_CTRL |=  (1U << CLK_SRC_SR_SHIFT);

    /* Start with C4 period but 0% duty (silent). */
    REG_GTM_TOM0_CH11_SR0 = (BUZZER_CLOCK_HZ / 262U);
    REG_GTM_TOM0_CH11_SR1 = 0U;

    /* TOUT3 <- TOM0 channel 11. */
    REG_GTM_TOUTSEL0 &= ~(0x3U << SEL3_SHIFT);

    /* Apply channel/output/shadow settings. */
    REG_GTM_TOM0_TGC1_GLB_CTRL |= (1U << HOST_TRIG_BIT);
}
