/*
 * This file is part of the Xice GLib ICE library.
 *
 * Contains a unit test for functionality to fallback to non-ICE 
 * operation if remote party does not support ICE.
 *
 * (C) 2007 Nokia Corporation. All rights reserved.
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

#include "agent.h"
//#include "agent-priv.h" /* for testing purposes */  // conflict with uv!!!

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <uv.h>


static XiceComponentState global_lagent_state = XICE_COMPONENT_STATE_LAST;
static XiceComponentState global_ragent_state = XICE_COMPONENT_STATE_LAST;
static guint global_components_ready = 0;
static guint global_components_ready_exit = 0;
static guint global_components_failed = 0;
static guint global_components_failed_exit = 0;
static uv_loop_t *global_mainloop = NULL;
static gboolean global_lagent_gathering_done = FALSE;
static gboolean global_ragent_gathering_done = FALSE;
static gboolean global_lagent_ibr_received = FALSE;
static gboolean global_ragent_ibr_received = FALSE;
static int global_lagent_cands = 0;
static int global_ragent_cands = 0;
static gint global_ragent_read = 0;
static gint global_ragent_read_exit = 0;
static gboolean global_accept_non_data = TRUE;

static void priv_print_global_status (void)
{
  g_debug ("\tgathering_done=%d", global_lagent_gathering_done && global_ragent_gathering_done);
  g_debug ("\tlstate=%d", global_lagent_state);
  g_debug ("\trstate=%d", global_ragent_state);
}

static void timer_cb (uv_handle_t* pointer)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, pointer);

  /* signal status via a global variable */

  /* note: should not be reached, abort */
  g_error ("ERROR: test has got stuck, aborting...");

  return FALSE;
}

static void cb_xice_recv (XiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, user_data);

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)component_id; (void)buf;

  /*
   * Lets ignore stun packets that got through
   */
  if (len != 16 || strncmp ("1234567812345678", buf, 16)) {
    if (global_accept_non_data)
      return;
    else
      g_error ("Got non-data packet of lenght %u", len);
  }

  if ((intptr_t)user_data == 2) {
    global_ragent_read += len;

    if (global_ragent_read == global_ragent_read_exit)
      uv_stop (global_mainloop);
  }
}

static void cb_candidate_gathering_done(XiceAgent *agent, guint stream_id, gpointer data)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, data);

  if ((intptr_t)data == 1)
    global_lagent_gathering_done = TRUE;
  else if ((intptr_t)data == 2)
    global_ragent_gathering_done = TRUE;

  if (global_lagent_gathering_done &&
      global_ragent_gathering_done)
    uv_stop (global_mainloop);

  /* XXX: dear compiler, these are for you: */
  (void)agent;
}

static void cb_component_state_changed (XiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, data);

  if ((intptr_t)data == 1)
    global_lagent_state = state;
  else if ((intptr_t)data == 2)
    global_ragent_state = state;
  
  if (state == XICE_COMPONENT_STATE_READY)
    global_components_ready++;
  if (state == XICE_COMPONENT_STATE_FAILED)
    global_components_failed++;

  g_debug ("test-fallback: READY %u exit at %u.", global_components_ready, global_components_ready_exit);

  /* signal status via a global variable */
  if (global_components_ready == global_components_ready_exit) {
    uv_stop (global_mainloop); 
    return;
  }

  /* signal status via a global variable */
  if (global_components_failed == global_components_failed_exit) {
    uv_stop (global_mainloop); 
    return;
  }

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)data; (void)component_id;
}

static void cb_new_selected_pair(XiceAgent *agent, guint stream_id, guint component_id, 
				 gchar *lfoundation, gchar* rfoundation, gpointer data)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, data);

  if ((intptr_t)data == 1)
    ++global_lagent_cands;
  else if ((intptr_t)data == 2)
    ++global_ragent_cands;

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)component_id; (void)lfoundation; (void)rfoundation;
}

static void cb_new_candidate(XiceAgent *agent, guint stream_id, guint component_id, 
			     gchar *foundation, gpointer data)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, data);

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)data; (void)component_id; (void)foundation;
}

