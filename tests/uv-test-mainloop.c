/*
* This file is part of the Xice GLib ICE library.
*
* (C) 2006, 2007 Collabora Ltd.
*  Contact: Dafydd Harries
* (C) 2006, 2007 Nokia Corporation. All rights reserved.
*  Contact: Kai Vehmanen
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
*   Dafydd Harries, Collabora Ltd.
*   Kai Vehmanen, Nokia
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
# include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <xice/xice.h>
#include <uv.h>

static uv_loop_t *loop = NULL;

static void
recv_cb(
	XiceAgent *agent,
	guint stream_id,
	guint component_id,
	guint len,
	gchar *buf,
	gpointer data)
{
	g_assert(agent != NULL);
	g_assert(stream_id == 1);
	g_assert(component_id == 1);
	g_assert(len == 6);
	g_assert(0 == strncmp(buf, "\x80hello", len));
	g_assert(42 == GPOINTER_TO_UINT(data));

	uv_stop(loop);
	uv_loop_close(loop);
}

int
main(void)
{
	XiceAgent *agent;
	XiceAddress addr;
	guint stream;
	xice_address_init(&addr);
	g_type_init();
	g_thread_init(NULL);

	loop = uv_default_loop();
	XiceContext* context = xice_context_create("libuv", (gpointer)loop);
	agent = xice_agent_new(context, XICE_COMPATIBILITY_RFC5245);

	xice_address_set_ipv4(&addr, 0x7f000001);
	xice_agent_add_local_address(agent, &addr);
	stream = xice_agent_add_stream(agent, 1);
	xice_agent_gather_candidates(agent, stream);


	xice_agent_attach_recv(agent, stream, XICE_COMPONENT_TYPE_RTP, recv_cb, GUINT_TO_POINTER(42));
	{
		XiceCandidate *candidate;
		GSList *candidates, *i;

		candidates = xice_agent_get_local_candidates(agent, 1, 1);
		candidate = candidates->data;

		xice_socket_send(candidate->sockptr, &(candidate->addr), 6, "\x80hello");
		for (i = candidates; i; i = i->next)
			xice_candidate_free((XiceCandidate *)i->data);
		g_slist_free(candidates);
	}
	uv_run(loop, UV_RUN_DEFAULT);
	g_object_unref(agent);

	return 0;
}

