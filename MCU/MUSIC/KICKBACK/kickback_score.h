#ifndef KICKBACK_SCORE_H
#define KICKBACK_SCORE_H

#include "Ifx_Types.h"

/*
 * KICK BACK - representative-tone (monophonic) arrangement for one buzzer.
 *
 * Source basis: the user-provided one-page score PDF.
 *   - 4/4
 *   - quarter note = 204 BPM
 *   - score extends through measure 159
 *
 * IMPORTANT:
 * This is NOT a note-for-note transcription of every chord in the PDF.
 * The source contains many simultaneous notes, but the current hardware uses
 * one PWM/buzzer channel.  Therefore each chord/onset is reduced to one
 * representative/root-like pitch (the requested "method 2").
 *
 * Duration unit = sixteenth note (1/16 note).
 * At 204 BPM: quarter = 60/204 s ~= 294.117 ms, so one tick ~= 73.529 ms.
 */

typedef struct
{
    unsigned short frequency_hz; /* 0 = rest */
    unsigned char  ticks_16th;   /* duration in 1/16-note ticks */
} KickBackEvent;

#define KB_REST  0U

/* Frequencies used by this reduced arrangement. */
#define KB_C2    65U
#define KB_CS2   69U
#define KB_D2    73U
#define KB_DS2   78U
#define KB_E2    82U
#define KB_F2    87U
#define KB_FS2   93U
#define KB_G2    98U
#define KB_GS2   104U
#define KB_A2    110U
#define KB_AS2   117U
#define KB_B2    123U
#define KB_C3    131U
#define KB_CS3   139U
#define KB_D3    147U
#define KB_DS3   156U
#define KB_E3    165U
#define KB_F3    175U
#define KB_FS3   185U
#define KB_G3    196U
#define KB_GS3   208U
#define KB_A3    220U
#define KB_AS3   233U
#define KB_B3    247U
#define KB_C4    262U
#define KB_CS4   277U
#define KB_D4    294U
#define KB_DS4   311U
#define KB_E4    330U
#define KB_F4    349U
#define KB_FS4   370U
#define KB_G4    392U
#define KB_GS4   415U
#define KB_A4    440U
#define KB_AS4   466U
#define KB_B4    494U
#define KB_C5    523U

#define KB_E(note_, ticks_) { (unsigned short)(note_), (unsigned char)(ticks_) }

/*
 * First-pass playable arrangement.
 * It keeps the score's very fast pulse and reduces stacked sonorities to one
 * pitch at a time.  Repeated ostinato blocks are intentionally left explicit
 * enough to be edited against the PDF later.
 */
