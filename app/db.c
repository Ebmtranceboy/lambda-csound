//
// db.c: Mock database implementation
// 
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//
#include "db.h"
#include "string.h"

#include "clib/mem.h"


struct Post_t {
    dstring id;
    char instance[5];
    unsigned int rate;
    float freq;
    float wet;
    int buf_size;
    float **buf;
};

Post* Post_new(dstring id, char* instance, unsigned int rate, int buf_size,float **buf) {
    Post* post = mem_alloc(sizeof(*post));
    post->id = id;
    strcpy(post->instance, instance);
    post->rate = rate;
    post->buf_size = buf_size;
    post->buf = buf;
    post->freq = 0.0f;
    post->wet = 0.0f;
    return post;
}

void Post_free(Post* post) {
    dstring_free(post->id);
    mem_free(post);
}

dstring Post_get_id(Post* post) {
    return post->id;
}

char* Post_get_instance(Post* post) {
    return post->instance;
}

unsigned int Post_get_rate(Post* post) {
    return post->rate;
}

int Post_get_buf_size(Post* post){
	return post->buf_size;
}

float** Post_get_buf(Post* post){
	return post->buf;
}

void Post_set_freq(Post* post, float freq){
	post->freq = freq;
}

float Post_get_freq(Post* post){
	return post->freq;
}

void Post_set_wet(Post* post, float wet){
	post->wet = wet;
}

float Post_get_wet(Post* post){
	return post->wet;
}
