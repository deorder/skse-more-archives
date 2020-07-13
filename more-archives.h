#pragma once

#ifndef __MORE_ARCHIVES_H__
#define __MORE_ARCHIVES_H__

#include "skse64/PluginAPI.h"

extern "C"
{

	bool ma_init(PluginHandle plugin, SKSEMessagingInterface* messaging, char *skse_version);

}

#endif