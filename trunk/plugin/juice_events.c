#ifndef FROM_JUICE_PLUGIN
#error Do not compile this file
#endif


#ifndef G_QUEUE_INIT
#define G_QUEUE_INIT { NULL, NULL, 0 }
#endif

static GQueue queue = G_QUEUE_INIT;
static GHashTable *channels = NULL;
struct _ConnectedSignals {
	guint disconnect_timer;
	guint clear_old_events_timer;
	
	gulong received_im_signal;
	gulong received_chat_signal;
	gulong buddy_typing_signal;
	gulong buddy_typing_stopped_signal;
	gulong sent_im_signal;
	gulong sent_chat_signal;
};
static struct _ConnectedSignals ConnectedSignals = {0,0,0,0,0,0,0,0};
typedef struct _JuiceEvent {
	gchar *event;
	guint64 timestamp;
} JuiceEvent;
typedef struct _JuiceChannel {
	gint fd;
	guint timeout;
	guint64 timestamp;
	gboolean is_eventstream;
} JuiceChannel;

static gboolean 
remove_old_events(gpointer dunno)
{
	//Events older than this timestamp will be removed
	guint64 old_timestamp;
	JuiceEvent *event;
	
	//purple_debug_info("pidgin_juice", "Remove old events from event queue\n");
	
	//old_timestamp = time(NULL) - 3 * 60 * 1000; //3 minutes old
	old_timestamp = ((guint64)time(NULL)) * 1000 - 10 * 1000; //3 minutes old
	while (!g_queue_is_empty(&queue))
	{
		event = g_queue_peek_head(&queue);
		//purple_debug_info("pidgin_juice", "queue is not empty. old:%" G_GUINT64_FORMAT " event:%" G_GUINT64_FORMAT "\n", old_timestamp, event->timestamp);
		if (event->timestamp > old_timestamp)
			break;
		//purple_debug_info("pidgin_juice", "timestamp is old\n");
		event = g_queue_pop_head(&queue);
		g_free(event->event);
		g_free(event);
	}

	//purple_debug_info("pidgin_juice", "Done removing old events.\n");
	return TRUE;
}

static gboolean 
is_event_since(guint64 time)
{
	JuiceEvent *event;
	
	event = g_queue_peek_tail(&queue);
	if (event && event->timestamp > time)
		return TRUE;
	
	return FALSE;	
}

static GList 
*get_events_since(guint64 time)
{
	GList *returnlist = NULL;
	JuiceEvent *event;
	gint i;
	guint max_length;
	
	max_length = g_queue_get_length(&queue);
	for(i=max_length-1; i>=0; i--)
	{
		event = g_queue_peek_nth(&queue, i);
		//purple_debug_info("pidgin_juice", "comparing event timestamp %" G_GUINT64_FORMAT " against requested timestamp %" G_GUINT64_FORMAT "\n", event->timestamp, time);
		if (event->timestamp <= time) {
			//purple_debug_info("pidgin_juice", "Found event with timestamp %" G_GUINT64_FORMAT " so not adding any more events to return list.\n", event->timestamp);
			break;
		}
		else {
			//purple_debug_info("pidgin_juice", "Adding event with timestamp %" G_GUINT64_FORMAT " to return list.\n", event->timestamp);
			returnlist = g_list_prepend(returnlist, event);
		}
	}
	
	return returnlist;
}

