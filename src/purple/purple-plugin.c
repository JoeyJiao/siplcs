/**
 * @file purple-plugin.c
 *
 * pidgin-sipe
 *
 * Copyright (C) 2010-12 SIPE Project <http://sipe.sourceforge.net/>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#include <glib.h>

#include "sipe-common.h"

/* Flag needed for correct version of PURPLE_INIT_PLUGIN() */
#ifndef PURPLE_PLUGINS
#define PURPLE_PLUGINS
#endif

/* for LOCALEDIR
 * as it's determined on runtime, as Pidgin installation can be anywhere.
 */
#ifdef _WIN32
#include "win32/win32dep.h"
#endif

#include "accountopt.h"
#include "blist.h"
#include "connection.h"
#include "core.h"
#include "dnssrv.h"
#ifdef HAVE_VV
#include "media.h"
#endif
#include "prpl.h"
#include "plugin.h"
#include "request.h"
#include "status.h"
/*
 * NOTE: Currently PURPLE_VERSION_CHECK(2,y,z) returns FALSE for libpurple >= 3.0.0.
 *       See also <http://developer.pidgin.im/ticket/14551>
 *
 * As a workaround an additional PURPLE_VERSION_CHECK(3,0,0) needs to be added.
 */
#include "version.h"

#include "sipe-backend.h"
#include "sipe-core.h"
#include "sipe-nls.h"

#define _PurpleMessageFlags PurpleMessageFlags
#include "purple-private.h"

/* Backward compatibility when compiling against 2.4.x API */
#if !PURPLE_VERSION_CHECK(2,5,0) && !PURPLE_VERSION_CHECK(3,0,0)
#define PURPLE_CONNECTION_ALLOW_CUSTOM_SMILEY 0x0100
#endif

/* Sipe core activity <-> Purple status mapping */
static const gchar * const activity_to_purple_map[SIPE_ACTIVITY_NUM_TYPES] = {
/* SIPE_ACTIVITY_UNSET       */ "unset",     /* == purple_primitive_get_id_from_type(PURPLE_STATUS_UNSET) */
/* SIPE_ACTIVITY_AVAILABLE   */ "available", /* == purple_primitive_get_id_from_type(PURPLE_STATUS_AVAILABLE) */
/* SIPE_ACTIVITY_ONLINE      */ "online",
/* SIPE_ACTIVITY_INACTIVE    */ "idle",
/* SIPE_ACTIVITY_BUSY        */ "busy",
/* SIPE_ACTIVITY_BUSYIDLE    */ "busyidle",
/* SIPE_ACTIVITY_DND         */ "do-not-disturb",
/* SIPE_ACTIVITY_BRB         */ "be-right-back",
/* SIPE_ACTIVITY_AWAY        */ "away",      /* == purple_primitive_get_id_from_type(PURPLE_STATUS_AWAY) */
/* SIPE_ACTIVITY_LUNCH       */ "out-to-lunch",
/* SIPE_ACTIVITY_INVISIBLE   */ "invisible", /* == purple_primitive_get_id_from_type(PURPLE_STATUS_INVISIBLE) */
/* SIPE_ACTIVITY_OFFLINE     */ "offline",   /* == purple_primitive_get_id_from_type(PURPLE_STATUS_OFFLINE) */
/* SIPE_ACTIVITY_ON_PHONE    */ "on-the-phone",
/* SIPE_ACTIVITY_IN_CONF     */ "in-a-conference",
/* SIPE_ACTIVITY_IN_MEETING  */ "in-a-meeting",
/* SIPE_ACTIVITY_OOF         */ "out-of-office",
/* SIPE_ACTIVITY_URGENT_ONLY */ "urgent-interruptions-only",
};

GHashTable *purple_token_map;

static void sipe_purple_activity_init(void)
{
	guint index;

	purple_token_map = g_hash_table_new(g_str_hash, g_str_equal);
	for (index = SIPE_ACTIVITY_UNSET;
	     index < SIPE_ACTIVITY_NUM_TYPES;
	     index++) {
		g_hash_table_insert(purple_token_map,
				    (gchar *) activity_to_purple_map[index],
				    GUINT_TO_POINTER(index));
	}
}

static void sipe_purple_activity_shutdown(void)
{
	g_hash_table_destroy(purple_token_map);
}

