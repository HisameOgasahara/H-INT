#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "pokemon_rby_score.h"

/*
 * Pokemon Red/Blue/Yellow opening - one-buzzer TC275 player
 *
 * Easy Module Shield V1 + ShieldBuddy TC275 mapping:
 *   SW1       D2 -> P02.0 : PLAY
 *   SW2       D3 -> P02.1 : STOP by ERU interrupt
 *   Buzzer    D5 -> P02.3 -> GTM TOUT3 -> TOM0_CH11
 *   Rotation  A0 -> ADCL.1 -> SAR4.7 / P32.3 : volume control
 *
 * Cpu0_Main.c keeps ownership of core0_main() and cpuSyncEvent.
 * Call: extern void pokemon_rby_opening_run(void);
 */

/* ============================ PORT 2 ============================ */
#define PORT2_BASE_ADDRESS          (0xF003A200U)
#define REG_PORT2_IOCR0             (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x10U))
#define REG_PORT2_INPUT             (*(volatile unsigned int *)(PORT2_BASE_ADDRESS + 0x24U))

#define PC0_SHIFT                   3U
#define PC1_SHIFT                   11U
#define PC3_SHIFT                   27U
#define SW1_PIN                     0U

/* ============================== SCU ============================= */
#define SCU_BASE_ADDRESS            (0xF0036000U)
#define REG_SCU_WDTCPU0CON0         (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x100U))
#define REG_SCU_EICR1               (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x214U))
#define REG_SCU_IGCR0               (*(volatile unsigned int *)(SCU_BASE_ADDRESS + 0x22CU))

#define LCK                         1U
#define ENDINIT                     0U

/* SW2: P02.1 -> REQ14 -> ERS2/ETL2 -> OGU0 -> CPU0 */
#define EXIS0_SHIFT                 4U
#define FEN0_BIT                    8U
#define EIEN0_BIT                   11U
#define INP0_SHIFT                  12U
#define IGP0_SHIFT                  14U

/* ======================= Interrupt Router ======================= */
#define SRC_BASE_ADDRESS            (0xF0038000U)
#define REG_SRC_SCU_ERU0            (*(volatile unsigned int *)(SRC_BASE_ADDRESS + 0xCD4U))

#define SRE_BIT                     10U
#define TOS_SHIFT                   11U
#define SW2_ISR_PRIORITY            0x0FU

/* ============================== STM ============================= */
#define STM0_BASE_ADDRESS           (0xF0000000U)
#define REG_STM0_TIM0               (*(volatile unsigned int *)(STM0_BASE_ADDRESS + 0x10U))
#define STM0_FREQUENCY_HZ           100000000U
#define VOLUME_UPDATE_TICKS         (STM0_FREQUENCY_HZ / 200U) /* 5 ms */

/* ============================== VADC ============================ */
/*
 * MCU lecture ADC project mapping:
 *   Shield A0 -> SAR4.7 / P32.3 -> VADC Group 4 Channel 7.
 * Group result register 1 stores a right-aligned 12-bit result (0..4095).
 */
#define VADC_BASE_ADDRESS           (0xF0020000U)
#define REG_VADC_CLC                (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x000U))
#define REG_VADC_G4ARBCFG           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1480U))
#define REG_VADC_G4ARBPR            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1484U))
#define REG_VADC_G4ICLASS0          (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x14A0U))
#define REG_VADC_G4QMR0             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1504U))
#define REG_VADC_G4QINR0            (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1510U))
#define REG_VADC_G4CHCTR7           (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x161CU))
#define REG_VADC_G4RES1             (*(volatile unsigned int *)(VADC_BASE_ADDRESS + 0x1704U))

#define VADC_DISS_BIT               1U
#define VADC_DISR_BIT               0U
#define VADC_ANONC_SHIFT            0U
#define VADC_ASEN0_BIT              24U
#define VADC_CSM0_BIT               3U
#define VADC_PRIO0_SHIFT            0U
#define VADC_CMS_SHIFT              8U
#define VADC_FLUSH_BIT              10U
#define VADC_TREV_BIT               9U
#define VADC_ENGT_SHIFT             0U
#define VADC_RF_BIT                 5U
#define VADC_REQCHNR_SHIFT          0U
#define VADC_RESPOS_BIT             21U
#define VADC_RESREG_SHIFT           16U
#define VADC_ICLSEL_SHIFT           0U
#define VADC_VF_BIT                 31U
#define VADC_MAX_12BIT              4095U