static gboolean
disconnect_signals_cb(gpointer data)
{
	JuiceEvent *event;
	
	purple_debug_info("pidgin_juice", "Disconnecting signals\n");
	
	purple_signals_disconnect_by_handle(&ConnectedSignals);
	ConnectedSignals.disconnect_timer = 0;
	ConnectedSignals.clear_old_events_timer = 0;
	ConnectedSignals.received_im_signal = 0;
	ConnectedSignals.buddy_typing_signal = 0;
	ConnectedSignals.buddy_typing_stopped_signal = 0;
	ConnectedSignals.sent_im_signal = 0;
	
	while((event = g_queue_pop_head(&queue)))
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
write_to_client(gint output_fd)
{
	GString *returnstring = NULL;
	gchar *headers = NULL;
	JuiceEvent *next_event;
	gboolean first = TRUE;
	JuiceChannel *chan = NULL;
	GList *latest_events;
	GList *current;
	
	purple_debug_info("pidgin_juice", "Writing to client.\n");
	if (channels == NULL)
	{
		purple_debug_info("pidgin_juice", "Channels is NULL, aborting write.\n");
		return FALSE;
	}
		
	chan = g_hash_table_lookup(channels, GINT_TO_POINTER(output_fd));
	g_hash_table_steal(channels, GINT_TO_POINTER(output_fd));
	if (!chan || !chan->timeout) {
		purple_debug_info("pidgin_juice", "Couldn't find channel struct.\n");
		//there was no real record of the channel.. don't try write to it
		return FALSE;
	}
	else {
		purple_timeout_remove(chan->timeout);
	}
	
	returnstring = g_string_new("{ \"events\" : [ ");
	
	if (!chan || !is_event_since(chan->timestamp))
	{
		returnstring = g_string_append(returnstring, 
									   "{ \"type\":\"continue\" }");
	} else {
		latest_events = get_events_since(chan->timestamp);
		//purple_debug_misc("pidgin_juice", "Events in the list: %u\n", g_list_length(latest_events));
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
	
	if (!chan->is_eventstream)
	{
		headers = g_strdup_printf(
					   "Content-type: application/json\r\n"
					   "Content-length: %d\r\n\r\n", returnstring->len);
	
		g_string_prepend(returnstring, headers);
	} else {
		headers = g_strdup_printf("id: %" G_GUINT64_FORMAT "\r\ndata: ", chan->timestamp);
		g_string_prepend(returnstring, headers);
		g_string_append_printf(returnstring, "\r\n\r\n");
	}
	
	purple_debug_misc("pidgin_juice", "The following events are about to be written to client:\n%s\n", returnstring->str);
	write(output_fd, returnstring->str, returnstring->len);
	
	g_free(headers);
	g_string_free(returnstring, TRUE);
	
	if (!chan->is_eventstream)
		disconnect_signals();
	else if (ConnectedSignals.disconnect_timer)
		purple_timeout_remove(ConnectedSignals.disconnect_timer);
	
	return FALSE;
}

static void
events_table_foreach_cb(gpointer key, gpointer value, gpointer user_data)
{
	purple_timeout_add(500, (GSourceFunc)write_to_client, key);
	//write_to_client(key);
}

static void
events_push_to_queue(gchar *output, guint64 timestamp)
{
	JuiceEvent *event;
	JuiceEvent *lastevent;
	GString *new_output = NULL;
	
	
	lastevent = g_queue_peek_tail(&queue);
	if (lastevent && (lastevent->timestamp - timestamp < 1000))
	{
		//these two events occured in the same second
		timestamp = lastevent->timestamp + 1;
	}
	
	new_output = g_string_new(NULL);
	g_string_printf(new_output, "{\"timestamp\":%" G_GUINT64_FORMAT ",%s", timestamp, (output+1));
	g_free(output);
	purple_debug_info("purple_juice", "New event output: %s\n", new_output->str);
	
	event = g_new0(JuiceEvent, 1);
	event->event = g_string_free(new_output, FALSE);
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
	guint64 timestamp;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = juice_utf8_json_encode(purple_normalize(account, buddyname));
	escaped_message = juice_utf8_json_encode(message);
	
	timestamp = (guint64)time(NULL) * 1000;
	
	output = g_strdup_printf("{ \"type\":\"received\","
							 "\"message\":\"%s\", "
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\""
							 //", \"timestamp\":%" G_GUINT64_FORMAT ""
							 " }",
							 escaped_message,
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account)
							 //, timestamp
							 );
	
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
	guint64 timestamp;
	
	if (!buddyname || !message)
		return;
	
	escaped_buddyname = juice_utf8_json_encode(purple_normalize(account, buddyname));
	escaped_message = juice_utf8_json_encode(message);
	
	timestamp = (guint64)time(NULL) * 1000;
	
	output = g_strdup_printf("{ \"type\":\"sent\","
							 "\"message\":\"%s\", "
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\""
							 //", \"timestamp\":%" G_GUINT64_FORMAT ""
							 " }",
							 escaped_message,
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account)
							 //, timestamp
							 );
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_message);
	g_free(escaped_buddyname);
}

static void
buddy_typing_cb(PurpleAccount *account, const char *buddyname, gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	guint64 timestamp;
	
	if (!buddyname)
		return;
	
	escaped_buddyname = juice_utf8_json_encode(purple_normalize(account, buddyname));
	
	timestamp = (guint64)time(NULL) * 1000;
	
	output = g_strdup_printf("{ \"type\":\"typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\""
							 //", \"timestamp\":%" G_GUINT64_FORMAT ""
							 " }", 
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account)
							 //, timestamp
							 );
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_buddyname);
}
static void 
buddy_typing_stopped_cb(PurpleAccount *account, const char *buddyname, gpointer user_data)
{
	gchar *output;
	gchar *escaped_buddyname;
	guint64 timestamp;
	
	if (!buddyname)
		return;
	
	escaped_buddyname = juice_utf8_json_encode(purple_normalize(account, buddyname));
	
	timestamp = (guint64)time(NULL) * 1000;
	
	output = g_strdup_printf("{ \"type\":\"not_typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\""
							 //", \"timestamp\":%" G_GUINT64_FORMAT ""
							 " }",
							 escaped_buddyname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account)
							 //, timestamp
							 );
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_buddyname);
}