const gchar *sipe_purple_activity_to_token(guint type)
{
	return(activity_to_purple_map[type]);
}

guint sipe_purple_token_to_activity(const gchar *token)
{
	return(GPOINTER_TO_UINT(g_hash_table_lookup(purple_token_map, token)));
}

gchar *sipe_backend_version(void)
{
	return(g_strdup_printf("Purple/%s", purple_core_get_version()));
}

/* PurplePluginProtocolInfo function calls & data structure */
static const char *sipe_list_icon(SIPE_UNUSED_PARAMETER PurpleAccount *a,
				  SIPE_UNUSED_PARAMETER PurpleBuddy *b)
{
	return "sipe";
}

static gchar *sipe_purple_status_text(PurpleBuddy *buddy)
{
	const PurpleStatus *status = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
	return sipe_core_buddy_status(PURPLE_BUDDY_TO_SIPE_CORE_PUBLIC,
				      buddy->name,
				      sipe_purple_token_to_activity(purple_status_get_id(status)),
				      purple_status_get_name(status));
}

static void sipe_purple_tooltip_text(PurpleBuddy *buddy,
				     PurpleNotifyUserInfo *user_info,
				     SIPE_UNUSED_PARAMETER gboolean full)
{
	const PurplePresence *presence = purple_buddy_get_presence(buddy);
	sipe_core_buddy_tooltip_info(PURPLE_BUDDY_TO_SIPE_CORE_PUBLIC,
				     buddy->name,
				     purple_status_get_name(purple_presence_get_active_status(presence)),
				     purple_presence_is_online(presence),
				     (struct sipe_backend_buddy_tooltip *) user_info);
}

static GList *sipe_purple_status_types(SIPE_UNUSED_PARAMETER PurpleAccount *acc)
{
	PurpleStatusType *type;
	GList *types = NULL;

	/* Macros to reduce code repetition.
	   Translators: noun */
#define SIPE_ADD_STATUS(prim,id,name,user) type = purple_status_type_new_with_attrs( \
		prim, id, name,             \
		TRUE, user, FALSE,          \
		SIPE_PURPLE_STATUS_ATTR_ID_MESSAGE, _("Message"), purple_value_new(PURPLE_TYPE_STRING), \
		NULL);                      \
	types = g_list_append(types, type);

	/* Online */
	SIPE_ADD_STATUS(PURPLE_STATUS_AVAILABLE,
			NULL,
			NULL,
			TRUE);

	/* Busy */
	SIPE_ADD_STATUS(PURPLE_STATUS_UNAVAILABLE,
			sipe_purple_activity_to_token(SIPE_ACTIVITY_BUSY),
			sipe_core_activity_description(SIPE_ACTIVITY_BUSY),
			TRUE);

	/* Do Not Disturb */
	SIPE_ADD_STATUS(PURPLE_STATUS_UNAVAILABLE,
			sipe_purple_activity_to_token(SIPE_ACTIVITY_DND),
			NULL,
			TRUE);

	/* Away */
	/* Goes first in the list as
	 * purple picks the first status with the AWAY type
	 * for idle.
	 */
	SIPE_ADD_STATUS(PURPLE_STATUS_AWAY,
			NULL,
			NULL,
			TRUE);

	/* Be Right Back */
	SIPE_ADD_STATUS(PURPLE_STATUS_AWAY,
			sipe_purple_activity_to_token(SIPE_ACTIVITY_BRB),
			sipe_core_activity_description(SIPE_ACTIVITY_BRB),
			TRUE);

	/* Appear Offline */
	SIPE_ADD_STATUS(PURPLE_STATUS_INVISIBLE,
			NULL,
			NULL,
			TRUE);

	/* Offline */
	type = purple_status_type_new(PURPLE_STATUS_OFFLINE,
				      NULL,
				      NULL,
				      TRUE);
	types = g_list_append(types, type);

	return types;
}

static GList *sipe_purple_blist_node_menu(PurpleBlistNode *node)
{
	if(PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		return sipe_purple_buddy_menu((PurpleBuddy *) node);
	} else if(PURPLE_BLIST_NODE_IS_CHAT(node)) {
		return sipe_purple_chat_menu((PurpleChat *)node);
	} else {
		return NULL;
	}
}

