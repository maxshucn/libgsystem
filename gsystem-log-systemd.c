/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Colin Walters <walters@verbum.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>

#include "gsystem-log.h"
/* No point including location since we wrap */
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>

/**
 * gs_slog_id:
 * @message_id: An 128bit ID, formatted as a lowercase hexadecimal (32 bytes)
 * @format: A format string
 * 
 * Log a message with a unique ID.  On systemd-based operating
 * systems, this writes to the journal.  The @message_id
 * must follow the restrictions listed here:
 * <ulink url="http://www.freedesktop.org/software/systemd/man/systemd.journal-fields.html">.
 */
void
gs_slog_id (const char *message_id,
            int         priority,
            const char *format, ...)
{
  va_list args;
  const char *msgid_key = "MESSAGE_ID=";
  const int msgid_keylen = strlen (msgid_key);
  char msgid_buf[msgid_keylen + 32]; /* NOTE: Not NUL terminated */
  const char *priority_key = "PRIORITY=";
  const int priority_keylen = strlen (priority_key);
  char priority_buf[priority_keylen + 1]; /* NOTE: Not NUL terminated */
  struct iovec iov[3];
  char *raw_msg;
  char *msg;
  int n_iov = 0;
  int res;

  g_return_if_fail (priority >= 0 && priority <= 7);

  va_start (args, format);
  raw_msg = g_strdup_vprintf (format, args);
  va_end (args);

  msg = g_strconcat ("MESSAGE=", raw_msg, NULL);
  g_free (raw_msg);

  iov[n_iov].iov_base = msg;
  iov[n_iov].iov_len = strlen (msg);
  n_iov++;
  memcpy (priority_buf, priority_key, priority_keylen);
  *(priority_buf + priority_keylen) = 48 + priority;
  iov[n_iov].iov_base = priority_buf;
  iov[n_iov].iov_len = priority_keylen + 1;
  n_iov++;

  if (message_id)
    {
      memcpy (msgid_buf, msgid_key, msgid_keylen);
      g_assert (strlen (message_id) == 32);
      memcpy (msgid_buf + msgid_keylen, message_id, 32);

      iov[n_iov].iov_base = msgid_buf;
      iov[n_iov].iov_len = sizeof (msgid_buf);
      n_iov++;
    }

  res = sd_journal_sendv (iov, n_iov);
  if (res != 0)
    {
      g_printerr ("sd_journal_send(): %s\n", g_strerror (-res));
    }

  g_free (msg);
}

void
gs_log_error (GError *error)
{
  (void) sd_journal_send ("MESSAGE=%s", error->message,
                          "PRIORITY=%d", 4, /* LOG_ERR */
                          "GERROR_DOMAIN=%d", error->domain,
                          "GERROR_CODE=%d", error->code,
                          NULL);
}
