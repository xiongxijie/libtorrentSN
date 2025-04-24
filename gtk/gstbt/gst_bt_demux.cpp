/***********************This Version USed by transmission-gtk *************************/

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

/*
 * TODO:
 * + Implement queries:
 *   buffering level
 *   position
 * + Implenent the element to work in pull mode
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gst_bt.h"
#include "gst_bt_demux.hpp"
#include <gst/base/gsttypefindhelper.h>
#include <glib/gstdio.h>

#include <iterator>
#include <vector>
#include <string>
#include <memory>
#include <cstdio>
#include <functional>
#include <string_view>




#define DEFAULT_TYPEFIND TRUE
#define DEFAULT_BUFFER_PIECES 3
#define DEFAULT_DIR "btdemux"

GST_DEBUG_CATEGORY_EXTERN (gst_bt_demux_debug);
#define GST_CAT_DEFAULT gst_bt_demux_debug

/* Forward declarations */
static void
gst_bt_demux_send_buffering (GstBtDemux * thiz, libtorrent::torrent_handle h);
static void
gst_bt_demux_check_no_more_pads (GstBtDemux * thiz);


static void
gst_bt_demux_switch_streams (GstBtDemux * thiz, gint desired_fileidx);


static gboolean
gst_bt_demux_stream_activate (GstBtDemuxStream * thiz, libtorrent::torrent_handle h, int max_pieces);

static void
gst_bt_demux_stream_info (GstBtDemuxStream * thiz,
    libtorrent::torrent_handle h, gint * start_offset,
    gint * start_piece, gint * end_offset, gint * end_piece,
    gint64 * size, gint64 * start_byte, gint64 * end_byte);



typedef struct _GstBtDemuxBufferData
{
  boost::shared_array <char> buffer;
  int piece;
  int size;
} GstBtDemuxBufferData;

/*----------------------------------------------------------------------------*
 *                            The buffer helper                               *
 *----------------------------------------------------------------------------*/
static void gst_bt_demux_buffer_data_free (gpointer data)
{
  g_free (data);
}

GstBuffer * gst_bt_demux_buffer_new (boost::shared_array <char> const buffer,
    gint piece, gint size, GstBtDemuxStream * s)
{
  GstBuffer *buf;
  GstBtDemuxBufferData *buf_data;
  guint8 *data;

  buf_data = g_new0 (GstBtDemuxBufferData, 1);
  buf_data->buffer = buffer;

  data = (guint8 *)buffer.get ();
  
  /* handle the offsets */
  //prefer push the whole piece except the beginning piece and ending piece
  //but if the piece length is large, there maybe only need to pusha portion of one piece,how to handle this ?

  /*case 3:
  Interlacing portion in just one piece (not expanding in two or more pieces)
  |----*******---|
       |<--->|
  */
 if (s->start_piece == s->end_piece)
 {
   data += s->start_offset;
   size = s->end_offset - s->start_offset;
 }

  /*case 1:
  Starting piece partially
  |----****|
       |->
  */
  else if (piece == s->start_piece) 
  {
    data += s->start_offset;
    size -= s->start_offset;
  }

  /*case 2:
  End piece partially
  |****----|
    <-|
  */
  else if (piece == s->end_piece) 
  {
    size -= size - s->end_offset;
  }



  /* create the buffer */
#if HAVE_GST_1
  buf = gst_buffer_new_wrapped_full ((GstMemoryFlags)0, data, size, 0, size,
      buf_data, gst_bt_demux_buffer_data_free);

                                printf("(gst_bt_demux_buffer_new) thiz->start_piece=%d, piece=%d, this buffer actual size:%d \n", 
                                    s->start_piece, piece, size);

#else
  buf = gst_buffer_new ();
  GST_BUFFER_DATA (buf) = data;
  GST_BUFFER_SIZE (buf) = size;
  GST_BUFFER_MALLOCDATA (buf) = (guint8 *)buf_data;
  GST_BUFFER_FREE_FUNC (buf) = gst_bt_demux_buffer_data_free;
#endif

  return buf;
}















/*----------------------------------------------------------------------------*
 *                             The stream class                               *
 *----------------------------------------------------------------------------*/

G_DEFINE_TYPE (GstBtDemuxStream, gst_bt_demux_stream, GST_TYPE_PAD);




static void
gst_bt_demux_stream_push_loop (gpointer user_data)
{

  using namespace libtorrent;
  GstBtDemux *demux;
  GstBtDemuxStream *thiz;
  GstBtDemuxBufferData *ipc_data;
  GstBuffer *buf;
  GstFlowReturn ret;
  GstBtDemuxBufferData *buf_data;
  GSList *walk;
  guint8 *data;
  torrent_handle* ptr_h = NULL;
  torrent_handle h;
  gboolean update_buffering = FALSE;
  gboolean send_eos = FALSE;
  gint buf_size;

  //our current stream, there may be multiple stream within torrent
  thiz = GST_BT_DEMUX_STREAM (user_data);

  
              
                                // printf("(bt_demux_stream_push_loop) Pad is %d 1-PUSH, 2-PULL, >start_piece= %d,current_piece=(%d)\n", 
                                //     (int)GST_PAD_MODE (thiz), thiz->start_piece, thiz->current_piece);

                    
  demux = GST_BT_DEMUX (gst_pad_get_parent (GST_PAD (thiz)));

  if (!demux->tor_handle)
  {
    return;
  }

                                GstTaskState tstate = gst_pad_get_task_state (GST_PAD_CAST (thiz));  

                                printf("(bt_demux_stream_push_loop) FIRE %s [start piece idx:%d, end piece idx:%d],current_piece:%d,pad task state %d \n", 
                                          GST_PAD_NAME (thiz), thiz->start_piece, thiz->end_piece, thiz->current_piece, (int)tstate);

  //demux->finished field doesn't means that the whole torrent finished, we are seeder
  //it is set to TRUE when torrent_removed_alert, so push_loop will terminate
  if (demux->finished) 
  {
                                printf("(bt_demux_stream_push_loop) %s demux->finished, so return\n", GST_PAD_NAME (thiz));
    gst_pad_pause_task (GST_PAD (thiz));
    return;
  }

  //check requested, if modified , means we should not keep pushing the current stream
  if (!thiz->requested)
  {
                                printf("(bt_demux_stream_push_loop) Undesired stream %s checked\n", GST_PAD_NAME (thiz));
    
    gst_pad_pause_task (GST_PAD (thiz));

    return;
  }

          gboolean have_type_emitted = FALSE;
          gboolean need_re_push = FALSE;
          // the peer pad of our btdemux src pad is 
          // gstdecodebin2's static sink pad which is ghost pad to gsttypefindelement
          GstPad* peerpad = gst_pad_get_peer (GST_PAD (thiz));
          if (peerpad && GST_IS_GHOST_PAD (peerpad))
          {
              GstPad* internal_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (peerpad));
              if (!internal_pad) {
                  printf ("(bt_demux_stream_push_loop) decodebin ghost sink pad Has no target yet, return\n");
                return;
              } else {

                  printf ("(bt_demux_stream_push_loop) decodebin ghost sink pad is ok to have target\n");

                  GstObject* parent = gst_pad_get_parent (internal_pad);
                  if (parent) {
                    g_object_get (G_OBJECT (parent), "have-type-emitted", &have_type_emitted, NULL);
                    if (FALSE == have_type_emitted) {
                      printf ("(bt_demux_stream_push_loop) have_type not emitted yet\n");
                    } else {
                      printf ("(bt_demux_stream_push_loop) have_type emitted, proceeding \n");
                    }
                    gst_object_unref (parent);
                  } else {
                    printf ("(bt_demux_stream_push_loop) parent is NULL \n");
                  } 
                  gst_object_unref (internal_pad);
              }

            gst_object_unref (peerpad);
            
          } else if (peerpad && !GST_IS_GHOST_PAD (peerpad)) {
            printf ("(bt_demux_stream_push_loop) peer pad is not Ghost pad\n");
            gst_object_unref (peerpad);
          }


  //----Pushed in read_piece_alert handling code, pop up here
  // If thiz->ipc (gasyncqueue) is empty, `g_async_queue_pop` blocks until data becomes available
  ipc_data = (GstBtDemuxBufferData *) g_async_queue_pop (thiz->ipc);
  if (!ipc_data) 
  {
                          printf("(bt_demux_stream_push_loop) nil ipc_data, so return\n");

    gst_pad_pause_task (GST_PAD (thiz));
    return;
  }
  if (!ipc_data->size) 
  {
                          printf("(bt_demux_stream_push_loop) ipc_data->size is zero,means btdemux have cleanup so return\n");

    gst_bt_demux_buffer_data_free (ipc_data);
    g_static_rec_mutex_unlock (thiz->lock);
    return;
  }


  ptr_h = (torrent_handle*)demux->tor_handle;
  if (ptr_h != NULL)
    h = *ptr_h;
  ptr_h = NULL;

printf("(bt_demux_stream_push_loop) waiting lock thiz->current_piece(%d), ipc_data->piece(%d)\n", thiz->current_piece,ipc_data->piece);
  g_static_rec_mutex_lock (thiz->lock);//***************************************************************************************************