static void sipe_purple_login(PurpleAccount *account)
{
	PurpleConnection *gc   = purple_account_get_connection(account);
	const gchar *username  = purple_account_get_username(account);
	const gchar *email     = purple_account_get_string(account, "email", NULL);
	const gchar *email_url = purple_account_get_string(account, "email_url", NULL);
	const gchar *transport = purple_account_get_string(account, "transport", "auto");
	const gchar *auth      = purple_account_get_string(account, "authentication", "ntlm");
	struct sipe_core_public *sipe_public;
	gchar **username_split;
	gchar *login_domain = NULL;
	gchar *login_account = NULL;
	const gchar *errmsg;
	guint type;
	struct sipe_backend_private *purple_private;

	/* username format: <username>,[<optional login>] */
	SIPE_DEBUG_INFO("sipe_purple_login: username '%s'", username);
	username_split = g_strsplit(username, ",", 2);

	/* login name specified? */
	if (username_split[1] && strlen(username_split[1])) {
		/* Allowed domain-account separators are / or \ */
		gchar **domain_user = g_strsplit_set(username_split[1], "/\\", 2);
		gboolean has_domain = domain_user[1] != NULL;
		SIPE_DEBUG_INFO("sipe_purple_login: login '%s'", username_split[1]);
		login_domain  = has_domain ? g_strdup(domain_user[0]) : NULL;
		login_account = g_strdup(domain_user[has_domain ? 1 : 0]);
		SIPE_DEBUG_INFO("sipe_purple_login: auth domain '%s' user '%s'",
				login_domain ? login_domain : "",
				login_account);
		g_strfreev(domain_user);
	}

	sipe_public = sipe_core_allocate(username_split[0],
					 login_domain, login_account,
					 purple_connection_get_password(gc),
					 email,
					 email_url,
					 &errmsg);
	g_free(login_domain);
	g_free(login_account);
	g_strfreev(username_split);

	if (!sipe_public) {
#if PURPLE_VERSION_CHECK(3,0,0)
		purple_connection_error(
#else
		purple_connection_error_reason(
#endif
					       gc,
					       PURPLE_CONNECTION_ERROR_INVALID_USERNAME,
					       errmsg);
		return;
	}

	sipe_public->backend_private = purple_private = g_new0(struct sipe_backend_private, 1);
	purple_private->public = sipe_public;
	purple_private->gc = gc;
	purple_private->account = account;

	sipe_purple_chat_setup_rejoin(purple_private);

	/* map option list to flags - default is NTLM */
	SIPE_CORE_FLAG_UNSET(KRB5);
	SIPE_CORE_FLAG_UNSET(TLS_DSK);
#if defined(HAVE_LIBKRB5) || defined(HAVE_SSPI)
	if (sipe_strequal(auth, "krb5")) {
		SIPE_CORE_FLAG_SET(KRB5);
	} else
#endif
#ifndef HAVE_SSPI
	/*
	 * @TODO: SSL handshake support isn't implemented in sip-sec-sspi.c.
	 *        So ignore configuration setting for now.
	 */
	if (sipe_strequal(auth, "tls-dsk")) {
		SIPE_CORE_FLAG_SET(TLS_DSK);
	}
#endif

	/* @TODO: is this correct?
	   "sso" is only available when Kerberos/SSPI support is compiled in */
	if (purple_account_get_bool(account, "sso", TRUE))
		SIPE_CORE_FLAG_SET(SSO);

	gc->proto_data = sipe_public;
	gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_FORMATTING_WBFO | PURPLE_CONNECTION_NO_BGCOLOR |
		PURPLE_CONNECTION_NO_FONTSIZE | PURPLE_CONNECTION_NO_URLDESC | PURPLE_CONNECTION_ALLOW_CUSTOM_SMILEY;
	purple_connection_set_display_name(gc, sipe_public->sip_name);
	purple_connection_update_progress(gc, _("Connecting"), 1, 2);

	username_split = g_strsplit(purple_account_get_string(account, "server", ""), ":", 2);
	if (sipe_strequal(transport, "auto")) {
		type = (username_split[0] == NULL) ?
			SIPE_TRANSPORT_AUTO : SIPE_TRANSPORT_TLS;
	} else if (sipe_strequal(transport, "tls")) {
		type = SIPE_TRANSPORT_TLS;
	} else {
		type = SIPE_TRANSPORT_TCP;
	}
	sipe_core_transport_sip_connect(sipe_public,
					type,
					username_split[0],
					username_split[0] ? username_split[1] : NULL);
	g_strfreev(username_split);
}

static void sipe_purple_close(PurpleConnection *gc)
{
	struct sipe_core_public *sipe_public = PURPLE_GC_TO_SIPE_CORE_PUBLIC;

	if (sipe_public) {
		struct sipe_backend_private *purple_private = sipe_public->backend_private;

		sipe_core_deallocate(sipe_public);

		if (purple_private->roomlist_map)
			g_hash_table_destroy(purple_private->roomlist_map);
		sipe_purple_chat_destroy_rejoin(purple_private);
		g_free(purple_private);
		gc->proto_data = NULL;
	}
}

static int sipe_purple_send_im(PurpleConnection *gc,
			       const char *who,
			       const char *what,
			       SIPE_UNUSED_PARAMETER PurpleMessageFlags flags)
{
	sipe_core_im_send(PURPLE_GC_TO_SIPE_CORE_PUBLIC, who, what);
	return 1;
}

#define SIPE_TYPING_SEND_TIMEOUT 4

static unsigned int sipe_purple_send_typing(PurpleConnection *gc,
					    const char *who,
					    PurpleTypingState state)
{
	if (state == PURPLE_NOT_TYPING)
		return 0;

	sipe_core_user_feedback_typing(PURPLE_GC_TO_SIPE_CORE_PUBLIC,
				       who);

	return SIPE_TYPING_SEND_TIMEOUT;
}

static void sipe_purple_get_info(PurpleConnection *gc, const char *who)
{
	sipe_core_buddy_get_info(PURPLE_GC_TO_SIPE_CORE_PUBLIC,
				 who);
}

static void sipe_purple_add_permit(PurpleConnection *gc, const char *name)
{
	sipe_core_contact_allow_deny(PURPLE_GC_TO_SIPE_CORE_PUBLIC, name, TRUE);
}

static void sipe_purple_add_deny(PurpleConnection *gc, const char *name)
{
	sipe_core_contact_allow_deny(PURPLE_GC_TO_SIPE_CORE_PUBLIC, name, FALSE);
}

static void sipe_purple_keep_alive(PurpleConnection *gc)
{
	struct sipe_core_public *sipe_public = PURPLE_GC_TO_SIPE_CORE_PUBLIC;
	struct sipe_backend_private *purple_private = sipe_public->backend_private;
	time_t now = time(NULL);

	if ((sipe_public->keepalive_timeout > 0) &&
	    ((guint) (now - purple_private->last_keepalive) >= sipe_public->keepalive_timeout) &&
	    ((guint) (now - gc->last_received) >= sipe_public->keepalive_timeout)
		) {
		sipe_core_transport_sip_keepalive(sipe_public);
		purple_private->last_keepalive = now;
	}
}

static void sipe_purple_alias_buddy(PurpleConnection *gc, const char *name,
				    SIPE_UNUSED_PARAMETER const char *alias)
{
	sipe_core_group_set_user(PURPLE_GC_TO_SIPE_CORE_PUBLIC, name);
}

static void sipe_purple_group_rename(PurpleConnection *gc,
				     const char *old_name,
				     PurpleGroup *group,
				     SIPE_UNUSED_PARAMETER GList *moved_buddies)
{
	sipe_core_group_rename(PURPLE_GC_TO_SIPE_CORE_PUBLIC, old_name, group->name);
}

static void sipe_purple_convo_closed(PurpleConnection *gc,
				     const char *who)
{
	sipe_core_im_close(PURPLE_GC_TO_SIPE_CORE_PUBLIC, who);
}

static void sipe_purple_group_remove(PurpleConnection *gc, PurpleGroup *group)
{
	sipe_core_group_remove(PURPLE_GC_TO_SIPE_CORE_PUBLIC, group->name);
}

#if PURPLE_VERSION_CHECK(2,5,0) || PURPLE_VERSION_CHECK(3,0,0)
static GHashTable *
sipe_purple_get_account_text_table(SIPE_UNUSED_PARAMETER PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("user@company.com"));
	return table;
}

