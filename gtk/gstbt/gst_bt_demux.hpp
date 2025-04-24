/***********************This version USed by transmission-gtk torrent client*************************/

/* Gst-Bt - BitTorrent related GStreamer elements
 * Copyright (C) 2015 Jorge Luis Zapata
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GST_BT_DEMUX_H
#define GST_BT_DEMUX_H


#include <gst/gst.h>
// #include <gst/base/gstadapter.h>

//libtorrent
#include "libtorrent/session.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/time.hpp"
#include "libtorrent/units.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/load_torrent.hpp"
#include "libtorrent/download_priority.hpp"


G_BEGIN_DECLS

#define GST_TYPE_BT_DEMUX            (gst_bt_demux_get_type())
#define GST_BT_DEMUX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                         GST_TYPE_BT_DEMUX, GstBtDemux))
#define GST_BT_DEMUX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                         GST_TYPE_BT_DEMUX, GstBtDemuxClass))
#define GST_BT_DEMUX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                         GST_TYPE_BT_DEMUX, GstBtDemuxClass))
#define GST_IS_BT_DEMUX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                         GST_TYPE_BT_DEMUX))
#define GST_IS_BT_DEMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                         GST_TYPE_BT_DEMUX))

#define GST_TYPE_BT_DEMUX_STREAM            (gst_bt_demux_stream_get_type())
#define GST_BT_DEMUX_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),\
                                                GST_TYPE_BT_DEMUX_STREAM, GstBtDemuxStream))
#define GST_BT_DEMUX_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),\
                                                GST_TYPE_BT_DEMUX_STREAM, GstBtDemuxStreamClass))
#define GST_BT_DEMUX_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),\
                                                GST_TYPE_BT_DEMUX_STREAM, GstBtDemuxStreamClass))
#define GST_IS_BT_DEMUX_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),\
                                               GST_TYPE_BT_DEMUX_STREAM))
#define GST_IS_BT_DEMUX_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),\
                                               GST_TYPE_BT_DEMUX_STREAM))







typedef struct _GstBtDemuxStream
{
  GstPad pad;

  gchar *path;

  gint current_piece;
  gint start_offset;
  gint start_piece;
  gint end_offset;
  gint end_piece;
  gint last_piece;

  gint file_idx;

  /*seeking segment range */
  gint64 start_byte_global;
  gint64 end_byte_global;
  gint64 start_byte;
  gint64 end_byte;

  /*pending segment flag*/
  gboolean pending_segment;
  gboolean flush_start_sent;
  gboolean is_user_seek;
  gboolean moov_after_mdat;

  gboolean requested;
  gboolean added;
  
  
  //finished means this stream(movie) has completed downloading, holds all pieces belonging to this stream now
  gboolean finished;

  //buffering means this stream(movie) needs downloading
  gboolean buffering;
  gint buffering_level;
  gint buffering_count;

  GStaticRecMutex *lock;

  //push ipc_data in read_piece_alert handling code <====> retrieve ipc_data in bt_demux_stream_push_loop
  GAsyncQueue *ipc;

  //gboolean array, signaling whether piece needs to downloading/buffering in Three-Piece-Area
  GArray* cur_buffering_flags;

} GstBtDemuxStream;



typedef struct _GstBtDemuxStreamClass {
  GstPadClass parent_class;
} GstBtDemuxStreamClass;


GType gst_bt_demux_stream_get_type (void);

typedef struct _GstBtDemux
{
  GstElement parent;
 
  GMutex *streams_lock;
  GSList *streams;
  gchar *requested_streams;
  gboolean typefind;

  //current playing/streaming file_index of video within that torrent
  gint cur_streaming_fileidx;

  //finished doesn't means this torrent have finished downloading, we are seeder now
  gboolean finished;

  //buffering means we are in downloading state
  gboolean buffering;
  gint buffer_pieces;

  //piece related info 
  gint num_video_file;
  gint total_num_blocks;
  gint total_num_pieces;
  gint num_blocks_last_piece;
  gint blocks_per_piece_normal;


  gpointer tor_handle; // from transmission Session feed us

  
} GstBtDemux;




typedef struct _GstBtDemuxClass
{
  GstElementClass parent_class;

} GstBtDemuxClass;


GType gst_bt_demux_get_type (void);

G_END_DECLS









#ifdef __cplusplus
extern "C" {
#endif
//Exposed Functions to totem for feed data(alerts ... etc) to us
void btdemux_feed_playlist(GObject *thiz, libtorrent::torrent_handle const handle_copy);
//read_piece_alert, piece_finished_alert,  
void btdemux_feed_read_piece_alert(GObject* thiz, libtorrent::alert* a);
void btdemux_feed_piece_finished_alert(GObject* thiz, libtorrent::alert* a);
#ifdef __cplusplus
}
#endif



#endif //GST_BT_DEMUX_H
