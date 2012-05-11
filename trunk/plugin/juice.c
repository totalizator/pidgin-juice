#define PURPLE_PLUGINS

#include <glib.h>

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <sys/types.h>
#ifdef __GNUC__
#include	<unistd.h>
#include	<fcntl.h>
#endif

#ifndef G_GNUC_NULL_TERMINATED
#	if __GNUC__ >= 4
#		define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#	else
#		define G_GNUC_NULL_TERMINATED
#	endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef _WIN32
#	include "win32dep.h"
#	define dlopen(a,b) LoadLibrary(a)
#	define RTLD_LAZY
#	define dlsym(a,b) GetProcAddress(a,b)
#	define dlclose(a) FreeLibrary(a)
#else
#	include <arpa/inet.h>
#	include <dlfcn.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif

#include <time.h>

#include "network.h"
#include "eventloop.h"
#include "plugin.h"
#include "prefs.h"
#include "debug.h"
#include "sslconn.h"
#include "ft.h"
#include "blist.h"

#define PREFS_BASE		"/plugins/core/juice"
#define PREF_PORT		PREFS_BASE "/port_number"
#define PREF_USERNAME	PREFS_BASE "/username"
#define PREF_PASSWORD	PREFS_BASE "/password"

static PurpleNetworkListenData *listen_data = NULL;
static guint input_handle = 0;
static gint listenfd = -1;
static GHashTable *resource_handlers = NULL;

typedef struct _JuiceHandles {
	guint http_input_handle;
	gint acceptfd;
	GString *databuffer;
} JuiceHandles;

static GHashTable *
juice_parse_query(const gchar *query)
{
	GHashTable *$_GET;
	gchar** pairs, *pair[2], **url_encoded_pair;
	int i;
	
	//Setup a php-like $_GET array (hash table)
	$_GET = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	
	if (query == NULL)
		return $_GET;
	
	pairs = g_strsplit(query, "&", -1);
	for (i = 0; pairs[i]; i++)
	{
		url_encoded_pair = g_strsplit(pairs[i], "=", 2);
		if (url_encoded_pair[0] != NULL)
		{
			pair[0] = g_strdup(purple_url_decode(url_encoded_pair[0]));
			if (url_encoded_pair[1] != NULL)
				pair[1] = g_strdup(purple_url_decode(url_encoded_pair[1]));
			else
				pair[1] = g_strdup("");
			
			purple_debug_info("pidgin_juice", "Adding %s, %s to hash table.\n", pair[0], pair[1]);
			g_hash_table_insert($_GET, pair[0], pair[1]);
		}
		g_strfreev(url_encoded_pair);
	}
	g_strfreev(pairs);
	
	return $_GET;
}

static GHashTable *
juice_parse_headers(const gchar *head)
{
	GHashTable *headers;
	gchar **lines, **pair;
	int i;
	
	headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	if (head == NULL)
		return headers;
	
	lines = g_strsplit(head, "\r\n", -1);
	for (i = 0; lines[i]; i++)
	{
		pair = g_strsplit(lines[i], ": ", 2);
		if (pair[0] != NULL)
		{
			g_hash_table_insert(headers, g_strdup(pair[0]), g_strdup(pair[1]));
		}
		g_strfreev(pair);
	}
	g_strfreev(lines);
	
	return headers;
}

typedef struct _JuiceRequestObject {
	const gchar *first_line;
	const gchar *headers;
	const gchar *postdata;
	
	const gchar *uri;
	const gchar *request_type;
	
	const gchar *filename;
	const gchar *query;
} JuiceRequestObject;

typedef gboolean (*JuiceResourceHandlerFunc)(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length);

static void juice_handle_events(JuiceRequestObject *request, gint output_fd);

static gboolean
juice_add_resource_handler(const gchar *resource, JuiceResourceHandlerFunc handler)
{
	if (g_hash_table_lookup(resource_handlers, resource))
		return FALSE;
	
	g_hash_table_insert(resource_handlers, (gpointer)resource, handler);
	return TRUE;
}

