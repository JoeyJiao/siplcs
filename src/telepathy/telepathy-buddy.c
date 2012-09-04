/**
 * @file telepathy-buddy.c
 *
 * pidgin-sipe
 *
 * Copyright (C) 2012 SIPE Project <http://sipe.sourceforge.net/>
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

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/base-contact-list.h>
#include <telepathy-glib/telepathy-glib.h>

#include "sipe-backend.h"
#include "sipe-common.h"
#include "sipe-core.h"

#include "telepathy-private.h"

G_BEGIN_DECLS
/*
 * Contact List class - data structures
 */
typedef struct _SipeContactListClass {
	TpBaseContactListClass parent_class;
} SipeContactListClass;

typedef struct _SipeContactList {
	TpBaseContactList parent;

	TpBaseConnection *connection;
	TpHandleRepoIface *contact_repo;
	TpHandleSet *contacts;

	GHashTable *groups;

	gboolean initial_received;
} SipeContactList;

/*
 * Contact List class - type macros
 */
static GType sipe_contact_list_get_type(void) G_GNUC_CONST;
#define SIPE_TYPE_CONTACT_LIST \
	(sipe_contact_list_get_type())
#define SIPE_CONTACT_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), SIPE_TYPE_CONTACT_LIST, \
				    SipeContactList))
G_END_DECLS

/*
 * Contact List class - type definition
 */
static void contact_group_list_iface_init(TpContactGroupListInterface *);
G_DEFINE_TYPE_WITH_CODE(SipeContactList,
			sipe_contact_list,
			TP_TYPE_BASE_CONTACT_LIST,
			G_IMPLEMENT_INTERFACE (TP_TYPE_CONTACT_GROUP_LIST,
					       contact_group_list_iface_init);
)


/*
 * Contact List class - instance methods
 */
static TpHandleSet *dup_contacts(TpBaseContactList *contact_list)
{
	SipeContactList *self = SIPE_CONTACT_LIST(contact_list);
	return(tp_handle_set_copy(self->contacts));
}

static void dup_states(SIPE_UNUSED_PARAMETER TpBaseContactList *contact_list,
		       SIPE_UNUSED_PARAMETER TpHandle contact,
		       TpSubscriptionState *subscribe,
		       TpSubscriptionState *publish,
		       gchar **publish_request)
{
	/* @TODO */
	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::dup_states - NOT IMPLEMENTED");

	if (subscribe)
		*subscribe = TP_SUBSCRIPTION_STATE_YES;
	if (publish)
		*publish = TP_SUBSCRIPTION_STATE_YES;
	if (publish_request)
		*publish_request = g_strdup("");
}

static void sipe_contact_list_constructed(GObject *object)
{
	SipeContactList *self = SIPE_CONTACT_LIST(object);
	void (*chain_up)(GObject *) = G_OBJECT_CLASS(sipe_contact_list_parent_class)->constructed;

	if (chain_up)
		chain_up(object);

	g_object_get(self, "connection", &self->connection, NULL);
	self->contact_repo = tp_base_connection_get_handles(self->connection,
							    TP_HANDLE_TYPE_CONTACT);
	self->contacts     = tp_handle_set_new(self->contact_repo);
}

static void sipe_contact_list_dispose(GObject *object)
{
	SipeContactList *self = SIPE_CONTACT_LIST(object);
	void (*chain_up)(GObject *) = G_OBJECT_CLASS(sipe_contact_list_parent_class)->dispose;

	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::dispose");

	tp_clear_pointer(&self->contacts, tp_handle_set_destroy);
	tp_clear_object(&self->connection);
	tp_clear_pointer(&self->groups, g_hash_table_unref);

	if (chain_up)
		chain_up(object);
}

/*
 * Contact List class - type implementation
 */
static void sipe_contact_list_class_init(SipeContactListClass *klass)
{
	GObjectClass *object_class         = G_OBJECT_CLASS(klass);
	TpBaseContactListClass *base_class = TP_BASE_CONTACT_LIST_CLASS(klass);

	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::class_init");

	object_class->constructed = sipe_contact_list_constructed;
	object_class->dispose     = sipe_contact_list_dispose;

	base_class->dup_contacts = dup_contacts;
	base_class->dup_states   = dup_states;
}

