#ifndef KICKBACK_SCORE_H
#define KICKBACK_SCORE_H

typedef struct
{
    unsigned short frequency_hz; /* 0 = rest */
    unsigned short midi_ticks;   /* duration in MIDI ticks, PPQ=128 */
} KickBackEvent;

#define KICKBACK_REST               0U
#define KICKBACK_BPM                150U
#define KICKBACK_TICKS_PER_QUARTER  128U

extern const KickBackEvent g_kickback_score[];
extern const unsigned int g_kickback_score_event_count;

#define KICKBACK_SCORE_EVENT_COUNT (g_kickback_score_event_count)

#endif /* KICKBACK_SCORE_H */