/* ============================== GTM ============================= */
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

volatile unsigned int g_pokemon_stop_requested = 0U;
volatile unsigned int g_pokemon_note_index = 0U;
volatile unsigned int g_pokemon_volume_adc = VADC_MAX_12BIT;

static unsigned int g_current_buzzer_period = 0U;

static void init_ports(void);
static void init_sw2_interrupt(void);
static void init_vadc(void);
static void vadc_start_conversion(void);
static unsigned int vadc_read_result(void);
static void update_buzzer_volume(void);
static void init_buzzer_pwm(void);
static unsigned int read_sw1(void);
static void buzzer_set_frequency(unsigned int frequency_hz);
static void buzzer_off(void);
static unsigned int wait_rtttl_duration(unsigned int denominator, unsigned int dotted);
static unsigned int note_frequency(char note, unsigned int sharp, unsigned int octave);
static unsigned int play_rtttl_sequence(const char *sequence);

__interrupt(SW2_ISR_PRIORITY) __vector_table(0)
void ISR_SW2_POKEMON_STOP(void)
{
    g_pokemon_stop_requested = 1U;
    buzzer_off();
}

void pokemon_rby_opening_run(void)
{
    unsigned int previous_sw1 = 1U;
    unsigned int current_sw1;

    IfxCpu_enableInterrupts();

    init_ports();
    init_vadc();
    init_buzzer_pwm();
    init_sw2_interrupt();
    buzzer_off();

    while (1)
    {
        current_sw1 = read_sw1();

        if ((previous_sw1 != 0U) && (current_sw1 == 0U))
        {
            g_pokemon_stop_requested = 0U;
            g_pokemon_note_index = 0U;
            (void)play_rtttl_sequence(g_pokemon_rby_rtttl);
            buzzer_off();

            while (read_sw1() == 0U)
            {
                if (g_pokemon_stop_requested != 0U)
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
    REG_SCU_EICR1 |=  (1U << FEN0_BIT);
    REG_SCU_EICR1 |=  (1U << EIEN0_BIT);
    REG_SCU_EICR1 &= ~(0x7U << INP0_SHIFT);

    REG_SCU_IGCR0 &= ~(0x3U << IGP0_SHIFT);
    REG_SCU_IGCR0 |=  (0x1U << IGP0_SHIFT);

    REG_SRC_SCU_ERU0 &= ~0xFFU;
    REG_SRC_SCU_ERU0 |= SW2_ISR_PRIORITY;
    REG_SRC_SCU_ERU0 |= (1U << SRE_BIT);
    REG_SRC_SCU_ERU0 &= ~(0x3U << TOS_SHIFT);
}

static void init_vadc(void)
{
    /* Unlock ENDINIT, enable VADC clock, then protect ENDINIT again. */
    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U) { }

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) & ~(1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U) { }

    REG_VADC_CLC &= ~(1U << VADC_DISR_BIT);

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) & ~(1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) != 0U) { }

    REG_SCU_WDTCPU0CON0 = ((REG_SCU_WDTCPU0CON0 ^ 0xFCU) | (1U << LCK)) | (1U << ENDINIT);
    while ((REG_SCU_WDTCPU0CON0 & (1U << LCK)) == 0U) { }

    while ((REG_VADC_CLC & (1U << VADC_DISS_BIT)) != 0U) { }

    /* Group 4 request source 0: highest priority, wait-for-start, enabled. */
    REG_VADC_G4ARBPR |=  (0x3U << VADC_PRIO0_SHIFT);
    REG_VADC_G4ARBPR &= ~(1U << VADC_CSM0_BIT);
    REG_VADC_G4ARBPR |=  (1U << VADC_ASEN0_BIT);

    /* Queue source 0: enable pending conversion requests and clear old queue. */
    REG_VADC_G4QMR0 &= ~(0x3U << VADC_ENGT_SHIFT);
    REG_VADC_G4QMR0 |=  (0x1U << VADC_ENGT_SHIFT);
    REG_VADC_G4QMR0 |=  (1U << VADC_FLUSH_BIT);

    /* Group 4 converter normal operation. */
    REG_VADC_G4ARBCFG |= (0x3U << VADC_ANONC_SHIFT);

    /* Group-specific input class 0, 12-bit conversion. */
    REG_VADC_G4ICLASS0 &= ~(0x7U << VADC_CMS_SHIFT);

    /* Channel 7 -> Group Result Register 1, right-aligned, input class 0. */
    REG_VADC_G4CHCTR7 |=  (1U << VADC_RESPOS_BIT);
    REG_VADC_G4CHCTR7 &= ~(0xFU << VADC_RESREG_SHIFT);
    REG_VADC_G4CHCTR7 |=  (0x1U << VADC_RESREG_SHIFT);
    REG_VADC_G4CHCTR7 &= ~(0x3U << VADC_ICLSEL_SHIFT);
}

