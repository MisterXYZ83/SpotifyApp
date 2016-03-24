#include "wav_file.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


WaveFile::WaveFile ( const char *track_name )
{
	char fullpath[1024];
	memset(fullpath, 0, 1024);
	
	this->fp = 0;

	if ( track_name )
	{
		//_snprintf(fullpath, 1023, "%s/%s-%s.wav", dest_dir, album_name, track_name);
		//_snprintf(fullpath, 1023, "prova.wav");
		_snprintf(fullpath, 1023, "%s", track_name);

		this->fp = fopen(fullpath, "wb");

		if ( this->fp )
		{
			//file creato, scrivo l'header
			//header lungo 44 byte

			unsigned char header[44];
			memset(header, 0, 44);

			int p = fwrite(header, 1, 44, this->fp);

			//scrivo i campi fissi
			
			PrepareHeader(0,0);

			this->header_flag = 0;
			this->num_samples = 0;
		}
	}

}

void WaveFile::WriteShortValue(long pos, unsigned short val)
{
	if ( this->fp )
	{
		long old_pos = ftell(this->fp);

		fseek(this->fp, pos, SEEK_SET);

		fwrite(&val, 1, 2, this->fp);

		fflush(this->fp);
		
		fseek(this->fp, old_pos, SEEK_SET);
	}
}

void WaveFile::WriteIntValue(long pos, unsigned int val)
{
	if ( this->fp )
	{
		long old_pos = ftell(this->fp);

		fseek(this->fp, pos, SEEK_SET);

		fwrite(&val, 1, 4, this->fp);

		fflush(this->fp);

		fseek(this->fp, old_pos, SEEK_SET);
	}
}

void WaveFile::WriteMarkValue(long pos, char *mark)
{
	if ( this->fp )
	{
		long old_pos = ftell(this->fp);

		fseek(this->fp, pos, SEEK_SET);

		fwrite(mark, 1, 4, this->fp);
		
		fflush(this->fp);

		fseek(this->fp, old_pos, SEEK_SET);
	}
}



int WaveFile::PrepareHeader ( int nchann, int samplerate )
{
	this->sample_rate = samplerate;
	this->num_channels = nchann;

	WriteMarkValue(WAVE_HEADER_CHUNKID_OFFSET, "RIFF");
	WriteIntValue(WAVE_HEADER_CHUNKSIZE_OFFSET, 0);
	WriteMarkValue(WAVE_HEADER_FORMAT_OFFSET, "WAVE");

	WriteMarkValue(WAVE_HEADER_SUBCHUNK1ID_OFFSET, "fmt ");
	WriteIntValue(WAVE_HEADER_SUBCHUNK1SIZE_OFFSET, 16);
	WriteShortValue(WAVE_HEADER_AUDIOFORMAT_OFFSET, 1);
	WriteShortValue(WAVE_HEADER_NUMCHANNELS_OFFSET, nchann);
	WriteIntValue(WAVE_HEADER_SAMPLERATE_OFFSET, samplerate);
	WriteIntValue(WAVE_HEADER_BYTERATE_OFFSET, 2 * this->sample_rate * this->num_channels);
	WriteShortValue(WAVE_HEADER_BLOCKALIGN_OFFSET, 2 * this->num_channels);
	WriteShortValue(WAVE_HEADER_BITSPERSAMPLE_OFFSET, 16);
	
	WriteMarkValue(WAVE_HEADER_SUBCHUNK2ID_OFFSET, "data");
	WriteIntValue(WAVE_HEADER_SUBCHUNK2SIZE_OFFSET, 0);

	
	this->actual_pos = WAVE_HEADER_DATA_OFFSET;
	
	this->header_flag = 1;
	
	return 1;
}

int WaveFile::AddSamples ( void *data, int nsamples )
{
	int nwritten = 0;

	//scrivo su file
	if ( this->fp && nsamples > 0 )
	{
		nwritten = fwrite(data, 1, 2 * nsamples * this->num_channels, this->fp);

		fflush(this->fp);

		nwritten /= ( 2 * this->num_channels);
		
		this->num_samples += nwritten;

		//aggiorno i campi
		WriteIntValue(WAVE_HEADER_CHUNKSIZE_OFFSET, 36 + this->num_samples * this->num_channels * 2);
		WriteIntValue(WAVE_HEADER_SUBCHUNK2SIZE_OFFSET, this->num_samples * this->num_channels * 2);
	}

	return nwritten;
}

int WaveFile::FileReady()
{
	return (this->fp != 0);
}

int WaveFile::HeaderPrepared()
{
	return this->header_flag;
}

void WaveFile::CloseFile()
{
	if ( this->fp )
	{
		fclose(this->fp);
		fp = 0;
	}
}

WaveFile::~WaveFile()
{

}
