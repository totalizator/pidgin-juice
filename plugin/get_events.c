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
};
static struct _ConnectedSignals ConnectedSignals = {0,0,0,0};

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
	
	g_free(headers);
	g_string_free(returnstring, TRUE);
	
	disconnect_signals();
	
	current_seq++;
	
	return FALSE;
}
static void
events_push_to_queue(gchar *output)
{
	//Add the event to the queue
	g_queue_push_tail(&queue, output);
	
	//Loop through the channels to return the events
	if (channels && g_hash_table_size(channels))
	{
		//g_hash_table_foreach(channels, events_table_foreach_cb, NULL);		
		g_hash_table_foreach(channels, (GHFunc)write_to_client, NULL);
	}
}
static void
received_im_msg_cb(PurpleAccount *account, char *sender, char *message,
				   PurpleConversation *conv, PurpleMessageFlags flags,
				   gpointer user_data)
{
	gchar *output;
	JsonObject *json_message;
	JsonNode *json_message_node;
	JsonNode *sender_node, *message_node, *timestamp_node, *type_node;
	JsonNode *proto_id_node, *proto_username_node;
	gchar *escaped = NULL;
	JsonGenerator *generator = NULL;
	
	json_message = json_object_new();
	
	sender_node = json_node_new(JSON_NODE_VALUE);
	escaped = g_strescape(sender, "");
	json_node_set_string(sender_node, escaped);
	g_free(escaped);
	json_object_add_member(json_message, "buddyname", sender_node);
	
	message_node = json_node_new(JSON_NODE_VALUE);
	escaped = g_strescape(message, "");
	json_node_set_string(message_node, escaped);
	g_free(escaped);
	json_object_add_member(json_message, "message", message_node);
	
	type_node = json_node_new(JSON_NODE_VALUE);
	json_node_set_string(type_node,
						 (flags & PURPLE_MESSAGE_RECV?"received":"sent"));
	json_object_add_member(json_message, "type", type_node);
	
	timestamp_node = json_node_new(JSON_NODE_VALUE);
	json_node_set_int(timestamp_node, 
					  time(NULL));
	json_object_add_member(json_message, "timestamp", timestamp_node);
	
	proto_id_node = json_node_new(JSON_NODE_VALUE);
	escaped = g_strescape(purple_account_get_protocol_id(account), "");
	json_node_set_string(proto_id_node, escaped);
	g_free(escaped);
	json_object_add_member(json_message, "proto_id", proto_id_node);
	
	proto_username_node = json_node_new(JSON_NODE_VALUE);
	escaped = g_strescape(purple_account_get_username(account), "");
	json_node_set_string(proto_username_node, escaped);
	g_free(escaped);
	json_object_add_member(json_message, "account_username", proto_username_node);
	
	json_message_node = json_node_new(JSON_NODE_OBJECT);
	json_node_take_object(json_message_node, json_message);
	
	generator = json_generator_new();
	json_generator_set_root(generator, json_message_node);
	
	output = json_generator_to_data(generator, NULL);	
	json_node_free(json_message_node);	
	
	events_push_to_queue(output);
}

static void
buddy_typing_cb(PurpleAccount *account, const char *name, gpointer user_data)
{
	gchar *output;
	
	output = g_strdup_printf("{ \"type\":\"typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\" }", name,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account));
	
	events_push_to_queue(output);
}
static void 
buddy_typing_stopped_cb(PurpleAccount *account, const char *name, gpointer user_data)
{
	gchar *output;
	
	output = g_strdup_printf("{ \"type\":\"not_typing\","
							 "\"buddyname\":\"%s\", "
							 "\"proto_id\":\"%s\", "
							 "\"account_username\":\"%s\" }", name,
							 purple_account_get_protocol_id(account),
							 purple_account_get_username(account));
	
	events_push_to_queue(output);
}

//static void
//events_table_foreach_cb(gpointer key, gpointer value, gpointer user_data)
//{
//	write_to_client(key);
//}



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
