/*
 *  get_events.c
 *  pidgin_juice
 *
 *  Created by MyMacSpace on 8/08/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */

static void
received_im_msg_cb(PurpleAccount *account, char *sender, char *message,
					PurpleConversation *conv, PurpleMessageFlags flags,
					gpointer user_data)

{


}

static void
buddy_typing_cb(PurpleAccount *account, const char *name, gpointer user_data)
{
	
}
static void 
buddy_typing_stopped_cb(PurpleAccount *account, const char *name, gpointer user_data)
{
	
}

struct _ConnectedSignals {
	guint disconnect_timer;
	
	gulong received_im_signal;
	gulong buddy_typing_signal;
	gulong buddy_typing_stoppped_signal;
};

static struct _ConnectedSignals ConnectedSignals = NULL;

static void
connect_to_signals()
{
    void *conv_handle = purple_conversations_get_handle();
	
	if (ConnectedSignals == NULL)
		ConnectedSignals = g_new0(struct _ConnectedSignals, 1);
	else
		return;
	
	purple_signal_connect(conv_handle, "received-im-msg",
						  ConnectedSignals, PURPLE_CALLBACK(received_im_msg_cb), NULL);
    purple_signal_connect(conv_handle, "buddy-typing",
						  ConnectedSignals, PURPLE_CALLBACK(buddy_typing_cb), NULL);
	purple_signal_connect(conv_handle, "buddy-typing-stopped",
						  ConnectedSignals, PURPLE_CALLBACK(buddy_typing_stopped_cb), NULL);

	//add a timeout here to disconnect
	if (ConnectedSignals->disconnect_timer)
		purple_timeout_remove(ConnectedSignals->disconnect_timer);
	
	ConnectedSignals->disconnect_timer = purple_timeout_add_seconds(180, 
									disconnect_signals_cb, ConnectedSignals);
}

static void
disconnect_signals()
{
	if (ConnectedSignals->disconnect_timer)
		purple_timeout_remove(ConnectedSignals->disconnect_timer);
	
	ConnectedSignals->disconnect_timer = purple_timeout_add_seconds(60, 
																	disconnect_signals_cb, ConnectedSignals);
}

static gboolean
disconnect_signals_cb(gpointer data)
{
	purple_signal_disconnect_by_handle(ConnectedSignals);
	g_free(ConnectedSignals);
	ConnectedSignals = NULL;
	
	return FALSE; //return false so that the event doesn't get fired twice
}

static void
juice_GET_events(gsize *length)
{
	//this funciton should block until there's an event, or until a minute is up
	connect_to_signals();
	
	//block here
	
	disconnect_signals();
}