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
//juice_GET_buddyicon(gchar *buddyname, gchar *proto_id, gchar *proto_username, gsize *length)
juice_GET_buddyicon(GHashTable *$_GET, gsize *length)
{
	PurpleBuddyIcon *icon;
	PurpleAccount *account;
	gconstpointer buddy_icon_data;
	gchar * buddy_icon_data_copy;
	size_t buddy_icon_len;
	const gchar *buddyname = NULL;
	const gchar *proto_id = NULL;
	const gchar *proto_username = NULL;
	
	buddyname = g_hash_table_lookup($_GET, "buddyname");
	proto_id = g_hash_table_lookup($_GET, "proto_id");
	proto_username = g_hash_table_lookup($_GET, "proto_username");
	
	account = purple_accounts_find(proto_username, proto_id);
	if (account == NULL)
		return g_strdup("");
	purple_debug_info("purple_juice", "account not null\n");
	
	icon = purple_buddy_icons_find(account, buddyname);
	if (icon == NULL)
		return g_strdup("");
	purple_debug_info("purple_juice", "icon not null\n");
	
	buddy_icon_data = purple_buddy_icon_get_data(icon, &buddy_icon_len);
	//purple_debug_info("purple_juice", "buddy_icon_data address: %d \n", buddy_icon_data);
	
	
	buddy_icon_data_copy = (gchar *)g_memdup(buddy_icon_data, buddy_icon_len);
	
	
	//purple_debug_info("purple_juice", "buddy_icon_data address2: %d \n", buddy_icon_data);
	
	purple_buddy_icon_unref(icon);
	//purple_debug_info("purple_juice", "buddy_icon_data address3: %d \n", buddy_icon_data);
	
	*length = buddy_icon_len;
	return buddy_icon_data_copy;
}
static gchar *
juice_GET_proto_icon(GHashTable *$_GET, gsize *length) {
	const gchar *proto_id = NULL;
	const PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;
	const gchar *protoname;
	GError *error = NULL;
	gsize file_length = 0;
	gchar *file_contents = NULL;
	gchar *tmp = NULL;
	gchar *filename = NULL;
	
	proto_id = g_hash_table_lookup($_GET, "proto_id");
	prpl = purple_find_prpl(proto_id);
	
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info->list_icon == NULL)
		return NULL;

	protoname = prpl_info->list_icon(NULL, NULL);
	if (protoname == NULL)
		return NULL;
		
	tmp = g_strconcat(protoname, ".png", NULL);

	filename = g_build_filename(DATADIR, "pixmaps", "pidgin", "protocols",
				    "16",
				    tmp, NULL);
	g_free(tmp);
		
	g_file_get_contents(filename, &file_contents, &file_length, &error);
	
	purple_debug_info("pidgin_juice", "protocol image length: %d\n", file_length);
	*length = 4;
	return g_strdup("test");
}