static void cb_initial_binding_request_received(XiceAgent *agent, guint stream_id, gpointer data)
{
  g_debug ("test-fallback:%s: %p", G_STRFUNC, data);

  if ((intptr_t)data == 1)
    global_lagent_ibr_received = TRUE;
  else if ((intptr_t)data == 2)
    global_ragent_ibr_received = TRUE;

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)data;
}

static void priv_get_local_addr (XiceAgent *agent, guint stream_id, guint component_id, XiceAddress *dstaddr)
{
  GSList *cands, *i;
  cands = xice_agent_get_local_candidates(agent, stream_id, component_id);
  for (i = cands; i; i = i->next) {
    XiceCandidate *cand = i->data;
    if (cand) {
      g_assert (dstaddr);
      *dstaddr = cand->addr;
      break;
    }
  }
  for (i = cands; i; i = i->next)
    xice_candidate_free ((XiceCandidate *) i->data);
  g_slist_free (cands);
}

static int run_fallback_test (XiceAgent *lagent, XiceAgent *ragent, XiceAddress *baseaddr)
{
  XiceAddress laddr, raddr, laddr_rtcp, raddr_rtcp;   
  XiceCandidate cdes;
  GSList *cands;
  guint ls_id, rs_id;

  memset (&cdes, 0, sizeof(XiceCandidate));
  cdes.priority = 100000;
  strcpy (cdes.foundation, "1");
  cdes.type = XICE_CANDIDATE_TYPE_HOST;
  cdes.transport = XICE_CANDIDATE_TRANSPORT_UDP;
  cdes.base_addr = *baseaddr;

  /* step: initialize variables modified by the callbacks */
  global_components_ready = 0;
  global_components_ready_exit = 4;
  global_components_failed = 0;
  global_components_failed_exit = 4;
  global_lagent_gathering_done = FALSE;
  global_ragent_gathering_done = FALSE;
  global_lagent_ibr_received =
    global_ragent_ibr_received = FALSE;
  global_lagent_cands = 
    global_ragent_cands = 0;
  global_ragent_read_exit = -1;

  g_object_set (G_OBJECT (lagent), "controlling-mode", TRUE, NULL);
  g_object_set (G_OBJECT (ragent), "controlling-mode", FALSE, NULL);

  /* step: add one stream, with RTP+RTCP components, to each agent */
  ls_id = xice_agent_add_stream (lagent, 2);
  rs_id = xice_agent_add_stream (ragent, 2);
  g_assert (ls_id > 0);
  g_assert (rs_id > 0);

  xice_agent_gather_candidates (lagent, ls_id);
  xice_agent_gather_candidates (ragent, rs_id);

  /* step: attach to mainloop (needed to register the fds) */
  xice_agent_attach_recv (lagent, ls_id, XICE_COMPONENT_TYPE_RTP,
      cb_xice_recv, (gpointer)1);
  xice_agent_attach_recv (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP,
      cb_xice_recv, (gpointer)1);
  xice_agent_attach_recv (ragent, rs_id, XICE_COMPONENT_TYPE_RTP,
      cb_xice_recv, (gpointer)2);
  xice_agent_attach_recv (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP,
      cb_xice_recv, (gpointer)2);

  /* step: run mainloop until local candidates are ready 
   *       (see timer_cb() above) */
  {
    g_debug ("test-fallback: Added streams, running mainloop until 'candidate-gathering-done'...");
    uv_run (global_mainloop, UV_RUN_DEFAULT);
    g_assert (global_lagent_gathering_done == TRUE);
    g_assert (global_ragent_gathering_done == TRUE);
  }

  /* step: find out the local candidates of each agent */

  priv_get_local_addr (ragent, rs_id, XICE_COMPONENT_TYPE_RTP, &raddr);
  g_debug ("test-fallback: local RTP port R %u",
           xice_address_get_port (&raddr));

  priv_get_local_addr (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, &laddr);
  g_debug ("test-fallback: local RTP port L %u",
           xice_address_get_port (&laddr));

  priv_get_local_addr (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP, &raddr_rtcp);
  g_debug ("test-fallback: local RTCP port R %u",
           xice_address_get_port (&raddr_rtcp));

  priv_get_local_addr (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP, &laddr_rtcp);
  g_debug ("test-fallback: local RTCP port L %u",
           xice_address_get_port (&laddr_rtcp));

  /* step: exchange candidate information but not the credentials */

  cands = g_slist_append (NULL, &cdes);
  cdes.component_id = XICE_COMPONENT_TYPE_RTP;
  cdes.addr = raddr;
  xice_agent_set_remote_candidates (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, cands);
  cdes.addr = laddr;
  xice_agent_set_remote_candidates (ragent, rs_id, XICE_COMPONENT_TYPE_RTP, cands);
  cdes.component_id = XICE_COMPONENT_TYPE_RTCP;
  cdes.addr = raddr_rtcp;
  xice_agent_set_remote_candidates (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP, cands);
  cdes.addr = laddr_rtcp;
  xice_agent_set_remote_candidates (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP, cands);

  /* step: fall back to non-ICE mode on both sides */
  g_assert (xice_agent_set_selected_pair (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, "1", "1") == TRUE);
  g_assert (xice_agent_set_selected_pair (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP, "1", "1") == TRUE);
  g_assert (xice_agent_set_selected_pair (ragent, rs_id, XICE_COMPONENT_TYPE_RTP, "1", "1") == TRUE);
  g_assert (xice_agent_set_selected_pair (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP, "1", "1") == TRUE);

  g_debug ("test-fallback: Requested for fallback, running mainloop until component state change is completed...");

  /* step: run the mainloop until connectivity checks succeed 
   *       (see timer_cb() above) */
  uv_run (global_mainloop, UV_RUN_DEFAULT);

  /* note: verify that agents are in correct state */
  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);
  g_assert (global_ragent_state == XICE_COMPONENT_STATE_READY);

  /* step: next send a packet -> should work even if no ICE processing
   *       has been done */

  g_debug ("test-fallback: Sent a payload packet, run mainloop until packet received.");

  /* step: send a new test packet from L ot R */
  global_ragent_read = 0;
  g_assert (xice_agent_send (lagent, ls_id, 1, 16, "1234567812345678") == 16);
  global_ragent_read_exit = 16;
  uv_run(global_mainloop, UV_RUN_DEFAULT);

  /* note: verify that payload was succesfully received */
  g_assert (global_ragent_read == 16);

  g_debug ("test-fallback: Ran mainloop, removing streams...");

  /* step: clean up resources and exit */

  g_slist_free (cands);
  xice_agent_remove_stream (lagent, ls_id);
  xice_agent_remove_stream (ragent, rs_id);

  g_debug ("test-fallback: test COMPLETED");

  return 0;
}


