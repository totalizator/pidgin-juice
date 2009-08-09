/*
 *  get_events.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 8/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

static void
received_im_msg_cb(PurpleAccount *account, char *sender, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags,
				   gpointer user_data)

{
	
	g_queue_push_tail(&message_queue, g_strdup(""));
}

static void
buddy_typing_cb(PurpleAccount *account, const char *name, gpointer user_data)
{
	
	g_queue_push_tail(&message_queue, g_strdup(""));
}
static void 
buddy_typing_stopped_cb(PurpleAccount *account, const char *name, gpointer user_data)
{
	
	g_queue_push_tail(&message_queue, g_strdup(""));
}

#ifndef G_QUEUE_INIT
#define G_QUEUE_INIT { NULL, NULL, 0 }
#endif

static GQueue message_queue = G_QUEUE_INIT;

struct _ConnectedSignals {
	guint disconnect_timer;
	
	gulong received_im_signal;
	gulong buddy_typing_signal;
	gulong buddy_typing_stopped_signal;
};

static struct _ConnectedSignals ConnectedSignals = NULL;

static void
connect_to_signals()
{
    void *conv_handle = purple_conversations_get_handle();
	
	purple_debug_info("pidgin_juice", "Connecting signals\n");
	
	if (ConnectedSignals == NULL)
		ConnectedSignals = g_new0(struct _ConnectedSignals, 1);
	
	if (!ConnectedSignals->received_im_signal)
	{
		ConnectedSignals->received_im_signal =
			purple_signal_connect(conv_handle, "received-im-msg",
								  ConnectedSignals, PURPLE_CALLBACK(received_im_msg_cb), 
								  NULL);
	}
	if (!ConnectedSignals->buddy_typing_signal)
	{
		ConnectedSignals->buddy_typing_signal = 
			purple_signal_connect(conv_handle, "buddy-typing",
								  ConnectedSignals, PURPLE_CALLBACK(buddy_typing_cb), 
								  NULL);
	}
	if (!ConnectedSignals->buddy_typing_stopped_signal)
	{
		ConnectedSignals->buddy_typing_stopped_signal = 
			purple_signal_connect(conv_handle, "buddy-typing-stopped",
								  ConnectedSignals, PURPLE_CALLBACK(buddy_typing_stopped_cb), 
								  NULL);
	}
	
	//add a timeout here to disconnect after 3 mins
	if (ConnectedSignals->disconnect_timer)
		purple_timeout_remove(ConnectedSignals->disconnect_timer);
	
	ConnectedSignals->disconnect_timer = purple_timeout_add_seconds(180, 
																	disconnect_signals_cb, ConnectedSignals);
}

static void
disconnect_signals()
{
	if (ConnectedSignals->disconnect_timer)
		purple_timeout_remove(ConnectedSignals->disconnect_timer);
	
	ConnectedSignals->disconnect_timer = purple_timeout_add_seconds(60, 
																	disconnect_signals_cb, ConnectedSignals);
}

static gboolean
disconnect_signals_cb(gpointer data)
{
	purple_debug_info("pidgin_juice", "Disconnecting signals\n");
	
	purple_signal_disconnect_by_handle(ConnectedSignals);
	g_free(ConnectedSignals);
	ConnectedSignals = NULL;
	
	while(g_queue_pop_head(&queue))
	{
		//clear the queue
	}
	
	return FALSE; //return false so that the event doesn't get fired twice
}

static gboolean
write_to_client(GIOChannel *channel)
{
	GString returnstring = NULL;
	gchar *headers = NULL;
	gchar *next_event;
	gboolean first = TRUE;
	
	returnstring = g_string_new("{ \"events\" : [ ");
	
	if (g_queue_is_empty())
	{
		returnstring = g_string_append(returnstring, 
									   "{ \"type\":\"continue\" }");
	} else {
		while (next_event = g_queue_pop_head(&queue))
		{
			if (first)
				first = FALSE;
			else
				returnstring = g_string_append_c(returnstring, ',');
			returnstring = g_string_append(returnstring, next_event);
			g_free(next_event);
		}
	}
	
	returnstring = g_string_append(returnstring, " ] }");
	
	headers = g_strdup_printf("HTTP/1.0 200 OK\r\n"
					   "Content-type: application/json\r\n"
					   "Content-length: %d\r\n\r\n", returnstring->len);
	
	write_data(channel, G_IO_OUT, headers, strlen(headers))
	write_data(channel, G_IO_OUT, returnstring, returnstring->len + 1);
	
	g_free(headers);
	g_string_free(returnstring, TRUE);
	
	disconnect_signals();
	
	current_seq++;
	
	return FALSE;
}

static guint current_seq = 0;

static void
juice_GET_events(GIOChannel *channel)
{	
	connect_to_signals();
	
	if (!g_queue_is_empty())
		write_to_client(channel);
}