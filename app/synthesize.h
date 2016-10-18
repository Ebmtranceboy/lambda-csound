//
//  synthesize.h: Core synthesize interface
//
#ifndef SYNTHESIZE_H
#define SYNTHESIZE_H

#include "clib/dstring.h"
#include "db.h"
#include "plugin_manager.h"


// Calls into plugins (represented by PluginManager) if applicable.
//
void synthesize(PluginManager* pm, Post* post);

#endif /* SYNTHESIZE_H */

