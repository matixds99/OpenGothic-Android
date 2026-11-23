// NB fix 183: the implementation of dr_mp3 (lib/dr_libs, a system include: its warnings do not stop the build)
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>
