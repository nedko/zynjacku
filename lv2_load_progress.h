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

/**
 * @file lv2_load_progress.h
 * @brief LV2 plugin load progress notification extension definition
 *
 * The purpose of this extension is to prevent host UI thread freeze for
 * plugins doing intensive computations during load. Plugin should call
 * the host provided callback on regular basis during load. 1 second between
 * calls is good target. Everything between half second and two seconds
 * should provide enough motion so user does not get "the thing freezed"
 * impression.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Adjust editor indent */
#endif

/** URI for the plugin load progress feature */
#define LV2_LOAD_PROGRESS_URI "http://lv2plug.in/ns/dev/load_progress"

/** @brief host feature structure */
struct lv2_load_progress
{
  /** to be supplied as first parameter of load_progres() callback  */
  void * context;

  /**
   * This function is called by plugin to notify host load progress.
   *
   * @param context Host context
   * @param progress Load progress, from 0.0 to 100.0
   * @param message Optional (may be NULL) string describing current operation.
   * If called once with non-NULL message, subsequent calls will NULL message
   * mean that host will reuse the previous message.
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
