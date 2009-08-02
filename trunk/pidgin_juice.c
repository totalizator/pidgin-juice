/*
 * Release Notification Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

#include "internal.h"

#include <string.h>

#include "connection.h"
#include "core.h"
#include "debug.h"
#include "gtkblist.h"
#include "gtkutils.h"
#include "notify.h"
#include "pidginstock.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

#include "pidgin.h"

#include "winsock2.h"
#include "get_buddylist.c"

static gboolean
get_resource(GString *path, GString *query, GString *resource)
{
	FILE *fp = NULL;
	GString *filename = NULL;
	gchar *json_string = NULL;
	
	if (strcmp(path->str, "/buddies_list.js") ==0) {
		json_string = juice_GET_buddylist();
		g_string_append(resource, json_string);
		g_free(json_string);
		return TRUE;
	}
	if (strncmp(path->str, "/", 1) == 0) {
		filename = g_string_new(NULL);
		g_string_append_printf(filename, "%s%s%s%s%s", DATADIR, G_DIR_SEPARATOR_S, "juice", G_DIR_SEPARATOR_S, path->str+1);
		purple_debug_info("pidgin_juice", "filename: %s\n", filename->str);
		fp = fopen(filename->str, "r");
		if (fp == NULL) {
			g_string_free(filename, TRUE);
			return FALSE;
		}
		
		g_string_append(resource, "file exists\n");
		fclose(fp);
		g_string_free(filename, TRUE);
		return TRUE;
	}
	return FALSE;
}

static gboolean
process_request(GString *request_string, GString *reply_string)
{
	GString *uri = NULL, *content = NULL;
	GString *path = NULL, *query = NULL;
	GString *resource = NULL;
	int position = 0;
	char *temp_substr;
	
	purple_debug_info("pidgin_juice", "Parsing request\n");
	if (request_string->len <= 4 || strncmp(request_string->str, "GET ", 4) != 0)
	{
		g_string_append(reply_string, "HTTP/1.1 400 Bad Request\n");
		g_string_append(reply_string, "Content-length: 0\n");
		g_string_append(reply_string, "\n");
		purple_debug_info("pidgin_juice", "Bad request. Ignoring.\n");
		return FALSE;
	}
	
	temp_substr = strstr(request_string->str, " HTTP/1.1");
	if (temp_substr == NULL) {
		g_string_append(reply_string, "HTTP/1.1 400 Bad Request\n");
		g_string_append(reply_string, "Content-length: 0\n");
		g_string_append(reply_string, "\n");
		purple_debug_info("pidgin_juice", "Bad request. Ignoring.\n");
		return FALSE;
	}
	
	position = strlen(request_string->str) - strlen(temp_substr);
	if (position < 0) {
		g_string_append(reply_string, "HTTP/1.1 400 Bad Request\n");
		g_string_append(reply_string, "Content-length: 0\n");
		g_string_append(reply_string, "\n");
		purple_debug_info("pidgin_juice", "Bad request. Ignoring.\n");
		return FALSE;
	}
	
	uri = g_string_new(NULL);
	g_string_append_len(uri, request_string->str+4, position-4);
	purple_debug_info("pidgin_juice", "Request for %s\n", uri->str);
	
	temp_substr = strstr(uri->str, "?");
	if (temp_substr == NULL)
	{
		path = g_string_new(uri->str);
		query = g_string_new("");
	}
	else
	{
		path = g_string_new_len(uri->str, strlen(uri->str) - strlen(temp_substr));
		query = g_string_new(temp_substr);
	}
	
	if (g_str_equal(path->str, "/"))
	{
		g_string_append(path, "index.html");
	}
	
	
	content = g_string_new(NULL);
	
	
	/* begin creating response */
	resource = g_string_new(NULL);
	if (get_resource(path, query, resource))
	{
		purple_debug_info("pidgin_juice", "Found the resource.\n");
		g_string_append_printf(content, "Thank you for requesting the url: %s. \nThe response is: %s\n", uri->str, resource->str);
	}
	else
	{
		purple_debug_info("pidgin_juice", "Could not find resource.\n");
		g_string_append(reply_string, "HTTP/1.1 404 Not Found\n");
		g_string_append(reply_string, "Content-length: 0\n");
		g_string_append(reply_string, "\n");
		purple_debug_info("pidgin_juice", "Bad request. Ignoring.\n");
		g_string_free(resource, TRUE);
		g_string_free(content, TRUE);
		return FALSE;
	}
	g_string_free(resource, TRUE);
	/* end response */
	
	
	g_string_append(reply_string, "HTTP/1.1 200 OK\n");
	g_string_append_printf(reply_string, "Content-length: %d\n", content->len);
	g_string_append(reply_string, "\n");
	g_string_append(reply_string, content->str);
	
	g_string_free(uri, TRUE);
	g_string_free(content, TRUE);
	
	g_string_free(path, TRUE);
	g_string_free(query, TRUE);
	return TRUE;
}

static gboolean
write_data (GIOChannel *gio, GIOCondition condition, gpointer data)
{
	GIOStatus ret;
	GError *err = NULL;
	gsize len;

	if (condition & G_IO_HUP)
		g_error ("Write end of pipe died!\n");

	g_io_channel_flush(gio, NULL);
		ret = g_io_channel_write_chars (gio, data, -1, &len, &err);

	if (ret == G_IO_STATUS_ERROR)
		g_error ("Error writing: (%u) %s\n", err->code, err->message);
	else
		g_io_channel_flush(gio, NULL);

	return TRUE;
}

