#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <glib.h>

static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  g_print("Message received!\n");

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
player_controls_function(GstElement *pipeline)
{
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    char str;
    while(TRUE){
      scanf("%c", &str);
      switch (str) {
        case 'P':
        {
          if(GST_STATE_TARGET(pipeline) == GST_STATE_PLAYING){
            gst_element_set_state (pipeline, GST_STATE_PAUSED);
            g_print ("Paused\n");
          }
          else{
            gst_element_set_state (pipeline, GST_STATE_PLAYING);
            g_print ("Running\n");
          }
          break;
        }
        case 'S':
        {
          gst_bus_post (bus, gst_message_new_eos(NULL));
          break;
        }
        case '+':
        {
          //TODO
          break;
        }
        case '-':
        {
          //TODO
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

  GstElement *pipeline, *source, *wavparser, *conv, *sink;
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
  conv     = gst_element_factory_make ("audioconvert",  "converter");
  sink     = gst_element_factory_make ("autoaudiosink", "audio-output");

  if (!pipeline || !source || !wavparser || !conv || !sink) {
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
                    source, wavparser, conv, sink, NULL);

  /* we link the elements together */
  /* file-source -> wav-parser -> converter -> alsa-output */
  //gst_element_link (source, wavparser);
  gst_element_link_many (source, wavparser, conv, sink, NULL);

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
  if(!(ControlsThread = g_thread_try_new(NULL, (GThreadFunc)player_controls_function, pipeline, &err))){
    g_printerr("Thread create failed: %s\n", err->message );
    g_error_free(err);
    return -1;
  }
  g_main_loop_run(loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Stopped\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  g_thread_unref(ControlsThread);

  return 0;
}