static void sipe_contact_list_init(SIPE_UNUSED_PARAMETER SipeContactList *self)
{
	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::init");

	self->groups = g_hash_table_new_full(g_str_hash, g_str_equal,
					     g_free, NULL);

	self->initial_received = FALSE;
}

/*
 * Contact List class - interface implementation
 *
 * Contact groups
 */
static GStrv dup_groups(TpBaseContactList *contact_list)
{
	SipeContactList *self = SIPE_CONTACT_LIST(contact_list);
	GPtrArray *groups     = g_ptr_array_sized_new(
		g_hash_table_size(self->groups) + 1);
	GHashTableIter iter;
	gpointer name;

	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::dup_groups called");

	g_hash_table_iter_init(&iter, self->groups);
	while (g_hash_table_iter_next(&iter, &name, NULL))
		g_ptr_array_add(groups, g_strdup(name));
	g_ptr_array_add(groups, NULL);

	return((GStrv) g_ptr_array_free(groups, FALSE));
}

static TpHandleSet *dup_group_members(TpBaseContactList *contact_list,
				      SIPE_UNUSED_PARAMETER const gchar *group)
{
	SipeContactList *self = SIPE_CONTACT_LIST(contact_list);
	TpHandleSet *members  = tp_handle_set_new(self->contact_repo);

	/* @TODO */
	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::dup_group_members - NOT IMPLEMENTED");

	return(members);
}

static GStrv dup_contact_groups(TpBaseContactList *contact_list,
				SIPE_UNUSED_PARAMETER TpHandle contact)
{
	SipeContactList *self = SIPE_CONTACT_LIST(contact_list);
	GPtrArray *groups     = g_ptr_array_sized_new(
		g_hash_table_size(self->groups) + 1);

	/* @TODO */
	SIPE_DEBUG_INFO_NOFORMAT("SipeContactList::dup_contact_groups - NOT IMPLEMENTED");

	g_ptr_array_add(groups, NULL);

	return((GStrv) g_ptr_array_free(groups, FALSE));
}

static void contact_group_list_iface_init(TpContactGroupListInterface *iface)
{
	iface->dup_groups         = dup_groups;
	iface->dup_group_members  = dup_group_members;
	iface->dup_contact_groups = dup_contact_groups;
}

/* create new contact list object */
SipeContactList *sipe_telepathy_contact_list_new(TpBaseConnection *connection)
{
	return(g_object_new(SIPE_TYPE_CONTACT_LIST,
			    "connection", connection,
			    NULL));
}

/*
 * Backend adaptor functions
 */
void sipe_backend_buddy_list_processing_finish(struct sipe_core_public *sipe_public)
{
	struct sipe_backend_private *telepathy_private = sipe_public->backend_private;
	SipeContactList *contact_list                  = telepathy_private->contact_list;

	if (!contact_list->initial_received) {
		/* we can only call this once */
		contact_list->initial_received = TRUE;
		SIPE_DEBUG_INFO_NOFORMAT("sipe_backend_buddy_list_processing_finish called");
		tp_base_contact_list_set_list_received(TP_BASE_CONTACT_LIST(contact_list));
	}
}

gboolean sipe_backend_buddy_group_add(struct sipe_core_public *sipe_public,
				      const gchar *group_name)
{
	struct sipe_backend_private *telepathy_private = sipe_public->backend_private;
	SipeContactList *contact_list                  = telepathy_private->contact_list;
	gchar *group                                   = g_hash_table_lookup(contact_list->groups,
									     group_name);

	if (!group) {
		group = g_strdup(group_name);
		g_hash_table_insert(contact_list->groups, group, group);
		tp_base_contact_list_groups_created(TP_BASE_CONTACT_LIST(contact_list),
						    &group_name,
						    1);
	}

	return(group != NULL);
}


/*
  Local Variables:
  mode: c
  c-file-style: "bsd"
  indent-tabs-mode: t
  tab-width: 8
  End:
*/