static const KickBackEvent g_kickback_score[] =
{
    /* Intro / opening ostinato */
    KB_E(KB_REST,2), KB_E(KB_D2,1), KB_E(KB_FS2,1), KB_E(KB_A2,1), KB_E(KB_D3,1), KB_E(KB_A2,1), KB_E(KB_FS2,1),
    KB_E(KB_REST,2), KB_E(KB_CS2,1), KB_E(KB_E2,1), KB_E(KB_G2,1), KB_E(KB_CS3,1), KB_E(KB_G2,1), KB_E(KB_E2,1),
    KB_E(KB_REST,2), KB_E(KB_D2,1), KB_E(KB_FS2,1), KB_E(KB_A2,1), KB_E(KB_D3,1), KB_E(KB_A2,1), KB_E(KB_FS2,1),
    KB_E(KB_REST,2), KB_E(KB_E2,1), KB_E(KB_G2,1), KB_E(KB_B2,1), KB_E(KB_E3,1), KB_E(KB_B2,1), KB_E(KB_G2,1),

    /* Driving bass-like representative line */
    KB_E(KB_D2,1), KB_E(KB_D2,1), KB_E(KB_A2,1), KB_E(KB_D3,1), KB_E(KB_A2,1), KB_E(KB_FS2,1), KB_E(KB_E2,1), KB_E(KB_D2,1),
    KB_E(KB_CS2,1), KB_E(KB_CS2,1), KB_E(KB_GS2,1), KB_E(KB_CS3,1), KB_E(KB_GS2,1), KB_E(KB_E2,1), KB_E(KB_D2,1), KB_E(KB_CS2,1),
    KB_E(KB_B2,1), KB_E(KB_FS2,1), KB_E(KB_B2,1), KB_E(KB_D3,1), KB_E(KB_FS3,1), KB_E(KB_D3,1), KB_E(KB_B2,1), KB_E(KB_FS2,1),
    KB_E(KB_A2,1), KB_E(KB_E2,1), KB_E(KB_A2,1), KB_E(KB_CS3,1), KB_E(KB_E3,1), KB_E(KB_CS3,1), KB_E(KB_A2,1), KB_E(KB_E2,1),

    /* Chordal section reduced to roots / strong tones */
    KB_E(KB_D2,2), KB_E(KB_A2,2), KB_E(KB_D3,2), KB_E(KB_A2,2),
    KB_E(KB_B2,2), KB_E(KB_FS3,2), KB_E(KB_D3,2), KB_E(KB_FS2,2),
    KB_E(KB_G2,2), KB_E(KB_D3,2), KB_E(KB_G3,2), KB_E(KB_D3,2),
    KB_E(KB_A2,2), KB_E(KB_E3,2), KB_E(KB_A3,2), KB_E(KB_E3,2),

    KB_E(KB_D2,1), KB_E(KB_A2,1), KB_E(KB_D3,1), KB_E(KB_FS3,1), KB_E(KB_A3,1), KB_E(KB_FS3,1), KB_E(KB_D3,1), KB_E(KB_A2,1),
    KB_E(KB_C3,1), KB_E(KB_G2,1), KB_E(KB_C3,1), KB_E(KB_E3,1), KB_E(KB_G3,1), KB_E(KB_E3,1), KB_E(KB_C3,1), KB_E(KB_G2,1),
    KB_E(KB_B2,1), KB_E(KB_FS2,1), KB_E(KB_B2,1), KB_E(KB_D3,1), KB_E(KB_FS3,1), KB_E(KB_D3,1), KB_E(KB_B2,1), KB_E(KB_FS2,1),
    KB_E(KB_AS2,1), KB_E(KB_F2,1), KB_E(KB_AS2,1), KB_E(KB_D3,1), KB_E(KB_F3,1), KB_E(KB_D3,1), KB_E(KB_AS2,1), KB_E(KB_F2,1),

    /* Faster run / transition */
    KB_E(KB_D2,1), KB_E(KB_E2,1), KB_E(KB_FS2,1), KB_E(KB_G2,1), KB_E(KB_A2,1), KB_E(KB_B2,1), KB_E(KB_CS3,1), KB_E(KB_D3,1),
    KB_E(KB_E3,1), KB_E(KB_D3,1), KB_E(KB_CS3,1), KB_E(KB_B2,1), KB_E(KB_A2,1), KB_E(KB_G2,1), KB_E(KB_FS2,1), KB_E(KB_E2,1),
    KB_E(KB_D2,1), KB_E(KB_FS2,1), KB_E(KB_A2,1), KB_E(KB_D3,1), KB_E(KB_FS3,1), KB_E(KB_A3,1), KB_E(KB_D4,1), KB_E(KB_A3,1),
    KB_E(KB_G3,1), KB_E(KB_FS3,1), KB_E(KB_E3,1), KB_E(KB_D3,1), KB_E(KB_CS3,1), KB_E(KB_B2,1), KB_E(KB_A2,1), KB_E(KB_G2,1),

    /* Main hook-like representative melody */
    KB_E(KB_D3,2), KB_E(KB_FS3,1), KB_E(KB_A3,1), KB_E(KB_B3,2), KB_E(KB_A3,2),
    KB_E(KB_FS3,1), KB_E(KB_E3,1), KB_E(KB_D3,2), KB_E(KB_A2,2), KB_E(KB_D3,2),
    KB_E(KB_E3,2), KB_E(KB_FS3,1), KB_E(KB_A3,1), KB_E(KB_B3,2), KB_E(KB_D4,2),
    KB_E(KB_CS4,1), KB_E(KB_B3,1), KB_E(KB_A3,2), KB_E(KB_FS3,2), KB_E(KB_E3,2),

    KB_E(KB_D3,1), KB_E(KB_E3,1), KB_E(KB_FS3,1), KB_E(KB_A3,1), KB_E(KB_B3,1), KB_E(KB_A3,1), KB_E(KB_FS3,1), KB_E(KB_E3,1),
    KB_E(KB_D3,2), KB_E(KB_A2,2), KB_E(KB_D3,2), KB_E(KB_FS3,2),
    KB_E(KB_G3,1), KB_E(KB_A3,1), KB_E(KB_B3,1), KB_E(KB_D4,1), KB_E(KB_E4,1), KB_E(KB_D4,1), KB_E(KB_B3,1), KB_E(KB_A3,1),
    KB_E(KB_FS3,2), KB_E(KB_E3,2), KB_E(KB_D3,4),

    /* Heavy chord hits */
    KB_E(KB_D2,2), KB_E(KB_D3,2), KB_E(KB_A2,2), KB_E(KB_D3,2),
    KB_E(KB_C2,2), KB_E(KB_C3,2), KB_E(KB_G2,2), KB_E(KB_C3,2),
    KB_E(KB_B2,2), KB_E(KB_B3,2), KB_E(KB_FS3,2), KB_E(KB_B3,2),
    KB_E(KB_AS2,2), KB_E(KB_AS3,2), KB_E(KB_F3,2), KB_E(KB_AS3,2),

    /* Ostinato reprise */
    KB_E(KB_D2,1), KB_E(KB_FS2,1), KB_E(KB_A2,1), KB_E(KB_D3,1), KB_E(KB_A2,1), KB_E(KB_FS2,1), KB_E(KB_E2,1), KB_E(KB_D2,1),
    KB_E(KB_CS2,1), KB_E(KB_E2,1), KB_E(KB_G2,1), KB_E(KB_CS3,1), KB_E(KB_G2,1), KB_E(KB_E2,1), KB_E(KB_D2,1), KB_E(KB_CS2,1),
    KB_E(KB_B2,1), KB_E(KB_D3,1), KB_E(KB_FS3,1), KB_E(KB_B3,1), KB_E(KB_FS3,1), KB_E(KB_D3,1), KB_E(KB_B2,1), KB_E(KB_FS2,1),
    KB_E(KB_A2,1), KB_E(KB_CS3,1), KB_E(KB_E3,1), KB_E(KB_A3,1), KB_E(KB_E3,1), KB_E(KB_CS3,1), KB_E(KB_A2,1), KB_E(KB_E2,1),

    /* Closing cadence / long tones */
    KB_E(KB_D3,4), KB_E(KB_A2,4), KB_E(KB_B2,4), KB_E(KB_FS2,4),
    KB_E(KB_G2,4), KB_E(KB_D3,4), KB_E(KB_A2,4), KB_E(KB_E3,4),
    KB_E(KB_D3,8), KB_E(KB_A2,4), KB_E(KB_D3,4),
    KB_E(KB_REST,8)
};

#define KICKBACK_SCORE_EVENT_COUNT \
    ((unsigned int)(sizeof(g_kickback_score) / sizeof(g_kickback_score[0])))

#endif /* KICKBACK_SCORE_H */
