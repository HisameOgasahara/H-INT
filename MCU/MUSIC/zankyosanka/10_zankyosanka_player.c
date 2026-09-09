#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "zankyosanka_score.h"

/*
 * Zankyo Zanka - one-buzzer TC275 player
 *
 * Easy Module Shield V1 + ShieldBuddy TC275 mapping:
 *   SW1    D2 -> P02.0 : PLAY
 *   SW2    D3 -> P02.1 : STOP by ERU interrupt
 *   Buzzer D5 -> P02.3 -> GTM TOUT3 -> TOM0_CH11
 *
 * Cpu0_Main.c owns core0_main() and cpuSyncEvent.
 */

#define PORT2_BASE_ADDRESS          (0xF003A200U)
#define REG_PORT2_IOCR0             (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10U))
#define REG_PORT2_INPUT             (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24U))
#define PC0_SHIFT                   3U
#define PC1_SHIFT                   11U
#define PC3_SHIFT                   27U
#define SW1_PIN                     0U

#define SCU_BASE_ADDRESS            (0xF0036000U)
#define REG_SCU_WDTCPU0CON0         (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100U))
#define REG_SCU_EICR1               (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x214U))
#define REG_SCU_IGCR0               (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22CU))
#define LCK                         1U
#define ENDINIT                     0U

#define EXIS0_SHIFT                 4U
#define FEN0_BIT                    8U
#define EIEN0_BIT                   11U
#define INP0_SHIFT                  12U
#define IGP0_SHIFT                  14U

#define SRC_BASE_ADDRESS            (0xF0038000U)
#define REG_SRC_SCU_ERU0            (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0xCD4U))
#define SRE_BIT                     10U
#define TOS_SHIFT                   11U
#define SW2_ISR_PRIORITY            0x0FU

#define STM0_BASE_ADDRESS           (0xF0000000U)
#define REG_STM0_TIM0               (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))
#define STM0_FREQUENCY_HZ           100000000U

#define GTM_BASE_ADDRESS            (0xF0100000U)
#define REG_GTM_CLC                 (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD00U))
#define REG_GTM_TOUTSEL0            (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x9FD30U))
#define GTM_DISS                    1U
#define GTM_DISR                    0U
#define SEL3_SHIFT                  6U
#define REG_GTM_CMU_CLK_EN          (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00300U))
#define REG_GTM_CMU_FXCLK_CTRL      (*(volatile unsigned int *)(GTM_BASE_ADDRESS + 0x00344U))
#define EN_FXCLK_SHIFT              22U
#define FXCLK_SEL_SHIFT             0U
#define BUZZER_CLOCK_HZ             6250000U

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

volatile unsigned int g_zankyo_stop_requested = 0U;
volatile unsigned int g_zankyo_current_event = 0U;

static void init_ports(void);
static void init_sw2_interrupt(void);
static void init_buzzer_pwm(void);
static unsigned int read_sw1(void);
static void buzzer_set_frequency(unsigned int frequency_hz);
static void buzzer_off(void);
static unsigned int wait_midi_ticks_abortable(unsigned int midi_ticks);
static void play_zankyosanka(void);

__interrupt(SW2_ISR_PRIORITY) __vector_table(0)
void ISR_SW2_ZANKYO_STOP(void)
{
    g_zankyo_stop_requested = 1U;
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOM0_CH11_CM1 = 0U;
}

