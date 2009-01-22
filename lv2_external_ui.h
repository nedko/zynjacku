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

#ifndef LV2_EXTERNAL_UI_H__5AFE09A5_0FB7_47AF_924E_2AF0F8DE8873__INCLUDED
#define LV2_EXTERNAL_UI_H__5AFE09A5_0FB7_47AF_924E_2AF0F8DE8873__INCLUDED

#define LV2_EXTERNAL_UI_URI "http://lv2plug.in/ns/extensions/ui#external"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Adjust editor indent */
#endif

struct lv2_ui_external
{
  void (* run)(struct lv2_ui_external * _this_);
  void (* show)(struct lv2_ui_external * _this_);
  void (* hide)(struct lv2_ui_external * _this_);
};

#define LV2_UI_EXTERNAL_RUN(ptr) (ptr)->run(ptr)
#define LV2_UI_EXTERNAL_SHOW(ptr) (ptr)->show(ptr)
#define LV2_UI_EXTERNAL_HIDE(ptr) (ptr)->hide(ptr)

#if 0
{ /* Adjust editor indent */
#endif
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef LV2_EXTERNAL_UI_H__5AFE09A5_0FB7_47AF_924E_2AF0F8DE8873__INCLUDED */
