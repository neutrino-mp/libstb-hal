/*
 * this code is originally from
 *   pidvbip - tvheadend client for the Raspberry Pi
 *   (C) Dave Chapman 2012-2013
 *
 * adaption for libstb-hal
 *   (C) Stefan Seyfried 2013
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * avcodec_omx.c -- audio / video decoder for the Raspberry Pi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <assert.h>
#include <libavformat/avformat.h>
#include <lt_debug.h>
#include "codec.h"
#include "avcodec_omx.h"
#include "omx_utils.h"

#include "bcm_host.h"
/* set in glfb.cpp */
extern DISPMANX_MODEINFO_T output_info;

static void* acodec_omx_thread(struct codec_init_args_t* args)
{
  struct codec_t* codec = args->codec;
  struct omx_pipeline_t* pi = args->pipe;
  struct codec_queue_t* current = NULL;
  int res = 0;
  int is_paused = 0;
  OMX_BUFFERHEADERTYPE *buf;

  free(args);
  hal_set_threadname("hal:omx_audio");
  fprintf(stderr,"Starting acodec_omx_thread\n");

new_channel:
  codec->first_packet = 1;
  while(1)
  {
next_packet:
    /* NOTE: This lock is only used by the video thread when setting
       up or tearing down the pipeline, so we are not blocking normal
       video playback 
    */
    //fprintf(stderr,"[acodec] - waiting for omx_active_mutex\n");
    pthread_mutex_lock(&pi->omx_active_mutex);
    //fprintf(stderr,"[acodec] - got omx_active_mutex\n");
    while (pi->omx_active != 1) {
      pthread_cond_wait(&pi->omx_active_cv, &pi->omx_active_mutex);
      //fprintf(stderr,"[acodec] - omx_active=%d\n",pi->omx_active);
    }
    if (is_paused) {
      // Wait for resume message
      fprintf(stderr,"acodec: Waiting for resume\n");
      pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
      is_paused = 0;
      pthread_mutex_unlock(&codec->queue_mutex);
    }
    current = codec_queue_get_next_item(codec);

    if (current->msgtype == MSG_STOP) {
      printf("[acodec] Stopping\n");
      codec_queue_free_item(codec,current);
      pi->omx_active = 2; /* 2 == restarting, allows the vcodec to cleanly shutdown and restart */
      pthread_mutex_unlock(&pi->omx_active_mutex);
      goto new_channel;
    } else if (current->msgtype == MSG_NEW_CHANNEL) {
      fprintf(stderr,"[acodec] NEW_CHANNEL received, going to new_channel\n");
      codec_queue_free_item(codec,current);
      pthread_mutex_unlock(&pi->omx_active_mutex);
      goto new_channel;;
    } else if (current->msgtype == MSG_PAUSE) {
      fprintf(stderr,"acodec: Paused\n");
      codec_queue_free_item(codec,current);
      is_paused = 1;
      pthread_mutex_unlock(&pi->omx_active_mutex);
      goto next_packet;
    } else if (current->msgtype == MSG_SET_VOLUME) {
      fprintf(stderr, "[acodec] SET_VOLUME %ld\n", (long)current->data);
      omx_audio_volume(&pi->audio_render, (long)current->data);
      free(current);
      pthread_mutex_unlock(&pi->omx_active_mutex);
      goto next_packet;
    }

    buf = get_next_buffer(&pi->audio_render);
    buf->nTimeStamp = pts_to_omx(current->data->PTS);
    //fprintf(stderr,"Audio timestamp=%lld\n",current->data->PTS);

#if 0
    res = -1;
    if (codec->acodectype == CODEC_ID_MP2 || codec->acodectype == CODEC_ID_MP3) {
      res = mpg123_decode(m,current->data->packet,current->data->packetlength,buf->pBuffer,buf->nAllocLen,&buf->nFilledLen);
      res = (res == MPG123_ERR);
    }
#endif
    if (current->data->packetlength > (int)buf->nAllocLen) {
      fprintf(stderr, "packetlength > alloclen: %d > %u\n", current->data->packetlength, buf->nAllocLen);
      res = -1;
    } else {
      memcpy(buf->pBuffer, current->data->packet, current->data->packetlength);
      buf->nFilledLen = current->data->packetlength;
      res = 0;
    }


    if (res == 0) {
      buf->nFlags = 0;
      if(codec->first_packet)
      {
        //usleep(1000000);
        fprintf(stderr,"First audio packet\n");
        buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        codec->first_packet = 0;
      }

      buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

      OERR(OMX_EmptyThisBuffer(pi->audio_render.h, buf));
    }

    pthread_mutex_unlock(&pi->omx_active_mutex);

    codec_set_pts(codec,current->data->PTS);

    codec_queue_free_item(codec,current);
  }

  return 0;
}

void acodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pi)
{
  fprintf(stderr, "%s\n", __func__);
  codec->codecstate = NULL;

  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pi;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))acodec_omx_thread,(void*)args);
}

static void* vcodec_omx_thread(struct codec_init_args_t* args)
{
   struct codec_t* codec = args->codec;
   struct omx_pipeline_t* pi = args->pipe;
   char* audio_dest = args->audio_dest;
   OMX_VIDEO_CODINGTYPE coding;
   int width, height;
   struct codec_queue_t* current = NULL;
   int is_paused = 0;
   int64_t prev_DTS;
   OMX_BUFFERHEADERTYPE *buf;
   int current_aspect;
   int aspect;
   int gopbytes,totalbytes;
   uint64_t gopfirstdts;
   uint64_t firstdts = -1;
   double min_bitrate, max_bitrate;

   free(args);
   hal_set_threadname("hal:omx_video");

   codec->first_packet = 1;

   pthread_mutex_lock(&pi->omx_active_mutex);

   fprintf(stderr,"Starting vcodec_omx_thread\n");

next_channel:
   fprintf(stderr,"vcodec_omx_thread: next_channel\n");
   coding = OMX_VIDEO_CodingUnused;
   codec->first_packet = 1;
   prev_DTS = -1;
   current_aspect = 0;
   pi->video_render.aspect = 0;
   aspect = 0;
   firstdts = -1;
   totalbytes = 0;
   gopbytes = -1;
   min_bitrate = 0;
   max_bitrate = 0;

   while (1)
   {
next_packet:
     if (current == NULL) {
       if (is_paused) {
         // Wait for resume message
         fprintf(stderr,"vcodec: Waiting for resume\n");
         pthread_cond_wait(&codec->resume_cv,&codec->queue_mutex);
         pthread_mutex_unlock(&codec->queue_mutex);
         omx_clock_set_speed(&pi->clock, 1<<16);
         is_paused = 0;
       }
       //fprintf(stderr,"[vcodec] getting next item\n\n");
       current = codec_queue_get_next_item(codec); 
       //fprintf(stderr,"[vcodec] got next item\n\n");

       if ((current->msgtype == MSG_NEW_CHANNEL) || (current->msgtype == MSG_STOP)) {
         codec_queue_free_item(codec,current);
         current = NULL;
         if (pi->omx_active) {
           fprintf(stderr,"[vcodec] NEW_CHANNEL received, restarting pipeline\n");
           goto stop;
         } else {
           fprintf(stderr,"[vcodec] NEW_CHANNEL received, pipeline not active\n");
///           pthread_mutex_unlock(&pi->omx_active_mutex); /* next_channel wants the mutex already locked */
///           fprintf(stderr,"[vcodec] unlocked omx_active_mutex\n");
           goto next_channel;
         }
       } else if (current->msgtype == MSG_PAUSE) {
         fprintf(stderr,"vcodec: Paused\n");
         codec_queue_free_item(codec,current);
         current = NULL;
         omx_clock_set_speed(&pi->clock, 0);
         is_paused = 1;
         goto next_packet;
       } else if (current->msgtype == MSG_SET_ASPECT_4_3) {
         omx_set_display_region(pi, 240, 0, 1440, 1080);
         current = NULL;
         goto next_packet;
       } else if (current->msgtype == MSG_SET_ASPECT_16_9) {
         omx_set_display_region(pi, 0, 0, 1920, 1080);
         current = NULL;
         goto next_packet;
       } else if (current->msgtype == MSG_PIG) {
         struct pig_params_t *pig = (struct pig_params_t *)current->data;
         if (pig->x < 0)
           omx_set_display_region(pi, 0, 0, output_info.width, output_info.height);
         else {
           int x = pig->x * output_info.width / 1280;
           int y = pig->y * output_info.height / 720;
           int w = pig->w * output_info.width / 1280;
           int h = pig->h * output_info.height / 720;
           omx_set_display_region(pi, x, y, w, h);
         }
         free(pig);
         free(current);
         current = NULL;
         goto next_packet;
       }
       if ((prev_DTS != -1) && ((prev_DTS + 40000) != current->data->DTS) && ((prev_DTS + 20000) != current->data->DTS)) {
         fprintf(stderr,"DTS discontinuity - DTS=%lld, prev_DTS=%lld (diff = %lld)\n",current->data->DTS,prev_DTS,current->data->DTS-prev_DTS);
       }
       prev_DTS = current->data->DTS;
     }

     if (current->data == NULL) {
       fprintf(stderr,"ERROR: data is NULL (expect segfault!)");
     }

     if (current->data->frametype == 'I') {
       if (firstdts == (uint64_t)-1) { firstdts = current->data->DTS; }
       if (gopbytes != -1) {
         double duration = current->data->DTS-gopfirstdts;
         double total_duration = current->data->DTS-firstdts;
         double bitrate = (1000000.0/duration) * gopbytes * 8.0;
         double total_bitrate = (1000000.0/total_duration) * totalbytes * 8.0;
         if ((min_bitrate == 0) || (bitrate < min_bitrate)) { min_bitrate = bitrate; }
         if ((max_bitrate == 0) || (bitrate > max_bitrate)) { max_bitrate = bitrate; }
         fprintf(stderr,"GOP: %d bytes (%dms) - %.3fMbps  (avg: %.3fMbps, min: %.3fMbps, max: %.3fMbps                    \r",gopbytes,(int)(current->data->DTS-gopfirstdts),bitrate/1000000,total_bitrate/1000000,min_bitrate/1000000,max_bitrate/1000000);
       }
       gopbytes = current->data->packetlength;
       gopfirstdts = current->data->DTS;
       totalbytes += current->data->packetlength;
     } else {
       if (gopbytes >= 0)
         gopbytes += current->data->packetlength;
       totalbytes += current->data->packetlength;
     }
     if ((current->data->frametype == 'I') && (codec->vcodectype == OMX_VIDEO_CodingMPEG2)) {
       unsigned char* p = current->data->packet;
       /* Parse the MPEG stream to extract the aspect ratio.
          TODO: Handle the Active Format Description (AFD) which is frame-accurate.  This is just GOP-accurate .

          "AFD is optionally carried in the user data of video elementary bitstreams, after the sequence
          extension, GOP header, and/or picture coding extension."
        */
       if ((p[0]==0) && (p[1]==0) && (p[2]==1) && (p[3]==0xb3)) { // Sequence header
         //int width = (p[4] << 4) | (p[5] & 0xf0) >> 4;
         //int height = (p[5] & 0x0f) << 8 | p[6];
         aspect = (p[7] & 0xf0) >> 4;

         //fprintf(stderr,"MPEG-2 sequence header - width=%d, height=%d, aspect=%d\n",width,height,aspect);
       }
     }

     /* Check if aspect ratio in video_render component has changed */
     if ((codec->vcodectype == OMX_VIDEO_CodingMPEG2) && (pi->video_render.aspect != current_aspect)) {
       if (pi->video_render.aspect == 2) { // 4:3
         fprintf(stderr,"Switching to 4:3\n");
         omx_set_display_region(pi, 240, 0, 1440, 1080);
       } else { // 16:9 - DVB can only be 4:3 or 16:9
         fprintf(stderr,"Switching to 16:9\n");
         omx_set_display_region(pi, 0, 0, 1920, 1080);
       }
       current_aspect = pi->video_render.aspect;
     }

     if (coding == OMX_VIDEO_CodingUnused) {
       fprintf(stderr,"Setting up OMX pipeline... - vcodectype=%d\n",codec->vcodectype);
       omx_setup_pipeline(pi, codec->vcodectype, audio_dest, ((codec->width*codec->height) > 720*576) ? 1 : 0);
 
       fprintf(stderr,"Done setting up OMX pipeline.\n");
       coding = codec->vcodectype;
       width = codec->width;
       height = codec->height;
       fprintf(stderr,"Initialised video codec - %s width=%d, height=%d\n",((coding == OMX_VIDEO_CodingAVC) ? "H264" : "MPEG-2"), width, height);
       codec->acodec->first_packet = 1;
       codec->acodec->is_running = 1; /* hack, this makes avdec.cpp start feeding PCM data */

       /* We are ready to go, allow the audio codec back in */
       pi->omx_active = 1;
       pthread_cond_signal(&pi->omx_active_cv);
       //fprintf(stderr,"[vcodec] unlocking omx_active_mutex\n");
       pthread_mutex_unlock(&pi->omx_active_mutex);
       //fprintf(stderr,"[vcodec] unlocked omx_active_mutex\n");
     } else if ((coding != codec->vcodectype) || (width != codec->width) || (height != codec->height)) {
       fprintf(stderr,"Change of codec detected, restarting video codec\n");
       goto stop;
     }

     int bytes_left = current->data->packetlength;
     unsigned char* p = current->data->packet;
     //fprintf(stderr,"Processing video packet - %d bytes\n",bytes_left);
     while (bytes_left > 0) {
       // fprintf(stderr,"OMX buffers: v: %02d/20 a: %02d/32 free, vcodec queue: %4d, acodec queue: %4d\r",omx_get_free_buffer_count(&pi->video_decode),omx_get_free_buffer_count(&pi->audio_render),codec->queue_count, codec->acodec->queue_count);
       buf = get_next_buffer(&pi->video_decode);   /* This will block if there are no empty buffers */

       int to_copy = OMX_MIN(bytes_left, (int)buf->nAllocLen);
       //fprintf(stderr,"Copying %d bytes\n",to_copy);

       memcpy(buf->pBuffer, p, to_copy);
       p += to_copy;
       bytes_left -= to_copy;
       buf->nTimeStamp = pts_to_omx(current->data->PTS);
       buf->nFilledLen = to_copy;

       buf->hMarkTargetComponent = pi->video_render.h;
       buf->pMarkData = (OMX_PTR)aspect;

       buf->nFlags = 0;
       if(codec->first_packet)
       {
         fprintf(stderr,"First video packet\n");
         buf->nFlags |= OMX_BUFFERFLAG_STARTTIME;
         codec->first_packet = 0;
       }

       if (bytes_left == 0)
         buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

       if (pi->video_decode.port_settings_changed == 1)
       {
         pi->video_decode.port_settings_changed = 2;
         fprintf(stderr,"video_decode port_settings_changed = 1\n");

         if (pi->do_deinterlace) {
           OERR(OMX_SetupTunnel(pi->video_decode.h, 131, pi->image_fx.h, 190));
           omx_send_command_and_wait(&pi->video_decode, OMX_CommandPortEnable, 131, NULL);

           omx_send_command_and_wait(&pi->image_fx, OMX_CommandPortEnable, 190, NULL);
           omx_send_command_and_wait(&pi->image_fx, OMX_CommandStateSet, OMX_StateExecuting, NULL);
         } else {
           OERR(OMX_SetupTunnel(pi->video_decode.h, 131, pi->video_scheduler.h, 10));
           omx_send_command_and_wait(&pi->video_decode, OMX_CommandPortEnable, 131, NULL);

           omx_send_command_and_wait(&pi->video_scheduler, OMX_CommandPortEnable, 10, NULL);
           omx_send_command_and_wait(&pi->video_scheduler, OMX_CommandStateSet, OMX_StateExecuting, NULL);
           omx_send_command_and_wait(&pi->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
         }
       }

       if (pi->image_fx.port_settings_changed == 1)
       {
         pi->image_fx.port_settings_changed = 2;
         fprintf(stderr,"image_fx port_settings_changed = 1\n");

         OERR(OMX_SetupTunnel(pi->image_fx.h, 191, pi->video_scheduler.h, 10));
         omx_send_command_and_wait(&pi->image_fx, OMX_CommandPortEnable, 191, NULL);

         omx_send_command_and_wait(&pi->video_scheduler, OMX_CommandPortEnable, 10, NULL);
         omx_send_command_and_wait(&pi->video_scheduler, OMX_CommandStateSet, OMX_StateExecuting, NULL);
         omx_send_command_and_wait(&pi->video_render, OMX_CommandStateSet, OMX_StateIdle, NULL);
       }

       if (pi->video_scheduler.port_settings_changed == 1)
       {
         pi->video_scheduler.port_settings_changed = 2;
         fprintf(stderr,"video_scheduler port_settings_changed = 1\n");

         OERR(OMX_SetupTunnel(pi->video_scheduler.h, 11, pi->video_render.h, 90));  
         omx_send_command_and_wait(&pi->video_scheduler, OMX_CommandPortEnable, 11, NULL);
         omx_send_command_and_wait(&pi->video_render, OMX_CommandPortEnable, 90, NULL);
         omx_send_command_and_wait(&pi->video_render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
       }

       OERR(OMX_EmptyThisBuffer(pi->video_decode.h, buf));
     }

     codec_queue_free_item(codec,current);
     current = NULL;
   }

stop:
   /* We lock the mutex to stop the audio codec.  It is unlocked after the pipline is setup again */

   //fprintf(stderr,"[vcodec] - waiting for omx_active_mutex\n");
   pthread_mutex_lock(&pi->omx_active_mutex);
   //fprintf(stderr,"[vcodec] - got omx_active_mutex, tearing down pipeline.\n");

   omx_teardown_pipeline(pi);
   //fprintf(stderr,"[vcodec] - End of omx thread, pipeline torn down.\n");
   pi->omx_active = 0;

   goto next_channel;

   return 0;
}

void vcodec_omx_init(struct codec_t* codec, struct omx_pipeline_t* pi, char* audio_dest)
{
  fprintf(stderr, "%s\n", __func__);
//codec->vcodectype = OMX_VIDEO_CodingUnused;
  codec_queue_init(codec);

  struct codec_init_args_t* args = malloc(sizeof(struct codec_init_args_t));
  args->codec = codec;
  args->pipe = pi;
  args->audio_dest = audio_dest;

  pthread_create(&codec->thread,NULL,(void * (*)(void *))vcodec_omx_thread,(void*)args);
}