#if PURPLE_VERSION_CHECK(2,6,0) || PURPLE_VERSION_CHECK(3,0,0)
#ifdef HAVE_VV

static void
sipe_purple_sigusr1_handler(SIPE_UNUSED_PARAMETER int signum)
{
	capture_pipeline("PURPLE_SIPE_PIPELINE");
}

static gboolean sipe_purple_initiate_media(PurpleAccount *account, const char *who,
					   SIPE_UNUSED_PARAMETER PurpleMediaSessionType type)
{
	sipe_core_media_initiate_call(PURPLE_ACCOUNT_TO_SIPE_CORE_PUBLIC,
				      who,
				      (type & PURPLE_MEDIA_VIDEO));
	return TRUE;
}

static PurpleMediaCaps sipe_purple_get_media_caps(SIPE_UNUSED_PARAMETER PurpleAccount *account,
						  SIPE_UNUSED_PARAMETER const char *who)
{
	return   PURPLE_MEDIA_CAPS_AUDIO
	       | PURPLE_MEDIA_CAPS_AUDIO_VIDEO
	       | PURPLE_MEDIA_CAPS_MODIFY_SESSION;
}
#endif
#endif
#endif

/*
 * Simplistic source upward compatibility path for newer libpurple APIs
 *
 * Usually we compile with -Werror=missing-field-initializers if GCC supports
 * it. But that means that the compilation of this structure can fail if the
 * newer API has added additional plugin callbacks. For the benefit of the
 * user we downgrade it to a warning here.
 *
 * Diagnostic #pragma was added in GCC 4.2.0
 * Diagnostic push/pop was added in GCC 4.6.0
 */
