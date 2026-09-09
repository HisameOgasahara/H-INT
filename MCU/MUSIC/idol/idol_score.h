#ifndef IDOL_SCORE_H
#define IDOL_SCORE_H

typedef struct
{
    unsigned short frequency_hz; /* 0 = rest */
    unsigned short midi_ticks;   /* duration in MIDI ticks, PPQ=128 */
} IdolEvent;

#define IDOL_REST               0U
#define IDOL_BPM                166U
#define IDOL_TICKS_PER_QUARTER  128U

extern const IdolEvent g_idol_score[];
extern const unsigned int g_idol_score_event_count;

#define IDOL_SCORE_EVENT_COUNT (g_idol_score_event_count)

#endif /* IDOL_SCORE_H */
