/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"

BUFFER newbuf(int initial_length) {
  BUFFER buf = malloc(sizeof(Buffer));

  buf->buflength = initial_length;
  buf->pos = 0;
  buf->buf = malloc(initial_length);
  memset(buf->buf, 0, initial_length);

  return buf;
}

BUFFER dupbuf(BUFFER buf) {
  BUFFER n = malloc(sizeof(Buffer));

  n->buflength = buf->buflength;
  n->pos = buf->pos;
  n->buf = malloc(buf->buflength);
  memcpy(n->buf, buf->buf, buf->buflength);

  return n;
}

void killbuf(BUFFER buf) {
  free(buf->buf);
  free(buf);
}

void buf_append(BUFFER buf, char ch) {
  if (buf->pos >= buf->buflength) {
    char *newbuf = malloc(buf->buflength + 128);

    if (newbuf == NULL) {
      fprintf(stderr, "buf_append: could not grow buffer\n");
      exit(1);
    }

    memset(newbuf, 0, buf->buflength + 128);
    memcpy(newbuf, buf->buf, buf->buflength);
    free(buf->buf);
    buf->buf = newbuf;
    buf->buflength += 128;
  }

  buf->buf[buf->pos++] = ch;
}

void buf_insert(BUFFER buf, char ch, int pos) {
  int i;

  if (pos < 0)
    pos = 0;
  if (pos > buf->pos)
    pos = buf->pos;

  buf_append(buf, 0);
  for (i = buf->pos; i > pos; i--)
    buf->buf[i] = buf->buf[i-1];
  buf->buf[pos] = ch;
}

void buf_delete(BUFFER buf, int pos) {
  int i;

  if (pos < 0)
    pos = 0;
  if (pos >= buf->pos)
    pos = buf->pos - 1;

  for (i = pos; i < buf->pos; i++)
    buf->buf[i] = buf->buf[i+1];
  buf->buf[buf->pos - 1] = '\0';
  buf->pos--;
}
