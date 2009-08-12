/*
 *  get_events.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 8/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */


#ifndef G_QUEUE_INIT
#define G_QUEUE_INIT { NULL, NULL, 0 }
#endif

static GQueue queue = G_QUEUE_INIT;
static GHashTable *channels = NULL; 
static guint current_seq = 0;
struct _ConnectedSignals {
	guint disconnect_timer;
	
	gulong received_im_signal;
	gulong buddy_typing_signal;
	gulong buddy_typing_stopped_signal;
	gulong sent_im_signal;
};
static struct _ConnectedSignals ConnectedSignals = {0,0,0,0,0};



static gboolean
disconnect_signals_cb(gpointer data)
{
	purple_debug_info("pidgin_juice", "Disconnecting signals\n");
	
	purple_signals_disconnect_by_handle(&ConnectedSignals);
	
	while(g_queue_pop_head(&queue))
	{
		//clear the queue
	}
	
	current_seq = 0;
	
	return FALSE; //return false so that the event doesn't get fired twice
}
static void
disconnect_signals()
{
	if (ConnectedSignals.disconnect_timer)
		purple_timeout_remove(ConnectedSignals.disconnect_timer);
	
	ConnectedSignals.disconnect_timer = purple_timeout_add_seconds(60, 
																	disconnect_signals_cb, &ConnectedSignals);
}

static gboolean
write_to_client(GIOChannel *channel)
{
	GString *returnstring = NULL;
	gchar *headers = NULL;
	gchar *next_event;
	gboolean first = TRUE;
	guint timeout;
	
	if (channels != NULL)
	{
		timeout = GPOINTER_TO_INT(g_hash_table_lookup(channels, channel));
		if (timeout)
			purple_timeout_remove(timeout);
		g_hash_table_steal(channels, channel);
	}
	
	//Jeremy put this here in an attempt to stop the segfaulting
	//if (channel == NULL)
	//	return FALSE;
	
	returnstring = g_string_new("{ \"events\" : [ ");
	
	if (g_queue_is_empty(&queue))
	{
		returnstring = g_string_append(returnstring, 
									   "{ \"type\":\"continue\" }");
	} else {
		while ((next_event = g_queue_pop_head(&queue)))
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
	
	write_data(channel, G_IO_OUT, headers, strlen(headers));
	write_data(channel, G_IO_OUT, returnstring->str, returnstring->len + 1);
	g_io_channel_flush(channel, NULL);
	
	g_free(headers);
	g_string_free(returnstring, TRUE);
	
	disconnect_signals();
	
	current_seq++;
	
	return FALSE;
}static void
events_table_foreach_cb(gpointer key, gpointer value, gpointer user_data)
{
	write_to_client(key);
}
static void
events_push_to_queue(gchar *output)
{
	//Add the event to the queue
	g_queue_push_tail(&queue, output);
	
	//Loop through the channels to return the events
	if (channels && g_hash_table_size(channels))
	{
		g_hash_table_foreach(channels, events_table_foreach_cb, NULL);		
		//g_hash_table_foreach(channels, (GHFunc)write_to_client, NULL);
	}
}
static void
received_im_msg_cb(PurpleAccount *account, char *buddyname, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags,
				   gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	gchar *escaped_message;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = g_strescape(buddyname);
	escaped_message = g_strescape(message);
	
	output = g_strdup_printf("{ \"type\":\"received\","
							 "\"message\":\"%s\", "
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\", "
							 "\"timestamp\":%lu }",
							 escaped_message,
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account),
							 (gulong)time(NULL));
	
	events_push_to_queue(output);
	
	g_free(escaped_message);
	g_free(escaped_buddyname);
}

