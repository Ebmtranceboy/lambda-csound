//
// plugin_manager.h: Plugin manager interface. It's used both by plugins
// and the main application.
// The main application will create a PluginManager and pass a pointer to
// plugins, which use the PluginManager_register_* functions to register
// hooks on the PluginManager.
//
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//
#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "clib/dstring.h"
#include "db.h"


//
// Types of hook callbacks registered by plugins
//

// Role hook. Will be called with: Post objects.
//
typedef void (*PluginRoleHook)(Post*);


typedef struct PluginManager_t PluginManager;

// Create a new plugin manager.
//
PluginManager* PluginManager_new();

// Free the memory of a plugin manager.
//
void PluginManager_free(PluginManager* pm);

// Register a hook for a specific role.
// Note: rolename is copied to an internal data structure.
//
void PluginManager_register_role_hook(PluginManager* pm, dstring rolename, 
                                      PluginRoleHook hook);


// Apply the registered role hooks to the given rolename/rolecontents,
// returning the string that should replace the role.
// The first plugin that agrees to handle this role is used. If no such plugin
// is found, NULL is returned.
//
void PluginManager_apply_role_hooks(PluginManager* pm, 
                                       dstring rolename,
                                       Post* post);

#endif /* PLUGIN_MANAGER_H */

