#ifndef CONAN_SCORE_H
#define CONAN_SCORE_H

typedef struct
{
    unsigned short frequency_hz; /* 0 = rest */
    unsigned char ticks_8th;     /* duration in 1/8-note units */
} ConanEvent;

#define CONAN_REST 0U
#define CONAN_BPM 145U

extern const ConanEvent g_conan_score[];
extern const unsigned int g_conan_score_count;

#endif /* CONAN_SCORE_H */
