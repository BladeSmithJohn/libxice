/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2008 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2008 Nokia Corporation. All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Xice GLib ICE library.
 *
 * The Initial Developers of the Original Code are Collabora Ltd and Nokia
 * Corporation. All Rights Reserved.
 *
 * Contributors:
 *   Youness Alaoui, Collabora Ltd.
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * the GNU Lesser General Public License Version 2.1 (the "LGPL"), in which
 * case the provisions of LGPL are applicable instead of those above. If you
 * wish to allow use of your version of this file only under the terms of the
 * LGPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the LGPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the LGPL.
 */

/*
 * Implementation of TCP relay socket interface using TCP Berkeley sockets. (See
 * http://en.wikipedia.org/wiki/Berkeley_sockets.)
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "http.h"

#include <string.h>
#include <stdlib.h>


#ifndef G_OS_WIN32
#include <unistd.h>
#endif


#define HTTP_USER_AGENT "libxice"

typedef enum {
  HTTP_STATE_INIT,
  HTTP_STATE_HEADERS,
  HTTP_STATE_BODY,
  HTTP_STATE_CONNECTED,
  HTTP_STATE_ERROR
} HttpState;

typedef struct {
  HttpState state;
  XiceSocket *base_socket;
  XiceAddress addr;
  gchar *username;
  gchar *password;
  GQueue send_queue;
  gchar *recv_buf;
  gint recv_len;
  gint content_length;
} HttpPriv;


struct to_be_sent {
  guint length;
  gchar *buf;
  XiceAddress to;
};

static gboolean read_callback(
	XiceSocket *socket,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from);

static void socket_close (XiceSocket *sock);

static gboolean socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf);
static gboolean socket_is_reliable (XiceSocket *sock);

static void add_to_be_sent (XiceSocket *sock, const XiceAddress *to,
    const gchar *buf, guint len);
static void free_to_be_sent (struct to_be_sent *tbs);


XiceSocket *
xice_http_socket_new (XiceSocket *base_socket,
    XiceAddress *addr, gchar *username, gchar *password)
{
  HttpPriv *priv;
  XiceSocket *sock = NULL;

  if (addr) {
    sock = g_slice_new0 (XiceSocket);
    sock->priv = priv = g_slice_new0 (HttpPriv);

    priv->base_socket = base_socket;
    priv->addr = *addr;
    priv->username = g_strdup (username);
    priv->password = g_strdup (password);
    priv->recv_buf = NULL;
    priv->recv_len = 0;
    priv->content_length = 0;

	xice_socket_set_callback(base_socket, read_callback, sock);

    sock->fileno = priv->base_socket->fileno;
    sock->addr = priv->base_socket->addr;
    sock->send = socket_send;
    sock->is_reliable = socket_is_reliable;
    sock->close = socket_close;
	sock->get_fd = priv->base_socket->get_fd;
    /* Send HTTP CONNECT */
    {
      gchar *msg = NULL;
      gchar *credential = NULL;
      gchar host[INET6_ADDRSTRLEN];
      gint port = xice_address_get_port (&priv->addr);
      xice_address_to_string (&priv->addr, host);

      if (username) {
        gchar * userpass = g_strdup_printf ("%s:%s", username,
            password ? password : "");
        gchar * auth = g_base64_encode ((guchar *)userpass, strlen (userpass));
        credential = g_strdup_printf ("Proxy-Authorization: Basic %s\r\n", auth);
        g_free (auth);
        g_free (userpass);
      }
      msg = g_strdup_printf ("CONNECT %s:%d HTTP/1.0\r\n"
          "Host: %s\r\n"
          "User-Agent: %s\r\n"
          "Content-Length: 0\r\n"
          "Proxy-Connection: Keep-Alive\r\n"
          "Connection: Keep-Alive\r\n"
          "Cache-Control: no-cache\r\n"
          "Pragma: no-cache\r\n"
          "%s\r\n", host, port, host, HTTP_USER_AGENT,
          credential? credential : "" );
      g_free (credential);

      xice_socket_send (priv->base_socket, NULL, strlen (msg), msg);
      priv->state = HTTP_STATE_INIT;
      g_free (msg);
    }
  }

  return sock;
}


