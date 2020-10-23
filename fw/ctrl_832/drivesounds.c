#include <stdio.h>
#include <string.h>

#include "hardware.h"
#include "audio.h"
#include "rafile.h"

#include "drivesounds.h"


struct drivesound
{
	enum DriveSound_Type type;
	short *base;
	int length;
	int active;
	int chain;
	int cursor;
};

struct dsevent
{
	enum DriveSound_Type type;
	int timestamp;
};

#define DSEVENTBUFFER_SIZE 8

struct dseventbuffer
{
	volatile int in;
	volatile int out;
	int	loaded;
	int timestamp;
	int	cursor;
	int silence;
	int active;
	int playing;
	struct dsevent events[DSEVENTBUFFER_SIZE];
	struct drivesound sounds[DRIVESOUND_COUNT];
};

struct dseventbuffer drivesounds;


void drivesounds_queueevent(enum DriveSound_Type type)
{
	if(!drivesounds.active)
		drivesounds_start();
	drivesounds.events[drivesounds.in].type=type;
	drivesounds.events[drivesounds.in].timestamp=TIMER;
	drivesounds.in=(drivesounds.in+1)&(DSEVENTBUFFER_SIZE-1);
}


void drivesounds_stop()
{
	audio_stop();
	drivesounds.active=0;
	drivesounds.playing=0;
}


void drivesounds_start()
{
	drivesounds.active=1;
	drivesounds.silence=0;
	drivesounds.timestamp=TIMER;
	drivesounds.cursor=0;
	drivesounds.playing=0;
}


struct dsevent *drivesounds_nextevent()
{
	struct dsevent *result=0;
	if(drivesounds.in!=drivesounds.out)
		result=&drivesounds.events[drivesounds.out++];
	drivesounds.out&=(DSEVENTBUFFER_SIZE-1);

//	if(result)
//		printf("Found event %d at time %d\n",result->type,result->timestamp);
	return(result);
}


void drivesounds_finishsound(int i)
{
	int result=0;
	drivesounds.sounds[i].active=0;
	drivesounds.sounds[i].cursor=0;
	i=drivesounds.sounds[i].chain;
	if(i)
	{
//						printf("chained with sound %d\n",drivesounds.sounds[i].chain);
		drivesounds.sounds[i].active=1;
		drivesounds.sounds[i].cursor=0;
	}
}


