//
// main.c: The main program
//
// Joël Duet (joel.duet@gmail.com)
// This code is in the public domain
//
#include "clib/die.h"
#include "clib/mem.h"
#include "db.h"
#include "synthesize.h"
#include "plugin_discovery.h"
#include "plugin_manager.h"
#include <signal.h>   
#include <stdio.h>   
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <alloca.h>

#define POLY 10
#define GAIN 1.0
#define BUFSIZE 512
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
#define INT_MAX (32767)
#define max( a, b ) ( ( a > b) ? a : b )
#define min( a, b ) ( ( a < b) ? a : b )

snd_seq_t *seq_handle;
snd_pcm_t *playback_handle;
double pitch;
int note[POLY], gate[POLY], note_active[POLY];
unsigned int rate;
int portid;
Post* post[POLY];
PluginManager* pm;
void* pdstate;
double env_time[POLY], env_level[POLY], velocity[POLY];
short* bufcard;
    

    double attack = 0.01;
    double decay = 0.1;
    double sustain = 0.2;
    double release = 0.5;
    

// Read a whole file into a dstring and return it
dstring read_whole_file(FILE* file) {
    static const size_t BUF_SIZE = 1024;
    char buf[BUF_SIZE];
    dstring s = dstring_empty();

    while (fgets(buf, BUF_SIZE, file)) {
        dstring_concat_cstr(s, buf);
    }

    return s;
}