static void
socket_close (XiceSocket *sock)
{
  HttpPriv *priv = sock->priv;

  if (priv->base_socket)
    xice_socket_free (priv->base_socket);

  if (priv->username)
    g_free (priv->username);

  if (priv->password)
    g_free (priv->password);

  if (priv->recv_buf)
    g_free (priv->recv_buf);

  g_queue_foreach (&priv->send_queue, (GFunc) free_to_be_sent, NULL);
  g_queue_clear (&priv->send_queue);

  g_slice_free(HttpPriv, sock->priv);
}

static gboolean read_callback(
	XiceSocket *base_sock,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from) {
	XiceSocket* sock = (XiceSocket*)data;
	HttpPriv *priv = sock->priv;
	if (condition != XICE_SOCKET_READABLE || len <= 0) {
		goto error;
	}

	if (priv->state == HTTP_STATE_CONNECTED) {
		if (priv->base_socket && sock->callback) {
			return sock->callback(sock, XICE_SOCKET_READABLE, sock->data, buf, len, from);
		}
		else {
			return FALSE;
		}
	}
	priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len + len);
	memcpy(priv->recv_buf + priv->recv_len, buf, len);
	priv->recv_len += len;
retry:
	xice_debug("Receiving from HTTP proxy (state %d) : %d \n'%s'", priv->state, priv->recv_len, priv->recv_buf);
	switch (priv->state) {
	case HTTP_STATE_INIT:
	{
		gint pos = 0;

		/* Remove any leading spaces (could happen!) */
		while (pos < priv->recv_len && priv->recv_buf[pos] == ' ')
			pos++;

		/* Make sure we have enough data */
		if (pos >= priv->recv_len)
			goto not_enough_data;

		if (pos + 7 > priv->recv_len)
			goto not_enough_data;
		if (strncmp(priv->recv_buf + pos, "HTTP/1.", 7) != 0)
			goto error;
		pos += 7;

		if (pos >= priv->recv_len)
			goto not_enough_data;
		if (priv->recv_buf[pos] != '0' && priv->recv_buf[pos] != '1')
			goto error;
		pos++;

		/* Make sure we have a space after the HTTP version */
		if (pos >= priv->recv_len)
			goto not_enough_data;
		if (priv->recv_buf[pos] != ' ')
			goto error;

		/* Skip all spaces (could be more than one!) */
		while (pos < priv->recv_len && priv->recv_buf[pos] == ' ')
			pos++;
		if (pos >= priv->recv_len)
			goto not_enough_data;

		/* Check for a successfull 2xx code */
		if (pos + 3 > priv->recv_len)
			goto not_enough_data;
		if (priv->recv_buf[pos] != '2' ||
			priv->recv_buf[pos + 1] < '0' || priv->recv_buf[pos + 1] > '9' ||
			priv->recv_buf[pos + 2] < '0' || priv->recv_buf[pos + 2] > '9')
			goto error;

		/* Clear any trailing chars */
		while (pos + 1 < priv->recv_len &&
			priv->recv_buf[pos] != '\r' && priv->recv_buf[pos + 1] != '\n')
			pos++;
		if (pos + 1 >= priv->recv_len)
			goto not_enough_data;
		pos += 2;

		/* consume the data we just parsed */
		priv->recv_len -= pos;
		memmove(priv->recv_buf, priv->recv_buf + pos, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);

		priv->content_length = 0;
		priv->state = HTTP_STATE_HEADERS;
		goto retry;
	}
	break;
	case HTTP_STATE_HEADERS:
	{
		gint pos = 0;

		if (pos + 15 < priv->recv_len &&
			g_ascii_strncasecmp(priv->recv_buf, "Content-Length:", 15) == 0) {
			priv->content_length = atoi(priv->recv_buf + 15);
		}
		while (pos + 1 < priv->recv_len &&
			priv->recv_buf[pos] != '\r' && priv->recv_buf[pos + 1] != '\n')
			pos++;
		xice_debug("pos = %d, len = %d", pos, priv->recv_len);
		if (pos + 1 >= priv->recv_len)
			goto not_enough_data;
		pos += 2;

		/* consume the data we just parsed */
		priv->recv_len -= pos;
		memmove(priv->recv_buf, priv->recv_buf + pos, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);

		if (pos == 2)
			priv->state = HTTP_STATE_BODY;
		goto retry;
	}
	break;
	case HTTP_STATE_BODY:
	{
		gint consumed = priv->content_length;
		if (priv->content_length == 0) {
			priv->state = HTTP_STATE_CONNECTED;
			goto retry;
		}
		if (priv->recv_len == 0)
			goto not_enough_data;

		if (priv->content_length > priv->recv_len)
			consumed = priv->recv_len;

		priv->recv_len -= consumed;
		priv->content_length -= consumed;
		memmove(priv->recv_buf, priv->recv_buf + consumed, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);
		goto retry;
	}
	break;
	case HTTP_STATE_CONNECTED:
	{
		guint read = priv->recv_len;
		struct to_be_sent *tbs = NULL;
		gboolean ret = FALSE;
		//if (read > len)
		//	read = len;

		//memcpy(buf, priv->recv_buf, read);
		if (sock->callback && read > 0) {
			ret = sock->callback(sock, XICE_SOCKET_READABLE, sock->data, priv->recv_buf, read, from);
		}

		/* consume the data we returned */
		priv->recv_len -= read;
		memmove(priv->recv_buf, priv->recv_buf + read, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);

		/* Send the pending data */
		while ((tbs = g_queue_pop_head(&priv->send_queue))) {
			xice_socket_send(priv->base_socket, &tbs->to,
				tbs->length, tbs->buf);
			g_free(tbs->buf);
			g_slice_free(struct to_be_sent, tbs);
		}

		return ret;
	}
	break;
	default:
		/* Unknown status */
		goto error;
	}