void drivesounds_render(int timestamp)
{
	int samples;
	int *buf=(int *)AUDIO_BUFFER;
	short *src,*src2,*src3;
	int srcs,srcs2,srcs3;
	int i;
	int b1,b2;

	if(!drivesounds.loaded)
	{
		drivesounds.timestamp=timestamp;
		return;
	}

	samples=(timestamp-drivesounds.timestamp);
	if(samples<0)
		samples+=65536;
	if(samples>127)
		samples=127;
	samples*=256;

//	printf("Rendering %d samples\n",samples);

	while(samples)
	{
		int span=samples;

		src=src2=src3=0;

		for(i=0;i<DRIVESOUND_COUNT;++i)
		{
			if(drivesounds.sounds[i].active)
			{
				src=drivesounds.sounds[i].base;
				src+=drivesounds.sounds[i].cursor;
				srcs=i;
				++i;
				break;
			}
		}
		for(;i<DRIVESOUND_COUNT;++i)
		{
			if(drivesounds.sounds[i].active)
			{
				src2=drivesounds.sounds[i].base;
				src2+=drivesounds.sounds[i].cursor;
				srcs2=i;
				++i;
				break;
			}
		}
		for(;i<DRIVESOUND_COUNT;++i)
		{
			if(drivesounds.sounds[i].active)
			{
				src3=drivesounds.sounds[i].base;
				src3+=drivesounds.sounds[i].cursor;
				srcs3=i;
				++i;
				break;
			}
		}
		for(;i<DRIVESOUND_COUNT;++i)
		{
			drivesounds.sounds[i].active=0;
		}

		if(((AUDIO_BUFFER_SIZE-drivesounds.cursor)>>1)<span)
			span=(AUDIO_BUFFER_SIZE-drivesounds.cursor)>>1;
		if(src && ((drivesounds.sounds[srcs].length-drivesounds.sounds[srcs].cursor)<span))
			span=drivesounds.sounds[srcs].length-drivesounds.sounds[srcs].cursor;
		if(src2 && ((drivesounds.sounds[srcs2].length-drivesounds.sounds[srcs2].cursor)<span))
			span=drivesounds.sounds[srcs2].length-drivesounds.sounds[srcs2].cursor;
		if(src3 && ((drivesounds.sounds[srcs3].length-drivesounds.sounds[srcs3].cursor)<span))
			span=drivesounds.sounds[srcs3].length-drivesounds.sounds[srcs3].cursor;

		buf=(int *)AUDIO_BUFFER;
		buf+=drivesounds.cursor>>1;
		b1=drivesounds.cursor&0x4000;
		drivesounds.cursor+=2*span;
		drivesounds.cursor&=(AUDIO_BUFFER_SIZE)-1;
		b2=drivesounds.cursor&0x4000;

 		samples-=span;

		if(src3)
		{
			drivesounds.silence=0;
			drivesounds.sounds[srcs].cursor+=span;
			drivesounds.sounds[srcs2].cursor+=span;
			drivesounds.sounds[srcs3].cursor+=span;
			if(!(AUDIO&AUDIOF_AMIGA))
			{
				while(span--)
				{
					int c=*src++;
					c+=*src2++;
					c+=*src3++;
					c=((c&255)<<8) | ((c>>8)&255);
					c=c*0x10001;
					*buf++=c;
				}
			}
			if(drivesounds.sounds[srcs].cursor>=drivesounds.sounds[srcs].length)
				drivesounds_finishsound(srcs);
			if(drivesounds.sounds[srcs2].cursor>=drivesounds.sounds[srcs2].length)
				drivesounds_finishsound(srcs2);
			if(drivesounds.sounds[srcs3].cursor>=drivesounds.sounds[srcs3].length)
				drivesounds_finishsound(srcs3);
		}
		else if(src2)
		{
			drivesounds.silence=0;
			drivesounds.sounds[srcs].cursor+=span;
			drivesounds.sounds[srcs2].cursor+=span;
			if(!(AUDIO&AUDIOF_AMIGA))
			{
				while(span--)
				{
					int c=*src++;
					c+=*src2++;
					c=((c&255)<<8) | ((c>>8)&255);
					c=c*0x10001;
					*buf++=c;
				}
			}
			if(drivesounds.sounds[srcs].cursor>=drivesounds.sounds[srcs].length)
				drivesounds_finishsound(srcs);
			if(drivesounds.sounds[srcs2].cursor>=drivesounds.sounds[srcs2].length)
				drivesounds_finishsound(srcs2);
		}
		else if(src)
		{
			drivesounds.silence=0;
			drivesounds.sounds[srcs].cursor+=span;
			if(!(AUDIO&AUDIOF_AMIGA))
			{
				while(span--)
				{
					int c=*src++;
					c=((c&255)<<8) | ((c>>8)&255);
					c=c*0x10001;
					*buf++=c;
				}
			}
			if(drivesounds.sounds[srcs].cursor>=drivesounds.sounds[srcs].length)
				drivesounds_finishsound(srcs);
		}
		else if(span)
		{
			drivesounds.silence+=span;
			if(!(AUDIO&AUDIOF_AMIGA))
			{
				while(span--)
					*buf++=0;
			}
		}
	}

	if(drivesounds.silence>=AUDIO_BUFFER_SIZE)
		drivesounds_stop();
	else if(drivesounds.active && !drivesounds.playing && drivesounds.cursor&0x4000)
		audio_start();	// Start playing audio once the first buffer is full...

	drivesounds.timestamp=timestamp;
}


/* Pick a step sound at random, while keeping no more than "maxactive" other step sounds playing. */