static void
sent_im_msg_cb(PurpleAccount *account, char *buddyname, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags,
				   gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	gchar *escaped_message;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = g_strescape(buddyname);
	escaped_message = g_strescape(message);
	
	output = g_strdup_printf("{ \"type\":\"sent\","
							 "\"message\":\"%s\", "
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\", "
							 "\"timestamp\":%lu }",
							 escaped_message,
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account),
							 (gulong)time(NULL));
	
	events_push_to_queue(output);
	
	g_free(escaped_message);
	g_free(escaped_buddyname);
}

static void
buddy_typing_cb(PurpleAccount *account, const char *buddyname, gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = g_strescape(buddyname);
	
	output = g_strdup_printf("{ \"type\":\"typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\" }", 
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account));
	
	events_push_to_queue(output);
	
	g_free(escaped_buddyname);
}
static void 
buddy_typing_stopped_cb(PurpleAccount *account, const char *buddyname, gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = g_strescape(buddyname);
	
	output = g_strdup_printf("{ \"type\":\"not_typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\" }", escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account));
	
	events_push_to_queue(output);
	
	g_free(escaped_buddyname);
}



static void
connect_to_signals()
{
    void *conv_handle = purple_conversations_get_handle();
	
	purple_debug_info("pidgin_juice", "Connecting signals\n");
	
	if (!ConnectedSignals.received_im_signal)
	{
		ConnectedSignals.received_im_signal =
			purple_signal_connect(conv_handle, "received-im-msg",
								  &ConnectedSignals, PURPLE_CALLBACK(received_im_msg_cb), 
								  NULL);
	}
	if (!ConnectedSignals.sent_im_signal)
	{
		ConnectedSignals.sent_im_signal =
			purple_signal_connect(conv_handle, "sent-im-msg",
								  &ConnectedSignals, PURPLE_CALLBACK(sent_im_msg_cb), 
								  NULL);
	}
	if (!ConnectedSignals.buddy_typing_signal)
	{
		ConnectedSignals.buddy_typing_signal = 
			purple_signal_connect(conv_handle, "buddy-typing",
								  &ConnectedSignals, PURPLE_CALLBACK(buddy_typing_cb), 
								  NULL);
	}
	if (!ConnectedSignals.buddy_typing_stopped_signal)
	{
		ConnectedSignals.buddy_typing_stopped_signal = 
			purple_signal_connect(conv_handle, "buddy-typing-stopped",
								  &ConnectedSignals, PURPLE_CALLBACK(buddy_typing_stopped_cb), 
								  NULL);
	}
	
	//add a timeout here to disconnect after 3 mins
	if (ConnectedSignals.disconnect_timer)
		purple_timeout_remove(ConnectedSignals.disconnect_timer);
	
	ConnectedSignals.disconnect_timer = purple_timeout_add_seconds(180, 
																	disconnect_signals_cb, &ConnectedSignals);
}

static void
juice_GET_events(GIOChannel *channel)
{
	gchar *body = NULL;
	gchar *headers = NULL;
	guint timeout;
	
	connect_to_signals();
	
	if (FALSE)
	{ //todo, change this to check $_GET['sequence'] with the current_seq
		body = g_strdup_printf("{ \"events\" : [ { \"type\":\"new_seq\", \"seq\":%d } ] }",
							   current_seq);
		
		headers = g_strdup_printf("HTTP/1.0 200 OK\r\n"
								  "Content-type: application/json\r\n"
								  "Content-length: %d\r\n\r\n", strlen(body));
		
		write_data(channel, G_IO_OUT, headers, strlen(headers));
		write_data(channel, G_IO_OUT, body, strlen(body)+1);
		
		g_free(headers);
		g_free(body);
		return;
	}
	
	if (!g_queue_is_empty(&queue))
	{
		write_to_client(channel);
		return;
	}
	
	if (channels == NULL)
		channels = g_hash_table_new_full(NULL, NULL, NULL, NULL);
	
	//Otherwise, store up the channel
	timeout = purple_timeout_add_seconds(60, (GSourceFunc)write_to_client, channel);
	g_hash_table_insert(channels, channel, GINT_TO_POINTER(timeout));
}
