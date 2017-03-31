#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <glib.h>


/**
* Structure used to provide application data to a ControlsThread
* @see ControlsThread
* @see main
*/
struct Data{
  GstBus *bus; /**< GStreamer bus used in application.*/
  GstElement *pipeline, *volume; /**< GStreamer pipeline and volume controlling elements.*/
};

/**
* Function to use as bus watch, which captures messages posted on bus.
* @param bus - bus capturing messages.
* @param msg - message to handle.
* @param data - pointer to main loop.
* @see main
* @return TRUE
*/
static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;
  // Message handling
  switch (GST_MESSAGE_TYPE (msg)) {
    // End of stream message -> closing application.
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    // Error message -> parse it, print on stderr and close application.
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
    // Default -> do nothing.
    default:
      break;
  }

  return TRUE;
}

/**
* Function used by ControlsThread. It is waiting for commands on standard input.
* @param data_instance - A Structure keeping all necessary data for handling commands.
* @see main
* @see Data
*/
void *
player_controls_function(struct Data *data_instance)
{
    char str; // Used for capturing characters.
    while(TRUE){
      // Will exit and unref from main.
      scanf("%c", &str);
      // Commands handling.
      switch (str) {
        // P - play or pause.
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
        // S - stop. Send end of stream message to bus.
        case 'S':
        {
          gst_bus_post (data_instance->bus, gst_message_new_eos(NULL));
          break;
        }
        // + - increase volume if possible.
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
        // - - decrease volume if possible.
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
        // Default not definied.
      }
    }
}

/**
* Main function. Creates all necessary objects and handlers and starts main loop. Memory is freed after quit.
* @param argc - arguments counter.
* @param argv - file name with extension and optional play flag.
* @see player_controls_function
* @see Data
* @see bus_call
* @return int;
*/
int
main (int   argc,
      char *argv[])
{
  // Declaring all needed objects.
  GMainLoop *loop;

  GThread *ControlsThread;
  GError *err = NULL;

  GstElement *pipeline, *source, *parser, *volume, *conv, *sink;
  GstBus *bus;
  guint bus_watch_id;

  // GStreamer and Glib Main Loop initialisation.
  gst_init (&argc, &argv);

  loop = g_main_loop_new (NULL, FALSE);


  // Check input arguments
  if (argc != 3 && argc != 2){
    g_printerr ("Usage: %s <wav or acc filename> [-P play]\nor: %s -h (for help)\n", argv[0], argv[0]);
    g_main_loop_unref (loop);
    return -1;
  }
  else if (argc == 3 && strcmp(argv[2], "-P")){
    g_printerr ("Inappropriate flag\n");
    g_main_loop_unref (loop);
    return -1;
  }
  // Help request handling
  else if (argc == 2 && !(strcmp(argv[1], "-h"))){
    g_print ("Usage: %s\n\
    args:\n\
      <wav or acc filename with extension>\n\
      [-P play if you want to start playing file all over]\n\
    commands(type in console):\n\
      (P) - play/pause\n\
      (S) - stop and close\n\
      (+) - volume up\n\
      (-) - volume down\n", argv[0]);
    g_main_loop_unref (loop);
    return 0;
  }
  // Check extension
  guint len = strlen(argv[1]);
  if (len < 5){
    g_printerr ("Usage: %s <wav or acc filename> [-P play]\nor: %s -h (for help)\n", argv[0], argv[0]);
    g_main_loop_unref (loop);
    return -1;
  }
  const gchar *extension = &argv[1][len-3];
  // Create parser first
  if (strcmp(extension, "wav") == 0)
    parser  = gst_element_factory_make ("wavparse", "wav-parser");
  else if (strcmp(extension, "aac") == 0)
    parser  = gst_element_factory_make ("faad", "acc-parser");
  else{
    g_printerr ("Unknown file extension\n");
    return -1;
    g_main_loop_unref (loop);
  }
  // Create the rest of GStreamer elements
  pipeline = gst_pipeline_new ("audio-player");
  source   = gst_element_factory_make ("filesrc", "file-source");
  volume = gst_element_factory_make ("volume", "volume");
  conv     = gst_element_factory_make ("audioconvert",  "converter");
  sink     = gst_element_factory_make ("autoaudiosink", "audio-output");

  if (!pipeline || !source || !parser || !volume || !conv || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
    g_main_loop_unref (loop);
  }

  // Set up the pipeline.

  // We set the input filename to the source element.
  g_object_set (G_OBJECT (source), "location", argv[1], NULL);

  // Adding message handler to bus.
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  // Adding elements to pipeline.
  gst_bin_add_many (GST_BIN (pipeline),
                    source, parser, volume, conv, sink, NULL);

  // Linking the elements.
  gst_element_link_many (source, parser, volume, conv, sink, NULL);

  // Setting pipeline state.
  g_print ("Now playing: %s\n", argv[1]);
  if (argc == 3){
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    g_print ("Running\n");
  }
  else{
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    g_print ("Paused\n");
  }

  // Creating Data object instance for ControlsThread.
  struct Data *data_instance = malloc(sizeof(struct Data));
  data_instance->bus = bus;
  data_instance->volume = volume;
  data_instance->pipeline = pipeline;

  // Creating ControlsThread, which provide command handling.
  if(!(ControlsThread = g_thread_try_new(NULL, (GThreadFunc)player_controls_function, data_instance, &err))){
    g_printerr("Thread create failed: %s\n", err->message );
    g_error_free(err);
    return -1;
  }

  // Setting starting volume level to 50%.
  g_object_set(G_OBJECT (volume), "volume", 0.5, NULL);
  g_main_loop_run(loop);

  // Out of main loop. Clearing memory.
  g_print ("Stopped\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  g_thread_unref(ControlsThread);

  free(data_instance);

  return 0;
}