printf("(bt_demux_stream_push_loop) recovery lock (%d)\n", thiz->current_piece);


  //----CHECK
  //judge whether `ipc_data` belongs to this stream(file)
  if (ipc_data->piece < thiz->start_piece && ipc_data->piece > thiz->end_piece) 
  {
                                    printf("(bt_demux_stream_push_loop) judge whether `ipc_data` belongs to this stream(file), so return\n");

    gst_bt_demux_buffer_data_free (ipc_data);
    g_static_rec_mutex_unlock (thiz->lock);
    return;
  }
  //whether it is our desired fileidx, if not dont Pushing buffer
  if (!thiz->requested) 
  {
                                    printf("(bt_demux_stream_push_loop) this stream is not requested(aka. not playing currently in playlist), so return\n");
    
    gst_bt_demux_buffer_data_free (ipc_data);
    g_static_rec_mutex_unlock (thiz->lock);
    return;
  }
  /* in case got a seek event(current_piece has changed) intercept pushing it */
  //this check to some extent guarantee the piece pushed in order
  if (ipc_data->piece != thiz->current_piece + 1) 
  {
    // GST_DEBUG_OBJECT (thiz, "Dropping piece %d, waiting for %d on "
    //     "file %d", ipc_data->piece, thiz->current_piece + 1, thiz->file_idx);

                                      printf("(bt_demux_stream_push_loop) Dropping piece (%d), waiting for (%d) on file %d \n", ipc_data->piece, thiz->current_piece + 1, thiz->file_idx);

    gst_bt_demux_buffer_data_free (ipc_data);
    g_static_rec_mutex_unlock (thiz->lock);
    return;
  }


  buf = gst_bt_demux_buffer_new (ipc_data->buffer, ipc_data->piece,
    ipc_data->size, thiz);

  //get buffer size for debug
  buf_size = gst_buffer_get_size (buf);

  // GST_DEBUG_OBJECT (thiz, "Received piece %d of size %d on file %d",
  //     ipc_data->piece, ipc_data->size, thiz->file_idx);

                                      printf("(bt_demux_stream_push_loop) Received piece (%d) of size %d and actual size %d on file %d \n", ipc_data->piece, ipc_data->size, buf_size,thiz->file_idx);

  //----FOR SEEKING
  if (thiz->pending_segment) 
  {
    GstEvent *event;
#if HAVE_GST_1
    GstSegment *segment;
    gboolean update;
#endif

#if HAVE_GST_1
    segment = gst_segment_new ();
    gst_segment_init (segment, GST_FORMAT_BYTES);
    gst_segment_do_seek (segment, 1.0, GST_FORMAT_BYTES, GST_SEEK_FLAG_NONE, 
        GST_SEEK_TYPE_SET, thiz->start_byte-thiz->start_byte_global,
        GST_SEEK_TYPE_SET, thiz->end_byte-thiz->start_byte_global, 
        &update);

    event = gst_event_new_segment (segment);
#else
    event = gst_event_new_segment (FALSE, 1.0, GST_FORMAT_BYTES,
        thiz->start_byte, thiz->end_byte, thiz->start_byte);
#endif

                                        printf("(bt_demux_stream_push_loop) Push SEGMENT event, start_byte=%d, end_byte=%d \n", 
                                            thiz->start_byte-thiz->start_byte_global, 
                                            thiz->end_byte-thiz->start_byte_global);

    if(thiz->flush_start_sent)
    {

                                        printf("(bt_demux_stream_push_loop) since flush_start sent, send flush_stop then \n");

      GstEvent* flush_stop = gst_event_new_flush_stop (TRUE);
      gst_pad_push_event (GST_PAD (thiz), flush_stop);
      thiz->flush_start_sent = FALSE;
    }

    gst_pad_push_event (GST_PAD (thiz), event);

    //clear the `pending_segment` field
    thiz->pending_segment = FALSE;

  }


  // GST_DEBUG_OBJECT (thiz, "Pushing buffer, size: %d, file: %d, piece: %d",
  //     ipc_data->size, thiz->file_idx, ipc_data->piece);


  /***** BtdemuxStream->current_piece is UPDATED here  *****/
  gint old_current_piece = thiz->current_piece;
  thiz->current_piece = ipc_data->piece;

                                          printf("(bt_demux_stream_push_loop) Modifying thiz->current_piece from %d to %d \n",
                                              old_current_piece, 
                                              thiz->current_piece);


                // before pushing buffer on pad, check whether it is flushing
                // if (GST_PAD_IS_FLUSHING (GST_PAD (thiz)))
                // {
                //   printf ("(bt_demux_stream_push_loop) Before pushing  buffer, it is flushing\n");
                // }else
                // {
                //   printf ("(bt_demux_stream_push_loop) Before pushing  buffer, it is NOT flushing\n");
                // }


                                          printf("(bt_demux_stream_push_loop) Pushing buffer, actual size: %d, file: %d, cur piece: (%d) \n", buf_size, thiz->file_idx, thiz->current_piece);

  /*this call may block*/
  ret = gst_pad_push (GST_PAD (thiz), buf);

  if (ret != GST_FLOW_OK) 
  {

                                           printf("(bt_demux_stream_push_loop)!!Failed Pushing buffer,actual size: %d, file: %d, piece: (%d) ret=%d\n", 
                                              buf_size, thiz->file_idx, thiz->current_piece, (int)ret);

    if (ret == GST_FLOW_NOT_LINKED ) 
    {
      printf ("(bt_demux_stream_push_loop) it is Pad-not-linked errors \n");

      send_eos = TRUE;
      GST_ELEMENT_ERROR (demux, STREAM, FAILED,
          ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)", gst_flow_get_name (ret),
          ret));
    }
    // such as seek, flush-start, but flush-stop not sent yet
    else if (ret == GST_FLOW_FLUSHING)
    {
        printf ("(bt_demux_stream_push_loop) it is flushing  \n");

        if (FALSE == have_type_emitted) {

              printf ("(bt_demux_stream_push_loop) have-type not sent yet, repush this piece \n");
                
          g_async_queue_push_front (thiz->ipc, ipc_data);
          //dont update the current_piece here, since we need re-push this piece data again to guarantee it pushed successful
          thiz->current_piece = old_current_piece;
          need_re_push = TRUE;
        }else {
          printf ("(bt_demux_stream_push_loop) otherwise, send eos downstream to avoid blcking on push_loop \n");
          send_eos = TRUE;
        }
    }
    // that is GST_FLOW_EOS, 
    // where is the EOS originated ? who populates it ?
    // it was found in 'goto eos' eos labeled code in qtdemux_process_adapter which gonna return GST_FLOW_EOS in qtdemux.c , 
    else if (ret == GST_FLOW_UNEXPECTED)
    {
      // if already got eos event when gst_pad_push, why not we don't send the same another eos event, 
      // send eos or not ,does it matter that much ? yes
      send_eos = TRUE;
      printf ("(bt_demux_stream_push_loop) it is End of stream \n");
    }
    // maybe GST_FLOW_NOT_NEGOTIATE, GST_FLOW_ERROR etc... but i never meet them returned
    else if (ret < GST_FLOW_UNEXPECTED)
    {
      printf ("(bt_demux_stream_push_loop) it is unexpected errors \n");

      send_eos = TRUE;
      GST_ELEMENT_ERROR (demux, STREAM, FAILED,
          ("Internal data flow error."),
          ("streaming task paused, reason %s (%d)", gst_flow_get_name (ret),
          ret));
    }
  }


#if 0
  /* send the end of segment in case we need to */
  if (ipc_data->piece == thiz->end_piece) {

  }
#endif


  /* send the EOS downstream, check that last push didnt trigger a new seek */
  if (!thiz->moov_after_mdat && ipc_data->piece == thiz->last_piece && !thiz->pending_segment)
  {

                                  printf("(bt_demux_stream_push_loop) during last push set send_eos to TRUE %d %d\n",ipc_data->piece, thiz->last_piece);

    send_eos = TRUE;
  }

  if (send_eos) 
  {
    GstEvent *eos;
    // send eos means previous stream is end, new seek in stream occur 
    // Elements that receive the EOS event on a pad can return #GST_FLOW_EOS when data after the EOS event arrives
    eos = gst_event_new_eos ();

    // GST_DEBUG_OBJECT (thiz, "Sending EOS on file %d", thiz->file_idx);

                                  printf("(bt_demux_stream_push_loop) Sending EOS event on file %d, piece:%d \n", thiz->file_idx, ipc_data->piece );

    gst_pad_push_event (GST_PAD (thiz), eos);
    gst_pad_pause_task (GST_PAD (thiz));

  }


  //-----------HANDLING MOOV AFTER MDAT CASE---------
  // when moov header after mdat, reset pushing from first piece after push last piece
  //the gstdeocdebin2 srcpad is not created until moov header piece pushed and reset pushing from start, after that , the stream become seekable
  // No need to worry about that user will trigger a seek before or during the moov-header-seek posted bt qtdemux and parsing 
  if (thiz->moov_after_mdat && ipc_data->piece == thiz->end_piece)
  {

                                      printf("(bt_demux_stream_push_loop) When got moov header after mdat , reset pushing start from first piece \n");

      gint start_piece, start_offset, end_piece, end_offset;
      gint64 start_byte, end_byte;

      /* get the piece length */
      torrent_info ti = h.get_torrent_info ();
      int piece_length = ti.piece_length ();

      //// gst_bt_demux_stream_info (thiz, h, &start_offset,
      //// &start_piece, &end_offset, &end_piece, NULL, &start_byte, &end_byte);

      thiz->start_byte = thiz->start_byte_global;
      thiz->end_byte = thiz->end_byte_global;
      thiz->start_piece = thiz->start_byte / piece_length;
      thiz->start_offset = thiz->start_byte % piece_length;
      thiz->end_piece = thiz->end_byte / piece_length;
      thiz->end_offset = thiz->end_byte % piece_length; 
      
      gboolean update_buffering = gst_bt_demux_stream_activate (thiz, h, demux->buffer_pieces);
      if (update_buffering) 
      {
                                      printf ("(bt_demux_stream_push_loop) When got moov header after mdat , call send_buffering\n");

        gst_bt_demux_send_buffering (demux, h);
      } 
      else
      {
                                      printf ("(bt_demux_stream_push_loop) When got moov header after mdat, call read_piece() on start piece %d\n",
                                      thiz->start_piece);

        //**fire the read on start_piece, the rest will follow automatically, like a chain reaction, or domino effect
        h.read_piece (thiz->start_piece);
      }
      thiz->moov_after_mdat = FALSE;
  }

  

  //-----------TRY ADD MORE ADJENCENT PIECES---------
  /* read the next piece, make sure not exceeds `end_piece` */
  if (!need_re_push && (ipc_data->piece+1<=thiz->end_piece)) 
  {
      int next = ipc_data->piece+1;    
      //in case we got a seek, current_piece modified
      if (next<=thiz->current_piece || next>thiz->current_piece+demux->buffer_pieces-1) 
      {
                printf ("(bt_demux_stream_push_loop) current_piece modified, give up call read_piece() on next piece %d, current:%d\n", ipc_data->piece+1, thiz->current_piece);
      } 
      else 
      {

        //here,we dont check Three-Piece-Area availability, just check the only one piece next to us, it is more loose than bt_demux_stream_activate()
        if (h.have_piece (next)) 
        {
          // GST_DEBUG_OBJECT (thiz, "Reading next piece %d, current: %d",
          //     ipc_data->piece + 1, thiz->current_piece);

          if (send_eos ==FALSE) {
                          printf ("(bt_demux_stream_push_loop) Luckily we have next piece %d, call read_piece() on it, current:%d\n", ipc_data->piece+1, thiz->current_piece);
            //**fire the read on start_piece, the rest will follow automatically, like a chain reaction, or domino effect
            h.read_piece (next);
          } else {
                          //generally, it is reached when EOS occured
                          printf ("(bt_demux_stream_push_loop) due to EOS or internal Error, suspend call read_piece() on next piece %d, current:%d, end/last:%d \n", ipc_data->piece + 1, thiz->current_piece, thiz->last_piece);
          }
        } 
        // if we do not hold the next piece following the current, we need to buffering the Three-Piece-Area 
        else 
        {
          int i;

          // GST_DEBUG_OBJECT (thiz, "Start buffering next piece %d",
          //      ipc_data->piece + 1);

                                            {
                                                int begin = next;
                                                int ending = ipc_data->piece+demux->buffer_pieces-1;
                                                if (ending > thiz->end_piece) {
                                                  ending = thiz->end_piece;
                                                }

                                                printf ("(bt_demux_stream_push_loop) Unlickily We dont hold the next piece, buffering between [%d,%d] \n", 
                                                  begin, 
                                                  ending);
                                            }

          //Clear previous buffering stats
          thiz->buffering_level = 0;
          thiz->buffering_count = 0;

          if (thiz->cur_buffering_flags) {
              //clear history
              thiz->cur_buffering_flags = g_array_remove_range (thiz->cur_buffering_flags, 0, thiz->cur_buffering_flags->len);
  printf ("(bt_demux_stream_push_loop) clearing historical cur_buffering_flags for new push\n");
              //Three-Piece-Area
              for (i=1; i<demux->buffer_pieces; i++) 
              {
                  libtorrent::download_priority_t priority;
                  int idx = next;

                  //border checking -- if hit, bail out
                  if (idx > thiz->end_piece) 
                  {
                    break;
                  }
                  
                  if (h.have_piece (idx))
                  {
                    //if we hold this piece, thiz->cur_buffering_flags set to FALSE on that index
                    gboolean tmp_false = FALSE;
                    g_array_append_vals (thiz->cur_buffering_flags, &tmp_false, 1);
                    continue;
                  }

                  gboolean tmp_true = TRUE;
                  g_array_append_vals (thiz->cur_buffering_flags, &tmp_true, 1);

                  /* if it's already set to `top_priority` just skip */
                  priority = h.piece_priority (idx);
                  if (priority == libtorrent::top_priority)
                  {
                    continue;
                  }

                  /* set to top priority */
                  priority = libtorrent::top_priority;

                  h.piece_priority (idx, priority);

                  thiz->buffering_count++;
              }

              if (thiz->buffering_count)
              {
                thiz->buffering = TRUE;
              }

              update_buffering = TRUE;
          } 

          else {
            printf ("(bt_demux_stream_push_loop) failed clear cur_buffering_flags \n");
          }

        }

      }
  }

                                  printf("(bt_demux_stream_push_loop) unlock cur:(%d)\n", thiz->current_piece);

  g_static_rec_mutex_unlock (thiz->lock);

  // g_mutex_unlock (demux->streams_lock);

  if (update_buffering)
  {
    //this is just for post gst buffering message so application can know
    gst_bt_demux_send_buffering (demux, h);
  }

  // g_mutex_unlock (demux->streams_lock);
  // still push current piece instead of moving to push next piece
  if (!need_re_push) {
    gst_bt_demux_buffer_data_free (ipc_data);
  }

                                  printf("bt_demux_stream_push_loop, END \n");

}