#ifdef __GNUC__
#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 2)
#if __GNUC_MINOR__ >= 6
#pragma GCC diagnostic push
#endif
#pragma GCC diagnostic warning "-Wmissing-field-initializers"
#endif
#endif
static PurplePluginProtocolInfo sipe_prpl_info =
{
#if PURPLE_VERSION_CHECK(3,0,0)
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
#endif
	OPT_PROTO_CHAT_TOPIC,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,				/* icon_spec */
	sipe_list_icon,				/* list_icon */
	NULL,					/* list_emblems */
	sipe_purple_status_text,		/* status_text */
	sipe_purple_tooltip_text,		/* tooltip_text */	// add custom info to contact tooltip
	sipe_purple_status_types,		/* away_states */
	sipe_purple_blist_node_menu,		/* blist_node_menu */
	sipe_purple_chat_info,			/* chat_info */
	sipe_purple_chat_info_defaults,		/* chat_info_defaults */
	sipe_purple_login,			/* login */
	sipe_purple_close,			/* close */
	sipe_purple_send_im,			/* send_im */
	NULL,					/* set_info */		// TODO maybe
	sipe_purple_send_typing,		/* send_typing */
	sipe_purple_get_info,			/* get_info */
	sipe_purple_set_status,			/* set_status */
	sipe_purple_set_idle,			/* set_idle */
	NULL,					/* change_passwd */
	sipe_purple_add_buddy,			/* add_buddy */
	NULL,					/* add_buddies */
	sipe_purple_remove_buddy,		/* remove_buddy */
	NULL,					/* remove_buddies */
	sipe_purple_add_permit,			/* add_permit */
	sipe_purple_add_deny,			/* add_deny */
	sipe_purple_add_deny,			/* rem_permit */
	sipe_purple_add_permit,			/* rem_deny */
	NULL,					/* set_permit_deny */
	sipe_purple_chat_join,			/* join_chat */
	NULL,					/* reject_chat */
	NULL,					/* get_chat_name */
	sipe_purple_chat_invite,		/* chat_invite */
	sipe_purple_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	sipe_purple_chat_send,			/* chat_send */
	sipe_purple_keep_alive,			/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */	// deprecated
#if !PURPLE_VERSION_CHECK(3,0,0)
	NULL,					/* get_cb_away */	// deprecated
#endif
	sipe_purple_alias_buddy,		/* alias_buddy */
	sipe_purple_group_buddy,		/* group_buddy */
	sipe_purple_group_rename,		/* rename_group */
	NULL,					/* buddy_free */
	sipe_purple_convo_closed,		/* convo_closed */
	purple_normalize_nocase,		/* normalize */
	NULL,					/* set_buddy_icon */
	sipe_purple_group_remove,		/* remove_group */
	NULL,					/* get_cb_real_name */	// TODO?
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	sipe_purple_roomlist_get_list,		/* roomlist_get_list */
	sipe_purple_roomlist_cancel,		/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	sipe_purple_ft_send_file,		/* send_file */
	sipe_purple_ft_new_xfer,		/* new_xfer */
	NULL,					/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
	NULL,					/* send_raw */
	NULL,					/* roomlist_room_serialize */
	NULL,					/* unregister_user */
	NULL,					/* send_attention */
	NULL,					/* get_attention_types */
#if !PURPLE_VERSION_CHECK(2,5,0) && !PURPLE_VERSION_CHECK(3,0,0)
	/* Backward compatibility when compiling against 2.4.x API */
	(void (*)(void))			/* _purple_reserved4 */
#endif
#if !PURPLE_VERSION_CHECK(3,0,0)
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
#endif
#if PURPLE_VERSION_CHECK(2,5,0) || PURPLE_VERSION_CHECK(3,0,0)
	sipe_purple_get_account_text_table,	/* get_account_text_table */
#if PURPLE_VERSION_CHECK(2,6,0) || PURPLE_VERSION_CHECK(3,0,0)
#ifdef HAVE_VV
	sipe_purple_initiate_media,		/* initiate_media */
	sipe_purple_get_media_caps,		/* get_media_caps */
#else
	NULL,					/* initiate_media */
	NULL,					/* get_media_caps */
#endif
#if PURPLE_VERSION_CHECK(2,7,0) || PURPLE_VERSION_CHECK(3,0,0)
	NULL,					/* get_moods */
	NULL,					/* set_public_alias */
	NULL,					/* get_public_alias */
#if PURPLE_VERSION_CHECK(2,8,0)
	NULL,					/* add_buddy_with_invite */
	NULL,					/* add_buddies_with_invite */
#endif
#endif
#endif
#endif
};
#ifdef __GNUC__
#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif
#endif
/* Original GCC error checking restored from here on... (see above) */

