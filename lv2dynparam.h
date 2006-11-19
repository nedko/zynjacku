/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *  lv2-dynparam.h - header file for using dynamic parameters in LV2 plugins
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

#ifndef LV2DYNPARAM_H__31DEB371_3874_44A0_A9F2_AAFB0360D8C5__INCLUDED
#define LV2DYNPARAM_H__31DEB371_3874_44A0_A9F2_AAFB0360D8C5__INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Adjust editor indent */
#endif

typedef void * lv2dynparam_parameter_handle;
typedef void * lv2dynparam_group_handle;
typedef void * lv2dynparam_command_handle;

struct lv2dynparam_host_callbacks
{
  int (*group_appear)(
    void * instance_host_context,
    void * parent_group_host_context,
    lv2dynparam_group_handle group,
    void ** group_host_context);

  void (*group_disappear)(
    void * instance_host_context,
    void * group_host_context);

  int (*parameter_appear)(
    void * instance_host_context,
    void * group_host_context,
    lv2dynparam_parameter_handle parameter,
    void ** parameter_host_context);

  void (*parameter_disappear)(
    void * instance_host_context,
    void * parameter_host_context);

  void (*parameter_change)(
    void * instance_host_context,
    void * parameter_host_context);

  int (*command_appear)(
    void * instance_host_context,
    void * group_host_context,
    lv2dynparam_command_handle command,
    void ** command_host_context);

  void (*command_disappear)(
    void * instance_host_context,
    void * command_host_context);
};

struct lv2dynparam_plugin_callbacks
{
  int (*host_attach)(
    LV2_Handle instance,
    struct lv2dynparam_host_callbacks * host_callbacks,
    void * instance_host_context);

  int (*group_get_type_uri)(lv2dynparam_group_handle group, char * buffer, size_t buffer_size);
  int (*group_get_name)(lv2dynparam_group_handle group, char * buffer, size_t buffer_size);

  int (*parameter_get_type_uri)(lv2dynparam_parameter_handle parameter, char * buffer, size_t buffer_size);
  int (*parameter_get_name)(lv2dynparam_parameter_handle parameter, char * buffer, size_t buffer_size);
  void (*parameter_get_value)(lv2dynparam_parameter_handle parameter, void ** value_buffer);
  void (*parameter_get_range)(lv2dynparam_parameter_handle parameter, void ** value_min_buffer, void ** value_max_buffer);
  void (*parameter_change)(lv2dynparam_parameter_handle parameter);

  int (*command_get_name)(lv2dynparam_command_handle command, char * buffer, size_t buffer_size);
  void (*command_execute)(lv2dynparam_command_handle command);
};

#if 0
{ /* Adjust editor indent */
#endif
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef LV2DYNPARAM_H__31DEB371_3874_44A0_A9F2_AAFB0360D8C5__INCLUDED */