//Used in piece_finished_alert handling code
//Updating the buffering progress infomation
static void
gst_bt_demux_stream_update_buffering (GstBtDemuxStream * thiz,
    libtorrent::torrent_handle h, int max_pieces)
{
  using namespace libtorrent;
  int i;
  //cuz thiz->current_pieces was assigned as thiz->start_piece minus 1 in gst_bt_demux_stream_activate
  //3 pieces treat as a unit
  int start = thiz->current_piece + 1;
  int end = thiz->current_piece + max_pieces;
  // num of pieces that already downloaded, it should within sector of [0,3]
  int buffered_pieces = 0;

  /* do not exceeds piece boundry */
  if (start >= thiz->end_piece) {
    start = thiz->end_piece;
    end = thiz->end_piece;
  }
  if (end > thiz->end_piece) {
    end = thiz->end_piece;
  }
          
  if (thiz->cur_buffering_flags) {


      int flag_idx = 0;
      /* count how many pieces have been downloaded */
      for (i = start; i <= end; i++) {
        if ( h.have_piece (i) && g_array_index (thiz->cur_buffering_flags, gboolean, flag_idx) == TRUE) {
          buffered_pieces++;
        }
        flag_idx++;
      }

            printf ("(gst_bt_demux_stream_update_buffering) between [%d,%d] buffered:%d, buffering:%d\n", 
                start, end,
                buffered_pieces, 
                thiz->buffering_count);


      if (buffered_pieces > thiz->buffering_count) {
        //correction on the buffered_pieces , making sure it not greater than actual buffering_count
        buffered_pieces = thiz->buffering_count;
      }

      //Hot Three-Piece-Area progress
      //******************Compute the average percent progress*************************
      thiz->buffering_level = (buffered_pieces * 100) / thiz->buffering_count;

      // GST_DEBUG_OBJECT (thiz, "Buffering level %d (%d/%d)", 
      //     thiz->buffering_level,
      //     buffered_pieces, 
      //     thiz->buffering_count);
  
          printf("(gst_bt_demux_stream_update_buffering) Buffering level %d (%d/%d) \n", thiz->buffering_level, buffered_pieces, thiz->buffering_count);
  
  } 

  else {

    printf ("(gst_bt_demux_stream_update_buffering) cur_buffering_flags is empty \n"); 
  }
}






static void
gst_bt_demux_stream_add_one_piece (GstBtDemuxStream * thiz,
    libtorrent::torrent_handle h, libtorrent::piece_index_t piece)
{
  using namespace libtorrent;

  // GST_DEBUG_OBJECT (thiz, "Adding more pieces at %d, current: %d, "
  //     "max: %d", piece, thiz->current_piece, max_pieces);

                        printf("(bt_demux_stream_add_piece) Adding more pieces at %d, current: %d\n", piece, thiz->current_piece);

  // select (set priorirt to top) ONLY one piece that following the selected (of top_priority) piece and break the loop
  // if we are seeder, it do nothing
  for (piece; piece <= static_cast<piece_index_t>(thiz->end_piece); piece++) 
  {
    libtorrent::download_priority_t priority;


    //we already hold this piece, just continue looping
    if (h.have_piece (piece))
    {
      continue;
    }


    /* if it's already set to `top_priority` just skip */
    priority = h.piece_priority (piece);
    if (priority == libtorrent::top_priority)
    {
      continue;
    }



    /* set to top priority */
    priority = libtorrent::top_priority;


    // this individual piece we dont hold now will be downloaded at top_priority
    // priority will affect piece_picker
    // the other pieces still will be downloaded after all pieces in your requested stream downloaded
    //when we dont have those pieces needed to play it, we set priority of these pieces,
    //not means the other pieces is forbidden to be downloaded
    //actually the pieces which is in non-requested streams still will be downlaoded
    h.piece_priority (piece, priority);


    // GST_DEBUG_OBJECT (thiz, "Requesting piece %d, prio: %d, current: %d ",
    //     piece, priority, thiz->current_piece);

                        printf("(bt_demux_stream_add_piece) Since WE Dont Have, Requesting piece %d at top priority, current: %d\n", piece, thiz->current_piece);
    
    // once set priority on one piece, exit the loop
    break;
  }
}












/*
    |---Piece 0---|---Piece 1---|---Piece 2---|---Piece 3---|---Piece 4---|---Piece 5---|---Piece 6---| ...... 
    |_________________________________________| <- window current_piece starts from -1
                                              |_________________________________________| <- window current_piece starts from 0                          
*/

/*used in gst_bt_demux_switch_streams 
gst_bt_demux_stream_seek 
and moov_after_mdat handling code in push_loop*/
// should call this function after we set fileidx in totem-playlist
//for non-seeder torrent, this function set first three pieces to top_priority 
//and the rest pieces will 
//set priority ==> let  libtorrent download it ==> add piece the piece_finished_alert handling code
//=> call read_piece() in bt_demux_send_buffering() => start push_loop in read_piece_alert hanlding code 
static gboolean
gst_bt_demux_stream_activate (GstBtDemuxStream * thiz,
    libtorrent::torrent_handle h, int max_pieces)
{
  using namespace libtorrent;

  gboolean ret = FALSE;

printf ("(bt_demux_stream_activate) waiting lock\n");
  g_static_rec_mutex_lock (thiz->lock);//********************************************************************************
printf ("(bt_demux_stream_activate) recovery lock\n");

  // we want this stream - A stream is just a video file within same torrent, there may be multiple
  if( !thiz->requested )
  {
    thiz->requested = TRUE;
  }

  //update current_piece which is equal to start_piece minus one, make it point to the index preceding the start_piece
  thiz->current_piece = thiz->start_piece - 1;
  thiz->pending_segment = TRUE;


printf("(bt_demux_stream_activate) Modifying thiz->current_piece to %d (start_piece minus one) \n",
                                              thiz->current_piece);


  // GST_DEBUG_OBJECT (thiz, "Activating stream '%s', start: %d, "
  //     "start_offset: %d, end: %d, end_offset: %d, current: %d",
  //     GST_PAD_NAME (thiz), thiz->start_piece, thiz->start_offset,
  //     thiz->end_piece, thiz->end_offset, thiz->current_piece);


  //Clear previous buffering fields, avoid unneeded push
  thiz->buffering_level = 0;
  thiz->buffering_count = 0;
  if (thiz->buffering != FALSE) 
  {
    thiz->buffering = FALSE;
  }


  int start = thiz->start_piece;
  int end = thiz->start_piece + max_pieces - 1;
  /* do not exceeds piece boundry */
  if (start >= thiz->end_piece) {
    start = thiz->end_piece;
    end = thiz->end_piece;
  }
  if (end > thiz->end_piece) {
    end = thiz->end_piece;
  }


  if (thiz->cur_buffering_flags) {


    //clear history
    thiz->cur_buffering_flags = g_array_remove_range (thiz->cur_buffering_flags, 0, thiz->cur_buffering_flags->len);
  
    printf ("(bt_demux_stream_activate) clearing historical cur_buffering_flags for activate stream\n");

    int i;
    /* count how many pieces need downloading/buffering in 3-Piece-Area */
    for (i=start; i<=end; i++) 
    {
      //have_piece() return TRUE means we have downloaded this piece, passed hash-check, and flushed to disk
      if (h.have_piece (i)) 
      {
        // if we hold this piece, thiz->cur_buffering_flags set to FALSE on that index, 
        // so it won't be taken into account when calculate buffering level
        gboolean tmp_false = FALSE;
        g_array_append_vals (thiz->cur_buffering_flags, &tmp_false, 1);
        continue;
      }

      //we need to download this piece, it needs take into account when calculate buffering level
      gboolean tmp_true = TRUE;
      g_array_append_vals (thiz->cur_buffering_flags, &tmp_true, 1);

      libtorrent::download_priority_t priority;

      /* if it's already set to `top_priority` just skip */
      priority = h.piece_priority (i);
      if (priority == libtorrent::top_priority)
      {
        continue;
      }

      /* set to top priority */
      priority = libtorrent::top_priority;

      h.piece_priority (i, priority);

      thiz->buffering_count++;
    }

    if (thiz->buffering_count)
    {
      thiz->buffering = TRUE;
      // set to TRUE means we need to buffering (download the torrent)
      ret = TRUE;
    }



              printf("(bt_demux_stream_activate) Activating stream '%s', start: (%d), start_offset: %d, end: %d, end_offset: %d, current: (%d) buffering(%s)\n", 
                                GST_PAD_NAME (thiz), thiz->start_piece, thiz->start_offset, thiz->end_piece, thiz->end_offset, thiz->current_piece,
                                thiz->buffering_count>0 ? "Yes" : "No");


  } else {
    printf ("(bt_demux_stream_activate) failed clear cur_buffering_flags \n");
  }

printf("(bt_demux_stream_activate) unlock\n");
  g_static_rec_mutex_unlock (thiz->lock);//********************************************************************************


  // GstTaskState tstate = gst_pad_get_task_state (GST_PAD_CAST (thiz));  
  //   printf ("(bt_demux_stream_activate) pad task state is %d \n", (int)tstate);

  return ret;
}







/*init the BtDemuxStream */
static void
gst_bt_demux_stream_info (GstBtDemuxStream * thiz,
    libtorrent::torrent_handle h, gint * start_offset,
    gint * start_piece, gint * end_offset, gint * end_piece,
    gint64 * size, gint64 * start_byte, gint64 * end_byte)
{
  using namespace libtorrent;
  file_entry fe;
  int piece_length;
  torrent_info ti = h.get_torrent_info ();

  piece_length = ti.piece_length ();
  fe = ti.file_at (thiz->file_idx);


  // fe.offset -- the offset of this file inside the torrent 
  // fe.size -- the size of the file (in bytes) 


  // this file starts in `start_piece`th piece within the torrent
  // conversely, fe.offest = start_piece * piece_length + start_offset, start_piece value is >= 0, 
  // must not increment one on start_piece before multiply it piece_length
  if (start_piece)
  {
    *start_piece = fe.offset / piece_length;
  }


  // the number of bytes  whom this file starts after in its `start piece` 
  if (start_offset)
  {
    *start_offset = fe.offset % piece_length;
  }


  // this file ends at `end_piece`th piece within the torrent
  if (end_piece)
  {
    *end_piece = (fe.offset + fe.size) / piece_length;
  }


  // the number of bytes past the  `end_piece`
  if (end_offset)
  {
    *end_offset = (fe.offset + fe.size) % piece_length;
  }


  if (size)
  {
    *size = fe.size;
  }


  if (start_byte)
  {
    *start_byte = fe.offset; 
  }


  if (end_byte)
  {
    *end_byte = fe.offset + fe.size;
  }

}




