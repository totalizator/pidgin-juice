/*
 * libjuice
 *
 * libjuice is the property of its developers.  See the COPYRIGHT file
 * for more details.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef JUICEPRPL_CONN_H
#define JUICEPRPL_CONN_H

#include "libjuice.h"

/*
 * This is a bitmask.
 */
typedef enum
{
	FB_METHOD_GET  = 0x0001,
	FB_METHOD_POST = 0x0002,
	FB_METHOD_SSL  = 0x0004
} JuiceMethod;

typedef struct _JuiceConnection JuiceConnection;
struct _JuiceConnection {
	JuiceAccount *fba;
	JuiceMethod method;
	gchar *hostname;
	gchar *url;
	GString *request;
	JuiceProxyCallbackFunc callback;
	gpointer user_data;
	char *rx_buf;
	size_t rx_len;
	PurpleProxyConnectData *connect_data;
	PurpleSslConnection *ssl_conn;
	int fd;
	guint input_watcher;
	gboolean connection_keepalive;
	time_t request_time;
};

void fb_connection_destroy(JuiceConnection *fbconn);
void fb_post_or_get(JuiceAccount *fba, JuiceMethod method,
		const gchar *host, const gchar *url, const gchar *postdata,
		JuiceProxyCallbackFunc callback_func, gpointer user_data,
		gboolean keepalive);

#endif /* JUICEPRPL_CONN_H */
