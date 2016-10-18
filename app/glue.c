#include "clib/dstring.h"
#include "db.h"
#include "plugin_manager.h"

mydsp* instr;

static void tt_role_hook(Post* post) {
    instr->fEntry0 = Post_get_freq(post);
    instr->fEntry1 = Post_get_wet(post);
    computemydsp(instr,Post_get_buf_size(post),NULL,Post_get_buf(post));
}


int init_tt(PluginManager* pm) {
    dstring rolename = dstring_new("tt");
    PluginManager_register_role_hook(pm, rolename, tt_role_hook);
    dstring_free(rolename);
    
    instr = newmydsp();
    initmydsp(instr,44100);
    return 1;
}