static int run_safe_fallback_test (XiceAgent *lagent, XiceAgent *ragent, XiceAddress *baseaddr)
{
  XiceAddress laddr, raddr, laddr_rtcp, raddr_rtcp;
  XiceCandidate cdes;
  guint ls_id, rs_id;

  memset (&cdes, 0, sizeof(XiceCandidate));
  cdes.priority = 100000;
  strcpy (cdes.foundation, "1");
  cdes.type = XICE_CANDIDATE_TYPE_HOST;
  cdes.transport = XICE_CANDIDATE_TRANSPORT_UDP;
  cdes.base_addr = *baseaddr;

  /* step: initialize variables modified by the callbacks */
  global_components_ready = 0;
  global_components_ready_exit = 4;
  global_components_failed = 0;
  global_components_failed_exit = 4;
  global_lagent_gathering_done = FALSE;
  global_ragent_gathering_done = FALSE;
  global_lagent_ibr_received =
    global_ragent_ibr_received = FALSE;
  global_lagent_cands =
    global_ragent_cands = 0;
  global_ragent_read_exit = -1;
  global_accept_non_data = FALSE;

  g_object_set (G_OBJECT (lagent), "controlling-mode", TRUE, NULL);
  g_object_set (G_OBJECT (ragent), "controlling-mode", FALSE, NULL);

  /* step: add one stream, with RTP+RTCP components, to each agent */
  ls_id = xice_agent_add_stream (lagent, 2);
  rs_id = xice_agent_add_stream (ragent, 2);
  g_assert (ls_id > 0);
  g_assert (rs_id > 0);

  xice_agent_gather_candidates (lagent, ls_id);
  xice_agent_gather_candidates (ragent, rs_id);

  /* step: attach to mainloop (needed to register the fds) */
  xice_agent_attach_recv (lagent, ls_id, XICE_COMPONENT_TYPE_RTP,
      cb_xice_recv, (gpointer)1);
  xice_agent_attach_recv (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP,
      cb_xice_recv, (gpointer)1);
  xice_agent_attach_recv (ragent, rs_id, XICE_COMPONENT_TYPE_RTP,
      cb_xice_recv, (gpointer)2);
  xice_agent_attach_recv (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP,
      cb_xice_recv, (gpointer)2);

  /* step: run mainloop until local candidates are ready
   *       (see timer_cb() above) */
  {
    g_debug ("test-fallback: Added streams, running mainloop until 'candidate-gathering-done'...");
    uv_run(global_mainloop, UV_RUN_DEFAULT);
    g_assert (global_lagent_gathering_done == TRUE);
    g_assert (global_ragent_gathering_done == TRUE);
  }

  /* step: find out the local candidates of each agent */

  priv_get_local_addr (ragent, rs_id, XICE_COMPONENT_TYPE_RTP, &raddr);
  g_debug ("test-fallback: local RTP port R %u",
           xice_address_get_port (&raddr));

  priv_get_local_addr (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, &laddr);
  g_debug ("test-fallback: local RTP port L %u",
           xice_address_get_port (&laddr));

  priv_get_local_addr (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP, &raddr_rtcp);
  g_debug ("test-fallback: local RTCP port R %u",
           xice_address_get_port (&raddr_rtcp));

  priv_get_local_addr (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP, &laddr_rtcp);
  g_debug ("test-fallback: local RTCP port L %u",
           xice_address_get_port (&laddr_rtcp));

  /* step: exchange candidate information but not the credentials */

  cdes.component_id = XICE_COMPONENT_TYPE_RTP;
  cdes.addr = raddr;
  g_assert (xice_agent_set_selected_remote_candidate (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, &cdes));

  cdes.addr = laddr;
  g_assert (xice_agent_set_selected_remote_candidate (ragent, rs_id, XICE_COMPONENT_TYPE_RTP, &cdes));

  cdes.component_id = XICE_COMPONENT_TYPE_RTCP;
  cdes.addr = raddr_rtcp;
  g_assert (xice_agent_set_selected_remote_candidate (lagent, ls_id, XICE_COMPONENT_TYPE_RTCP, &cdes));

  cdes.addr = laddr_rtcp;
  g_assert (xice_agent_set_selected_remote_candidate (ragent, rs_id, XICE_COMPONENT_TYPE_RTCP, &cdes));

  g_debug ("test-fallback: Requested for fallback, running mainloop until component state change is completed...");

  /* step: run the mainloop until connectivity checks succeed
   *       (see timer_cb() above) */

  uv_run(global_mainloop, UV_RUN_DEFAULT);

  /* note: verify that agents are in correct state */
  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);
  g_assert (global_ragent_state == XICE_COMPONENT_STATE_READY);

  /* step: next send a packet -> should work even if no ICE processing
   *       has been done */

  g_debug ("test-fallback: Sent a payload packet, run mainloop until packet received.");

  /* step: send a new test packet from L ot R */
  global_ragent_read = 0;
  g_assert (xice_agent_send (lagent, ls_id, 1, 16, "1234567812345678") == 16);
  global_ragent_read_exit = 16;
  uv_run(global_mainloop, UV_RUN_DEFAULT);

  /* note: verify that payload was succesfully received */
  g_assert (global_ragent_read == 16);

  g_debug ("test-fallback: Ran mainloop, removing streams...");

  /* step: clean up resources and exit */

  xice_agent_remove_stream (lagent, ls_id);
  xice_agent_remove_stream (ragent, rs_id);

  g_debug ("test-fallback: test COMPLETED");

  return 0;
}

