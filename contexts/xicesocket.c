/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2008-2009 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2008-2009 Nokia Corporation. All rights reserved.
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif


#include <glib.h>

#include "xicesocket.h"

gboolean
xice_socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf)
{
  return sock->send (sock, to, len, buf);
}

gboolean
xice_socket_is_reliable (XiceSocket *sock)
{
  return sock->is_reliable (sock);
}

void
xice_socket_free (XiceSocket *sock)
{
  if (sock) {
    sock->close (sock);
    g_slice_free (XiceSocket,sock);
  }
}

gboolean
xice_socket_set_callback(XiceSocket *sock, XiceSocketCallbackFunc callback, gpointer data) {
	if (sock) {
		sock->callback = callback;
		sock->data = data;
	}
	return TRUE;
}

int xice_socket_get_fd(XiceSocket* sock) {
	if (sock && sock->get_fd) {
		return sock->get_fd(sock);
	}
	return 0;
}