/*
 * Copyright (C) 2001,2002,2003,2004,2005 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3-or-later
 *
 */

 #pragma once

 #include <gst/gst.h>
 

 
#ifdef __cplusplus
extern "C" {
#endif

 void totem_gst_message_print (GstMessage *msg,
                   GstElement *element,
                   const char *filename);
 
 void totem_gst_disable_hardware_decoders (void);
 void totem_gst_ensure_newer_hardware_decoders (void);
 
 #ifdef __cplusplus
}
#endif