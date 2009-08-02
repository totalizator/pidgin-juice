/*
 *  get_buddylist.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 1/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <blist.h>
#include <json-glib/json-glib.h>

gchar *
juice_GET_buddylist(/* something in here? */)
{
	PurpleBlistNode *node;
	gchar *output;
	PurpleBlistNodeType type;
	GSList *buddies, *current;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	const gchar *status_message;
	const gchar *display_name;
	const gchar *username;
	PurpleBuddyIcon *icon;
	PurpleStatus *status;
	gboolean available;
	const gchar *proto_id;
	const gchar *proto_name;
	const ghcar *account_username;
	JsonObject *return_blist;
	JsonObject *json_buddy;
	JsonArray *json_buddies;
	JsonNode *display_name_node;
	JsonNode *username_node;
	JsonNode *available_node;
	JsonNode *status_message_node;
	JsonNode *proto_id_node;
	JsonNode *proto_name_node;
	JsonNode *account_username_node;
	JsonNode *json_buddy_node;
	JsonGenerator *generator;
	JsonNode *return_blist_node;
	
	return_blist = json_object_new();
	json_buddies = json_array_new();
	
 	buddies_list = purple_find_buddies(NULL, NULL);
	for(current = buddies_list;current; current=current->next)
	{
		buddy = current->data;
		json_buddy = json_object_new();
		
		/*
		 We want:
		 *	display name
		 *	username
		 *	status
		 *	status message
		 *	buddy icon url
		 *	account identifier of some kind
		 */
		display_name = purple_buddy_get_alias(buddy);
		username = purple_buddy_get_name(buddy);
		icon = purple_buddy_get_icon(buddy);
		
		status = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
		status_message = purple_status_get_attr_string (status, "message");
		available = purple_status_is_available(status);
		
		account = purple_buddy_get_account(buddy);
		proto_id = purple_account_get_protocol_id(account);
		proto_name = purple_account_get_protocol_name(account);
		account_username = purple_account_get_username(account);
		
		// Set the json nodes
		display_name_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(display_name_node, display_name);
		json_object_add_member(json_buddy, "display_name", display_name_node);
		
		username_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(username_node, username);
		json_object_add_member(json_buddy, "username", username_node);
		
		available_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_boolean(available_node, available);
		json_object_add_member(json_buddy, "available", available_node);
		
		status_message_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(status_message_node, status_message);
		json_object_add_member(json_buddy, "status_message", status_message_node);
		
		proto_id_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(proto_id_node, proto_id);
		json_object_add_member(json_buddy, "proto_id", proto_id_node);
		
		proto_name_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(proto_name_node, proto_name);
		json_object_add_member(json_buddy, "proto_name", proto_name_node);
		
		account_username_node = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(account_username_node, account_username);
		json_object_add_member(json_buddy, "account_username", account_username_node);
		
		json_buddy_node = json_node_new(JSON_NODE_OBJECT);
		json_node_set_object(json_buddy_node, json_buddy);
		json_array_add_node(json_buddies, json_buddy_node);
	}
	g_slist_free(buddies_list);
	
	json_buddies_node = json_node_new(JSON_NODE_ARRAY);
	json_node_take_array(json_buddies_node, json_buddies);
	json_object_add_member(return_blist, "buddies", json_buddies_node);
	
	return_blist_node = json_node_new(JSON_NODE_OBJECT);
	json_node_add_object(return_blist_node, return_blist);
	
	generator = json_generator_new();
	json_generator_set_root(generator, return_blist_node);
	
	output = json_generator_to_data(generator, NULL);
	
	return output;
}
