#include "plugin_manager.h"
#include "clib/mem.h"


// The hooks are contained in simple singly-linked lists
//

typedef struct PluginRoleHookList_t {
    // The role and instance for which this hook is registered, and the hook itself
    dstring role;
    int instance;
    PluginRoleHook hook;
    struct PluginRoleHookList_t* next;
} PluginRoleHookList;

struct PluginManager_t {
    PluginRoleHookList* role_hook_list;
};

PluginManager* PluginManager_new() {
    PluginManager* pm = mem_alloc(sizeof(*pm));
    pm->role_hook_list = NULL;
    return pm;
}

void PluginManager_free(PluginManager* pm) {
    PluginRoleHookList* role_plugin = pm->role_hook_list;
    while (role_plugin) {
        PluginRoleHookList* next = role_plugin->next;
        dstring_free(role_plugin->role);
        mem_free(role_plugin);
        role_plugin = next;
    }

    mem_free(pm);
}


void PluginManager_register_role_hook(PluginManager* pm, dstring rolename, int instance,
                                      PluginRoleHook hook) {
    PluginRoleHookList* node = mem_alloc(sizeof(*node));
    node->role = dstring_dup(rolename);
    node->instance = instance;
    node->hook = hook;
    node->next = pm->role_hook_list;
    pm->role_hook_list = node;
}

void PluginManager_apply_role_hooks(PluginManager* pm, dstring rolename,
                                       Post* post) {
    PluginRoleHookList* role_plugin = pm->role_hook_list;
    while (role_plugin) {
        if (dstring_compare(rolename, role_plugin->role) == 0 && role_plugin->instance == Post_get_instance(post)) {
            role_plugin->hook(post);
        }

        role_plugin = role_plugin->next;
    }
}