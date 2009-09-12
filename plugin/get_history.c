/*
 *  get_history.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 8/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */


static gchar *
juice_GET_history(GHashTable *$_GET, gsize *length)
{
	PurpleAccount *account = NULL;
	PurpleConversation *conv = NULL;
	GList *history;
	PurpleConvMessage *msg;
	PurpleMessageFlags flags;
	const gchar *buddyname = NULL;
	const gchar *proto_id = NULL;
	const gchar *proto_username = NULL;
	gchar *escaped;
	GString *history_output;
	guint64 timestamp;
	gboolean first;
	
	buddyname = (gchar *)g_hash_table_lookup($_GET, "buddyname");
	proto_id = (gchar *)g_hash_table_lookup($_GET, "proto_id");
	proto_username = (gchar *)g_hash_table_lookup($_GET, "proto_username");
	
	if (proto_username && proto_id)
	{
		account = purple_accounts_find(proto_username, proto_id);
	}
	if (account && buddyname)
	{
		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
													 buddyname, account);
	}
	if (conv == NULL)
	{
		if (length != NULL)
			*length = 0;
		return g_strdup("");
	}
	
	history = purple_conversation_get_message_history(conv);
	history_output = g_string_new("{\"history\":[");
	
	first = TRUE;	
	for(; history; history = g_list_next(history))
	{
		msg = history->data;
		
		flags = purple_conversation_message_get_flags(msg);
		if (flags & PURPLE_MESSAGE_SEND ||
			flags & PURPLE_MESSAGE_RECV)
		{
			//continue;
		}
		else
		{
			continue;
		}
		
		if (first)
			first = FALSE;
		else
			g_string_append_c(history_output, ',');
		
		escaped = g_strescape(purple_conversation_message_get_sender(msg), "");
		g_string_append_printf(history_output, "{\"sender\":\"%s\", ", escaped);
		g_free(escaped);
		
		escaped = g_strescape(purple_conversation_message_get_message(msg), "");
		g_string_append_printf(history_output, "\"message\":\"%s\", ", escaped);
		g_free(escaped);
		
		g_string_append_printf(history_output, "\"type\":\"%s\", ", 
								(flags & PURPLE_MESSAGE_RECV?"received":"sent"));
		
		timestamp = purple_conversation_message_get_timestamp(msg) * 1000; //we want in milliseconds
		
		g_string_append_printf(history_output, "\"timestamp\":%" G_GUINT64_FORMAT "}", 
								timestamp);
	}
	
	escaped = g_strescape(buddyname, "");
	g_string_append_printf(history_output, "],\"buddyname\":\"%s\", ", escaped);
	g_free(escaped);
	
	escaped = g_strescape(proto_id, "");
	g_string_append_printf(history_output, "\"account_username\":\"%s\", ", escaped);
	g_free(escaped);
	
	escaped = g_strescape(proto_username, "");
	g_string_append_printf(history_output, "\"proto_id\":\"%s\"}", escaped);
	g_free(escaped);
	
	if (length != NULL)
		*length = history_output->len;

	return g_string_free(history, FALSE);
	
}



