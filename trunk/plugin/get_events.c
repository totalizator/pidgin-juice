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

static GHashTable *message_queue = NULL;
static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
static GCond *condition = NULL;

#ifdef _WIN32
//these two #defines override g_static_mutex_lock and
// g_static_mutex_unlock so as to remove "strict-aliasing"
// compiler warnings
#define g_static_mutex_get_mutex2(mutex) \
g_static_mutex_get_mutex ((GMutex **)(void*)mutex)
#define g_static_mutex_lock2(mutex) \
g_mutex_lock (g_static_mutex_get_mutex2 (mutex))
#define g_static_mutex_unlock2(mutex) \
g_mutex_unlock (g_static_mutex_get_mutex2 (mutex))
#else
#define g_static_mutex_get_mutex2 g_static_mutex_get_mutex
#define g_static_mutex_lock2 g_static_mutex_lock
#define g_static_mutex_unlock2 g_static_mutex_unlock
#endif

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

static gchar *
juice_GET_events(gsize *length)
{
	gboolean condition_result;
	GTimeVal endtime = {0,0};
	gchar *returnstring;
	
	//this funciton should block until there's an event, or until a minute is up
	connect_to_signals();
	
	//block here
	//wait for message for a maximum of 60 seconds
	g_get_current_time(&endtime);
	g_time_val_add(&endtime, 60 * G_USEC_PER_SEC);
	condition_result = g_cond_timed_wait(condition, g_static_mutex_get_mutex2(&mutex), &endtime);
	
	if(!condition_result)
	{
		//we timed out while waiting
		g_static_mutex_unlock2(&mutex);
		returnstring = g_strdup("{ \"events\" : [ { \"type\":\"continue\" } ] }");
		if (length != NULL)
			*length = strlen(returnstring);
	} else {
		//do something :(
		returnstring = g_strdup("dummy");
		if (length != NULL)
			*length = strlen(returnstring);		
	}
	g_static_mutex_unlock2(&mutex);
	
	disconnect_signals();
	
	return returnstring;
}