static gboolean
juice_default_resource_handler(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length)
{
	gchar *real_filename;
	GError *error = NULL;
	gboolean success;
	const gchar *this_filename;
	
	if (request->filename[0] != '/')
		return FALSE;
	
	purple_debug_info("pidgin_juice", "Serving physical file\n");
	
	if (g_str_equal(request->filename, "/"))
		this_filename = "/index.html";
	else
		this_filename = request->filename;
#ifndef DATADIR
	gchar *DATADIR = g_get_current_dir();
#endif
	real_filename = g_strdup_printf("%s%s%s%s%s", DATADIR, G_DIR_SEPARATOR_S, "juice", G_DIR_SEPARATOR_S, this_filename+1);
#ifndef DATADIR
	g_free(DATADIR);
#endif
	purple_debug_info("juice", "filename: %s\n", real_filename);
	
	success = g_file_get_contents(real_filename, response, response_length, &error);
	g_free(real_filename);
	if (!success)
	{
		if (error)
		{
			purple_debug_error("juice", "error: %s\n", error->message);
			g_error_free(error);
		}
		return FALSE;
	}
	
	purple_debug_info("pidgin_juice", "file length: %d\n", *response_length);
	
	return TRUE;
}

static void
juice_handle_request(JuiceRequestObject *request, gint output_fd)
{
	JuiceResourceHandlerFunc handler;
	GHashTable *$_GET;
	gchar *response;
	gsize response_len;
	GString *reply_string;
	
	handler = g_hash_table_lookup(resource_handlers, request->filename);
	if (handler == NULL)
		handler = juice_default_resource_handler;
	
	$_GET = juice_parse_query(request->query);
	
	if (handler(request, $_GET, &response, &response_len))
	{
		reply_string = g_string_new(NULL);
		g_string_append(reply_string, "HTTP/1.0 200 OK\r\n");
		/* set appropriate mime type */
		if (g_str_has_suffix(request->filename, ".png"))
		{
			g_string_append(reply_string, "Content-type: image/png\r\n");
			if (!g_str_equal(request->filename, "/buddy_icon.png"))
			{
				g_string_append(reply_string, "Cache-Control: public\r\n");
				g_string_append(reply_string, "Pragma: cache\r\n");
				g_string_append(reply_string, "Expires: Tue, 01 Oct 2020 16:00:00 GMT\r\n");
			}
		}
		else if (g_str_has_suffix(request->filename, ".js"))
		{
				g_string_append(reply_string, "Cache-Control: no-cache\r\n");
				g_string_append(reply_string, "Content-type: text/javascript; charset=utf-8\r\n");
				g_string_append(reply_string, "Pragma: No-cache\r\n");
				g_string_append(reply_string, "Expires: Tue, 01 Sep 2000 16:00:00 GMT\r\n");
		}
		else {
			g_string_append(reply_string, "Cache-Control: public\r\n");
			g_string_append(reply_string, "Pragma: cache\r\n");
			g_string_append(reply_string, "Expires: Tue, 01 Oct 2009 16:00:00 GMT\r\n");
		}
		if (g_str_has_suffix(request->filename, ".html"))
			g_string_append(reply_string, "Content-type: text/html; charset=utf-8\r\n");
		else if (g_str_has_suffix(request->filename, ".css"))
			g_string_append(reply_string, "Content-type: text/css; charset=utf-8\r\n");
			
		/* end mime type */
		g_string_append_printf(reply_string, "Content-length: %d\r\n", response_len);
		//g_string_append(reply_string, "Connection: close\r\n");
		g_string_append(reply_string, "\r\n");
		
		g_string_append_len(reply_string, response, response_len);
		g_free(response);
		
		response_len = write(output_fd, reply_string->str, reply_string->len);
		purple_debug_info("juice", "write len %d\n", response_len);
		
		g_string_free(reply_string, TRUE);
	} else {
		reply_string = g_string_new(NULL);
		purple_debug_error("pidgin_juice", "Could not find resource.\n");
		g_string_append(reply_string, "HTTP/1.0 404 Not Found\r\n");
		g_string_append(reply_string, "Content-length: 0\r\n");
		//g_string_append(reply_string, "Connection: close\r\n");
		g_string_append(reply_string, "\r\n");
		write(output_fd, reply_string->str, reply_string->len);
		g_string_free(reply_string, TRUE);
	}
	
	g_hash_table_destroy($_GET);
}

