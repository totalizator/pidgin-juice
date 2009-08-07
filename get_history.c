/*
 *  get_history.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 8/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */


static gchar *
juice_GET_history(gchar *buddyname, gchar *proto_id, gchar *proto_username, gsize *length)
{
	PurpleAccount *account = NULL;
	PurpleConversation *conv;
	GList *history, *cur;
	PurpleConvMessage *msg;
	JsonObject *return_history;
	JsonArray *json_messages;
	JsonObject *json_message;
	JsonNode *json_message_node;
	JsonNode *sender_node, *message_node, *timestamp_node, *type_node;
	PurpleMessageFlags flags;
	JsonNode *history_node, *return_history_node;
	JsonNode *buddyname_node, *proto_id_node, *proto_username_node;
	gchar *output;
	
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
	
	
	return_history = json_object_new();
	json_messages = json_array_new();
	
	for(cur = history; cur; cur = g_list_next(cur))
	{
		msg = cur->data;
		
		flags = purple_conversation_message_get_flags(msg);
		if (flags != PURPLE_MESSAGE_SEND ||
			flags != PURPLE_MESSAGE_RECV)
		{
			continue;
		}
		
		json_message = json_object_new();
		
		sender_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(sender_node, 
							purple_conversation_message_get_sender(msg));
		json_object_add_member(json_message, "sender", sender_node);
		
		message_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(message_node, 
							 purple_conversation_message_get_message(msg));
		json_object_add_member(json_message, "message", message_node);
		
		type_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(sender_node,
							 (flags==PURPLE_MESSAGE_SEND?"sent":"received"));
		json_object_add_member(json_message, "type", type_node);
		
		timestamp_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_int(timestamp_node, 
							 purple_conversation_message_get_timestamp(msg));
		json_object_add_member(json_message, "timestamp", timestamp_node);
		
		json_message_node = json_node_new(JSON_NODE_OBJECT);
		json_node_set_object(json_message_node, json_message);
		json_array_add_element(json_messages, json_message_node);
	}
	
	json_history_node = json_node_new(JSON_NODE_ARRAY);
	json_node_take_array(json_history_node, json_messages);
	json_object_add_member(return_history, "history", json_history_node);
	
	//gchar *buddyname, gchar *proto_id, gchar *proto_username
	buddyname_node = json_node_new(JSON_NODE_VALUE);
	json_node_set_string(buddyname_node, buddyname);
	json_object_add_member(return_history, "buddyname", buddyname_node);
	
	proto_id_node = json_node_new(JSON_NODE_VALUE);
	json_node_set_string(proto_id_node, proto_id);
	json_object_add_member(return_history, "proto_id", proto_id_node);
	
	proto_username_node = json_node_new(JSON_NODE_VALUE);
	json_node_set_string(proto_username_node, proto_username);
	json_object_add_member(return_history, "proto_username", proto_username_node);
	
	return_history_node = json_node_new(JSON_NODE_OBJECT);
	json_node_take_object(return_history_node, return_history);
	
	generator = json_generator_new();
	json_generator_set_root(generator, return_history_node);
	
	output = json_generator_to_data(generator, NULL);
	if (length != NULL)
		*length = strlen(output);
	return output;
	
}



