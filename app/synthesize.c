//
// htmlize.c: Core htmlize functionality
// 
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//
#include "synthesize.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>


void synthesize(PluginManager* pm, Post* post) {
    dstring rolename = Post_get_id(post);

    PluginManager_apply_role_hooks(pm, rolename, post);
}