static gboolean
read_data(GIOChannel *channel, GIOCondition condition, gpointer data)
{
	GError *err = NULL;
	gchar character;
	gsize len = 0;
	GString *request_string = NULL, *request_buffer, *reply_string;
	int *sock;

	//Find the user's key(sock) and put it in shared memory space
	sock = g_new(gint, 1);
	*sock = g_io_channel_unix_get_fd(channel);

	//Read everything they give us.
	request_string = g_string_new(NULL);
	while(!(condition & G_IO_HUP)
		&& g_io_channel_read_chars (channel, &character, 1, &len, &err)
		&& len)
	{
		request_string = g_string_append_c(request_string, character);
	}

	//If they didn't give us anything, they probably closed the connection.
	if (!len && !request_string->len)
	{
		g_string_free(request_string, TRUE);
		g_io_channel_close(channel);
		return FALSE;
	}
	
	//While we're not using an external buffer, make sure we initialize it
	request_buffer = g_string_new(NULL);
	g_string_append_printf(request_buffer, "%s", request_string->str);
	
	reply_string = g_string_new(NULL);
	process_request(request_buffer, reply_string);
	
	write_data(channel, condition, reply_string->str);
	
	//Free resources allocated in this function
	g_string_free(request_string, TRUE);
	g_string_free(request_buffer, TRUE);
	g_string_free(reply_string, TRUE);
	
	return TRUE;
}

static gboolean
accept_channel(GIOChannel *listener, GIOCondition condition, gpointer data)
{
	GIOChannel *new_channel;
	struct sockaddr_in address_in;
	size_t length_address = sizeof(address_in);
	int fd = -1;
	int listener_sock;
	int user_sock;

	#ifdef _WIN32
		listener_sock = g_io_channel_unix_get_fd(listener);
		fd = accept(listener_sock, (struct sockaddr *) &address_in, &length_address);
	#else	
		listener_sock = g_io_channel_unix_get_fd(listener);
		fd = accept(listener_sock, (struct sockaddr *) &address_in, &length_address);
	#endif
	if (fd >= 0)
	{
		printf("Connection from %s:%hu\n", inet_ntoa(address_in.sin_addr), ntohs(address_in.sin_port));
		new_channel = g_io_channel_unix_new(fd);
		g_io_channel_set_flags(new_channel, G_IO_FLAG_NONBLOCK, NULL);
		g_io_add_watch(new_channel, G_IO_PRI | G_IO_IN | G_IO_HUP, (GIOFunc)read_data, NULL);
	}
	else
	{
		return FALSE;
	}
	//write_data(new_channel, condition, "Connected.\n");

	//Find their key
	user_sock = g_io_channel_unix_get_fd(new_channel);
	purple_debug_info("pidgin_juice", "Accepted connection. Got user socket.\n");

	return TRUE;
}

static GIOChannel
*make_listener(unsigned short int port)
{
	GIOChannel *channel;
	int sock = -1;
	struct sockaddr_in address;
	int i=0, sock_is_bound=0;

	sock = socket (PF_INET, SOCK_STREAM, 0);
	purple_debug_info("pidgin_juice", "The socket is made.\n");
	if (sock < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}
	/* Create the address on which to listen */
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	for (i = 1; i <= 10; i++)
	{
		if (bind(sock, (struct sockaddr *) &address, sizeof(address)) >= 0 )
		{
			sock_is_bound = 1;
			break;
		}
		else
		{
			port = port + i;
			address.sin_port = htons(port);
		}
	}
	if (sock_is_bound == 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}
	if (listen(sock, 10) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}

#ifdef _WIN32
	channel = g_io_channel_win32_new_socket(sock);
#else
	channel = g_io_channel_unix_new(sock);
#endif
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL);

	return channel;
}

static void
start_web_server()
{
	unsigned short int port = 8000;
	GIOChannel *listener;
	
	listener = make_listener(port);
	
	//Tell the loop what to do when we receive a connection
	g_io_add_watch(listener, G_IO_IN, (GIOFunc)accept_channel, NULL);
}










/*
static void
signed_on_cb(PurpleConnection *gc, void *data) {
	do_check();
}
*/

/**************************************************************************
 * Plugin stuff
 **************************************************************************/
static gboolean
plugin_load(PurplePlugin *plugin)
{
	//purple_signal_connect(purple_connections_get_handle(), "signed-on",
	//					plugin, PURPLE_CALLBACK(signed_on_cb), NULL);
		
	/* do our webserver startup stuff */
	start_web_server();
	purple_debug_info("pidgin_juice", "Web server is finished starting.\n");

	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                            /**< priority       */

	"pidgin_juice",                                     /**< id             */
	N_("Pidgin Juice"),                       /**< name           */
	DISPLAY_VERSION,                                  /**< version        */
	                                                  /**  summary        */
	N_("Allows remote use of Pidgin accounts remotely."),
	                                                  /**  description    */
	N_("Allows remote use of Pidgin accounts remotely."),
	"Eion Robb, Jeremy Lawson",          /**< author         */
	PURPLE_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/pidgin_juice");
	purple_prefs_add_int("/plugins/pidgin_juice/port", 0);
}

PURPLE_INIT_PLUGIN(pidgin_juice, init_plugin, info)