static void vadc_start_conversion(void)
{
    /* Single-shot conversion of Group 4 Channel 7. */
    REG_VADC_G4QINR0 &= ~(0x1FU << VADC_REQCHNR_SHIFT);
    REG_VADC_G4QINR0 |=  (0x7U << VADC_REQCHNR_SHIFT);
    REG_VADC_G4QINR0 &= ~(1U << VADC_RF_BIT);
    REG_VADC_G4QMR0  |=  (1U << VADC_TREV_BIT);
}

static unsigned int vadc_read_result(void)
{
    unsigned int result;

    while ((REG_VADC_G4RES1 & (1UL << VADC_VF_BIT)) == 0U)
    {
        if (g_pokemon_stop_requested != 0U)
        {
            return 0U;
        }
    }

    result = REG_VADC_G4RES1 & 0xFFFFU;
    return (result & VADC_MAX_12BIT);
}

static void update_buzzer_volume(void)
{
    unsigned int adc;
    unsigned int duty;

    if (g_current_buzzer_period == 0U)
    {
        return;
    }

    vadc_start_conversion();
    adc = vadc_read_result();
    g_pokemon_volume_adc = adc;

    /* A0 0..4095 maps to PWM duty 0..50% without changing pitch. */
    duty = (unsigned int)(((unsigned long long)g_current_buzzer_period * (unsigned long long)adc) /
                          (2ULL * (unsigned long long)VADC_MAX_12BIT));

    REG_GTM_TOM0_CH11_SR1 = duty;
    REG_GTM_TOM0_CH11_CM1 = duty;
}

static void buzzer_set_frequency(unsigned int frequency_hz)
{
    unsigned int period;

    if (frequency_hz == 0U)
    {
        buzzer_off();
        return;
    }

    period = (BUZZER_CLOCK_HZ + (frequency_hz / 2U)) / frequency_hz;
    g_current_buzzer_period = period;

    REG_GTM_TOM0_CH11_SR0 = period;
    REG_GTM_TOM0_CH11_CM0 = period;
    update_buzzer_volume();
}

static void buzzer_off(void)
{
    g_current_buzzer_period = 0U;
    REG_GTM_TOM0_CH11_SR1 = 0U;
    REG_GTM_TOM0_CH11_CM1 = 0U;
}

static unsigned int wait_rtttl_duration(unsigned int denominator, unsigned int dotted)
{
    unsigned int start;
    unsigned int target_ticks;
    unsigned int last_volume_update;
    unsigned long long numerator;
    unsigned long long divisor;

    if (denominator == 0U)
    {
        denominator = 4U;
    }

    /* RTTTL duration: whole-note time = 240 seconds / BPM. */
    numerator = (unsigned long long)STM0_FREQUENCY_HZ * 240ULL;
    divisor = (unsigned long long)POKEMON_RBY_BPM * (unsigned long long)denominator;

    if (dotted != 0U)
    {
        numerator *= 3ULL;
        divisor *= 2ULL;
    }

    target_ticks = (unsigned int)(numerator / divisor);
    start = REG_STM0_TIM0;
    last_volume_update = start;

    while ((unsigned int)(REG_STM0_TIM0 - start) < target_ticks)
    {
        unsigned int now = REG_STM0_TIM0;

        if (g_pokemon_stop_requested != 0U)
        {
            buzzer_off();
            return 0U;
        }

        /* Read Rotation A0 while a note is sounding, so volume changes live. */
        if ((g_current_buzzer_period != 0U) &&
            ((unsigned int)(now - last_volume_update) >= VOLUME_UPDATE_TICKS))
        {
            update_buzzer_volume();
            last_volume_update = now;
        }
    }

    return 1U;
}