static gboolean
gst_bt_demux_stream_seek (GstBtDemuxStream * thiz, GstEvent * event)
{
  using namespace libtorrent;
  GstBtDemux *demux;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gdouble rate;
  gint start_piece, start_offset, end_piece, end_offset;
  torrent_handle* ptr_h = NULL;
  torrent_handle h;
  int piece_length;
  gboolean update_buffering;
  gboolean ret = FALSE;



  demux = GST_BT_DEMUX (gst_pad_get_parent (GST_PAD (thiz)));
  if (!demux->tor_handle)
  {
    return ret;
  }
  ptr_h = (torrent_handle*)demux->tor_handle;
  if (ptr_h != NULL)
    h = *ptr_h;
  ptr_h = NULL;
  gst_object_unref (demux);


  /* get the piece length */
  torrent_info ti = h.get_torrent_info ();
  piece_length = ti.piece_length ();

  //if this seek event is triggered by [user], the format is GST_FORMAT_TIME in first enter this function
  //if this seek event is triggered by [qtdemux], which means that it failed to got moov header in first piece of data
  if(format == GST_FORMAT_TIME)
  {
    thiz->is_user_seek = TRUE; 
  }


  //Parses a seek @event and stores the results in the given result locations.
  gst_event_parse_seek (event, &rate, &format, &flags, &start_type,
      &start, &stop_type, &stop);


  /* sanitize stuff */
  if (format != GST_FORMAT_BYTES)
  {
                      printf("(bt_demux_stream_seek) format is not GST_FORMAT_BYTES \n");
    //dont proceeds, just return
    // goto beach;
    return ret;
  }

  if (rate < 0.0)
  {
return ret;
    // goto beach;
  }

  gst_bt_demux_stream_info (thiz, h, &start_offset,
      &start_piece, &end_offset, &end_piece, NULL, NULL, NULL);

                      printf ("(bt_demux_stream_seek) info %d,%d %d,%d \n", start_piece,start_offset, end_piece,end_offset);

  //the seek is triggerd by qtdemux for finding moov header after mdat atom, don't let it happen
  if(!thiz->is_user_seek)
  {
      //we just request last three piece, we may be lucky enough to got the [moov] atom, 
      //the earlier got moov atom, the sooner we can start watching
      thiz->moov_after_mdat = TRUE;

                      printf("(bt_demux_stream_seek) moov_after_mdat, this Seek event is posted by qtdemux \n");

  }

  if (start < 0)
  {
    start = 0;
  }

  //go to very end of video
  if (stop < 0) 
  {
    int num_pieces;

    num_pieces = end_piece - start_piece + 1;
    if (num_pieces == 1) 
    {
      /* all the bytes on a single piece */
      stop = end_offset - start_offset;
    } 
    else 
    {
      /* count the full pieces */
      stop = (num_pieces - 2) * piece_length;
      /* add the start bytes */
      stop += piece_length - start_offset;
      /* add the end bytes */
      stop += end_offset;
    }
  }

                      printf("(bt_demux_stream_seek) rate:%lf, start:%ld, stop:%ld \n", rate, start, stop);

  if (flags & GST_SEEK_FLAG_FLUSH) 
  {
                                        printf("(bt_demux_stream_seek) push flush_start \n");
    gst_pad_push_event (GST_PAD (thiz), gst_event_new_flush_start ());
        
    thiz->flush_start_sent = TRUE;
  } 
  else 
  {
    /* TODO we need to close the segment */
  }

  if (flags & GST_SEEK_FLAG_SEGMENT) 
  {
    GST_ERROR ("Segment seek");
  }

    

printf("(bt_demux_stream_seek) waiting lock (%d)\n", start_piece);
  g_static_rec_mutex_lock (thiz->lock);//********************************************************************************
printf("(bt_demux_stream_seek) recovery lock (%d)\n", start_piece);


      /* update the stream "Segment" (aka. NEW Range), actually update BYDEMYX_STREAM's start_piece, start_offset, end_piece, end_offset */
 

      thiz->end_piece = (thiz->start_byte_global + stop) / piece_length;
      thiz->end_offset =  (thiz->start_byte_global + stop) % piece_length; 
   

      //update thiz->strat_piece, so push_loop can know 
      thiz->start_piece = (thiz->start_byte_global + start) / piece_length;
      thiz->start_offset = (thiz->start_byte_global + start) % piece_length;
    

printf ("(bt_demux_stream_seek) fileidx(%d) start_byte_global = %d\n", 
        thiz->file_idx, 
        thiz->start_byte_global);

      
      //byte position within the torrent
      thiz->end_byte = thiz->start_byte_global + stop;
      thiz->start_byte = thiz->start_byte_global + start;


      // GST_DEBUG_OBJECT (thiz, "Seeking to, start: %d, start_offset: %d, end: %d, "
      //     "end_offset: %d", thiz->start_piece, thiz->start_offset,
      //     thiz->end_piece, thiz->end_offset);



                                        printf("(bt_demux_stream_seek) Seeking to, start:%d, start_offset:%d, end:%d, end_offset:%d, start_byte%ld,end_byte%ld \n", 
                                            thiz->start_piece, thiz->start_offset, thiz->end_piece, thiz->end_offset, thiz->start_byte, thiz->end_byte);


  //before activate, set previous already activated three-piece-area as default_priority 
  int old_start = thiz->current_piece + 1;
  int old_end = thiz->current_piece + demux->buffer_pieces;
  
  /* do not exceeds piece boundry */
  if (old_start >= thiz->end_piece) {
    old_start = thiz->end_piece;
    old_end = thiz->end_piece;
  }
  if (old_end > thiz->end_piece) {
    old_end = thiz->end_piece;
  }
  
  //only when the 3-Piece-Area not identical, we do the priority replacement
  if (thiz->start_piece != old_start) 
  {
                  printf ("(bt_demux_stream_seek) before activate, clear previous Three-Piece-Area [%d,%d]\n",
                  old_start,
                  old_end);

    int i;
    for (i = old_start; i <= old_end; i++) 
    {
      libtorrent::download_priority_t priority;
      priority = h.piece_priority (i);

      if (h.have_piece (i))
      {
        continue;
      }
      if (priority == libtorrent::default_priority)
      {
        continue;
      }
      if (!h.have_piece (i) && priority == libtorrent::low_priority)
      {
        continue;
      }
      if ( !h.have_piece (i) && priority==libtorrent::top_priority ) {
              printf ("(bt_demux_stream_seek) piece %d previously set at top prio,we got a seek ,so set it to default prio\n", i);
        h.piece_priority (i, libtorrent::default_priority);
      }
    }
  }



  /* activate stream */
  update_buffering = gst_bt_demux_stream_activate (thiz, h,
      demux->buffer_pieces);


  //area we seeking to do no need to buffer
  if (!update_buffering) 
  {
    /* start directly */
    // GST_DEBUG_OBJECT (thiz, "SEEK:Starting stream '%s', reading piece %d, "
    //     "current: %d", GST_PAD_NAME (thiz), thiz->start_piece,
    //     thiz->current_piece);

                              printf("(bt_demux_stream_seek) Starting SEEK stream '%s', reading piece %d, current: %d buffering(No) \n", 
                              GST_PAD_NAME (thiz), thiz->start_piece, thiz->current_piece);

    gboolean old_buffering = thiz->buffering;

    // When seek from area needing buffering to already-downloaded area
    // We should tell bvw via posting buffering level 100% to show buffering finished,
    // So it will not blocked on paused state even if we truly hold those piece
    if (old_buffering == TRUE)
    {

                              printf ("(bt_demux_stream_seek) transition from buffering to non-buffering area, send buffering level 100 to bvw \n");

      gst_element_post_message (GST_ELEMENT_CAST (demux),
      gst_message_new_buffering (GST_OBJECT_CAST (demux), 100));
    }

    thiz->buffering = FALSE;

                              printf("(bt_demux_stream_seek) call read_piece() on piece %d\n",
                                                                  thiz->start_piece);
    //we must already have this piece before we call `read_piece`
    //**fire the read on start_piece, the rest will follow automatically, like a chain reaction, or domino effect
    h.read_piece (thiz->start_piece);
  } 
  //area we seeking to do need to buffer
  else 
  {

                              printf("(bt_demux_stream_seek) Starting SEEK stream '%s' go update buffering, start:%d, current:%d, buffering(Yes) \n", 
                                                            GST_PAD_NAME (thiz), 
                                                            thiz->start_piece, 
                                                            thiz->current_piece);

            gst_bt_demux_stream_update_buffering (thiz, h, demux->buffer_pieces);
  }

  if(thiz->moov_after_mdat)
  {
                              printf ("(bt_demux_stream_seek) return FALSE to deliberately let qtdemux_seek_offset failed \n");
    //return FALSE to deliberately let qtdemux_seek_offset failed, so it won't start buffering mdat (QTDEMUX_STATE_BUFFER_MDAT)
    ret = FALSE;
  }
  else
  {
    ret = TRUE;
  }


printf("(bt_demux_stream_seek) unlock lock (%d)\n", start_piece);
  g_static_rec_mutex_unlock (thiz->lock);//********************************************************


  /* send the buffering if we need to */
  if (update_buffering)
  {
    gst_bt_demux_send_buffering (demux, h);
  }

  //reset
  thiz->is_user_seek = FALSE;

// beach:
  return ret;
}




static gboolean
gst_bt_demux_stream_event (GstPad * pad, GstObject * object, GstEvent * event)
{
  GstBtDemuxStream *thiz;
  gboolean ret = FALSE;

  thiz = GST_BT_DEMUX_STREAM (pad);

  // GST_DEBUG_OBJECT (thiz, "Event %s", GST_EVENT_TYPE_NAME (event));

                                          // printf("(gst_bt_demux_stream_event) Event %s \n", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) 
  {
    case GST_EVENT_SEEK:
    {
                                          printf("(bt_demux_stream_event) Got Event, GST_EVENT_SEEK \n");
     
      ret = gst_bt_demux_stream_seek (thiz, event);

      break;
    }
    case GST_EVENT_EOS:
    {
                                          printf("(bt_demux_stream_event) Got Event, GST_EVENT_EOS \n");
      break;
    }

    default:
      break;
  }
  // return TRUE if the pad could handle the event.
  return ret;
}




static gboolean
gst_bt_demux_stream_query (GstPad * pad, GstObject * object, GstQuery * query)
{
  using namespace libtorrent;
  GstBtDemux *demux;
  GstBtDemuxStream *thiz;
  gboolean ret = FALSE;

  thiz = GST_BT_DEMUX_STREAM (pad);
  demux = GST_BT_DEMUX (object);

  // GST_DEBUG_OBJECT (thiz, "Quering %s", GST_QUERY_TYPE_NAME (query));


             
  switch (GST_QUERY_TYPE (query)) 
  {
    //query is posted from gst_qtdemux_check_seekability()
    case GST_QUERY_SEEKING:
    {
      
                              // printf("(bt_demux_stream_query) Querying GST_QUERY_SEEKING, [%ld,%ld]\n", 
                              //     thiz->start_byte-thiz->start_byte_global, 
                              //     thiz->end_byte-thiz->start_byte_global);

      GstFormat format;
      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      if (format == GST_FORMAT_BYTES) 
      {
        // for two-video torrent case , the second video's start_byte is surely 0, and not the start_byte within torrent,
        // then end_byte is length of video , not the end_byte within torrent
        gst_query_set_seeking (query, GST_FORMAT_BYTES, TRUE, thiz->start_byte-thiz->start_byte_global, thiz->end_byte-thiz->start_byte_global);
        ret = TRUE;
      }
    }
    break;

    //query is posted from gst_qtdemux_check_seekability()
    case GST_QUERY_DURATION:
    {
                                // printf("(bt_demux_stream_query) Querying GST_QUERY_DURATION \n");

      torrent_handle* ptr_h = NULL;
      torrent_handle h;
      GstFormat fmt;
      gint64 bytes;

      if (!demux->tor_handle)
      {
        ret = FALSE;
        break;;
      }

      ptr_h = (torrent_handle*) demux->tor_handle;
      if(ptr_h)
        h = *ptr_h;
      ptr_h = NULL;


      gst_query_parse_duration (query, &fmt, NULL);
      //not handleing GST_FORMAT_TIME in btdemux, so it is of no use now
      if (fmt == GST_FORMAT_BYTES) {
        gst_bt_demux_stream_info (thiz, h, NULL, NULL, NULL, NULL, &bytes, NULL, NULL);
        gst_query_set_duration (query, GST_FORMAT_BYTES, bytes);
        ret = TRUE;
      }
    }
      break;

    case GST_QUERY_BUFFERING:
    {

        break;
    }
    default:
      break;
  }

  return ret;
}

