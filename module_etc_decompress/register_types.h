/*************************************************************************/
/*  register_types.h                                                     */
/*************************************************************************/

#pragma once
#include "modules/register_module_types.h"

void initialize_etcpak_decompress_module(ModuleInitializationLevel p_level);
void uninitialize_etcpak_decompress_module(ModuleInitializationLevel p_level);
void init_ver_regex();
void free_ver_regex();
