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
struct _ConnectedSignals {
	guint disconnect_timer;
	guint clear_old_events_timer;
	
	gulong received_im_signal;
	gulong buddy_typing_signal;
	gulong buddy_typing_stopped_signal;
	gulong sent_im_signal;
};
static struct _ConnectedSignals ConnectedSignals = {0,0,0,0,0};
typedef struct _JuiceEvent {
	gchar *event;
	gulong timestamp;
} JuiceEvent;
typedef struct _JuiceChannel {
	GIOChannel *channel;
	guint timeout;
	gulong timestamp;
} JuiceChannel;

gboolean remove_old_events(gpointer dunno)
{
	//Events older than this timestamp will be removed
	gulong old_timestamp;
	JuiceEvent *event;
	
	old_timestamp = time(NULL) - 3 * 60; //3 minutes old
	while (!g_queue_is_empty(&queue))
	{
		event = g_queue_peek_tail(&queue);
		if (event->timestamp > old_timestamp)
			break;
		event = g_queue_pop_tail(&queue);
		g_free(event->event);
		g_free(event);
	}

	return TRUE;	
}

gboolean is_event_since(gulong time)
{
	JuiceEvent *event;
	
	event = g_queue_peek_tail(&queue);
	if (event && event->timestamp > time)
		return TRUE;
	
	return FALSE;	
}

guint number_of_events_since(gulong time)
{
	guint count;
	JuiceEvent *event;
	guint max_length;
	
	max_length = g_queue_get_length(&queue);
	for(count=max_length-1; count>=0; count--)
	{
		event = g_queue_peek_nth(&queue, count);
		if (event->timestamp < time)
			break;
		returnlist = g_list_prepend(returnlist, event);
	}
	
	return count + 1;
}

GList *get_events_since(gulong time)
{
	GList *returnlist = NULL;
	JuiceEvent *event;
	guint i;
	guint max_length;
	
	max_length = g_queue_get_length(&queue);
	for(i=max_length-1; i>=0; i--)
	{
		event = g_queue_peek_nth(&queue, i);
		if (event->timestamp < time)
			break;
		returnlist = g_list_prepend(returnlist, event);
	}
	
	return returnlist;
}

static gboolean
disconnect_signals_cb(gpointer data)
{
	JuiceEvent *event;
	
	purple_debug_info("pidgin_juice", "Disconnecting signals\n");
	
	purple_signals_disconnect_by_handle(&ConnectedSignals);
	
	while(event = g_queue_pop_head(&queue))
	{
		//clear the queue
		g_free(event->event);
		g_free(event);
	}
	
	if (ConnectedSignals.clear_old_events_timer)
		purple_timeout_remove(ConnectedSignals.clear_old_events_timer);
	
	return FALSE; //return false so that the callback doesn't get fired twice
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
	JuiceEvent *next_event;
	gboolean first = TRUE;
	guint timeout;
	guint i;
	JuiceChannel *chan;
	GList *latest_events;
	GList *current;
	
	if (channels == NULL)
	{
		return FALSE;
	}
		
	chan = g_hash_table_lookup(channels, channel);
	if (chan && chan->timeout)
		purple_timeout_remove(timeout);
	g_hash_table_steal(channels, channel);
	
	returnstring = g_string_new("{ \"events\" : [ ");
	
	if (!chan || !is_event_since(chan->timestamp))
	{
		returnstring = g_string_append(returnstring, 
									   "{ \"type\":\"continue\" }");
	} else {
		latest_events = get_events_since(chan->timestamp);
		for(current=latest_events; current; current=g_list_next(current))
		{
			next_event = current->data;
			
			if (first)
				first = FALSE;
			else
				returnstring = g_string_append_c(returnstring, ',');
			
			returnstring = g_string_append(returnstring, next_event->event);
		}
		g_list_free(latest_events);
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
	
	return FALSE;
}

static void
events_table_foreach_cb(gpointer key, gpointer value, gpointer user_data)
{
	write_to_client(key);
}

static void
events_push_to_queue(gchar *output, gulong timestamp)
{
	JuiceEvent *event;
	
	event = g_new0(JuiceEvent, 1);
	event->event = output;
	event->timestamp = timestamp;
	
	//Add the event to the queue
	g_queue_push_tail(&queue, event);
	
	//Loop through the channels to return the events
	if (channels && g_hash_table_size(channels))
	{
		g_hash_table_foreach(channels, events_table_foreach_cb, NULL);
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
	gulong timestamp;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = g_strescape(purple_normalize(account, buddyname), "");
	escaped_message = g_strescape(message, "");
	
	timestamp = (gulong)time(NULL);
	
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
							 timestamp);
	
	events_push_to_queue(output, timestamp);
	
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
	gulong timestamp;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = g_strescape(purple_normalize(account, buddyname), "");
	escaped_message = g_strescape(message, "");
	
	timestamp = (gulong)time(NULL);
	
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
							 timestamp);
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_message);
	g_free(escaped_buddyname);
}

static void
buddy_typing_cb(PurpleAccount *account, const char *buddyname, gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	gulong timestamp;
	
	if (!buddyname)
		return;
	
	escaped_buddyname = g_strescape(purple_normalize(account, buddyname), "");
	
	timestamp = (gulong)time(NULL);
	
	output = g_strdup_printf("{ \"type\":\"typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\", "
							 "\"timestamp\":%lu }", 
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account),
							 timestamp);
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_buddyname);
}
static void 
buddy_typing_stopped_cb(PurpleAccount *account, const char *buddyname, gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	gulong timestamp;
	
	if (!buddyname)
		return;
	
	escaped_buddyname = g_strescape(purple_normalize(account, buddyname), "");
	
	timestamp = (gulong)time(NULL);
	
	output = g_strdup_printf("{ \"type\":\"not_typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\" }, "
							 "\"timestamp\":%lu }", escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account),
							 timestamp);
	
	events_push_to_queue(output, timestamp);
	
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
																	
	//add a timeout to delete really old events that we don't care about
	if (ConnectedSignals.clear_old_events_timer)
		purple_timeout_remove(ConnectedSignals.clear_old_events_timer);
	ConnectedSignals.clear_old_events_timer = purple_timeout_add_seconds(10, (GSourceFunc)remove_old_events, NULL);
	
}

static void
juice_GET_events(GIOChannel *channel, GHashTable *$_GET)
{
	gchar *body = NULL;
	gchar *headers = NULL;
	guint timeout;
	JuiceChannel *chan;
	gulong timestamp;
	
	connect_to_signals();
	
	if (channels == NULL)
		channels = g_hash_table_new_full(NULL, NULL, NULL, NULL);
	
	//Otherwise, store up the channel
	timeout = purple_timeout_add_seconds(60, (GSourceFunc)write_to_client, channel);
	timestamp = (gulong)strtoul(const char *nptr, NULL, 10);
	
	chan = g_new0(JuiceChannel, 1);
	chan->channel = channel;
	chan->timeout = timeout;
	chan->timestamp = timestamp;
	
	g_hash_table_insert(channels, channel, chan);
	
	if (is_event_since(timestamp))
	{
		write_to_client(channel);
		return;
	}
}