#if !HAVE_GST_1
static gboolean
gst_bt_demux_stream_event_simple (GstPad * pad, GstEvent * event)
{
  GstObject *object;
  gboolean ret = FALSE;

  object = gst_pad_get_parent (pad);
  ret = gst_bt_demux_stream_event (pad, object, event);
  gst_object_unref (object);

  return ret;
}

static gboolean
gst_bt_demux_stream_query_simple (GstPad * pad, GstQuery * query)
{
  GstObject *object;
  gboolean ret = FALSE;

  object = gst_pad_get_parent (pad);
  ret = gst_bt_demux_stream_query (pad, object, query);
  gst_object_unref (object);

  return ret;
}
#endif

static void
gst_bt_demux_stream_dispose (GObject * object)
{
  GstBtDemuxStream *thiz;

  thiz = GST_BT_DEMUX_STREAM (object);

  if (thiz->path) {
    g_free (thiz->path);
  }

  if (thiz->ipc) {
    g_async_queue_unref (thiz->ipc);
    thiz->ipc = NULL;
  }

  if (thiz->cur_buffering_flags)
  {
    g_array_free (thiz->cur_buffering_flags, TRUE);
  }


  g_static_rec_mutex_free (thiz->lock);
  g_free (thiz->lock);



  // GST_DEBUG_OBJECT (thiz, "Disposing");

          printf("~~~~Disposing \n");

  G_OBJECT_CLASS (gst_bt_demux_stream_parent_class)->dispose (object);
}

static void
gst_bt_demux_stream_class_init (GstBtDemuxStreamClass * klass)
{
  
            printf("gst_bt_demux_stream_class_init \n");

  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gst_bt_demux_stream_parent_class = g_type_class_peek_parent (klass);

  /* initialize the object class */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_bt_demux_stream_dispose);
}

static void
gst_bt_demux_stream_init (GstBtDemuxStream * thiz)
{

                    printf("gst_bt_demux_stream_init \n");

  thiz->lock = g_new (GStaticRecMutex, 1);
  g_static_rec_mutex_init (thiz->lock);


#if HAVE_GST_1
  gst_pad_set_event_function (GST_PAD (thiz),
      GST_DEBUG_FUNCPTR (gst_bt_demux_stream_event));
  gst_pad_set_query_function (GST_PAD (thiz),
      GST_DEBUG_FUNCPTR (gst_bt_demux_stream_query));
#else
  gst_pad_set_event_function (GST_PAD (thiz),
      GST_DEBUG_FUNCPTR (gst_bt_demux_stream_event_simple));
  gst_pad_set_query_function (GST_PAD (thiz),
      GST_DEBUG_FUNCPTR (gst_bt_demux_stream_query_simple));

#endif

  //not added to btdemux initially, only when call `gst_element_add_pad` it will be set to TRUE
  //and after added , when call ` gst_element_remove_pad` will set back to FALSE.
  thiz->added = FALSE;

  thiz->cur_buffering_flags = NULL;

  /* our ipc */
  thiz->ipc = g_async_queue_new_full (
      (GDestroyNotify) gst_bt_demux_buffer_data_free);
}






/*----------------------------------------------------------------------------*
 *                            The demuxer class                               *
 *----------------------------------------------------------------------------*/
enum {
  PROP_0,
  PROP_TYPEFIND,
  PROP_N_STREAMS,
  PROP_CURRENT_STREAM,
};

enum
{
  SIGNAL_STREAMS_CHANGED,
  SIGNAL_GET_PPI,
  LAST_SIGNAL
};

static guint gst_bt_demux_signals[LAST_SIGNAL] = { 0 };
 
G_DEFINE_TYPE (GstBtDemux, gst_bt_demux, GST_TYPE_ELEMENT);


//sink pad
// static GstStaticPadTemplate sink_factory =
//     GST_STATIC_PAD_TEMPLATE ("sink",
//     GST_PAD_SINK,
//     GST_PAD_ALWAYS,
//     GST_STATIC_CAPS ("application/x-bittorrent"));

//source pad
static GstStaticPadTemplate src_factory =
    GST_STATIC_PAD_TEMPLATE ("src_%02d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);







//handle which stream to push using totem-playlist 
// static GSList *
// gst_bt_demux_get_policy_streams (GstBtDemux * thiz)
// {
//   using namespace libtorrent;
//   GSList *ret = NULL;

//   switch (thiz->policy) 
//   {
//     //assumming all file within torrent are videos file (acutally it may not always the case)
//     case GST_BT_DEMUX_SELECTOR_POLICY_ALL:
//     {
//       GSList *walk;

//       /* copy the streams list */
//       for (walk = thiz->streams; walk; walk = g_slist_next (walk)) 
//       {
//         GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);
//         ret = g_slist_append (ret, gst_object_ref (stream));
//       }
//       break;
//     }

//     //the larger file is video file
//     case GST_BT_DEMUX_SELECTOR_POLICY_LARGER:
//     {
//       file_entry fe;
//       session *s;
//       std::vector<torrent_handle> torrents;
//       int i;
//       int index = 0;

//       s = (session *)thiz->session;
//       torrents = s->get_torrents ();
//       if (torrents.size () < 1)
//       {
//         break;
//       }

//       torrent_info ti = torrents[0].get_torrent_info ();
//       if (ti.num_files () < 1)
//       {
//         break;
//       }

//       fe = ti.file_at (0);
//       for (i = 0; i < ti.num_files (); i++) 
//       {
//         file_entry fee = ti.file_at (i);

//         /* get the larger file */
//         if (fee.size > fe.size) 
//         {
//           fe = fee;
//           index = i;
//         }
//       }

//       ret = g_slist_append (ret, gst_object_ref (g_slist_nth_data (
//           thiz->streams, index)));
//     }

//     default:
//       break;
//   }

//   return ret;
// }





static void
gst_bt_demux_check_no_more_pads (GstBtDemux * thiz)
{
  GSList *walk;
  gboolean send = TRUE;

  /* whenever every requested stream has an active pad (means we are OK),inform about the
   * no more pads
   */
  //although loop all streams here, only one stream is requested (see it is as playlist)
  for (walk = thiz->streams; walk; walk = g_slist_next (walk)) 
  {
    GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

    if (!stream->requested)
    {
      continue;
    }

    // we desire this stream, but it has not active, dont sent no-more-pads now
    if (stream->requested && !gst_pad_is_active (GST_PAD (stream)))
    {
      send = FALSE;
    }
    // we dont need this stream, but it active, dont sent no-more-pads now, we need remove it and add our desired sream (oad)
    if (!stream->requested && gst_pad_is_active (GST_PAD (stream)))
    {
      send = FALSE;
    }
  }

  if (send) 
  {
    // GST_DEBUG_OBJECT (thiz, "Sending no more pads");

            printf("(bt_demux_check_no_more_pads) We're done... Sending no more pads \n");

    gst_element_no_more_pads (GST_ELEMENT (thiz));
  }
}





// After set to top_proirty, check whether those pieces are really downloaded,
// if so, start read_piece on them, so we can receive 
// in read_piece_alert handling code and push_loop can execute
static void
gst_bt_demux_send_buffering (GstBtDemux * thiz, libtorrent::torrent_handle h)
{
  using namespace libtorrent;
  GSList *walk;
  gboolean start_pushing_requested = FALSE;

  // although loop all streams here, only one stream is requested (see it is as playlist)
  /* generate the real buffering level (for the only video we have requested) */
  for (walk = thiz->streams; walk; walk = g_slist_next (walk)) 
  {
    GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

printf("(gst_bt_demux_send_buffering) waiting lock (%d)\n", stream->current_piece);
    g_static_rec_mutex_lock (stream->lock);//***************************************************************************************
printf("(gst_bt_demux_send_buffering) recovery lock (%d)\n", stream->current_piece);

    if (!stream->requested) 
    {
// printf("(gst_bt_demux_send_buffering) unlock (%d)\n", stream->current_piece);
      g_static_rec_mutex_unlock (stream->lock);
      continue;
    }

    if (!stream->buffering) 
    {
// printf("(gst_bt_demux_send_buffering) unlock (%d)\n", stream->current_piece);
      g_static_rec_mutex_unlock (stream->lock);
      continue;
    }


    //post buffering message , so bvw can know: when to pause and waiting?  when to resume playing after buffered enough?
    gst_element_post_message (GST_ELEMENT_CAST (thiz),
            gst_message_new_buffering (GST_OBJECT_CAST (thiz), stream->buffering_level));


    // For the only video we requested
    if (stream->buffering_level >= 100) 
    {
      // unset the buffering state for this stream
      stream->buffering = FALSE;
      stream->buffering_level = 0;

      start_pushing_requested = TRUE;
    }
    else /* stream->buffering_level < 100 */ 
    {
      thiz->buffering = TRUE;
    }


    /****** if level >= 100% (all three pieces are downloaded)***
    * start pushing buffers on that only requested stream ******/
    if (start_pushing_requested) 
    {
      // GST_DEBUG_OBJECT (thiz, "Buffering finished, reading piece %d"
      //     ", current: %d", stream->current_piece + 1,
      //     stream->current_piece);

                                printf("(gst_bt_demux_send_buffering) Buffering finished, soon call read_piece() on piece %d, current:%d\n", stream->current_piece + 1, stream->current_piece);
      
      // every time current_piece plus one, which guarantee the piece be pushed in order, 
      // aka. read_piece_alert retrieved in order, so push_loop can push in piece order
      h.read_piece (stream->current_piece+1);
    } 
    else
    {
        printf("(gst_bt_demux_send_buffering) Buffering unfinished, post buffering level. current:%d\n", stream->current_piece);
    }


// printf("(gst_bt_demux_send_buffering) unlock (%d)\n", stream->current_piece);
    g_static_rec_mutex_unlock (stream->lock);


  }
}








static void
gst_bt_demux_switch_streams (GstBtDemux * thiz, gint desired_file_idx)
{
  using namespace libtorrent;
  GSList *walk;
  torrent_handle* ptr_h = NULL;
  torrent_handle h;
  gboolean update_buffering = FALSE;

  // g_mutex_lock (thiz->streams_lock);

  if (!thiz->tor_handle)
  {
    return;
  }

  if (!thiz->streams) 
  {
    // g_mutex_unlock (thiz->streams_lock);
    return;
  }

  
  ptr_h = (torrent_handle *)thiz->tor_handle;
  if (ptr_h != NULL)
    h = *ptr_h;
  ptr_h = NULL;


  for (walk = thiz->streams; walk; walk = g_slist_next (walk)) 
  {
    GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

// printf("(gst_bt_demux_switch_streams) waiting lock \n");
//     g_static_rec_mutex_lock (stream->lock);//***************************************************************************************************
// printf("(gst_bt_demux_switch_streams) recovery lock \n");

    if (stream->file_idx == desired_file_idx && stream->requested)
    {
        // field stream->start_piece may be changed if it has been modified 
        // such as during btdemux_stream_seek ... 
        // we should reset the field such as start_piece, end_piece etc... 
        gst_bt_demux_stream_info (stream, h, 
        &stream->start_offset, &stream->start_piece, 
        &stream->end_offset, &stream->end_piece,
        NULL, &stream->start_byte, &stream->end_byte);
  
        update_buffering = gst_bt_demux_stream_activate (stream, h,
          thiz->buffer_pieces);

        printf("(gst_bt_demux_switch_streams) Switching to stream '%s', reading piece %d, current: %d, buffering(%s)\n", 
                    GST_PAD_NAME (stream), stream->start_piece, stream->current_piece, update_buffering?"Yes":"No");

        if (update_buffering) 
        {
          //this is just to post gst buffering messages, so application can know and perform pause
          gst_bt_demux_send_buffering (thiz, h);
        } 
        else 
        {
        
          printf("(gst_bt_demux_switch_streams) call read_piece() on piece %d\n",
            stream->start_piece);
          //**fire the read on start_piece, the rest will follow automatically, like a chain reaction, or domino effect
          h.read_piece (stream->start_piece);

        }
    }
    else
    {
      /* when switch stream, what if the previous requested stream is buffering ? what should we do? */


      //also clear buffering_count,buffering_level --but this is not that essential
      if(stream->buffering_count > 0)
        stream->buffering_count = 0;
      if (stream->buffering_level > 0)
        stream->buffering_level = 0;


      //Clear top_priority, since this stream we no longer request
      //if the stream has not call stream_activate() yet, current_piece still is zero 
      int area_start = stream->current_piece + 1;
      int area_end = stream->current_piece + thiz->buffer_pieces;

      /* do not exceeds piece boundry */
      if (area_start >= stream->end_piece) {
        area_start = stream->end_piece;
        area_end = stream->end_piece;
      }
      if (area_end > stream->end_piece) {
        area_end = stream->end_piece;
      }

      //Judge whether stay within thie fileidx area
      if (area_start < stream->start_piece || area_end > stream->end_piece)
      {
        continue;
      }

      printf ("(gst_bt_demux_switch_streams) before activate, clear previous Three-Piece-Area [%d,%d]\n",
      area_start,
      area_end);
      
      int i;
      for (i = area_start; i <= area_end; i++) 
      {
        libtorrent::download_priority_t priority;
        priority = h.piece_priority (i);

        if (h.have_piece (i))
        {
          continue;
        }
        if (priority == libtorrent::low_priority)
        {
          continue;
        }
        if ( !h.have_piece (i) && priority>libtorrent::low_priority ) {
                        printf ("(gst_bt_demux_switch_streams) This stream no longer requested, we clear top or default prio on piece:%d in Three-Piece-Area\n", i);
          h.piece_priority (i, libtorrent::low_priority);
        }
      }
    
    }

// printf("(gst_bt_demux_switch_streams) unlock lock \n");
//     g_static_rec_mutex_unlock (stream->lock);

  }//End of for loop

  // g_mutex_unlock (thiz->streams_lock);

}





// static void
// gst_bt_demux_task_resume (GstBtDemux * thiz)
// {

//                           printf("(gst_bt_demux_task_resume) \n");

//   using namespace libtorrent;
//   GSList *walk;

//   /* pause every task */
//   g_mutex_lock (thiz->streams_lock);
//   for (walk = thiz->streams; walk; walk = g_slist_next (walk)) {
//     GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);
 
//     if(stream->requested == FALSE)
//       continue;


//     // GstBtDemuxBufferData *ipc_data;
//     // /* send a empty buffer, avoid g_async_queue block on empty state*/
//     // ipc_data = g_new0 (GstBtDemuxBufferData, 1);
//     // g_async_queue_push (stream->ipc, ipc_data);

//     GstTaskState tstate = gst_pad_get_task_state (GST_PAD (stream));
//     if(tstate == GST_TASK_PAUSED)
//     {
//       printf("(gst_bt_demux_task_pause) unset need_paused \n");

//       stream->need_paused = FALSE;
//     }
//   }
//   g_mutex_unlock (thiz->streams_lock);

//   printf("(gst_bt_demux_task_resume) END\n");

// }









// static void
// gst_bt_demux_task_pause (GstBtDemux * thiz)
// {

//                           printf("(gst_bt_demux_task_pause) \n");

//   using namespace libtorrent;
//   GSList *walk;

//   /* pause task */
//   g_mutex_lock (thiz->streams_lock);
//   for (walk = thiz->streams; walk; walk = g_slist_next (walk)) {
//     GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

//     if(stream->requested == FALSE)
//       continue;

//     GstBtDemuxBufferData *ipc_data;
//     /* send a empty buffer, avoid g_async_queue block on empty state */
//     ipc_data = g_new0 (GstBtDemuxBufferData, 1);
//     g_async_queue_push (stream->ipc, ipc_data);

//     GstTaskState tstate = gst_pad_get_task_state (GST_PAD (stream));
//     if(tstate == GST_TASK_STARTED)
//     {
//       printf("(gst_bt_demux_task_pause) perform gst_pad_pause_task \n");

//       gst_pad_pause_task (GST_PAD (stream));

//       // stream->need_paused = TRUE;
//     }
//   }
//   g_mutex_unlock (thiz->streams_lock);

//                             printf("(gst_bt_demux_task_pause) END\n");

// }






static void
gst_bt_demux_task_cleanup (GstBtDemux * thiz)
{

                          printf("(bt_demux_task_cleanup) \n");

  using namespace libtorrent;
  GSList *walk;

  /* pause every task */
  g_mutex_lock (thiz->streams_lock);
  for (walk = thiz->streams; walk; walk = g_slist_next (walk)) {
    GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);
    GstBtDemuxBufferData *ipc_data;

    /* send a cleanup buffer */
    ipc_data = g_new0 (GstBtDemuxBufferData, 1);
    g_async_queue_push (stream->ipc, ipc_data);
    GstTaskState tstate = gst_pad_get_task_state (GST_PAD (stream));
    if(tstate != GST_TASK_STOPPED)
      gst_pad_stop_task (GST_PAD (stream));
  }
  g_mutex_unlock (thiz->streams_lock);

}





