/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */


/**
 * \page page_pjmedia_samples_aectest_c Samples: AEC Test (aectest.c)
 *
 * Play a file to speaker, run AEC, and record the microphone input
 * to see if echo is coming.
 *
 * This file is pjsip-apps/src/samples/aectest.c
 *
 * \includelineno aectest.c
 */
#include <pjmedia/buffer_port.h>
#include <pjmedia.h>
#include <pjlib-util.h>	/* pj_getopt */
#include <pjlib.h>

#define THIS_FILE   "aectest.c"
#define PTIME	    20
#define TAIL_LENGTH 200

static const char *desc = 
" FILE		    						    \n"
"		    						    \n"
"  aectest.c	    						    \n"
"		    						    \n"
" PURPOSE	    						    \n"
"		    						    \n"
"  Test the AEC effectiveness.					    \n"
"		    						    \n"
" USAGE		    						    \n"
"		    						    \n"
"  aectest [options] <PLAY.WAV> <REC.WAV> <OUTPUT.WAV>		    \n"
"		    						    \n"
"  <PLAY.WAV>   is the signal played to the speaker.		    \n"
"  <REC.WAV>    is the signal captured from the microphone.	    \n"
"  <OUTPUT.WAV> is the output file to store the test result	    \n"
"\n"
" options:\n"
"  -d  The delay between playback and capture in ms, at least 25 ms.\n"
"      Default is 25 ms. See note below.                            \n"
"  -l  Set the echo tail length in ms. Default is 200 ms	    \n"
"  -r  Set repeat count (default=1)                                 \n"
"  -a  Algorithm: 0=default, 1=speex, 2=echo suppress, 3=WebRtc	    \n"
"  -i  Interactive						    \n"
"\n"
" Note that for the AEC internal buffering mechanism, it is required\n"
" that the echoed signal (in REC.WAV) is delayed from the           \n"
" corresponding reference signal (in PLAY.WAV) at least as much as  \n"
" frame time + PJMEDIA_WSOLA_DELAY_MSEC. In this application, frame \n"
" time is 20 ms and default PJMEDIA_WSOLA_DELAY_MSEC is 5 ms, hence \n"
" 25 ms delay is the minimum value.                                 \n";

/* 
 * Sample session:
 *
 * -d 100 -a 1 ../bin/orig8.wav ../bin/echo8.wav ../bin/result8.wav 
 */

static void app_perror(const char *sender, const char *title, pj_status_t st)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(st, errmsg, sizeof(errmsg));
    PJ_LOG(3,(sender, "%s: %s", title, errmsg));
}

pj_status_t play_cb(char *buf, unsigned int *len, unsigned int maxcap)
{
	pj_memset(buf, 0, maxcap / 2);
	*len = maxcap / 2;
	return PJ_SUCCESS;
}

pj_status_t rec_cb(char *buf, unsigned int *len, unsigned int maxcap)
{
	pj_memset(buf, 0, maxcap / 2);
	*len = maxcap / 2;
	return PJ_SUCCESS;
}


/*
 * main()
 */
