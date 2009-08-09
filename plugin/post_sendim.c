/*
 *  post_sendim.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 5/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

static gchar *
juice_POST_sendim(const gchar *buddyname, const gchar *proto_id, const gchar *proto_username, const gchar *message, gsize *length)
{
	PurpleAccount *account;
	gchar *return_string;
	PurpleConversation *conv;
	
	if (!buddyname || !proto_id || !proto_username)
	{
		if (length != NULL)
			*length = 0;
		return NULL;
	}
	
	account = purple_accounts_find(proto_username, proto_id);
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
													buddyname, account);
	if (conv == NULL)
	{
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, buddyname);
	}
	purple_conv_im_send(PURPLE_CONV_IM(conv), message);
	
	return_string = g_strdup("{ \"message_sent\" : 1 }");

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