static unsigned int note_frequency(char note, unsigned int sharp, unsigned int octave)
{
    /* C..B, integer Hz, octaves 2..7. */
    static const unsigned short freq[6][12] =
    {
        {65U,69U,73U,78U,82U,87U,93U,98U,104U,110U,117U,123U},
        {131U,139U,147U,156U,165U,175U,185U,196U,208U,220U,233U,247U},
        {262U,277U,294U,311U,330U,349U,370U,392U,415U,440U,466U,494U},
        {523U,554U,587U,622U,659U,698U,740U,784U,831U,880U,932U,988U},
        {1047U,1109U,1175U,1245U,1319U,1397U,1480U,1568U,1661U,1760U,1865U,1976U},
        {2093U,2217U,2349U,2489U,2637U,2794U,2960U,3136U,3322U,3520U,3729U,3951U}
    };
    unsigned int semitone;

    switch (note)
    {
        case 'c': semitone = 0U;  break;
        case 'd': semitone = 2U;  break;
        case 'e': semitone = 4U;  break;
        case 'f': semitone = 5U;  break;
        case 'g': semitone = 7U;  break;
        case 'a': semitone = 9U;  break;
        case 'b': semitone = 11U; break;
        default:  return 0U;
    }

    if (sharp != 0U)
    {
        semitone++;
        if (semitone >= 12U)
        {
            semitone = 0U;
            octave++;
        }
    }

    if ((octave < 2U) || (octave > 7U))
    {
        return 0U;
    }

    return (unsigned int)freq[octave - 2U][semitone];
}

static unsigned int play_rtttl_sequence(const char *sequence)
{
    const char *p = sequence;

    /* Skip optional RTTTL name/default header; score data begins after second ':'. */
    {
        unsigned int colon_count = 0U;
        const char *scan = p;
        while (*scan != '\0')
        {
            if (*scan == ':')
            {
                colon_count++;
                if (colon_count == 2U)
                {
                    p = scan + 1;
                    break;
                }
            }
            scan++;
        }
    }

    while (*p != '\0')
    {
        unsigned int denominator = 0U;
        unsigned int octave = 4U;
        unsigned int dotted = 0U;
        unsigned int sharp = 0U;
        unsigned int frequency = 0U;
        char note;

        if (g_pokemon_stop_requested != 0U)
        {
            return 0U;
        }

        while ((*p >= '0') && (*p <= '9'))
        {
            denominator = (denominator * 10U) + (unsigned int)(*p - '0');
            p++;
        }

        if (denominator == 0U)
        {
            denominator = 4U;
        }

        note = *p;
        if (note == '\0')
        {
            break;
        }
        p++;

        if (*p == '#')
        {
            sharp = 1U;
            p++;
        }

        while ((*p != ',') && (*p != '\0'))
        {
            if (*p == '.')
            {
                dotted = 1U;
            }
            else if ((*p >= '0') && (*p <= '9'))
            {
                octave = (unsigned int)(*p - '0');
            }
            p++;
        }

        if (note == 'p')
        {
            buzzer_off();
        }
        else
        {
            frequency = note_frequency(note, sharp, octave);
            buzzer_set_frequency(frequency);
        }

        g_pokemon_note_index++;

        if (wait_rtttl_duration(denominator, dotted) == 0U)
        {
            return 0U;
        }

        if (*p == ',')
        {
            p++;
        }
    }

    buzzer_off();
    return 1U;
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

    REG_GTM_TOM0_CH11_SR0 = (BUZZER_CLOCK_HZ / 392U);
    REG_GTM_TOM0_CH11_SR1 = 0U;

    REG_GTM_TOUTSEL0 &= ~(0x3U << SEL3_SHIFT);

    REG_GTM_TOM0_TGC1_GLB_CTRL |= (1U << HOST_TRIG_BIT);
}