snd_seq_t *open_seq() {

    snd_seq_t *seq_handle;

    rate = 44100;    
    if (snd_seq_open(&seq_handle, "default", \
        SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer.\n");
        exit(1);
    }
    snd_config_update_free_global();
    snd_seq_set_client_name(seq_handle, "faustSynth");
    if ((portid=snd_seq_create_simple_port(seq_handle, \
        "faustSynth",
        SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
        fprintf(stderr, "Error creating sequencer port.\n");
        exit(1);
    }
    printf("\nOn client id: %d, ",snd_seq_client_id(seq_handle));
    printf("\nsimple seq port created: %d\n",portid);
    return(seq_handle);
}

snd_pcm_t *open_pcm(char *pcm_name) {

    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_sw_params_t *sw_params;
            
    if (snd_pcm_open (&playback_handle, \
        pcm_name, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf (stderr, "cannot open audio device %s\n", \
           pcm_name);
        exit (1);
    }
    snd_config_update_free_global();
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(playback_handle, hw_params);
    snd_pcm_hw_params_set_access(playback_handle, \
              hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(playback_handle, \
              hw_params,SND_PCM_FORMAT_S16_LE); // SND_PCM_FORMAT_FLOAT64_BE//SND_PCM_FORMAT_S16_LE
    snd_pcm_hw_params_set_rate_near(playback_handle, \
              hw_params, &rate, 0);
    printf("\n rate: %u\n",rate);
    snd_pcm_hw_params_set_channels(playback_handle, \
              hw_params, 2);
    snd_pcm_hw_params_set_periods(playback_handle, \
              hw_params, 2, 0);
    snd_pcm_hw_params_set_period_size(playback_handle, \
              hw_params, BUFSIZE, 0);
    snd_pcm_hw_params(playback_handle, hw_params);
   
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(playback_handle, sw_params);
    snd_pcm_sw_params_set_avail_min(playback_handle, \
              sw_params, BUFSIZE);
    snd_pcm_sw_params(playback_handle, sw_params);
    return(playback_handle);
}

float wet[POLY];

int midi_callback() {

    snd_seq_event_t *ev;
    int l1;
  
    do {
        snd_seq_event_input(seq_handle, &ev);
        switch (ev->type) {
            /*case SND_SEQ_EVENT_PITCHBEND:
                pitch = (double)ev->data.control.value / 8192.0;
                break;*/
	    
            /*case SND_SEQ_EVENT_CONTROLLER:
                if (ev->data.control.param == 1) {
                    modulation = \
                         (double)ev->data.control.value / 10.0;
                } 
                break;*/
            case SND_SEQ_EVENT_NOTEON:
            printf("NoteOn: %d\n", ev->data.note.note);
                for (l1 = 0; l1 < POLY; l1++) {
                        if (!note_active[l1]) {
                                wet[l1]=0.93;
                                note[l1] = ev->data.note.note;
                                velocity[l1] = \
                                ev->data.note.velocity / 127.0;
                                env_time[l1] = 0;
                                gate[l1] = 1;
                                note_active[l1] = 1;
                                break;
                        }
                }
                break;        
            case SND_SEQ_EVENT_NOTEOFF:
    	    for (l1 = 0; l1 < POLY; l1++) {
                    if (gate[l1] && \
                         note_active[l1] && \
                        (note[l1] == ev->data.note.note)) {
                        env_time[l1] = 0;
                        gate[l1] = 0;
                    }
                }
                break;        
        }
        snd_seq_free_event(ev);
    } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
    return (0);
}

double envelope(int *note_active, int gate, double *env_level, \
                double t, \
                double attack, double decay, \
                double sustain, double release) {
    if (gate)  {
        if (t > attack + decay) return(*env_level = sustain);
        if (t > attack) return(*env_level = \
                      1.0 - (1.0 - sustain) * \
                      (t - attack) / decay);
        return(*env_level = t / attack);
    } else {
        if (t > release) {
            if (note_active) *note_active = 0;
            return(*env_level = 0);
        }
        return(*env_level * (1.0 - t / release));
    }
}

    
int playback_callback (snd_pcm_sframes_t nframes) {
    float freq_note;
    int l1, l2;
    
    float **buf;
    
    memset(bufcard, 0, nframes * 4);
    /*
    for(int i=0; i< 2*nframes;i++){
	buf[0][i] = 0.0f;
	buf[1][i] = 0.0f;
    }
    */

    for (l1 = 0; l1 < POLY; l1++) {
        if (note_active[l1]) {
          
	    freq_note = 8.176 * exp((double)(note[l1]) * \
                 log(2.0)/12.0);
	    Post_set_freq(post[l1],freq_note);
	    wet[l1] *= 0.995;
	    Post_set_wet(post[l1],wet[l1]);
                synthesize(pm, post[l1]);
        buf = Post_get_buf(post[l1]);
	    
	    for (l2 = 0; l2 < nframes; l2++) {
	        int c=0; // left
		float sound = buf[c][l2];
		 sound *= GAIN*envelope(&note_active[l1], \
                           gate[l1], 
                           &env_level[l1], 
                           env_time[l1], 
                           attack, decay, sustain, release)
                             * velocity[l1]/POLY;

		//bufcard[c+l2*2] += (short)(max(min(sound,1.0f),-1.0f)*(float)(INT_MAX/10));
		bufcard[c+l2*2] += (short)(sound*(float)INT_MAX);
                bufcard[1+l2*2] = bufcard[c+l2*2];
		
		env_time[l1] += 1.0 / 44100.0;
		
	    }
	}
    }
    
    
    return snd_pcm_writei (playback_handle, bufcard, nframes); 
    
 
}

void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);
     printf(" <-- looks like Ctrl-C.\n"
            "Do you really want to quit? [y/n] ");
     c = getchar();
     if (c == 'y' || c == 'Y'){
	for(int i=0;i<POLY;i++) Post_free(post[i]);
	PluginManager_free(pm);
	cleanup_plugins(pdstate);
    	snd_pcm_close (playback_handle);
	snd_seq_close (seq_handle);
        exit(0);
     }
     else
          signal(SIGINT, INThandler);
     getchar(); // Get new line character
}

float** createArray(int m, int n){
    float* values = calloc(m*n, sizeof(float));
    
    float** rows = malloc(n*sizeof(float*));
    for (int i=0; i<n; i++)
    {
        rows[i] = values + i*m;
    }
    return rows;
}

int main (int argc, char *argv[]) {

    int nfds, seq_nfds, l1;
    struct pollfd *pfds;
   
    pitch = 0;

    playback_handle = open_pcm(argv[1]);
    seq_handle = open_seq();
snd_seq_connect_from (seq_handle, portid, 20, 0);

    seq_nfds = \
         snd_seq_poll_descriptors_count(seq_handle, POLLIN);
    nfds = snd_pcm_poll_descriptors_count (playback_handle);
    pfds = (struct pollfd *)alloca(sizeof(struct pollfd) * \
         (seq_nfds + nfds));
    snd_seq_poll_descriptors(seq_handle, pfds, seq_nfds, POLLIN);
    snd_pcm_poll_descriptors (playback_handle, 
                             pfds+seq_nfds, nfds);
    
    bufcard = (short *) malloc (2 * sizeof (short) * BUFSIZE);
    
    dstring id = dstring_new("logan");
    // The Post object takes ownership of the strings passed to it
    
    float ** buf;
    
    for(int i=0; i<POLY; i++){
            buf= createArray(BUFSIZE,2);
            post[i] = Post_new(id,i,rate,BUFSIZE,buf);
            wet[i] = 0.93;
    }

    // Perform plugin discovery in the "plugins" directory relative to the
    // working directory.
    pm = PluginManager_new();
    dstring dirname = dstring_new("plugin");
    pdstate = discover_plugins(dirname, pm, POLY);  
    dstring_free(dirname);

    for (l1 = 0; l1 < POLY; note_active[l1++] = 0);
    
    signal(SIGINT, INThandler);
     
    while (1) {
        if (poll (pfds, seq_nfds + nfds, 1000) > 0) {
            for (l1 = 0; l1 < seq_nfds; l1++) {
               if (pfds[l1].revents > 0) midi_callback();
            }
            for (l1 = seq_nfds; l1 < seq_nfds + nfds; l1++) {    
                if (pfds[l1].revents > 0) { 
                    if (playback_callback(BUFSIZE) < BUFSIZE) {
                        fprintf (stderr, "xrun !\n");
                        snd_pcm_prepare(playback_handle);
                    }
                }
            }        
        }
    }
    return (0);
}
     
   
