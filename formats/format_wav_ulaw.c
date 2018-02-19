/*
 * Jared Davis <jdavis@ifbyphone.com>
 *
 */

/*! \file
 *
 * \brief Flat, binary, ulaw PCM file format with wav header.
 * \arg File name extension: wav
 * 
 * \ingroup formats
 */

/*** MODULEINFO
	<support_level>core</support_level>
 ***/
 
#include "asterisk.h"

ASTERISK_FILE_VERSION(__FILE__, "$Revision$")

#include "asterisk/mod_format.h"
#include "asterisk/module.h"
#include "asterisk/endian.h"
#include "asterisk/ulaw.h"
#include "asterisk/wav_ulaw.h"
#include "asterisk/format_cache.h"

static char wav_ulaw_silence[WAV_ULAW_BUF_SIZE];

/*
 * Enforces our specifications for valid audiofile format.
 */
static int check_header_fmt(FILE *f, int size)
{
  short format, chans, bysam, bisam;
  int bysec, freq;

  if (size < 16) {
    ast_log(LOG_WARNING, "Unexpected header size %d\n", size);
    return -1;
  }

  if (fread(&format, 1, 2, f) != 2) {
    ast_log(LOG_WARNING, "Read failed (format)\n");
    return -1;
  }

  if (ltohs(format) != WAVE_FORMAT_MULAW) {
    ast_log(LOG_WARNING, "Invalid audio format %d\n", ltohs(format));
    return -1;
  }

  if (fread(&chans, 1, 2, f) != 2) {
    ast_log(LOG_WARNING, "Read failed (format)\n");
    return -1;
  }
  
  if (ltohs(chans) != 1) {
    ast_log(LOG_WARNING, "Not in mono %d\n", ltohs(chans));
    return -1;
  }

  if (fread(&freq, 1, 4, f) != 4) {
    ast_log(LOG_WARNING, "Read failed (freq)\n");
    return -1;
  }
  freq = ltohl(freq);

  if (freq != WAVE_ULAW_FREQ) {
    ast_log(LOG_WARNING, "Unexpected frequency mismatch %d (expecting %d)\n", freq, WAVE_ULAW_FREQ);
    return -1;
  }

  /* Ignore the byte frequency */
  if (fread(&bysec, 1, 4, f) != 4) {
    ast_log(LOG_WARNING, "Read failed (BYTES_PER_SECOND)\n");
    return -1;
  }
  /* Check bytes per sample */
  if (fread(&bysam, 1, 2, f) != 2) {
    ast_log(LOG_WARNING, "Read failed (BYTES_PER_SAMPLE)\n");
    return -1;
  }
  if (ltohs(bysam) != 1) {
    ast_log(LOG_WARNING, "Can only handle 8 bits per sample: %d\n", ltohs(bysam));
    return -1;
  }
  if (fread(&bisam, 1, 2, f) != 2) {
    ast_log(LOG_WARNING, "Read failed (Bits Per Sample): %d\n", ltohs(bisam));
    return -1;
  }

  if (fseek(f,size-16,SEEK_CUR) == -1 ) {
    ast_log(LOG_WARNING, "Failed to skip remaining header bytes: %d\n", size-15 );
    return -1;
  }

  return 0;
}

/*
 * Enforces our specifications for valid audiofile header data. 
 */
static int check_header(FILE *f)
{
  // model after formats/format_wav.c
  int type, size, formtype, data;
  
  // first four bytes contain type info
  if (fread(&type, 1, 4, f) != 4) {
    ast_log(LOG_WARNING, "Read failed (type)\n");
    return -1;
  }
  // ensure type info is "RIFF"
  if (memcmp(&type, "RIFF", 4)) {
    ast_log(LOG_WARNING, "Does not begin with RIFF\n");
    return -1;
  }

  // next four bytes contain size info
  if (fread(&size, 1, 4, f) != 4) {
    ast_log(LOG_WARNING, "Read failed (size)\n");
    return -1;
  }
  // convert bytes for size into an int
  size = ltohl(size);

  // next four bytes contain format type
  if (fread(&formtype, 1, 4, f) != 4) {
    ast_log(LOG_WARNING, "Read failed (format)");
    return -1;
  }
  // ensure type is WAVE, 4) {
  if (memcmp(&formtype, "WAVE", 4)) {
    ast_log(LOG_WARNING, "Does not contain WAVE\n");
    return -1;
  }

  for(;;)
  {
    char buf[4];

    // we are going to search for one of two header pairs
    // these will be named either "fmt " or "data"...
    if (fread(&buf, 1, 4, f) != 4) {
      ast_log(LOG_WARNING, "Read failed (block header format)\n");
      return -1;
    }

    // ...and the data will follow immediately
    if (fread(&data, 1, 4, f) != 4) {
      ast_log(LOG_WARNING, "Read failed (block '%.4s' header length)\n", buf);
      return -1;
    }

    if (memcmp(buf, "fmt ", 4) == 0) {
      // "fmt " will indicate where we may find strict information regarding
      // the actual audio data. the format_check function will parse it for us
      if (check_header_fmt(f, data)) {
        // non-zero value indicates failure
        return -1;
      }
      // move forward in the loop, seeking the "data" field
      continue;
    }

    if (memcmp(buf, "data", 4) == 0) {
      break; // we will break and return value of data
    }

    ast_log(LOG_DEBUG, "Skipping unknown block '%.4s' block: %d\n", buf, data);
    if (fseek(f, data, SEEK_CUR) == -1) {
      ast_log(LOG_WARNING, "Failed to skip '%.4s' block: %d\n", buf, data);
      return -1;
    }
  }

  return data;
}