static gboolean
juice_check_auth(const gchar *header_str)
{
	GHashTable *headers;
	const gchar *authorization;
	guchar *decoded_auth;
	gchar **auth_parts;
	gboolean success = FALSE;
	const gchar *username, *password;
	
	username = purple_prefs_get_string(PREF_USERNAME);
	password = purple_prefs_get_string(PREF_PASSWORD);
	if (!username || !*username || !password || !*password)
		return TRUE;
	
	headers = juice_parse_headers(header_str);
	
	authorization = g_hash_table_lookup(headers, "Authorization");
	if (authorization && *authorization)
	{
		if (g_str_has_prefix(authorization, "Basic "))
		{
			decoded_auth = purple_base64_decode(&authorization[6], NULL);
			auth_parts = g_strsplit(decoded_auth, ":", 2);
			if (auth_parts[0] && auth_parts[1] && 
				g_str_equal(auth_parts[0], username) &&
				g_str_equal(auth_parts[1], password))
			{
				success = TRUE;
			}
			g_strfreev(auth_parts);
			g_free(decoded_auth);
		} //TODO handle Digest auth
	}
	
	g_hash_table_destroy(headers);
	
	return success;
}

static void
juice_process_http_request(const GString *request, gint output_fd)
{
	JuiceRequestObject *jro;
	
	gchar *first_line;
	gchar *headers;
	gchar *postdata;
	gchar **first_line_info;
	gchar **uri_split;
	GString *reply_string;
	
	const gchar *buffer = request->str;
	gssize len = request->len;

	const gchar *first_line_end = g_strstr_len(buffer, len, "\r\n");
	const gchar *headers_end = g_strstr_len(buffer, len, "\r\n\r\n");
	
	do //This is a bit of a cludge, but makes the code easier to follow IMO :)
	{
		if (!first_line_end || !headers_end)
			break;
		
		first_line = g_strndup(buffer, (first_line_end - buffer));
		headers = g_strndup(first_line_end + 2, (headers_end - first_line_end - 2));
		postdata = g_strndup(headers_end + 4, (len - (headers_end - buffer) - 4));
		
		purple_debug_misc("juice", "Got request %s\n", first_line);
		purple_debug_misc("juice", "Got headers %s\n", headers);
		purple_debug_misc("juice", "Got postdata %s\n", postdata);
		
		first_line_info = g_strsplit_set(first_line, " ", 3);
		if (first_line_info[0] == NULL)
		{
			// Invalid request
			g_free(postdata);
			g_free(headers);
			g_free(first_line);
			g_strfreev(first_line_info);
			break;
		}
		if (headers)
		{
			if (juice_check_auth(headers) == FALSE)
			{
				reply_string = g_string_new(NULL);
				g_string_append(reply_string, "HTTP/1.1 401 Authorization Required\r\n");
				g_string_append(reply_string, "Content-length: 0\r\n");
				g_string_append(reply_string, "WWW-Authenticate: Basic realm=\"Juice Security\"\r\n");
				//g_string_append(reply_string, "Connection: close\r\n");
				g_string_append(reply_string, "\r\n");
				purple_debug_warning("pidgin_juice", "Did not authenticate.\n");
				len = write(output_fd, reply_string->str, reply_string->len);
				g_string_free(reply_string, TRUE);
				
				g_free(postdata);
				g_free(headers);
				g_free(first_line);
				g_strfreev(first_line_info);
				return;
			}
		}
		
		
		purple_debug_misc("juice", "Request type %s\n", first_line_info[0]);
		purple_debug_misc("juice", "Request URI %s\n", first_line_info[1]);
		
		uri_split = g_strsplit_set(first_line_info[1], "?", 2);
		if (uri_split[0] == NULL)
		{
			// Invalid request
			g_free(postdata);
			g_free(headers);
			g_free(first_line);
			g_strfreev(uri_split);
			g_strfreev(first_line_info);
			break;
		}
		purple_debug_misc("juice", "Filename %s\n", uri_split[0]);
		purple_debug_misc("juice", "Query %s\n", uri_split[1]);
		
		jro = g_new0(JuiceRequestObject, 1);
		jro->first_line = first_line;
		jro->headers = headers;
		jro->postdata = postdata;
		jro->request_type = first_line_info[0];
		jro->uri = first_line_info[1];
		jro->filename = uri_split[0];
		jro->query = uri_split[1];
		
		//Handle request
		if (g_str_equal(jro->filename, "/events.js"))
			juice_handle_events(jro, output_fd);
		else
			juice_handle_request(jro, output_fd);
		
		g_free(jro);
		
		g_strfreev(uri_split);
		g_strfreev(first_line_info);
		
		g_free(postdata);
		g_free(headers);
		g_free(first_line);

		return;
	} while (FALSE);
	
	reply_string = g_string_new(NULL);
	g_string_append(reply_string, "HTTP/1.1 400 Bad Request\r\n");
	g_string_append(reply_string, "Content-length: 0\r\n");
	//g_string_append(reply_string, "Connection: close\r\n");
	g_string_append(reply_string, "\r\n");
	purple_debug_info("pidgin_juice", "Bad request. Ignoring.\n");
	len = write(output_fd, reply_string->str, reply_string->len);
	g_string_free(reply_string, TRUE);
}