void zankyosanka_run(void)
{
    unsigned int previous_sw1 = 1U;
    unsigned int current_sw1;

    IfxCpu_enableInterrupts();
    init_ports();
    init_buzzer_pwm();
    init_sw2_interrupt();
    buzzer_off();

    while (1)
    {
        current_sw1 = read_sw1();

        if ((previous_sw1 != 0U) && (current_sw1 == 0U))
        {
            g_zankyo_stop_requested = 0U;
            g_zankyo_current_event = 0U;
            play_zankyosanka();
            buzzer_off();

            while (read_sw1() == 0U)
            {
                if (g_zankyo_stop_requested != 0U)
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
}

static void init_ports(void)
{
    REG_PORT2_IOCR0 &= ~((0x1FU << PC0_SHIFT) | (0x1FU << PC1_SHIFT));
    REG_PORT2_IOCR0 |=  ((0x02U << PC0_SHIFT) | (0x02U << PC1_SHIFT));
    REG_PORT2_IOCR0 &= ~(0x1FU << PC3_SHIFT);
    REG_PORT2_IOCR0 |=  (0x11U << PC3_SHIFT);
}

static unsigned int read_sw1(void)
{
    return ((REG_PORT2_INPUT >> SW1_PIN) & 0x1U);
}

static void init_sw2_interrupt(void)
{
    REG_SCU_EICR1 &= ~(0x7U << EXIS0_SHIFT);
    REG_SCU_EICR1 |=  (0x1U << EXIS0_SHIFT);
    REG_SCU_EICR1 |= (1U << FEN0_BIT);
    REG_SCU_EICR1 |= (1U << EIEN0_BIT);
    REG_SCU_EICR1 &= ~(0x7U << INP0_SHIFT);

    REG_SCU_IGCR0 &= ~(0x3U << IGP0_SHIFT);
    REG_SCU_IGCR0 |=  (0x1U << IGP0_SHIFT);

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

    period = (BUZZER_CLOCK_HZ + (frequency_hz / 2U)) / frequency_hz;
    duty = period / 2U;

    REG_GTM_TOM0_CH11_SR0 = period;
    REG_GTM_TOM0_CH11_SR1 = duty;
    REG_GTM_TOM0_CH11_CM0 = period;
    REG_GTM_TOM0_CH11_CM1 = duty;
}

static void buzzer_off(void)
{
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOM0_CH11_CM1 = 0U;
}

static unsigned int wait_midi_ticks_abortable(unsigned int midi_ticks)
{
    unsigned int start;
    unsigned int target_ticks;
    unsigned long long numerator;
    unsigned long long divisor;

    numerator = (unsigned long long)STM0_FREQUENCY_HZ * 60ULL * (unsigned long long)midi_ticks;
    divisor = (unsigned long long)ZANKYO_BPM * (unsigned long long)ZANKYO_TICKS_PER_QUARTER;
    target_ticks = (unsigned int)(numerator / divisor);
    start = REG_STM0_TIM0;

    while ((unsigned int)(REG_STM0_TIM0 - start) < target_ticks)
    {
        if (g_zankyo_stop_requested != 0U)
        {
            buzzer_off();
            return 0U;
        }
    }

    return 1U;
}

static void play_zankyosanka(void)
{
    unsigned int i;

    for (i = 0U; i < ZANKYOSANKA_SCORE_EVENT_COUNT; i++)
    {
        const ZankyoEvent *event = &g_zankyosanka_score[i];
        g_zankyo_current_event = i;

        if (g_zankyo_stop_requested != 0U)
        {
            break;
        }

        if (event->frequency_hz == ZANKYO_REST)
        {
            buzzer_off();
        }
        else
        {
            buzzer_set_frequency((unsigned int)event->frequency_hz);
        }

        if (wait_midi_ticks_abortable((unsigned int)event->midi_ticks) == 0U)
        {
            break;
        }
    }

    buzzer_off();
}

static void init_buzzer_pwm(void)
{
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U) { }

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) & ~(1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U) { }

    REG_GTM_CLC &= ~(1U << GTM_DISR);

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U) { }

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U) { }

    while ((REG_GTM_CLC & (1U << GTM_DISS)) != 0U) { }

    REG_GTM_CMU_FXCLK_CTRL &= ~(0xFU << FXCLK_SEL_SHIFT);
    REG_GTM_CMU_CLK_EN |= (0x2U << EN_FXCLK_SHIFT);

    REG_GTM_TOM0_TGC1_GLB_CTRL |= (0x2U << UPEN_CTRL3_SHIFT);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= (0x2U << FUPD_CTRL3_SHIFT);
    REG_GTM_TOM0_TGC1_FUPD_CTRL |= (0x2U << RSTCN0_CH3_SHIFT);
    REG_GTM_TOM0_TGC1_ENDIS_CTRL |= (0x2U << ENDIS_CTRL3_SHIFT);
    REG_GTM_TOM0_TGC1_OUTEN_CTRL |= (0x2U << OUTEN_CTRL3_SHIFT);

    REG_GTM_TOM0_CH11_CTRL |= (1U << SL_BIT);
    REG_GTM_TOM0_CH11_CTRL &= ~(0x7U << CLK_SRC_SR_SHIFT);
    REG_GTM_TOM0_CH11_CTRL |=  (1U << CLK_SRC_SR_SHIFT);

    REG_GTM_TOM0_CH11_SR0 = (BUZZER_CLOCK_HZ / 440U);
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOUTSEL0 &= ~(0x3U << SEL3_SHIFT);
    REG_GTM_TOM0_TGC1_GLB_CTRL |= (1U << HOST_TRIG_BIT);
}
