/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2006-2007 Collabora Ltd.
 *  Contact: Dafydd Harries
 * (C) 2006-2007 Nokia Corporation. All rights reserved.
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

#ifndef _RANDOM_H
#define _RANDOM_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _XiceRNG XiceRNG;

struct _XiceRNG {
  void (*seed) (XiceRNG *src, guint32 seed);
  void (*generate_bytes) (XiceRNG *src, guint len, gchar *buf);
  guint (*generate_int) (XiceRNG *src, guint low, guint high);
  void (*free) (XiceRNG *src);
  gpointer priv;
};

XiceRNG *
xice_rng_new (void);

void
xice_rng_set_new_func (XiceRNG * (*func) (void));

void
xice_rng_seed (XiceRNG *rng, guint32 seed);

void
xice_rng_generate_bytes (XiceRNG *rng, guint len, gchar *buf);

void
xice_rng_generate_bytes_print (XiceRNG *rng, guint len, gchar *buf);

guint
xice_rng_generate_int (XiceRNG *rng, guint low, guint high);

void
xice_rng_free (XiceRNG *rng);

G_END_DECLS

#endif // _RANDOM_H