/* PurplePluginInfo function calls & data structure */
static gboolean sipe_purple_plugin_load(SIPE_UNUSED_PARAMETER PurplePlugin *plugin)
{
#ifdef HAVE_VV
	struct sigaction action;
	memset(&action, 0, sizeof (action));
	action.sa_handler = sipe_purple_sigusr1_handler;
	sigaction(SIGUSR1, &action, NULL);
#endif
	return TRUE;
}

static gboolean sipe_purple_plugin_unload(SIPE_UNUSED_PARAMETER PurplePlugin *plugin)
{
#ifdef HAVE_VV
	struct sigaction action;
	memset(&action, 0, sizeof (action));
	action.sa_handler = SIG_DFL;
	sigaction(SIGUSR1, &action, NULL);
#endif
	return TRUE;
}

static void sipe_purple_plugin_destroy(SIPE_UNUSED_PARAMETER PurplePlugin *plugin)
{
	GList *entry;

	sipe_purple_activity_shutdown();
	sipe_core_destroy();

	entry = sipe_prpl_info.protocol_options;
	while (entry) {
		purple_account_option_destroy(entry->data);
		entry = g_list_delete_link(entry, entry);
	}
	sipe_prpl_info.protocol_options = NULL;

	entry = sipe_prpl_info.user_splits;
	while (entry) {
		purple_account_user_split_destroy(entry->data);
		entry = g_list_delete_link(entry, entry);
	}
	sipe_prpl_info.user_splits = NULL;
}

static void sipe_purple_show_about_plugin(PurplePluginAction *action)
{
	gchar *tmp = sipe_core_about();
	purple_notify_formatted((PurpleConnection *) action->context,
				NULL, " ", NULL, tmp, NULL, NULL);
	g_free(tmp);
}

static void sipe_purple_find_contact_cb(PurpleConnection *gc,
					PurpleRequestFields *fields)
{
	GList *entries = purple_request_field_group_get_fields(purple_request_fields_get_groups(fields)->data);
	const gchar *given_name = NULL;
	const gchar *surname    = NULL;
	const gchar *email      = NULL;
	const gchar *company    = NULL;
	const gchar *country    = NULL;

	while (entries) {
		PurpleRequestField *field = entries->data;
		const char *id = purple_request_field_get_id(field);
		const char *value = purple_request_field_string_get_value(field);

		SIPE_DEBUG_INFO("sipe_purple_find_contact_cb: %s = '%s'", id, value ? value : "");

		if (value) {
			if (strcmp(id, "given") == 0) {
				given_name = value;
			} else if (strcmp(id, "surname") == 0) {
				surname = value;
			} else if (strcmp(id, "email") == 0) {
				email = value;
			} else if (strcmp(id, "company") == 0) {
				company = value;
			} else if (strcmp(id, "country") == 0) {
				country = value;
			}
		}

		entries = g_list_next(entries);
	};

	sipe_core_buddy_search(PURPLE_GC_TO_SIPE_CORE_PUBLIC,
			       given_name,
			       surname,
			       email,
			       company,
			       country);
}