/*
 *  Defers the file open until after we've verified
 *  valid header data and audiofile format.
 */
static int wav_ulaw_open(struct ast_filestream *s)
{
  if (check_header(s->f) < 0)
    return -1;
  return 0;
}

/* 
 * Write new, empty RIFF header values to be filled in later.
 */
static int write_header(FILE *f)
{
  unsigned int hz = 8000, bhz = 16000; 
  unsigned int hs = htoll(16);
  unsigned int size = htoll(0);
  unsigned short fmt = htols(1), chans = htols(1), bysam = htols(2), bisam = htols(16);

	fseek(f,0,SEEK_SET);
	if (fwrite("RIFF", 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&size, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite("WAVEfmt ", 1, 8, f) != 8) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&hs, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&fmt, 1, 2, f) != 2) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&chans, 1, 2, f) != 2) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&hz, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&bhz, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&bysam, 1, 2, f) != 2) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&bisam, 1, 2, f) != 2) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite("data", 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	if (fwrite(&size, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to write header\n");
		return -1;
	}
	return 0;
}

/*
 * Method for Asterisk to rewrite the audiofile, for instance,
 * if the file needs resaving or truncating.
 */
static int wav_ulaw_rewrite(struct ast_filestream *s, const char *comment)
{
  if (write_header(s->f))
    return -1;
  return 0;
}

/*
 * Method to write audio contents to a voice frame;
 * this is the data that is heard on the phonecall.
 */
static int wav_ulaw_write(struct ast_filestream *fs, struct ast_frame *f)
{
	int res;

	if (f->frametype != AST_FRAME_VOICE) {
		ast_log(LOG_WARNING, "Asked to write non-voice frame!\n");
		return -1;
	}
//	if (f->subclass.format != fs->fmt->format) {
//		ast_log(LOG_WARNING, "Asked to write incompatible format frame ()!\n");
//		return -1;
//	}
	
	if ((res = fwrite(f->data.ptr, 1, f->datalen, fs->f)) != f->datalen) {
		ast_log(LOG_WARNING, "Bad write (%d/%d): %s\n", res, f->datalen, strerror(errno));
		return -1;
	}
	return 0;
}

/* 
 * Wrapper for a call to standard library file seek.
 * This method will offset the cursor to ignore the wav
 * header contents when trying to write the audio to 
 * and output buffer.
 */
static int wav_ulaw_seek(struct ast_filestream *fs, off_t samples, int whence)
{
	off_t min = WAV_ULAW_HEADER_SIZE, max, cur, offset = 0;

	if ((cur = ftello(fs->f)) < 0) {
		ast_log(AST_LOG_WARNING, "Unable to determine current position in wav filestream %p: %s\n", fs, strerror(errno));
		return -1;
	}

	if (fseeko(fs->f, 0, SEEK_END) < 0) {
		ast_log(AST_LOG_WARNING, "Unable to seek to end of wav filestream %p: %s\n", fs, strerror(errno));
		return -1;
	}

	if ((max = ftello(fs->f)) < 0) {
		ast_log(AST_LOG_WARNING, "Unable to determine max position in wav filestream %p: %s\n", fs, strerror(errno));
		return -1;
	}

  switch(whence) {
  case SEEK_SET:
    offset = samples + min;
    break;
  case SEEK_CUR:
    offset = max + samples;
    break;
  case SEEK_FORCECUR:
    offset = max + samples;
    break;
  case SEEK_END:
    offset = max - samples;

    if (offset > max) {
      offset = max;
    }

    break;
  default:
		ast_log(LOG_WARNING, "invalid whence %d, assuming SEEK_SET\n", whence);
		offset = samples;
  }

	/* protect the header space. */
  if (offset < min) {
    offset = min;
  }

	return fseeko(fs->f, offset, SEEK_SET);
}

/*
 * This method will rewrite the header information for the size
 * of the audio file and the length of the audio data. Necessary if
 * Asterisk needs to truncate the audio file for some reason.
 */
static int update_header(FILE *f)
{
	off_t cur, end;
	int datalen, filelen, bytes, chksize_pos;

  // current cursor position
	cur = ftello(f);

  // move cursor to end
	fseek(f, 0, SEEK_END);

  // get end cursor position
	end = ftello(f);

	/* data starts WAV_ULAW_HEADER_SIZE bytes in */
	bytes = end - WAV_ULAW_HEADER_SIZE;

  // coerce to int
	datalen = htoll(bytes);
	filelen = htoll(end - 8);

  // data length chunk size field position
  chksize_pos = WAV_ULAW_HEADER_SIZE - 4;
	
	if (cur < 0) {
		ast_log(LOG_WARNING, "Unable to find our position\n");
		return -1;
	}
	if (fseek(f, 4, SEEK_SET)) {
		ast_log(LOG_WARNING, "Unable to set our position\n");
		return -1;
	}
	if (fwrite(&filelen, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to set write file size\n");
		return -1;
	}
	if (fseek(f, chksize_pos, SEEK_SET)) {
		ast_log(LOG_WARNING, "Unable to set our position\n");
		return -1;
	}
	if (fwrite(&datalen, 1, 4, f) != 4) {
		ast_log(LOG_WARNING, "Unable to set write datalen\n");
		return -1;
	}
	if (fseeko(f, cur, SEEK_SET)) {
		ast_log(LOG_WARNING, "Unable to return to position\n");
		return -1;
	}
	return 0;
}

/*
 * Wrapper for call to standard library file truncate.
 * The header data is updated to reflect the new filesize and data length.
 */
static int wav_ulaw_trunc(struct ast_filestream *fs)
{
	int fd;
	off_t cur;

	if ((fd = fileno(fs->f)) < 0) {
		ast_log(AST_LOG_WARNING, "Unable to determine file descriptor for au filestream %p: %s\n", fs, strerror(errno));
		return -1;
	}
	if ((cur = ftello(fs->f)) < 0) {
		ast_log(AST_LOG_WARNING, "Unable to determine current position in au filestream %p: %s\n", fs, strerror(errno));
		return -1;
	}
	/* Truncate file to current length */
	if (ftruncate(fd, cur)) {
		return -1;
	}
	return update_header(fs->f);
}

/*
 * Wrapper for standard library ftello, reports the file size in bytes
 * minus the defined header size.
 */
static off_t wav_ulaw_tell(struct ast_filestream *fs)
{
  off_t offset = ftello(fs->f);
  return offset - WAV_ULAW_HEADER_SIZE;
}

/* 
 * Send a frame from the file to the appropriate channel 
 */
static struct ast_frame *wav_ulaw_read(struct ast_filestream *s, int *whennext)
{
	int res;

	s->fr.frametype = AST_FRAME_VOICE;
//	s->fr.subclass.format = s->fmt->format;
	s->fr.mallocd = 0;
	AST_FRAME_SET_BUFFER(&s->fr, s->buf, AST_FRIENDLY_OFFSET, WAV_ULAW_BUF_SIZE);
	if ((res = fread(s->fr.data.ptr, 1, s->fr.datalen, s->f)) < 1) {
		if (res)
			ast_log(LOG_WARNING, "Short read (%d) (%s)!\n", res, strerror(errno));
		return NULL;
	}
	s->fr.datalen = res;
  *whennext = s->fr.samples = res;
	return &s->fr;
}

/*
 * Our file struct, equipped with methods to so stuff.
 */
//  .exts = "wav|ul-wav",
static struct ast_format_def wav_ulaw_f = {
  .name = "wav_ulaw",
  .exts = "wav|ulaw",
  .open = wav_ulaw_open,
  .rewrite = wav_ulaw_rewrite,
  .write = wav_ulaw_write,
  .seek = wav_ulaw_seek,
  .trunc = wav_ulaw_trunc,
  .tell = wav_ulaw_tell,
  .read = wav_ulaw_read,
  .buf_size = WAV_ULAW_BUF_SIZE + AST_FRIENDLY_OFFSET
};

// Every file format is registered via a call to load_module
static int load_module(void)
{
	int i;

	for (i = 0; i < ARRAY_LEN(wav_ulaw_silence); i++)
		wav_ulaw_silence[i] = AST_LIN2MU(0);

	wav_ulaw_f.format = ast_format_ulaw;
	if (ast_format_def_register(&wav_ulaw_f))
		return AST_MODULE_LOAD_DECLINE;
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	return ast_format_def_unregister(wav_ulaw_f.name);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "uLaw 8KHz (PCM) with WAVE header info",
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_APP_DEPEND
);
