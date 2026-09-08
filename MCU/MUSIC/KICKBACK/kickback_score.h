#ifndef KICKBACK_SCORE_H
#define KICKBACK_SCORE_H

#include "Ifx_Types.h"

typedef struct
{
    unsigned short frequency_hz; /* 0 = rest */
    unsigned char  ticks_16th;   /* duration in 1/16-note units */
} KickBackEvent;

#define KB_REST 0U
#define KB_D3   147U

extern const KickBackEvent g_kickback_score[];
extern const unsigned int g_kickback_score_event_count;

/* Keep the existing player source compatible after moving score data to .c. */
#define KICKBACK_SCORE_EVENT_COUNT (g_kickback_score_event_count)

#endif /* KICKBACK_SCORE_H */
