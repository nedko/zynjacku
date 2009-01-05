/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *  This work is in public domain.
 *
 *  This file is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  If you have questions, contact Nedko Arnaudov <nedko@arnaudov.name> or
 *  ask in #lad channel, FreeNode IRC network.
 *
 *****************************************************************************/

#ifndef LV2_LOAD_PROGRESS_H__F576843C_CA13_49C3_9BF9_CFF3A15AD18C__INCLUDED
#define LV2_LOAD_PROGRESS_H__F576843C_CA13_49C3_9BF9_CFF3A15AD18C__INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Adjust editor indent */
#endif

/** URI for the string port transfer mechanism feature */
#define LV2_LOAD_PROGRESS_URI "http://lv2plug.in/ns/dev/load_progress"

struct lv2_load_progress
{
  void * context; /**< to be supplied as first parameter of load_progres  */

  /**
   * This function is called by plugin to notify host load progress.
   *
   * @param context Host context
   * @param progress load progress, from 0.0 to 100.0
   * @param message optional (may be NULL) string describing current operation.
   * Once called with non-NULL message, subsequent calls will NULL message mean
   * that host should reuse the previous message.
   */
  void (*load_progress)(void * context, float progress, const char * message);
};

#if 0
{ /* Adjust editor indent */
#endif
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef LV2_LOAD_PROGRESS_H__F576843C_CA13_49C3_9BF9_CFF3A15AD18C__INCLUDED */
