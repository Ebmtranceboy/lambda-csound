//
// db.h: Mock database interface. It has no real functionality, consisting
// of some stubs that simulate what a real DB interface would look like.
// 
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//
#ifndef DB_H
#define DB_H

#include "clib/dstring.h"

// Opaque objects: a Post
typedef struct Post_t Post;


// Methods on a Post object.
// The Post object takes ownership of all the dynamic strings passed to
// it; its accessors return references to these objects, which must not be
// freed or modified.
//
Post* Post_new(dstring id, char* instance, unsigned int rate,int size,float** buf);
void Post_free(Post* post);
dstring Post_get_id(Post* post);
char* Post_get_instance(Post* post);
unsigned int Post_get_rate(Post* post);
int Post_get_buf_size(Post* post);
float** Post_get_buf(Post* post);
void Post_set_freq(Post* post, float freq);
float Post_get_freq(Post* post);
void Post_set_wet(Post* post, float freq);
float Post_get_wet(Post* post);

#endif /* DB_H */

