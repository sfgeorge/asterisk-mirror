/*
 * Jared Davis <jdavis@ifbyphone.com>
 *
 */

/*! \file
 *
 * \brief Definitions for ulaw audio wrapped in wav header.
 * 
 * \ingroup formats
 */

#define WAV_ULAW_BUF_SIZE 160		/* 160 bytes, and same number of samples */
#define WAV_ULAW_HEADER_SIZE 58
#define WAVE_FORMAT_MULAW 7
#define WAVE_ULAW_FREQ 8000

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define htoll(b) (b)
#define htols(b) (b)
#define ltohl(b) (b)
#define ltohs(b) (b)
#else
#if __BYTE_ORDER == __BIG_ENDIAN
#define htoll(b)  \
          (((((b)      ) & 0xFF) << 24) | \
	       ((((b) >>  8) & 0xFF) << 16) | \
		   ((((b) >> 16) & 0xFF) <<  8) | \
		   ((((b) >> 24) & 0xFF)      ))
#define htols(b) \
          (((((b)      ) & 0xFF) << 8) | \
		   ((((b) >> 8) & 0xFF)      ))
#define ltohl(b) htoll(b)
#define ltohs(b) htols(b)
#else
#error "Endianess not defined"
#endif
#endif

