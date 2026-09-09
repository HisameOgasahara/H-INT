#ifndef ZANKYOSANKA_SCORE_H
#define ZANKYOSANKA_SCORE_H

typedef struct
{
    unsigned short frequency_hz; /* 0 = rest */
    unsigned short midi_ticks;   /* duration in MIDI ticks, PPQ=128 */
} ZankyoEvent;

#define ZANKYO_REST               0U
#define ZANKYO_BPM                160U
#define ZANKYO_TICKS_PER_QUARTER  128U

extern const ZankyoEvent g_zankyosanka_score[];
extern const unsigned int g_zankyosanka_score_event_count;

#define ZANKYOSANKA_SCORE_EVENT_COUNT (g_zankyosanka_score_event_count)

#endif /* ZANKYOSANKA_SCORE_H */