not_enough_data:
	return TRUE;

error:
	xice_debug("http error");
	if (sock->callback) {
		if (condition == XICE_SOCKET_READABLE)
			condition = XICE_SOCKET_ERROR;
		sock->callback(sock, condition, sock->data, NULL, 0, NULL);
	}
	if (priv->base_socket) {
		xice_socket_free(priv->base_socket);
	}
	priv->base_socket = NULL;
	priv->state = HTTP_STATE_ERROR;

	return FALSE;
}

static gboolean
socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf)
{
  HttpPriv *priv = sock->priv;

  if (priv->state == HTTP_STATE_CONNECTED) {
    if (priv->base_socket)
      return xice_socket_send (priv->base_socket, to, len, buf);
    else
      return FALSE;
  } else if (priv->state == HTTP_STATE_ERROR) {
    return FALSE;
  } else {
    add_to_be_sent (sock, to, buf, len);
  }
  return TRUE;
}


static gboolean
socket_is_reliable (XiceSocket *sock)
{
  return TRUE;
}


static void
add_to_be_sent (XiceSocket *sock, const XiceAddress *to,
    const gchar *buf, guint len)
{
  HttpPriv *priv = sock->priv;
  struct to_be_sent *tbs = NULL;

  if (len <= 0)
    return;

  tbs = g_slice_new0 (struct to_be_sent);
  tbs->buf = g_memdup (buf, len);
  tbs->length = len;
  if (to)
    tbs->to = *to;
  g_queue_push_tail (&priv->send_queue, tbs);

}


static void
free_to_be_sent (struct to_be_sent *tbs)
{
  g_free (tbs->buf);
  g_slice_free (struct to_be_sent, tbs);
}