static void
juice_http_read(gpointer data, gint source, PurpleInputCondition cond)
{
	char buffer[1024];
	int len;
	JuiceHandles *handles = data;

	memset(buffer, 0, sizeof(buffer));
	//TODO purple_ssl_read
	len = recv(source, buffer, sizeof(buffer), 0);

	if (len <= 0 && (errno == EAGAIN))// || errno == EWOULDBLOCK || errno == EINTR
		return;
	else if (len < 0) {
		if (handles->http_input_handle)
			purple_input_remove(handles->http_input_handle);
		close(source);
		
		purple_debug_info("juice", "Closed connection (%d)\n", errno);
		
		g_string_free(handles->databuffer, TRUE);
		g_free(handles);
		
		return;
	}
	
	g_string_append_len(handles->databuffer, buffer, len);
	
	if (len < sizeof(buffer)) {
		if (handles->databuffer->len > 0)
		{
			juice_process_http_request(handles->databuffer, source);
			g_string_truncate(handles->databuffer, 0);
		} else {
			if (handles->http_input_handle)
				purple_input_remove(handles->http_input_handle);
			close(source);
			
			purple_debug_info("juice", "Closed connection\n");
		
			g_string_free(handles->databuffer, TRUE);
			g_free(handles);
			
			return;
		}
	}
}

static void
juice_read_listen_input(gpointer data, gint source, PurpleInputCondition cond)
{
	gint flags;
	JuiceHandles *handles;

	gint acceptfd = accept(listenfd, NULL, 0);
	if (acceptfd == -1)
	{
		/* Accepting the connection failed. This could just be related
		 * to the nonblocking nature of the listening socket, so we'll
		 * just try again next time */
		/* Let's print an error message anyway */
		purple_debug_warning("juice", "accept: %s\n", g_strerror(errno));
		return;
	}
	
	
	flags = fcntl(acceptfd, F_GETFL);
	fcntl(acceptfd, F_SETFL, flags | O_NONBLOCK);
#ifndef _WIN32
	fcntl(acceptfd, F_SETFD, FD_CLOEXEC);
#endif

	handles = g_new0(JuiceHandles, 1);
	handles->acceptfd = acceptfd;
	handles->databuffer = g_string_new(NULL);
	
	handles->http_input_handle = purple_input_add(acceptfd, PURPLE_INPUT_READ, juice_http_read, handles);
}

static void
juice_listen_callback(int fd, gpointer data)
{
	listenfd = fd;
	input_handle = purple_input_add(listenfd, PURPLE_INPUT_READ, juice_read_listen_input, data);
}

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

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin) {
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	pref = purple_plugin_pref_new_with_name(PREF_PORT);
	purple_plugin_pref_set_label(pref, _("Listening port"));
	purple_plugin_pref_set_bounds(pref, 80, 10000);
	purple_plugin_pref_frame_add(frame, pref);
	
	pref = purple_plugin_pref_new_with_name(PREF_USERNAME);
	purple_plugin_pref_set_label(pref, _("Username"));
	purple_plugin_pref_frame_add(frame, pref);
	
	pref = purple_plugin_pref_new_with_name(PREF_PASSWORD);
	purple_plugin_pref_set_label(pref, _("Password"));
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	gint port;
	
	port = purple_prefs_get_int(PREF_PORT);
	if (port > 0)
	{
		listen_data = purple_network_listen_family(port, AF_INET, SOCK_STREAM, juice_listen_callback, NULL);		
		if (listen_data == NULL)
		{
			gchar *port_error_msg = g_strdup_printf("Unable to listen on port %d\n", port);
			purple_notify_error(plugin, "Error opening port", port_error_msg, "Try changing the port number in preferences");
			g_free(port_error_msg);
		}
	}
	
	// I'd love to return FALSE here, but then we couldn't configure any settings :(
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	if (listen_data)
		g_free(listen_data);
	
	listen_data = NULL;
	
	return TRUE;
}