int pickstep(int maxactive)
{
	int bestcursor=0;
	int best=0;
	int count=4;
	int step;
	while(count>maxactive)
	{
		best=0;
		bestcursor=0;
		count=0;
		for(step=0;step<4;++step)
		{
			if(drivesounds.sounds[DRIVESOUND_STEP+step].active)
			{
				++count;
				if(drivesounds.sounds[DRIVESOUND_STEP+step].cursor>=bestcursor)
				{
					best=step;	
					bestcursor=drivesounds.sounds[DRIVESOUND_STEP+step].cursor;
				}
			}
		}
		if(count>maxactive)
		{
			drivesounds.sounds[DRIVESOUND_STEP+best].active=0;
			drivesounds.sounds[DRIVESOUND_STEP+best].cursor=0;
		}
	}
	step=TIMER&3;
	while(drivesounds.sounds[DRIVESOUND_STEP+step].active)
		step=(step+1)&3;
	return(step);
}


int drivesounds_fill()
{
	struct dsevent *dse;
	while(dse=drivesounds_nextevent())
	{
		drivesounds_render(dse->timestamp);
		drivesounds.sounds[dse->type].cursor=0;
		switch(dse->type)
		{
			case DRIVESOUND_MOTORSTART:
				drivesounds.sounds[DRIVESOUND_MOTORLOOP].chain=DRIVESOUND_MOTORLOOP;
				if(drivesounds.sounds[DRIVESOUND_MOTORSTOP].active)
				{
					drivesounds.sounds[DRIVESOUND_MOTORSTOP].active=0;
					drivesounds.sounds[DRIVESOUND_MOTORLOOP].active=1;
				}
				else if(drivesounds.sounds[DRIVESOUND_MOTORLOOP].active)
				{
					drivesounds.sounds[DRIVESOUND_MOTORLOOP].chain=DRIVESOUND_MOTORLOOP;
				}
				else
				{
					drivesounds.sounds[DRIVESOUND_MOTORSTART].chain=DRIVESOUND_MOTORLOOP;
					drivesounds.sounds[DRIVESOUND_MOTORSTART].active=1;
				}
				break;
			case DRIVESOUND_MOTORSTOP:
				drivesounds.sounds[DRIVESOUND_MOTORLOOP].chain=DRIVESOUND_MOTORSTOP;
				break;
			case DRIVESOUND_STEP:
				dse->type=DRIVESOUND_STEP+pickstep(1);	/* Pick a step sound at "random" */
				drivesounds.sounds[dse->type].active=1;
				drivesounds.sounds[dse->type].chain=0;				
			default:
				drivesounds.sounds[dse->type].active=1;
				drivesounds.sounds[dse->type].chain=0;
				break;
		}
	}
//	printf("Rendering remainder\n");
	if(drivesounds.active)
	{
		drivesounds_render(TIMER);
	}
	return(drivesounds.active);
}


int drivesounds_init(const char *filename)
{
	int result=0;
	char *buf=AUDIO_BUFFER+2*AUDIO_BUFFER_SIZE;
	RAFile file;
	drivesounds.in=0;
	drivesounds.out=0;
	drivesounds.loaded=0;
	if(RAOpen(&file,filename))
	{
		int size=file.size;
		if(size<(512*1024-2*AUDIO_BUFFER_SIZE))
		{
			printf("Audio file - length %d\n",file.size);		
			RARead(&file,buf,file.size);
			if(strncmp(buf,"DRIVESND",8)==0)
			{
				int id=0;
				buf+=8;
				id=*(long *)buf;
				buf+=4;
				size=*(long *)buf;
				buf+=4;
				while(size)
				{
					if(size && id<DRIVESOUND_COUNT)
					{
						printf("Sound %d at %x, length %d\n",id,buf,size);
						drivesounds.sounds[id].base=(short *)buf;
						drivesounds.sounds[id].length=size/2; /* Size needs to be in 16-bit words */
						buf+=size;
						result=1;
						id=*(long *)buf;
						buf+=4;
						size=*(long *)buf;
						buf+=4;
					}
					else
					{
						size=0;
					}
				}
				drivesounds.loaded=1;
			}
			else
				printf("Bad signature in DriveSounds file\n");
		}
		else
			printf("Drive sounds file too large\n");
	}
	drivesounds.cursor=0;
	drivesounds.active=0;
	drivesounds.playing=0;
	return(result);
}

