/*
 *  get_buddyicon.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 2/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <json-glib/json-glib.h>


static gchar *
juice_GET_buddyicon(gchar *buddyname, gchar *proto_id, gchar *proto_username)
{
	PurpleBuddyIcon *icon;
	PurpleAccount *account;
	gchar *buddy_icon_data;
	size_t buddy_icon_len;
	
	account = purple_accounts_find(proto_username, proto_id);
	if (account == NULL)
		return g_strdup("");
	purple_debug_info("purple_juice", "account not null\n");
	
	icon = purple_buddy_icons_find(account, buddyname);
	if (icon == NULL)
		return g_strdup("");
	purple_debug_info("purple_juice", "icon not null\n");
	
	buddy_icon_data = purple_buddy_icon_get_data(icon, &buddy_icon_len);
	purple_debug_info("purple_juice", "buddy_icon_data address: %d \n", buddy_icon_data);
	buddy_icon_data = g_strndup(buddy_icon_data, buddy_icon_len);
	purple_debug_info("purple_juice", "buddy_icon_data address2: %d \n", buddy_icon_data);
	
	purple_buddy_icon_unref(icon);
	purple_debug_info("purple_juice", "buddy_icon_data address3: %d \n", buddy_icon_data);
	
	return buddy_icon_data;
}