static PurplePluginUiInfo prefs_info = {
	get_plugin_pref_frame,
	0,    /* page_num (Reserved) */
	NULL, /* frame (Reserved) */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};


static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	2,//PURPLE_MAJOR_VERSION,
	7,//PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,   /**< type */
	NULL,                   /**< ui_requirement */
	0,                      /**< flags */
	NULL,                   /**< dependencies */
	PURPLE_PRIORITY_DEFAULT,  /**< priority */

	"pidgin-juice",         /**< id */
	"Pidgin Juice",         /**< name */
	"0.2",                  /**< version */
	"Remote HTTP",          /**< summary */
	"Access Pidgin remotely", /**< description */
	"Eion Robb and Jeremy Lawson", /**< author */
	"http://pidgin-juice.googlecode.com/", /**< homepage */

	plugin_load,            /**< load */
	plugin_unload,          /**< unload */
	NULL,                   /**< destroy */

	NULL,                   /**< ui_info */
	NULL,                   /**< extra_info */
	&prefs_info,            /**< prefs_info */
	NULL,                   /**< actions */

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static gboolean
juice_get_buddylist(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length)
{
	PurpleBuddy *buddy;
	PurpleAccount *account;
	const char *status_message_unescaped;
	gchar *status_message;
	gchar *display_name;
	const gchar *display_name_tmp;
	gchar *buddyname;
	PurpleBuddyIcon *icon;
	PurpleStatus *status;
	gboolean available;
	const gchar *proto_id;
	const gchar *proto_name;
	gchar *account_username;
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
		display_name_tmp = purple_buddy_get_alias(buddy);
		buddyname = juice_utf8_json_encode(purple_buddy_get_name(buddy));
		icon = purple_buddy_get_icon(buddy);

		display_name = juice_utf8_json_encode(display_name_tmp);
		
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
		account_username = juice_utf8_json_encode(purple_account_get_username(account));
			
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
		g_free(buddyname);
		g_free(account_username);
	}
	
	g_string_append(blist, "]}");
	
	*response = blist->str;
	*response_length = blist->len;
	
	g_string_free(blist, FALSE);
	
	return TRUE;
}


static gboolean
juice_get_buddyicon(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length)
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
		return FALSE;
	
	icon = purple_buddy_icons_find(account, buddyname);
	if (icon == NULL)
		return FALSE;
	
	buddy_icon_data = purple_buddy_icon_get_data(icon, &buddy_icon_len);
	buddy_icon_data_copy = (gchar *)g_memdup(buddy_icon_data, buddy_icon_len);
	purple_buddy_icon_unref(icon);
	
	*response_length = buddy_icon_len;
	*response = buddy_icon_data_copy;
	
	return TRUE;
}


static gboolean
juice_get_history(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length)
{
	PurpleAccount *account = NULL;
	PurpleConversation *conv = NULL;
	GList *history;
	PurpleConvMessage *msg;
	PurpleMessageFlags flags;
	const gchar *buddyname = NULL;
	const gchar *proto_id = NULL;
	const gchar *proto_username = NULL;
	gchar *escaped;
	GString *history_output;
	time_t purple_timestamp;
	guint64 timestamp;
	gboolean first;
	
	buddyname = (gchar *)g_hash_table_lookup($_GET, "buddyname");
	proto_id = (gchar *)g_hash_table_lookup($_GET, "proto_id");
	proto_username = (gchar *)g_hash_table_lookup($_GET, "proto_username");
	
	if (proto_username && proto_id)
	{
		account = purple_accounts_find(proto_username, proto_id);
	}
	if (account && buddyname)
	{
		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
													 buddyname, account);
	}
	if (conv == NULL)
	{
		*response = g_strdup("");
		*response_length = 0;
		return TRUE;
	}
	
	history = purple_conversation_get_message_history(conv);
	history_output = g_string_new("{\"history\":[");
	
	first = TRUE;	
	for(; history; history = g_list_next(history))
	{
		msg = history->data;
		
		flags = purple_conversation_message_get_flags(msg);
		if (flags & PURPLE_MESSAGE_SEND ||
			flags & PURPLE_MESSAGE_RECV)
		{
			//continue;
		}
		else
		{
			continue;
		}
		
		if (first)
			first = FALSE;
		else
			g_string_append_c(history_output, ',');
		
		escaped = juice_utf8_json_encode(purple_conversation_message_get_sender(msg));
		g_string_append_printf(history_output, "{\"sender\":\"%s\", ", escaped);
		g_free(escaped);
		
		escaped = juice_utf8_json_encode(purple_conversation_message_get_message(msg));
		g_string_append_printf(history_output, "\"message\":\"%s\", ", escaped);
		g_free(escaped);
		
		g_string_append_printf(history_output, "\"type\":\"%s\", ", 
								(flags & PURPLE_MESSAGE_RECV?"received":"sent"));
		
		purple_timestamp = purple_conversation_message_get_timestamp(msg);
		timestamp = purple_timestamp; //convert time_t to guint64
		timestamp = timestamp * 1000; //we want milliseconds
		
		g_string_append_printf(history_output, "\"timestamp\":%" G_GUINT64_FORMAT "}", timestamp);
	}
	
	escaped = juice_utf8_json_encode(buddyname);
	g_string_append_printf(history_output, "],\"buddyname\":\"%s\", ", escaped);
	g_free(escaped);
	
	escaped = juice_utf8_json_encode(proto_username);
	g_string_append_printf(history_output, "\"account_username\":\"%s\", ", escaped);
	g_free(escaped);
	
	escaped = juice_utf8_json_encode(proto_id);
	g_string_append_printf(history_output, "\"proto_id\":\"%s\"}", escaped);
	g_free(escaped);
	
	*response_length = history_output->len;
	*response = history_output->str;
	
	g_string_free(history_output, FALSE);
	
	return TRUE;
}

