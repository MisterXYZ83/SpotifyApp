#ifndef __WAVE__FILE__HEADER__

#define __WAVE__FILE__HEADER__

#include <stdio.h>
#include <stdlib.h>

#define WAVE_HEADER_CHUNKID_OFFSET			0L
#define WAVE_HEADER_CHUNKSIZE_OFFSET		4L
#define WAVE_HEADER_FORMAT_OFFSET			8L
#define WAVE_HEADER_SUBCHUNK1ID_OFFSET		12L
#define WAVE_HEADER_SUBCHUNK1SIZE_OFFSET	16L
#define WAVE_HEADER_AUDIOFORMAT_OFFSET		20L
#define WAVE_HEADER_NUMCHANNELS_OFFSET		22L
#define WAVE_HEADER_SAMPLERATE_OFFSET		24L
#define WAVE_HEADER_BYTERATE_OFFSET			28L
#define WAVE_HEADER_BLOCKALIGN_OFFSET		32L
#define WAVE_HEADER_BITSPERSAMPLE_OFFSET	34L
#define WAVE_HEADER_SUBCHUNK2ID_OFFSET		36L
#define WAVE_HEADER_SUBCHUNK2SIZE_OFFSET	40L
#define WAVE_HEADER_DATA_OFFSET				44L

class WaveFile 
{

private:

	int sample_rate;
	int num_channels;
	int num_samples;

	FILE *fp;

	long actual_pos;

	void WriteShortValue(long pos, unsigned short val);
	void WriteIntValue(long pos, unsigned int val);
	void WriteMarkValue(long pos, char *mark);

	int header_flag;

public:

	WaveFile ( const char *track_name );

	int PrepareHeader ( int nchann, int samplerate );
	int AddSamples ( void *data, int nsamples );

	void CloseFile();

	int HeaderPrepared();
	int FileReady();

	~WaveFile();

};


#endif