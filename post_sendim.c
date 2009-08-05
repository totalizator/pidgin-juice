/*
 *  post_sendim.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 5/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

//unused :)
PurpleBuddy *
juice_find_buddy(gchar *buddyname, gchar *proto_id, gchar *proto_username)
{
	PurpleAccount *account;
	
	g_return_if_fail(buddyname != NULL, NULL);
	g_return_if_fail(proto_id != NULL, NULL);
	g_return_if_fail(proto_username != NULL, NULL);
	
	account = purple_accounts_find(proto_username, proto_id);
	if (account == NULL)
		return NULL;
	
	return purple_find_buddy(account, buddyname);
}

static gchar *
juice_POST_sendim(gchar *buddyname, gchar *proto_id, gchar *proto_username, gchar *message, gsize *length)
{
	PurpleAccount *account;
	gint result;
	gchar *return_string;
	const gchar *reason;
	
	account = purple_accounts_find(proto_username, proto_id);
	result = serv_send_im(purple_account_get_connection(account), buddyname, message, PURPLE_MESSAGE_SEND);
	
	if (result < 0)
	{
		if (result == -E2BIG)
			reason = "too_big";
		else if (result == -ENOTCONN)
			reason = "not_connected";
		else
			reason = "";
		//Failed
		return_string = g_strdup_printf("{ \"message_sent\" : 0, \"reason\" : \"%s\" }", reason);
	} else {
		return_string = g_strdup("{ \"message_sent\" : 1 }");
	}
	
	if (length != NULL)
		*length = strlen(return_string);
	
	return return_string;
}

static gchar *
juice_POST_typing(gchar *buddyname, gchar *proto_id, gchar *proto_username, gsize *length)
{
	PurpleAccount *account;
	
	account = purple_accounts_find(proto_username, proto_id);
	serv_send_typing(purple_account_get_connection(account), buddyname, PURPLE_TYPING);
	
	if (length != NULL)
		*length = 0;
	
	return g_strdup("");
}

static gchar *
juice_POST_nottyping(gchar *buddyname, gchar *proto_id, gchar *proto_username, gsize *length)
{
	PurpleAccount *account;
	
	account = purple_accounts_find(proto_username, proto_id);
	serv_send_typing(purple_account_get_connection(account), buddyname, PURPLE_NOT_TYPING);
	
	if (length != NULL)
		*length = 0;
	
	return g_strdup("");
}