static void
gst_bt_demux_cleanup (GstBtDemux * thiz)
{
  /* remove every pad reference */
  if (thiz->streams) 
  {

  //cleaup up list of stream(src pad)
    g_slist_free_full (thiz->streams, gst_object_unref);
    thiz->streams = NULL;
  }
}



static GstStateChangeReturn
gst_bt_demux_change_state (GstElement * element, GstStateChange transition)
{

    GstBtDemux* thiz;
    GstStateChangeReturn ret;

    thiz = GST_BT_DEMUX (element);


    printf("(gst_bt_demux_change_state) TRANSITION:%d \n", (int)transition);


    switch (transition) 
    {

      case GST_STATE_CHANGE_READY_TO_PAUSED:

                
        //if already STARTED, dont do it again
        // if(tstate != GST_TASK_STARTED) {
        //   gst_bt_demux_task_setup (thiz);
        // }

        break;
      //such as eos,but we do not close bt_demux_task ,should stop push_loop
      case GST_STATE_CHANGE_PAUSED_TO_READY:
        printf("(gst_bt_demux_change_state) PAUSED_TO_READY \n");
        gst_bt_demux_task_cleanup (thiz);
        break;

      case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        printf("(gst_bt_demux_change_state) GST_STATE_CHANGE_PAUSED_TO_PLAYING \n");
        // gst_bt_demux_task_resume (thiz);
        break;


      default:
        break;
    }

    ret = GST_ELEMENT_CLASS (gst_bt_demux_parent_class)->change_state (element, transition);

    switch (transition) 
    {
      //user press Play/Pause Button to Pause, so for sure push_loop should be paused! but feed_read_piece_alert need be paused ?
      case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        printf("(gst_bt_demux_change_state) GST_STATE_CHANGE_PLAYING_TO_PAUSED \n");
        // gst_bt_demux_task_pause (thiz);
        break;


      case GST_STATE_CHANGE_PAUSED_TO_READY:

                  printf("(gst_bt_demux_change_state) PAUSED_TO_READY 2 \n");

        gst_bt_demux_cleanup (thiz);
        break;

      case GST_STATE_CHANGE_READY_TO_NULL:

                  printf("(gst_bt_demux_change_state) READY_TO_NULL \n");

        gst_bt_demux_task_cleanup (thiz);
        break;

      default:
        break;
    }

    return ret;

}





static void
gst_bt_demux_dispose(GObject * object)
{
  GstBtDemux *thiz;

  thiz = GST_BT_DEMUX (object);

  // GST_DEBUG_OBJECT (thiz, "Disposing");

              printf("(bt_demux_dispose) Disposing \n");

  gst_bt_demux_task_cleanup (thiz);
  gst_bt_demux_cleanup (thiz);


  //clear pointer of torrent_handle
  if (thiz->tor_handle) 
  {
    libtorrent::torrent_handle *handle_temp;

    handle_temp = (libtorrent::torrent_handle *)thiz->tor_handle;
    delete (handle_temp);
    thiz->tor_handle = NULL;
  }


  g_mutex_free (thiz->streams_lock);


  G_OBJECT_CLASS (gst_bt_demux_parent_class)->dispose (object);
}





//push_loop will check ->requested field, it it is FALSE, this fileidx it not we desired, so will not Pushing buffer downstream
// PLUS: if it is single-video torrent, things gets much easier
static void
update_requested_stream (GstBtDemux* thiz)
{

  gint desired_file_index;
  g_object_get (thiz, "current-video-file-index", &desired_file_index, NULL);

                          if (desired_file_index != -1)
                          {
                              printf ("(btdemux/update_requested_stream) desired fileidx %d \n", desired_file_index);
                          }
                         
  if(thiz->streams)
  {
      gint foo = 0;
      GSList *walk;
      //iterate each BtDemuxStream, only the stream of desired file_index will be requested
      for (walk = thiz->streams; walk; walk = g_slist_next (walk))
      {

                                    // printf ("(btdemux/update_requested_stream) foo = %d\n" , foo);

          GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

          g_static_rec_mutex_lock (stream->lock);//*********************************************

          if(stream->file_idx != desired_file_index)
          {
            stream->requested = FALSE;

            // if (gst_pad_is_active (GST_PAD (stream))) 
            // {
                  printf("(btdemux/update_requested_stream) stream-idx(%d) disable undesired streams(src pads)\n", foo);
        
                if(!gst_pad_set_active (GST_PAD (stream), FALSE)){
                    printf ("(btdemux/update_requested_stream) stream-idx(%d) DISABLE gst_pad_set_active failed\n",foo);
                } else {
                    printf ("(btdemux/update_requested_stream) stream-idx(%d) DISABLE gst_pad_set_active ok\n",foo);
                }
              
              // Remove only when we add it previously
              // If we remove a pad we never add it before, it will pop CRITICAL warning
              // We should prohibit it happening
              if (stream->added)
              {

                GstPad* peerpad = gst_pad_get_peer (GST_PAD (stream));
                if (peerpad && GST_IS_GHOST_PAD (peerpad))
                {
                  GstPad* internal_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (peerpad));
                  if (!internal_pad) 
                  {
                      printf ("(btdemux/update_requested_stream) decodebin ghost sink pad Has no target yet, return\n");
                    return;
                  } 
                  else 
                  {
                    GstObject* parent = gst_pad_get_parent (internal_pad);
                    if (parent) 
                    {
                      printf ("(btdemux/update_requested_stream) re-set typefindelement have-type to FALSE\n");
                      g_object_set (G_OBJECT (parent), "have-type-emitted", FALSE, NULL);
                      gst_object_unref (parent);
                    } 
                    else 
                    {
                      printf ("(btdemux/update_requested_stream) parent is NULL \n");
                    } 
                    gst_object_unref (internal_pad);
                  }
                  gst_object_unref (peerpad);
                
                }
                else if (peerpad && !GST_IS_GHOST_PAD (peerpad)) 
                {
                  printf ("(btdemux/update_requested_stream) peer pad is not Ghost pad\n");
                  gst_object_unref (peerpad);
                }
                else if (!peerpad)
                {
                  printf ("(btdemux/update_requested_stream) peer pad is NULL\n");

                }

                
                //// GstEvent *eos = gst_event_new_eos ();
                //// gst_pad_push_event (GST_PAD (stream), eos);
                ////                   printf("(btdemux/update_requested_stream) Sending EOS event on file %d before remove it, so we can send stream-start event later\n", foo );
                
                    
                gst_object_unref(stream);
                gst_element_remove_pad (GST_ELEMENT (thiz), GST_PAD (stream));

                stream->added = FALSE;
              }
              gst_pad_stop_task (GST_PAD (stream));

            // }

          }
          
          else
          {
                  printf ("(update_requested_stream) Mark stream of fileidx %d as Requested \n", desired_file_index);
          
            stream->requested = TRUE;

            gst_bt_demux_switch_streams (thiz, desired_file_index);
                
          }
        foo += 1;
          g_static_rec_mutex_unlock (stream->lock);//********************************************
      }
  } 

}