static void sipe_purple_show_find_contact(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("given", _("First name"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("surname", _("Last name"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("email", _("Email"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("company", _("Company"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);
	field = purple_request_field_string_new("country", _("Country"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(gc,
		_("Search"),
		_("Search for a contact"),
		_("Enter the information for the person you wish to find. Empty fields will be ignored."),
		fields,
		_("_Search"), G_CALLBACK(sipe_purple_find_contact_cb),
		_("_Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL, gc);
}

static void sipe_purple_join_conference_cb(PurpleConnection *gc,
					   PurpleRequestFields *fields)
{
	GList *entries = purple_request_field_group_get_fields(purple_request_fields_get_groups(fields)->data);

	if (entries) {
		PurpleRequestField *field = entries->data;
		const char *id = purple_request_field_get_id(field);
		const char *value = purple_request_field_string_get_value(field);

		if (!sipe_strequal(id, "meetingLocation"))
			return;

		sipe_core_conf_create(PURPLE_GC_TO_SIPE_CORE_PUBLIC, value);
	}
}

static void sipe_purple_show_join_conference(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("meetingLocation", _("Meeting location"), NULL, FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(gc,
		_("Join conference"),
		_("Join scheduled conference"),
		_("Enter meeting location string you received in the invitation.\n"
		  "\n"
		  "Valid location will be something like\n"
		  "meet:sip:someone@company.com;gruu;opaque=app:conf:focus:id:abcdef1234"),
		fields,
		_("_Join"), G_CALLBACK(sipe_purple_join_conference_cb),
		_("_Cancel"), NULL,
		purple_connection_get_account(gc), NULL, NULL, gc);
}

static void sipe_purple_republish_calendar(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	sipe_core_update_calendar(PURPLE_GC_TO_SIPE_CORE_PUBLIC);
}

static void sipe_purple_reset_status(PurplePluginAction *action)
{
	PurpleConnection *gc = (PurpleConnection *) action->context;
	sipe_core_reset_status(PURPLE_GC_TO_SIPE_CORE_PUBLIC);
}

static GList *sipe_purple_actions(SIPE_UNUSED_PARAMETER PurplePlugin *plugin,
				  SIPE_UNUSED_PARAMETER gpointer context)
{
	GList *menu = NULL;
	PurplePluginAction *act;

	act = purple_plugin_action_new(_("About SIPE plugin..."), sipe_purple_show_about_plugin);
	menu = g_list_prepend(menu, act);

	act = purple_plugin_action_new(_("Contact search..."), sipe_purple_show_find_contact);
	menu = g_list_prepend(menu, act);

	act = purple_plugin_action_new(_("Join scheduled conference..."), sipe_purple_show_join_conference);
	menu = g_list_prepend(menu, act);

	act = purple_plugin_action_new(_("Republish Calendar"), sipe_purple_republish_calendar);
	menu = g_list_prepend(menu, act);

	act = purple_plugin_action_new(_("Reset status"), sipe_purple_reset_status);
	menu = g_list_prepend(menu, act);

	return g_list_reverse(menu);
}

static PurplePluginInfo sipe_purple_info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                           /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                          /**< priority       */
	"prpl-sipe",                                   	  /**< id             */
	"Office Communicator",                            /**< name           */
	PACKAGE_VERSION,                                  /**< version        */
	"Microsoft Office Communicator Protocol Plugin",  /**< summary        */
	"A plugin for the extended SIP/SIMPLE protocol used by "          /**< description */
	"Microsoft Live/Office Communications Server (LCS2005/OCS2007+)", /**< description */
	"Anibal Avelar <avelar@gmail.com>, "              /**< author         */
	"Gabriel Burt <gburt@novell.com>, "               /**< author         */
	"Stefan Becker <stefan.becker@nokia.com>, "       /**< author         */
	"pier11 <pier11@operamail.com>",                  /**< author         */
	PACKAGE_URL,                                      /**< homepage       */
	sipe_purple_plugin_load,                          /**< load           */
	sipe_purple_plugin_unload,                        /**< unload         */
	sipe_purple_plugin_destroy,                       /**< destroy        */
	NULL,                                             /**< ui_info        */
	&sipe_prpl_info,                                  /**< extra_info     */
	NULL,
	sipe_purple_actions,
	NULL,
	NULL,
	NULL,
	NULL
};

static void sipe_purple_init_plugin(PurplePlugin *plugin)
{
	PurpleAccountUserSplit *split;
	PurpleAccountOption *option;

	/* This needs to be called first */
	sipe_core_init(LOCALEDIR);
	sipe_purple_activity_init();

	purple_plugin_register(plugin);

        /**
	 * When adding new string settings please make sure to keep these
	 * in sync:
	 *
	 *     api/sipe-backend.h
	 *     purple-settings.c:setting_name[]
	 */
	split = purple_account_user_split_new(_("Login\n   user  or  DOMAIN\\user  or\n   user@company.com"), NULL, ',');
	purple_account_user_split_set_reverse(split, FALSE);
	sipe_prpl_info.user_splits = g_list_append(sipe_prpl_info.user_splits, split);

	option = purple_account_option_string_new(_("Server[:Port]\n(leave empty for auto-discovery)"), "server", "");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	option = purple_account_option_list_new(_("Connection type"), "transport", NULL);
	purple_account_option_add_list_item(option, _("Auto"), "auto");
	purple_account_option_add_list_item(option, _("SSL/TLS"), "tls");
	purple_account_option_add_list_item(option, _("TCP"), "tcp");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	/*option = purple_account_option_bool_new(_("Publish status (note: everyone may watch you)"), "doservice", TRUE);
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);*/

	option = purple_account_option_string_new(_("User Agent"), "useragent", "");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	option = purple_account_option_list_new(_("Authentication scheme"), "authentication", NULL);
	purple_account_option_add_list_item(option, _("NTLM"), "ntlm");
#if defined(HAVE_LIBKRB5) || defined(HAVE_SSPI)
	purple_account_option_add_list_item(option, _("Kerberos"), "krb5");
#endif
#ifndef HAVE_SSPI
	/* see above */
	purple_account_option_add_list_item(option, _("TLS-DSK"), "tls-dsk");
#endif
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

#if defined(HAVE_LIBKRB5) || defined(HAVE_SSPI)
	/* Suitable for sspi/NTLM, sspi/Kerberos and krb5 security mechanisms
	 * No login/password is taken into account if this option present,
	 * instead used default credentials stored in OS.
	 */
	option = purple_account_option_bool_new(_("Use Single Sign-On"), "sso", TRUE);
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);
#endif

	/** Example (Exchange): https://server.company.com/EWS/Exchange.asmx
	 *  Example (Domino)  : https://[domino_server]/[mail_database_name].nsf
	 */
	option = purple_account_option_string_new(_("Email services URL\n(leave empty for auto-discovery)"), "email_url", "");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Email address\n(if different from Username)"), "email", "");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	/** Example (Exchange): DOMAIN\user  or  user@company.com
	 *  Example (Domino)  : email_address
	 */
	option = purple_account_option_string_new(_("Email login\n(if different from Login)"), "email_login", "");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	option = purple_account_option_string_new(_("Email password\n(if different from Password)"), "email_password", "");
	purple_account_option_set_masked(option, TRUE);
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);

	/** Example (federated domain): company.com      (i.e. ocschat@company.com)
	 *  Example (non-default user): user@company.com
	 */
	option = purple_account_option_string_new(_("Group Chat Proxy\n   company.com  or  user@company.com\n(leave empty to determine from Username)"), "groupchat_user", "");
	sipe_prpl_info.protocol_options = g_list_append(sipe_prpl_info.protocol_options, option);
}

/* This macro makes the code a purple plugin */
PURPLE_INIT_PLUGIN(sipe, sipe_purple_init_plugin, sipe_purple_info);

/*
  Local Variables:
  mode: c
  c-file-style: "bsd"
  indent-tabs-mode: t
  tab-width: 8
  End:
*/