static gboolean
juice_post_sendim(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length)
{
	PurpleAccount *account;
	gchar *return_string;
	PurpleConversation *conv;
	gchar *message_encoded = NULL;
	const gchar *buddyname;
	const gchar *proto_id;
	const gchar *proto_username;
	const gchar *message;
	
	buddyname = (gchar *)g_hash_table_lookup($_GET, "buddyname");
	proto_id = (gchar *)g_hash_table_lookup($_GET, "proto_id");
	proto_username = (gchar *)g_hash_table_lookup($_GET, "account_username");
	message = (gchar *)g_hash_table_lookup($_GET, "message");
	
	if (!buddyname || !proto_id || !proto_username)
	{
		return FALSE;
	}
	
	message_encoded = purple_strdup_withhtml(message);
	
	account = purple_accounts_find(proto_username, proto_id);
	conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
													buddyname, account);
	if (conv == NULL)
	{
		conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, buddyname);
	}
	purple_conv_im_send(PURPLE_CONV_IM(conv), message_encoded);
	g_free(message_encoded);
	
	return_string = g_strdup("{ \"message_sent\" : 1 }");

	*response_length = strlen(return_string);
	*response = return_string;
	
	return TRUE;
}

static gboolean
juice_post_sendtyping(JuiceRequestObject *request, GHashTable *$_GET, gchar **response, gsize *response_length)
{
	PurpleAccount *account;
	PurpleTypingState state;
	const gchar *proto_id;
	const gchar *proto_username;
	const gchar *buddyname;
	
	proto_id = (gchar *)g_hash_table_lookup($_GET, "proto_id");
	proto_username = (gchar *)g_hash_table_lookup($_GET, "proto_username");
	buddyname = (gchar *)g_hash_table_lookup($_GET, "buddyname");
	
	if (g_str_equal(g_hash_table_lookup($_GET, "typing"), "1"))
		state = PURPLE_TYPING;
	else
		state = PURPLE_NOT_TYPING;
	
	account = purple_accounts_find(proto_username, proto_id);
	serv_send_typing(purple_account_get_connection(account), buddyname, state);
	
	return TRUE;
}


static void juice_handle_events(JuiceRequestObject *request, gint output_fd);

#define FROM_JUICE_PLUGIN 1
#include "juice_events.c"

static void
init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none(PREFS_BASE);
	purple_prefs_add_int(PREF_PORT, 80);
	purple_prefs_add_string(PREF_USERNAME, "");
	purple_prefs_add_string(PREF_PASSWORD, "");
	
	resource_handlers = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
	
	
	juice_add_resource_handler("/buddies_list.js", juice_get_buddylist);
	juice_add_resource_handler("/buddy_icon.png", juice_get_buddyicon);
	juice_add_resource_handler("/history.js", juice_get_history);
	juice_add_resource_handler("/send_im.js", juice_post_sendim);
	juice_add_resource_handler("/send_typing.js", juice_post_sendtyping);
	
}


PURPLE_INIT_PLUGIN(juice, init_plugin, info);