int main(int argc, char *argv[])
{
    pj_caching_pool cp;
    pj_pool_t	  *pool;
    pj_status_t status;
    pjmedia_echo_state *ec;
    pjmedia_frame play_frame, rec_frame;
    unsigned opt = 0;
    unsigned latency_ms = 25;
    unsigned tail_ms = TAIL_LENGTH;
    pj_timestamp t0, t1;
    int i, repeat=1, interactive=0, c;
	struct buffer_port *play_port;
	struct buffer_port *rec_port;

	unsigned int clock_rate = 16000; 
	unsigned int channel_count = 1;
	unsigned int bits_per_sample = 16; 
	unsigned int samples_per_frame = clock_rate / 1000 * 10; // 10ms一帧
	unsigned int buffercap = bits_per_sample / 2 * samples_per_frame * 20; 
    pj_optind = 0;
    while ((c=pj_getopt(argc, argv, "d:l:a:r:i")) !=-1) {
	switch (c) {
	case 'd':
	    latency_ms = atoi(pj_optarg);
	    if (latency_ms < 25) {
		puts("Invalid delay");
		puts(desc);
	    }
	    break;
	case 'l':
	    tail_ms = atoi(pj_optarg);
	    break;
	case 'a':
	    {
		int alg = atoi(pj_optarg);
		switch (alg) {
		case 0:
		    opt = 0;
		    break;
		case 1:
		    opt = PJMEDIA_ECHO_SPEEX;
		    break;
		case 2:
		    opt = PJMEDIA_ECHO_SIMPLE;
		    break;
		case 3:
		    opt = PJMEDIA_ECHO_WEBRTC;
		    break;
		default:
		    puts("Invalid algorithm");
		    puts(desc);
		    return 1;
		}
	    }
	    break;
	case 'r':
	    repeat = atoi(pj_optarg);
	    if (repeat < 1) {
		puts("Invalid repeat count");
		puts(desc);
		return 1;
	    }
	    break;
	case 'i':
	    interactive = 1;
	    break;
	}
    }

    if (argc - pj_optind != 3) {
	puts("Error: missing argument(s)");
	puts(desc);
	return 1;
    }

    /* Must init PJLIB first: */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    /* Must create a pool factory before we can allocate any memory. */
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    /* Create memory pool for our file player */
    pool = pj_pool_create( &cp.factory,	    /* pool factory	    */
			   "wav",	    /* pool name.	    */
			   4000,	    /* init size	    */
			   4000,	    /* increment size	    */
			   NULL		    /* callback on error    */
			   );

    /* Open wav_play */
    play_port = create_buffer_port(
		pool,
		clock_rate,
		channel_count,
		bits_per_sample,
		samples_per_frame,
		buffercap,
		play_cb
	);
    
    /* Open recorded wav */
    rec_port = create_buffer_port(
		pool,
		clock_rate,
		channel_count,
		bits_per_sample,
		samples_per_frame,
		buffercap,
		rec_cb
	);


    

    /* Create output wav */

    /* Create echo canceller */
    status = pjmedia_echo_create2(pool, clock_rate,
				  channel_count,
				  samples_per_frame,
				  tail_ms, latency_ms,
				  opt, &ec);
    if (status != PJ_SUCCESS) {
	app_perror(THIS_FILE, "Error creating EC", status);
	return 1;
    }


    /* Processing loop */
    play_frame.buf = pj_pool_alloc(pool, samples_per_frame <<1);
    rec_frame.buf = pj_pool_alloc(pool, samples_per_frame <<1);
    pj_get_timestamp(&t0);
    for (i=0; i < repeat; ++i) {
	for (int j = 0; j < 1000; j++) {
	    play_frame.size = samples_per_frame << 1;
		pjmedia_port_get_frame(play_port, &play_frame);

	    status = pjmedia_echo_playback(ec, (short*)play_frame.buf);

	    rec_frame.size = samples_per_frame << 1;

		pjmedia_port_get_frame(rec_port, &rec_frame);
	    status = pjmedia_echo_capture(ec, (short*)rec_frame.buf, 0);

	    //status = pjmedia_echo_cancel(ec, (short*)rec_frame.buf, 
	    //			     (short*)play_frame.buf, 0, NULL);
	}

    }
    pj_get_timestamp(&t1);

    PJ_LOG(3,(THIS_FILE, "Completed in %u msec\n", pj_elapsed_msec(&t0, &t1)));

    /* Destroy file port(s) */
	pjmedia_port_destroy(play_port);
	pjmedia_port_destroy(rec_port);

    /* Destroy ec */
    pjmedia_echo_destroy(ec);

    /* Release application pool */
    pj_pool_release( pool );

    /* Destroy media endpoint. */

    /* Destroy pool factory */
    pj_caching_pool_destroy( &cp );

    /* Shutdown PJLIB */
    pj_shutdown();

    if (interactive) {
	char s[10], *dummy;
	puts("ENTER to quit");
	dummy = fgets(s, sizeof(s), stdin);
	PJ_UNUSED_ARG(dummy);
    }

    /* Done. */
    return 0;
}