int main (void)
{
  XiceContext* lcontext, *rcontext;
  XiceAgent *lagent, *ragent;      /* agent's L and R */
  XiceAddress baseaddr;
  int result;
  uv_timer_t timer_id;
  const char *stun_server = NULL, *stun_server_port = NULL;

#ifdef G_OS_WIN32
  WSADATA w;

  WSAStartup(0x0202, &w);
#endif
  g_type_init ();
#if !GLIB_CHECK_VERSION(2,31,8)
  g_thread_init (NULL);
#endif

  global_mainloop = malloc(sizeof(uv_loop_t));
  uv_loop_init(global_mainloop);

  /* Note: impl limits ...
   * - no multi-stream support
   * - no IPv6 support
   */

  lcontext = xice_context_create("libuv", (gpointer)global_mainloop);
  rcontext = xice_context_create("libuv", (gpointer)global_mainloop);

  /* step: create the agents L and R */
  lagent = xice_agent_new (lcontext, XICE_COMPATIBILITY_RFC5245);
  ragent = xice_agent_new (rcontext, XICE_COMPATIBILITY_RFC5245);


  /* step: add a timer to catch state changes triggered by signals */
  uv_timer_init(global_mainloop, &timer_id);
  uv_timer_start(&timer_id, timer_cb, 30000, 0);


  /* step: specify which local interface to use */
  if (!xice_address_set_from_string (&baseaddr, "127.0.0.1"))
    g_assert_not_reached ();
  xice_agent_add_local_address (lagent, &baseaddr);
  xice_agent_add_local_address (ragent, &baseaddr);

  g_signal_connect (G_OBJECT (lagent), "candidate-gathering-done", 
		    G_CALLBACK (cb_candidate_gathering_done), (gpointer)1);
  g_signal_connect (G_OBJECT (ragent), "candidate-gathering-done", 
		    G_CALLBACK (cb_candidate_gathering_done), (gpointer)2);
  g_signal_connect (G_OBJECT (lagent), "component-state-changed", 
		    G_CALLBACK (cb_component_state_changed), (gpointer)1);
  g_signal_connect (G_OBJECT (ragent), "component-state-changed", 
		    G_CALLBACK (cb_component_state_changed), (gpointer)2);
  g_signal_connect (G_OBJECT (lagent), "new-selected-pair", 
		    G_CALLBACK (cb_new_selected_pair), (gpointer)1);
  g_signal_connect (G_OBJECT (ragent), "new-selected-pair", 
		    G_CALLBACK (cb_new_selected_pair), (gpointer)2);
  g_signal_connect (G_OBJECT (lagent), "new-candidate", 
		    G_CALLBACK (cb_new_candidate), (gpointer)1);
  g_signal_connect (G_OBJECT (ragent), "new-candidate", 
		    G_CALLBACK (cb_new_candidate), (gpointer)2);
  g_signal_connect (G_OBJECT (lagent), "initial-binding-request-received", 
		    G_CALLBACK (cb_initial_binding_request_received), (gpointer)1);
  g_signal_connect (G_OBJECT (ragent), "initial-binding-request-received", 
		    G_CALLBACK (cb_initial_binding_request_received), (gpointer)2);

  stun_server = getenv ("XICE_STUN_SERVER");
  stun_server_port = getenv ("XICE_STUN_SERVER_PORT");
  if (stun_server) {
    g_object_set (G_OBJECT (lagent), "stun-server", stun_server,  NULL);
    g_object_set (G_OBJECT (lagent), "stun-server-port", atoi (stun_server_port),  NULL);
    g_object_set (G_OBJECT (ragent), "stun-server", stun_server,  NULL);
    g_object_set (G_OBJECT (ragent), "stun-server-port", atoi (stun_server_port),  NULL);
  }

  /* step: run test the first time */
  g_debug ("test-fallback: TEST STARTS / fallback test");
  result = run_fallback_test (lagent, ragent, &baseaddr);
  priv_print_global_status ();
  g_assert (result == 0);
  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);
  g_assert (global_ragent_state == XICE_COMPONENT_STATE_READY);

  /* step: run the safe test without sending any stnu */
  g_debug ("test-fallback: TEST STARTS / safe fallback test");
  result = run_safe_fallback_test (lagent, ragent, &baseaddr);
  priv_print_global_status ();
  g_assert (result == 0);
  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);
  g_assert (global_ragent_state == XICE_COMPONENT_STATE_READY);

  g_object_unref (lagent);
  g_object_unref (ragent);

  uv_stop(global_mainloop);
  uv_loop_close(global_mainloop);
  global_mainloop = NULL;
  uv_unref((uv_handle_t*)&timer_id);

  xice_context_destroy(lcontext);
  xice_context_destroy(rcontext);

#ifdef G_OS_WIN32
  WSACleanup();
#endif
  return result;
}
