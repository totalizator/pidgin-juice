#include <conversation.h>

static gchar *
juice_GET_conversations(const GHashTable *$_GET)
{
	GList *conversations;
	GString *convs;
	gboolean first;

	PurpleAccount *account;
	PurplePluginProtocolInfo *prpl_info;

	const gchar *type;
	const gchar *name;
	const gchar *prpl_icon;
	const gchar *proto_id;
	const gchar *proto_name;
	const gchar *account_username;
	const gchar *title;
	gint unseen_count;
	
	convs = g_string_new("{\"conversations\":[");
	first = TRUE;
	
	for(conversations = purple_get_conversations();
		conversations;
		conversations = conversations->next)
	{
		PurpleConversation *conv = conversations->data;

		if (first)
			first = FALSE;
		else
			g_string_append_c(convs, ',');
		
		switch(purple_conversation_get_type(conv))
		{
			case PURPLE_CONV_TYPE_IM:
				type = "im";
				break;
			case PURPLE_CONV_TYPE_CHAT:
				type = "chat";
				break;
			default:
				continue;
		}
		
		name = purple_conversation_get_name(conv);
		title = purple_conversation_get_title(conv);

		account = purple_conversation_get_account(conv);
		proto_id = purple_account_get_protocol_id(account);
		proto_name = purple_account_get_protocol_name(account);
		account_username = purple_account_get_username(account);
					
		prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(proto_id));
		if (prpl_info->list_icon)
			prpl_icon = prpl_info->list_icon(account, NULL);
		else
			prpl_icon = "";
		
		unseen_count = GPOINTER_TO_INT(purple_conversation_get_data(conv, "unseen-count"));
		
		g_string_append_printf(convs, "{ \"type\":\"%s\", "
										"\"name\":\"%s\", "
									  	"\"title\":\"%s\", "
									  	"\"prpl_icon\":\"%s\", "
									  	"\"proto_id\":\"%s\", "
									  	"\"proto_name\":\"%s\", "
									  	"\"account_username\":\"%s\", "
									  	"\"unseen-count\":%d }",
									  	type, name, title,
									  	prpl_icon, proto_id,
									  	proto_name, account_username,
									  	unseen_count);
	}
	
	g_string_append(convs, "]}");
	
	return convs->str;
}
