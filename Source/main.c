#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <glib.h>

struct Data{
  GstBus *bus;
  GstElement *pipeline, *volume;
};

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

void *
player_controls_function(struct Data *data_instance)
{
    char str;
    while(TRUE){
      scanf("%c", &str);
      switch (str) {
        case 'P':
        {
          if(GST_STATE_TARGET(data_instance->pipeline) == GST_STATE_PLAYING){
            gst_element_set_state (data_instance->pipeline, GST_STATE_PAUSED);
            g_print ("Paused\n");
          }
          else{
            gst_element_set_state (data_instance->pipeline, GST_STATE_PLAYING);
            g_print ("Running\n");
          }
          break;
        }
        case 'S':
        {
          gst_bus_post (data_instance->bus, gst_message_new_eos(NULL));
          break;
        }
        case '+':
        {
          gdouble val;
          g_object_get(G_OBJECT(data_instance->volume), "volume", &val, NULL);
          val += 0.1;
          if(val > 3.9)
            val = 4.0;
          g_object_set(G_OBJECT (data_instance->volume), "volume", val, NULL);
          g_print ("Volume: %d%%\n", (int)(((val/1.0)*100)+0.5));
          break;
        }
        case '-':
        {
          gdouble val;
          g_object_get(G_OBJECT(data_instance->volume), "volume", &val, NULL);
          val -= 0.1;
          if(val < 0.1)
            val = 0.0;
          g_object_set(G_OBJECT (data_instance->volume), "volume", val, NULL);
          g_print ("Volume: %d%%\n", (int)(((val/1.0)*100)+0.5));
          break;
        }
      }
    }
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *loop;

  GThread *ControlsThread;
  GError *err = NULL;

  GstElement *pipeline, *source, *wavparser, *volume, *conv, *sink;
  GstBus *bus;
  guint bus_watch_id;

  /* Initialisation */
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);


  /* Check input arguments */
  if (argc != 2 && argc != 3){
    g_printerr ("Usage: %s <Wav filename> [-P play]\n", argv[0]);
    return -1;
  }
  else if (argc == 3 && strcmp(argv[2], "-P")){
    g_printerr ("Inappropriate flag\n");
    return -1;
  }

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("audio-player");
  source   = gst_element_factory_make ("filesrc",       "file-source");
  wavparser  = gst_element_factory_make ("wavparse",      "wav-parser");
  volume = gst_element_factory_make ("volume",      "volume");
  conv     = gst_element_factory_make ("audioconvert",  "converter");
  sink     = gst_element_factory_make ("autoaudiosink", "audio-output");

  if (!pipeline || !source || !wavparser || !volume || !conv || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }

  /* Set up the pipeline */

  /* we set the input filename to the source element */
  g_object_set (G_OBJECT (source), "location", argv[1], NULL);

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  /* we add all elements into the pipeline */
  /* file-source | wav-parser | converter | alsa-output */
  gst_bin_add_many (GST_BIN (pipeline),
                    source, wavparser, volume, conv, sink, NULL);

  /* we link the elements together */
  /* file-source -> wav-parser -> converter -> alsa-output */
  gst_element_link_many (source, wavparser, volume, conv, sink, NULL);

  /* Set the pipeline to appropriate state*/
  g_print ("Now playing: %s\n", argv[1]);
  if (argc == 3){
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    g_print ("Running\n");
  }
  else{
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    g_print ("Paused\n");
  }

  struct Data *data_instance = malloc(sizeof(struct Data));
  data_instance->bus = bus;
  data_instance->volume = volume;
  data_instance->pipeline = pipeline;

  if(!(ControlsThread = g_thread_try_new(NULL, (GThreadFunc)player_controls_function, data_instance, &err))){
    g_printerr("Thread create failed: %s\n", err->message );
    g_error_free(err);
    return -1;
  }

  g_object_set(G_OBJECT (volume), "volume", 0.5, NULL);
  g_main_loop_run(loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Stopped\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  g_thread_unref(ControlsThread);

  free(data_instance);

  return 0;
}
