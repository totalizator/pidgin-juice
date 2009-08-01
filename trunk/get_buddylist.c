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
	GSList *buddies;
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
	
	return_blist = json_object_new();
	json_buddies = json_array_new();
	json_buddies_node = json_node_new();
	json_node_take_array(json_buddies_node, json_buddies);
	
	json_object_add_member(return_blist, "buddies", json_buddies_node);
	
 	buddies_list = purple_find_buddies(NULL, NULL);
	for(;buddies_list; buddies_list=budddies_list->next)
	{
		buddy = buddies_list->data;
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
	}
	
	g_slist_free(buddies_list);
	return output;
}
