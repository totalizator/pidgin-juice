/*
 *  get_buddylist.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 1/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

#include <blist.h>

static gchar *
juice_utf8_json_encode(const gchar *str)
{
	GString *out;
	gunichar wc;
	
	out = g_string_new(NULL);
	
	for (; *str; str = g_utf8_next_char(str))
	{
		wc = g_utf8_get_char(str);
		
		if (wc == '"' || wc == '/' || wc == '\\')
		{
			g_string_append_c(out, '\\');
			g_string_append_unichar(out, wc);
		}
		else if (wc == '\t')
		{
			g_string_append(out, "\\t");
		}
		else if (wc == '\r')
		{
			g_string_append(out, "\\r");
		}
		else if (wc == '\n')
		{
			g_string_append(out, "\\n");
		}
		else if (wc == '\f')
		{
			g_string_append(out, "\\f");
		}
		else if (wc == '\b')
		{
			g_string_append(out, "\\b");
		}
		else if (wc >= 0x80 || wc < 0x20)
		{
			g_string_append_printf(out, "\\u%04X", (guint16)wc);
		}
		else
		{
			g_string_append_unichar(out, wc);
		}
	}
	return g_string_free(out, FALSE);
}

static gchar *
juice_GET_buddylist(const GHashTable *$_GET)
{
	PurpleBuddy *buddy;
	PurpleAccount *account;
	const char *status_message_unescaped;
	gchar *status_message;
	const gchar *display_name, *display_name_tmp;
	const gchar *buddyname;
	PurpleBuddyIcon *icon;
	PurpleStatus *status;
	gboolean available;
	const gchar *proto_id;
	const gchar *proto_name;
	const gchar *account_username;
	PurpleBlistNode *purple_blist_node;
	PurplePluginProtocolInfo *prpl_info;
	const gchar *prpl_icon;
	gboolean first;
	GString *blist;
	
	blist = g_string_new("{\"buddies\":[");
	first = TRUE;
	
	for(purple_blist_node = purple_blist_get_root();
		purple_blist_node;
		purple_blist_node = purple_blist_node_next(purple_blist_node, FALSE))
	{
		if (PURPLE_BLIST_NODE_IS_CONTACT(purple_blist_node))
		{
			buddy = purple_contact_get_priority_buddy((PurpleContact *) purple_blist_node);
		} else {
			continue;
		}
		if (!PURPLE_BUDDY_IS_ONLINE(buddy))
			continue;
		
		if (first)
			first = FALSE;
		else
			g_string_append_c(blist, ',');
		
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
		buddyname = purple_buddy_get_name(buddy);
		icon = purple_buddy_get_icon(buddy);

		//display_name = g_strdup(display_name);
		//purple_util_chrreplace(display_name, "\\", "\\\\");
		//display_name_tmp = purple_utf8_ncr_encode(display_name);
		//g_free(display_name);
		//display_name = g_strescape(display_name_tmp, "\\");
		//g_free(display_name_tmp);
		display_name = juice_utf8_json_encode(display_name);
		
		status = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
		status_message_unescaped = purple_status_get_attr_string (status, "message");
		if (status_message_unescaped == NULL)
			status_message = g_strdup("");
		else
			status_message = juice_utf8_json_encode(status_message_unescaped);
		available = purple_status_is_available(status);
		
		account = purple_buddy_get_account(buddy);
		proto_id = purple_account_get_protocol_id(account);
		proto_name = purple_account_get_protocol_name(account);
		account_username = purple_account_get_username(account);
			
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(proto_id));
		if (prpl_info->list_icon)
			prpl_icon = prpl_info->list_icon(account, NULL);
		else
			prpl_icon = g_strdup("");
		
		g_string_append_printf(blist, "{ \"display_name\":\"%s\", "
									  	"\"buddyname\":\"%s\", "
									  	"\"prpl_icon\":\"%s\", "
									  	"\"available\":%s, "
									  	"\"status_message\":\"%s\", "
									  	"\"proto_id\":\"%s\", "
									  	"\"proto_name\":\"%s\", "
									  	"\"account_username\":\"%s\" }",
									  	display_name, buddyname,
									  	prpl_icon, (available?"true":"false"),
									  	status_message, proto_id,
									  	proto_name, account_username);
		g_free(status_message);
		g_free(display_name);
	}
	
	g_string_append(blist, "]}");
	
	return blist->str;
}