static void 
received_chat_msg_cb(PurpleAccount *account, char *sender, char *message, PurpleConversation *conv, PurpleMessageFlags flags)
{
	gchar *output;
	gchar *escaped_buddyname;
	gchar *escaped_message;
	gchar *escaped_chatname;
	guint64 timestamp;
	
	if (!sender || !message)
		return;
	
	escaped_buddyname = juice_utf8_json_encode(purple_normalize(account, sender));
	escaped_message = juice_utf8_json_encode(message);
	escaped_chatname = juice_utf8_json_encode(purple_conversation_get_name(conv));
	
	timestamp = (guint64)time(NULL) * 1000;
	
	output = g_strdup_printf("{ \"type\":\"received_chat\","
							 "\"message\":\"%s\", "
							 "\"buddyname\":\"%s\", "
							 "\"chatname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\""
							 //", \"timestamp\":%" G_GUINT64_FORMAT ""
							 " }",
							 escaped_message,
							 escaped_buddyname,
							 escaped_chatname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account)
							 //, timestamp
							 );
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_message);
	g_free(escaped_buddyname);
	g_free(escaped_chatname);
}
static void
sent_chat_msg_cb(PurpleAccount *account, const char *message, int id)
{
	PurpleConversation *conv;
	
	gchar *output;
	gchar *escaped_chatname;
	gchar *escaped_message;
	guint64 timestamp;
	
	conv = purple_find_chat(purple_account_get_connection(account), id);
	
	if (!conv || !message)
		return;
	
	escaped_message = juice_utf8_json_encode(message);
	escaped_chatname = juice_utf8_json_encode(purple_conversation_get_name(conv));
	
	timestamp = (guint64)time(NULL) * 1000;
	
	output = g_strdup_printf("{ \"type\":\"sent_chat\","
							 "\"message\":\"%s\", "
							 "\"chatname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\""
							 //", \"timestamp\":%" G_GUINT64_FORMAT ""
							 " }",
							 escaped_message,
							 escaped_chatname,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account)
							 //, timestamp
							 );
	
	events_push_to_queue(output, timestamp);
	
	g_free(escaped_message);
	g_free(escaped_chatname);
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
	if (!ConnectedSignals.received_chat_signal)
	{
		ConnectedSignals.received_chat_signal =
			purple_signal_connect(conv_handle, "received-chat-msg",
								  &ConnectedSignals, PURPLE_CALLBACK(received_chat_msg_cb), 
								  NULL);
	}
	if (!ConnectedSignals.sent_im_signal)
	{
		ConnectedSignals.sent_im_signal =
			purple_signal_connect(conv_handle, "sent-im-msg",
								  &ConnectedSignals, PURPLE_CALLBACK(sent_im_msg_cb), 
								  NULL);
	}
	if (!ConnectedSignals.sent_chat_signal)
	{
		ConnectedSignals.sent_chat_signal =
			purple_signal_connect(conv_handle, "sent-chat-msg",
								  &ConnectedSignals, PURPLE_CALLBACK(sent_chat_msg_cb), 
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

/*static gboolean 
channel_hung_up(GIOChannel *channel, GIOCondition cond, gpointer data)
{
	JuiceChannel *chan;
	
	//get the channel to use
	chan = g_hash_table_lookup(channels, channel);
	//remove it from the table, as we're about to get rid of it (but don't free! we're still using)
	g_hash_table_steal(channels, channel);
	if (chan && chan->timeout)
		purple_timeout_remove(chan->timeout);
	g_io_channel_unref(channel);
	
	//jeremy added this to get rid of a compiler warning. is this the right thing to return?
	return FALSE;
}*/

static void
juice_handle_events(JuiceRequestObject *request, gint output_fd)
{
	guint timeout;
	JuiceChannel *chan;
	guint64 timestamp;
	GHashTable *$_GET = juice_parse_query(request->query);
	
	write(output_fd, "HTTP/1.0 200 OK\r\nConnection: keep-alive\r\n", 41);
	
	purple_debug_info("pidgin_juice", "Client connected for events.\n");
	
	connect_to_signals();
	
	if (channels == NULL)
		channels = g_hash_table_new_full(NULL, NULL, NULL, NULL);
	
	//Otherwise, store up the channel
	timeout = purple_timeout_add_seconds(60, (GSourceFunc)write_to_client, GINT_TO_POINTER(output_fd));
	timestamp = g_ascii_strtoull((gchar *)g_hash_table_lookup($_GET, "timestamp"), NULL, 10);
	
	purple_debug_info("pidgin_juice", "get events since: %" G_GUINT64_FORMAT "\n", timestamp);
	
	chan = g_new0(JuiceChannel, 1);
	chan->fd = output_fd;
	chan->timeout = timeout;
	chan->timestamp = timestamp;
	chan->is_eventstream = (g_hash_table_lookup($_GET, "eventstream") != NULL);
	
	g_hash_table_destroy($_GET);
	
	if (chan->is_eventstream)
	{
		write(output_fd, "Content-type: text/event-stream\r\n\r\n", 35);
	}
	
	g_hash_table_insert(channels, GINT_TO_POINTER(output_fd), chan);
	
	if (is_event_since(timestamp))
	{
		write_to_client(output_fd);
		return;
	}
}