static void
gst_bt_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBtDemux *thiz = NULL;

  g_return_if_fail (GST_IS_BT_DEMUX (object));

  thiz = GST_BT_DEMUX (object);

  switch (prop_id) {

    case PROP_CURRENT_STREAM:
      thiz->cur_streaming_fileidx = g_value_get_int (value);
      break;
    case PROP_TYPEFIND:
      thiz->typefind = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bt_demux_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstBtDemux *thiz = NULL;

  g_return_if_fail (GST_IS_BT_DEMUX (object));

  thiz = GST_BT_DEMUX (object);
  switch (prop_id) {

    case PROP_N_STREAMS:
      g_value_set_int (value, g_slist_length (thiz->streams));
      break;

   case PROP_CURRENT_STREAM:
      g_value_set_int (value, thiz->cur_streaming_fileidx);
      break;

    case PROP_TYPEFIND:
      g_value_set_boolean (value, thiz->typefind);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bt_demux_class_init (GstBtDemuxClass * klass)
{

          printf("(gst_bt_demux_class_init) \n");


  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;

  gst_bt_demux_parent_class = g_type_class_peek_parent (klass);

  /* initialize the object class */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_bt_demux_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_bt_demux_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_bt_demux_get_property);

  g_object_class_install_property (gobject_class, PROP_N_STREAMS,
      g_param_spec_int ("n-video-mp4", "Number of mp4/quicktime streams",
          "Get the total number of mp4/quicktime streams",
          0, G_MAXINT, 0, G_PARAM_READABLE));

  g_object_class_install_property (gobject_class, PROP_CURRENT_STREAM,
      g_param_spec_int ("current-video-file-index", "Desired video file_index of Current Played Stream",
          "Get file_index in torrent of Current Played Stream (default is 0) ",
          -1, G_MAXINT, 0, G_PARAM_READWRITE));



  g_object_class_install_property (gobject_class, PROP_TYPEFIND,
      g_param_spec_boolean ("typefind", "Typefind",
          "Run typefind before negotiating", DEFAULT_TYPEFIND,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));



  /* initialize the element class and pad template */
  // gst_element_class_add_pad_template (element_class,
  //     gst_static_pad_template_get (&sink_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_bt_demux_change_state);

  gst_element_class_set_details_simple (element_class,
      "BitTorrent Demuxer", "Codec/Demuxer",
      "Streams a BitTorrent file",
      "Jorge Luis Zapata <jorgeluis.zapata@gmail.com>");

}
 
static void
gst_bt_demux_init (GstBtDemux * thiz)
{

  //when btdemux plugin created, connect to our transmission's Session
  //Now, btdemux plugin no longer hold its own libtorrent session, it depends on transmission's Session
  //and transmission embedded with libtorrent can send bitfield-scale info to Totem (now also become part of transmission client )

  //btdemux no longer send btdemux-video-info, bitfield-scale info to bacon-video-widget,
  //it just rely on Session to get streams to create src pads and get alerts for playnig video job
  //so sink is no more needed,we dont need read torrent file ourselves

            printf("(bt_demux_init) \n");

  using namespace libtorrent;

  thiz->tor_handle == NULL;
  
  
  thiz->streams_lock = g_mutex_new ();


  thiz->total_num_blocks = -1;
  thiz->total_num_pieces = -1;
  thiz->num_blocks_last_piece = -1;
  thiz->blocks_per_piece_normal = -1;


  /* default properties */
  thiz->buffer_pieces = DEFAULT_BUFFER_PIECES;
  thiz->num_video_file = 0;
  thiz->typefind = DEFAULT_TYPEFIND;

  //let totem-object to select which fileidx of video to push (play)
  g_signal_connect (thiz, "notify::current-video-file-index",
                    G_CALLBACK (update_requested_stream), thiz);


}










/******************** Exposed Functions to feed us************************/

//get our btdemux instance from BaconVideoWidget when use element_make_or_warn, and pass to us via transmission's Session
void btdemux_feed_playlist(GObject *thiz, libtorrent::torrent_handle const handle_copy)
{
  if(thiz == NULL || !GST_IS_BT_DEMUX(thiz))
  {
    return;
  }

  using namespace libtorrent;
  GstBtDemux *demux;
  demux = GST_BT_DEMUX (thiz);

  //As long as we hold the torrent_handle Session feed us, everything become easy
  //use gpointer to receive the torrent_handle
  demux->tor_handle = new libtorrent::torrent_handle(handle_copy);
  libtorrent::torrent_handle* ptr_h = NULL;
  torrent_handle h;

  ptr_h = (torrent_handle *)demux->tor_handle;
  if(ptr_h)
    h = *ptr_h;
  ptr_h = NULL;


printf ("[connect to btdemux...](btdemux_feed_playlist)\n");
  GSList *walk;
  int i;

  //get its torrent_info instnace, we need it (file_idx, file_name, ...etc) to create src pad
  std::shared_ptr<const torrent_info> ti = h.torrent_file();


  printf("(btdemux_feed_playlist)Start streaming, num files: %d, num pieces: %d, piece length: %d \n", 
      ti->num_files(),ti->num_pieces(),ti->piece_length());


  /*---------------------- create the streams playlist  -------------------*/
  /*-----------------------------------------------------------------------*/
  //there may be multiple videos within the torrent ,then create `GstBtDemuxStream` for each video
  for (i = 0; i <ti->num_files (); i++) 
  {
      GstBtDemuxStream *stream;
      gchar *name;
      file_entry fe;

      /* create the pads */
      name = g_strdup_printf ("src_%02d", i);

      /* ..............initialize the streams -- source pad*/
      stream = (GstBtDemuxStream *) g_object_new (
          GST_TYPE_BT_DEMUX_STREAM, "name", name, "direction",
          GST_PAD_SRC, "template", gst_static_pad_template_get (&src_factory), NULL);


            printf ("(btdemux_feed_playlist) create src pad %s (aka.BtDemuxStream) \n", name);


      //Free after use
      g_free (name);

      /* set the file idx within torrent*/
      stream->file_idx = i;

      stream->requested = FALSE;

      stream->finished = FALSE;


      if (stream->cur_buffering_flags == NULL){
        stream->cur_buffering_flags = g_array_new (FALSE, FALSE, sizeof(gboolean));
      }

      /* set the path */
      fe = ti->file_at (i);
      stream->path = g_strdup (fe.path.c_str ());

      //if not .mp4 file, skip, only support mp4/quicktime for now and is enough cuz video torrent mostly are .mp4
      if(!g_str_has_suffix (stream->path, ".mp4"))
      {
        continue;
      }
      demux->num_video_file++;

      //---reset           
      //--- *h is torrent_handle Session feed us,
      //--- To be more efficient and straightforward,  dont think too much whether by ref (&) or by copy(no &), temporarily forget its lifetime
      //--- just remember copy always ,dont stand too high
      gst_bt_demux_stream_info (stream, h, &stream->start_offset,
          &stream->start_piece, &stream->end_offset, &stream->end_piece,
          &stream->end_byte, &stream->start_byte, &stream->end_byte);

      if (stream->start_byte)
      {
        stream->start_byte_global = stream->start_byte;
      }
      if (stream->end_byte)
      {
        stream->end_byte_global = stream->end_byte;
      }

      stream->last_piece = stream->end_piece;

                              printf("(bt_demux_handle_alert) Adding stream %s for file %s at fileidx %d, start_piece:%d, start_ofset:%d,end_piece:%d,end_ofset:%d (%ld,%ld]\n", 
                                  GST_PAD_NAME (stream), stream->path, stream->file_idx, stream->start_piece, 
                                  stream->start_offset, stream->end_piece,stream->end_offset, stream->start_byte, stream->end_byte);

      /* Append it to our list of streams */
      demux->streams = g_slist_append (demux->streams, stream);
  }

  /* mark all pieces (across all files within torrent) to `low_priority` */
  //as soon as we open Totem for the chosen torrent, we set its proprity all to low_priority for streaming purpose 
  for (i = 0; i <ti->end_piece (); i++) 
  {
    h.piece_priority (i, libtorrent::low_priority);
    
    // libtorrent::download_priority_t ret = h.piece_priority (i);
    // printf ("(bt_demux_handle_alert) mark piece %d to low_priority, ret is %d\n", i, ret);
  }

  /* make sure to download sequentially */
  h.set_sequential_download (true);

}



void btdemux_feed_read_piece_alert(GObject* gobj, libtorrent::alert* a)
{
  if(gobj == NULL || !GST_IS_BT_DEMUX(gobj))
  {
    return;
  }

  using namespace libtorrent;

  GstBtDemux * thiz = GST_BT_DEMUX(gobj);

  torrent_handle* ptr_h = NULL;
  torrent_handle h;

  ptr_h = (torrent_handle*) thiz->tor_handle;
  if (ptr_h != NULL)
    h = *ptr_h;
  ptr_h = NULL;



  printf ("(btdemux_feed_read_piece_alert) \n");


  switch (a->type()) 
  {
    case read_piece_alert::alert_type:
    {
      GSList *walk;
      read_piece_alert *p = alert_cast<read_piece_alert>(a);
      //topology_changed means stream switched, that is :old stream unload, loading new stream selected
      gboolean topology_changed = FALSE;

                                printf("(bt_demux_handle_alert) BEGIN in read_piece_alert, piece idx:(%d)\n", 
                                static_cast<int>(p->piece));
      //this piece read is not available for now, maybe it is not downloaded yet
      if (p->buffer == NULL)
      {
                  printf ("(bt_demux_handle_alert) in read_piece_alert, read nothing, exit \n");
        break;
      }

      g_mutex_lock (thiz->streams_lock);
      gint foo = 0;

      /*************read the piece once it is finished and send downstream in order */
      for (walk = thiz->streams; walk; walk = g_slist_next (walk)) 
      {
        GstBtDemuxBufferData *ipc_data;
        GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

// printf("(bt_demux_handle_alert) waiting lock 2; alert piece idx(%d), stream->current_piece(%d)\n", p->piece, stream->current_piece);
        g_static_rec_mutex_lock (stream->lock);//***************************************************************************************************************
// printf("(bt_demux_handle_alert) recovery lock 2; alert piece idx(%d), stream->current_piece(%d)\n", p->piece, stream->current_piece);

        //Judge which piece belongs to which video file (`GstBtDemuxStream`) within torrent
        if (p->piece < stream->start_piece ||
            p->piece > stream->end_piece) 
        {

                      printf("(gst_bt_demux_handle_alert) judge whether this read piece belongs to this stream\n");

          g_static_rec_mutex_unlock (stream->lock);foo++;
          continue;
        }


        /* in case the pad is active but not/no more requested, disable it */
        if (gst_pad_is_active (GST_PAD (stream)) && !stream->requested) {
                                    printf("(bt_demux_handle_alert) stream-idx(%d) the pad is active but not requested, disable it (%d)\n", 
                                    foo, static_cast<int>(p->piece));

          topology_changed = TRUE;

          if(!gst_pad_set_active (GST_PAD (stream), FALSE)){
                    printf("(bt_demux_handle_alert) stream-idx(%d) Disable gst_pad_set_active failed\n", foo);
          }else{
                    printf("(bt_demux_handle_alert) stream-idx(%d) Disable gst_pad_set_active ok\n", foo);
          }
         
          if (stream->added) {
            gst_object_unref(stream);
            gst_element_remove_pad (GST_ELEMENT (thiz), GST_PAD (stream));
            stream->added = FALSE;
          }
          gst_pad_stop_task (GST_PAD (stream));
          g_static_rec_mutex_unlock (stream->lock);foo++;
          continue;
        }


        if (!stream->requested) {
          g_static_rec_mutex_unlock (stream->lock);foo++;
          continue;
        }

        //in case got a seek, current_piece will be modified in gst_bt_demux_stream_activate(), p->piece not within in Three-Piece-Area
        if (p->piece <= stream->current_piece ||
        p->piece > stream->current_piece+thiz->buffer_pieces-1) 
        {

                      printf("(gst_bt_demux_handle_alert) in read_piece_alert, current_piece modified, give up\n");

          g_static_rec_mutex_unlock (stream->lock);foo++;
          continue;
        }


        /* create the pad if has been requested */
        if (!gst_pad_is_active (GST_PAD (stream))) {

          // whether to run typefind before negotiating
          //since we run typefind to get caps  the btdemux srcpad can produce, so typefindelement in gstdecodebin2 become useless
          // if (thiz->typefind) 
          // {
          //   GstTypeFindProbability prob;
          //   GstCaps *caps;
          //   GstBuffer *buf;

          //   buf = gst_bt_demux_buffer_new (p->buffer, p->piece, p->size,
          //       stream);

          //   caps = gst_type_find_helper_for_buffer (GST_OBJECT (thiz), buf, &prob);
          //   gst_buffer_unref (buf);

          //   if (caps) 
          //   {

          //     gchar* capstr = gst_caps_to_string(caps);

          //                           printf("(bt_demux_handle_alert) Manually call typefind helper, caps is %s \n", capstr);
          //     // if(!g_str_has_prefix(capstr, "video/quicktime"))
          //     // {
          //     //                       printf("(bt_demux_handle_alert) this stream is not a video/quicktime which we only supoort, remove this stream \n");
          //     //                 thiz->stream      
          //     // }

          //     gst_pad_set_caps (GST_PAD (stream), caps);
          //     gst_caps_unref (caps);
          //     g_free (capstr);
          //   }else{
          //                           printf("(bt_demux_handle_alert) Manually call typefind helper, get caps Failed\n");
          //   }
          // }
      

                                    printf("(bt_demux_handle_alert) stream-idx(%d) Create the pad if needed and add the pad to element (%d)\n", foo, static_cast<int>(p->piece));

          // then activate it 
          if(!gst_pad_set_active (GST_PAD (stream), TRUE)) {
              printf ("(bt_demux_handle_alert) stream-idx(%d) ENABLE gst_pad_set_active failed\n", foo);
          } else {  
              printf ("(bt_demux_handle_alert) stream-idx(%d) ENABLE gst_pad_set_active ok\n", foo);
          }
          
          //avoid add an already-added one to btdemux 
          if (stream->added == FALSE)
          {
            // `gst_element_add_pad` will emit the #GstElement::pad-added signal on the element btdemux
            gst_element_add_pad (GST_ELEMENT (thiz), GST_PAD (
                gst_object_ref (stream)));
          }
    
          stream->added = TRUE;
          topology_changed = TRUE;
      
        }


        //NOTE: split the whole piece in case there may be data of two video in one piece,if you push the whole piece, 
/*
      eg.      Piece 904            Piece 905            Piece 906
    ...|---------------------|---------------------|---------------------|...
                                                |
   ...__________________________________________|__________________________________...                            

Video 1 territory (maybe moov header in the tail)        Video 2 territory  
*/
        //you will push the wrong data libav will show ERROR, which is a endless headache !
        //push ipc_data in read_piece_alert handling code <====> retrieve ipc_data in bt_demux_stream_push_loop
        /***** fill the `ipc_data` with read piece post by read_piece_alert, send the data to the stream thread */
        ipc_data = g_new0 (GstBtDemuxBufferData /*the type of the elements to allocate*/, 1 /* the number of elements to allocate */);
        ipc_data->buffer = p->buffer; // a buffer containing all the data of the piece
        ipc_data->piece = p->piece; // the piece index that was read
        ipc_data->size = p->size; // number of bytes that was read, this doesn't split the case when two video share/interlacing in one piece
        g_async_queue_push (stream->ipc, ipc_data);

                                        printf("(bt_demux_handle_alert) in read_piece_alert, piece_idx on alert(aka ipc_data->piece)=(%d), size=%d \n", 
                                            ipc_data->piece, ipc_data->size);


   

        /* start the task only if it do no paused by user(pressd Pause button in GUI)*/
        if (stream->requested) {
                printf("(bt_demux_handle_alert) stream-idx(%d) in read_piece_alert, Start the pad task(bt_demux_stream_push_loop) %d \n", foo,static_cast<int>(p->piece));
                #if HAVE_GST_1
                    gst_pad_start_task (GST_PAD (stream), gst_bt_demux_stream_push_loop,
                    stream, NULL);
                #else
                    gst_pad_start_task (GST_PAD (stream), gst_bt_demux_stream_push_loop,
                    stream);
                #endif
        }


        ++foo;

// printf("(bt_demux_handle_alert) unlock lock 2 ; stream-idx(%d) alert piece idx(%d), stream->current_piece(%d)\n", 
// foo, 
// p->piece, 
// stream->current_piece);

        g_static_rec_mutex_unlock (stream->lock);
      }

      //notify no-more-pads, meaning that we won't create more pads any more
      if (topology_changed)
      {
        gst_bt_demux_check_no_more_pads (thiz);
      }

      g_mutex_unlock (thiz->streams_lock);

printf ("(bt_demux_handle_alert) EXIT in read_piece_alert, piece idx:(%d)\n", static_cast<int>(p->piece));
    }
    break;


    default:
      break;
  }
}



void btdemux_feed_piece_finished_alert(GObject* gobj, libtorrent::alert* a)
{
  if(gobj == NULL || !GST_IS_BT_DEMUX(gobj))
  {
    return;
  }

  using namespace libtorrent;

  GstBtDemux * thiz = GST_BT_DEMUX(gobj);

  if (!thiz->tor_handle)
  {
    return;
  }

  torrent_handle* ptr_h = NULL;
  torrent_handle h;

  ptr_h = (torrent_handle*) thiz->tor_handle;
  if (ptr_h != NULL)
    h = *ptr_h;
  ptr_h = NULL;

  printf ("(btdemux_feed_piece_finished_alert) \n");


  switch (a->type()) 
  {
    case piece_finished_alert::alert_type:
    {
        GSList *walk;
        piece_finished_alert *p = alert_cast<piece_finished_alert>(a);
        torrent_handle h = p->handle;
        torrent_status s = h.status();

        gboolean update_buffering = FALSE;

        //the priority may differ from low_priority you set in add_torrent_alert handling,it doesn't matter
        libtorrent::download_priority_t pr;
        pr = h.piece_priority (p->piece_index);


        // GST_DEBUG_OBJECT (thiz, "Piece %d completed (down: %d kb/s, "
        //     "up: %d kb/s, peers: %d)", p->piece_index, s.download_rate / 1000,
        //     s.upload_rate  / 1000, s.num_peers);


                printf("Piece %d completed (down: %d kb/s, up: %d kb/s, peers: %d, prio:%d) \n", 
                  p->piece_index, s.download_rate / 1000,  s.upload_rate  / 1000, s.num_peers, (int)pr);


        g_mutex_lock (thiz->streams_lock);/***********************************************************************/


        // although loop all streams here, only one stream is requested (see it is as playlist)
        /* read the piece once it is finished and send downstream in order (only for the stream we requested)*/
        for (walk = thiz->streams; walk; walk = g_slist_next (walk)) 
        {
          GstBtDemuxStream *stream = GST_BT_DEMUX_STREAM (walk->data);

/* why lock? why not lock? extra lock ? lacking lock ? */

// printf("(gst_bt_demux_handle_alert) piece_finished_alert waiting lock \n");
          // g_static_rec_mutex_lock (stream->lock);//***********************************************************************************************
// printf("(gst_bt_demux_handle_alert) piece_finished_alert recovery lock \n");

          //Judge which piece belongs to which video file (`GstBtDemuxStream`) within torrent
          if (p->piece_index < stream->start_piece ||
              p->piece_index > stream->end_piece) 
          {
// printf("(gst_bt_demux_handle_alert) piece_finished_alert unlock \n");
            // g_static_rec_mutex_unlock (stream->lock);
            continue;
          }

          //pay no attention to stream we don't requested
          if (!stream->requested) 
          {
// printf("(gst_bt_demux_handle_alert) piece_finished_alert unlock \n");
            // g_static_rec_mutex_unlock (stream->lock);
            continue;
          }


          /* We already own this piece , so set its priority to dont_download 
          ,It is of no interests to us now*/
          h.piece_priority (p->piece_index, libtorrent::dont_download);


          /* everytime piece_finished_alert retrieved, never forget to update the buffering progress */
          // check if this stream is in buffering state
          if (stream->buffering) 
          {

                            printf("(gst_bt_demux_handle_alert) in piece_finished_alert, updating buffering progress information \n");

              gst_bt_demux_stream_update_buffering (stream, h, thiz->buffer_pieces);
              update_buffering |= TRUE;
          }

// printf("(gst_bt_demux_handle_alert) piece_finished_alert unlock \n");
          // g_static_rec_mutex_unlock (stream->lock);
        }

        if (update_buffering)
        {
          gst_bt_demux_send_buffering (thiz, h);
        }

        g_mutex_unlock (thiz->streams_lock);/***********************************************************************/
        break;
    }
    

    default:
      break;
  }
}







/*
adjust_piece_priority()
{
    // mark all pieces (across all files within torrent) to `low_priority` 
    for (i = 0; i <ti->end_piece (); i++) 
    {
      
      h.piece_priority (i, libtorrent::low_priority);
      
      // libtorrent::download_priority_t ret = h.piece_priority (i);

      // printf ("(bt_demux_handle_alert) mark piece %d to low_priority, ret is %d\n", i, ret);

    }

    //make sure to download sequentially
    h.set_sequential_download (true);
}


*/







// disconnect()
// {



// }


