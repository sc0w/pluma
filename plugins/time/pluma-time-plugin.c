/*
 * pluma-time-plugin.c
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

/*
 * Modified by the pluma Team, 2002. See the AUTHORS file for a
 * list of people on the pluma Team.
 * See the ChangeLog files for a list of changes.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

#include "pluma-time-plugin.h"
#include <pluma/pluma-help.h>

#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <libpeas/peas-activatable.h>
#include <libpeas-gtk/peas-gtk-configurable.h>

#include <pluma/pluma-window.h>
#include <pluma/pluma-debug.h>
#include <pluma/pluma-utils.h>

#define PLUMA_TIME_PLUGIN_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), \
					      PLUMA_TYPE_TIME_PLUGIN, \
					      PlumaTimePluginPrivate))

#define MENU_PATH "/MenuBar/EditMenu/EditOps_4"

/* GSettings keys */
#define TIME_SCHEMA			"org.mate.pluma.plugins.time"
#define PROMPT_TYPE_KEY		"prompt-type"
#define SELECTED_FORMAT_KEY	"selected-format"
#define CUSTOM_FORMAT_KEY	"custom-format"

#define DEFAULT_CUSTOM_FORMAT "%d/%m/%Y %H:%M:%S"

static const gchar *formats[] =
{
	"%c",
	"%x",
	"%X",
	"%x %X",
	"%Y-%m-%d %H:%M:%S",
	"%a %b %d %H:%M:%S %Z %Y",
	"%a %b %d %H:%M:%S %Y",
	"%a %d %b %Y %H:%M:%S %Z",
	"%a %d %b %Y %H:%M:%S",
	"%d/%m/%Y",
	"%d/%m/%y",
	"%D",
	"%A %d %B %Y",
	"%A %B %d %Y",
	"%Y-%m-%d",
	"%d %B %Y",
	"%B %d, %Y",
	"%A %b %d",
	"%H:%M:%S",
	"%H:%M",
	"%I:%M:%S %p",
	"%I:%M %p",
	"%H.%M.%S",
	"%H.%M",
	"%I.%M.%S %p",
	"%I.%M %p",
	"%d/%m/%Y %H:%M:%S",
	"%d/%m/%y %H:%M:%S",
#if __GLIBC__ >= 2
	"%a, %d %b %Y %H:%M:%S %z",
#endif
	NULL
};

enum
{
	COLUMN_FORMATS = 0,
	COLUMN_INDEX,
	NUM_COLUMNS
};

typedef struct _TimeConfigureDialog TimeConfigureDialog;

struct _TimeConfigureDialog
{
	GtkWidget *content;

	GtkWidget *list;

        /* Radio buttons to indicate what should be done */
        GtkWidget *prompt;
        GtkWidget *use_list;
        GtkWidget *custom;

	GtkWidget *custom_entry;
	GtkWidget *custom_format_example;

	GSettings *settings;
};

typedef struct _ChooseFormatDialog ChooseFormatDialog;

struct _ChooseFormatDialog
{
	GtkWidget *dialog;

	GtkWidget *list;

        /* Radio buttons to indicate what should be done */
        GtkWidget *use_list;
        GtkWidget *custom;

        GtkWidget *custom_entry;
	GtkWidget *custom_format_example;

	/* Info needed for the response handler */
	GtkTextBuffer   *buffer;

	GSettings *settings;
};

typedef enum
{
	PROMPT_SELECTED_FORMAT = 0,	/* Popup dialog with list preselected */
	PROMPT_CUSTOM_FORMAT,		/* Popup dialog with entry preselected */
	USE_SELECTED_FORMAT,		/* Use selected format directly */
	USE_CUSTOM_FORMAT		/* Use custom format directly */
} PlumaTimePluginPromptType;

struct _PlumaTimePluginPrivate
{
	GtkWidget *window;

	GSettings *settings;

	GtkActionGroup *action_group;
	guint           ui_id;
};

enum {
	PROP_0,
	PROP_OBJECT
};

static void peas_activatable_iface_init (PeasActivatableInterface *iface);
static void peas_gtk_configurable_iface_init (PeasGtkConfigurableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (PlumaTimePlugin,
                                pluma_time_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_TYPE_ACTIVATABLE,
                                                               peas_activatable_iface_init)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (PEAS_GTK_TYPE_CONFIGURABLE,
                                                               peas_gtk_configurable_iface_init))

static void time_cb (GtkAction *action, PlumaTimePlugin *plugin);

static const GtkActionEntry action_entries[] =
{
	{
		"InsertDateAndTime",
		NULL,
		N_("In_sert Date and Time..."),
		NULL,
		N_("Insert current date and time at the cursor position"),
		G_CALLBACK (time_cb)
	},
};

static void
pluma_time_plugin_init (PlumaTimePlugin *plugin)
{
	pluma_debug_message (DEBUG_PLUGINS, "PlumaTimePlugin initializing");

	plugin->priv = PLUMA_TIME_PLUGIN_GET_PRIVATE (plugin);

	plugin->priv->settings = g_settings_new (TIME_SCHEMA);
}

static void
pluma_time_plugin_finalize (GObject *object)
{
	PlumaTimePlugin *plugin = PLUMA_TIME_PLUGIN (object);

	pluma_debug_message (DEBUG_PLUGINS, "PlumaTimePlugin finalizing");

	g_object_unref (G_OBJECT (plugin->priv->settings));

	G_OBJECT_CLASS (pluma_time_plugin_parent_class)->finalize (object);
}

static void
pluma_time_plugin_dispose (GObject *object)
{
	PlumaTimePlugin *plugin = PLUMA_TIME_PLUGIN (object);

	pluma_debug_message (DEBUG_PLUGINS, "PlumaTimePlugin disposing");

	if (plugin->priv->window != NULL)
	{
		g_object_unref (plugin->priv->window);
		plugin->priv->window = NULL;
	}

	if (plugin->priv->action_group)
	{
		g_object_unref (plugin->priv->action_group);
		plugin->priv->action_group = NULL;
	}

	G_OBJECT_CLASS (pluma_time_plugin_parent_class)->dispose (object);
}

static void
update_ui (PlumaTimePluginPrivate *data)
{
	PlumaWindow *window;
	PlumaView *view;
	GtkAction *action;

	pluma_debug (DEBUG_PLUGINS);

	window = PLUMA_WINDOW (data->window);
	view = pluma_window_get_active_view (window);

	pluma_debug_message (DEBUG_PLUGINS, "View: %p", view);

	action = gtk_action_group_get_action (data->action_group,
					      "InsertDateAndTime");
	gtk_action_set_sensitive (action,
				  (view != NULL) &&
				  gtk_text_view_get_editable (GTK_TEXT_VIEW (view)));
}

static void
pluma_time_plugin_activate (PeasActivatable *activatable)
{
	PlumaTimePlugin *plugin;
	PlumaTimePluginPrivate *data;
	PlumaWindow *window;
	GtkUIManager *manager;

	pluma_debug (DEBUG_PLUGINS);

	plugin = PLUMA_TIME_PLUGIN (activatable);
	data = plugin->priv;
	window = PLUMA_WINDOW (data->window);

	manager = pluma_window_get_ui_manager (window);

	data->action_group = gtk_action_group_new ("PlumaTimePluginActions");
	gtk_action_group_set_translation_domain (data->action_group,
						 GETTEXT_PACKAGE);
	gtk_action_group_add_actions (data->action_group,
				      	   action_entries,
				      	   G_N_ELEMENTS (action_entries),
				           plugin);

	gtk_ui_manager_insert_action_group (manager, data->action_group, -1);

	data->ui_id = gtk_ui_manager_new_merge_id (manager);

	gtk_ui_manager_add_ui (manager,
			       data->ui_id,
			       MENU_PATH,
			       "InsertDateAndTime",
			       "InsertDateAndTime",
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	update_ui (data);
}

static void
pluma_time_plugin_deactivate (PeasActivatable *activatable)
{
	PlumaTimePluginPrivate *data;
	PlumaWindow *window;
	GtkUIManager *manager;

	pluma_debug (DEBUG_PLUGINS);

	data = PLUMA_TIME_PLUGIN (activatable)->priv;
	window = PLUMA_WINDOW (data->window);

	manager = pluma_window_get_ui_manager (window);

	gtk_ui_manager_remove_ui (manager, data->ui_id);
	gtk_ui_manager_remove_action_group (manager, data->action_group);
}

static void
pluma_time_plugin_update_state (PeasActivatable *activatable)
{
	pluma_debug (DEBUG_PLUGINS);

	update_ui (PLUMA_TIME_PLUGIN (activatable)->priv);
}

/* whether we should prompt the user or use the specified format */
static PlumaTimePluginPromptType
get_prompt_type (PlumaTimePlugin *plugin)
{
	PlumaTimePluginPromptType prompt_type;

	prompt_type = g_settings_get_enum (plugin->priv->settings,
			        	       PROMPT_TYPE_KEY);

	return prompt_type;
}

static void
set_prompt_type (GSettings *settings,
		 PlumaTimePluginPromptType  prompt_type)
{
	if (!g_settings_is_writable (settings,
					   PROMPT_TYPE_KEY))
	{
		return;
	}

	g_settings_set_enum (settings,
				 PROMPT_TYPE_KEY,
				 prompt_type);
}

/* The selected format in the list */
static gchar *
get_selected_format (PlumaTimePlugin *plugin)
{
	gchar *sel_format;

	sel_format = g_settings_get_string (plugin->priv->settings,
					      SELECTED_FORMAT_KEY);

	return sel_format ? sel_format : g_strdup (formats [0]);
}

static void
set_selected_format (GSettings *settings,
		     const gchar     *format)
{
	g_return_if_fail (format != NULL);

	if (!g_settings_is_writable (settings,
					   SELECTED_FORMAT_KEY))
	{
		return;
	}

	g_settings_set_string (settings,
				 SELECTED_FORMAT_KEY,
		       		 format);
}

/* the custom format in the entry */
static gchar *
get_custom_format (PlumaTimePlugin *plugin)
{
	gchar *format;

	format = g_settings_get_string (plugin->priv->settings,
					  CUSTOM_FORMAT_KEY);

	return format ? format : g_strdup (DEFAULT_CUSTOM_FORMAT);
}

static void
set_custom_format (GSettings *settings,
		   const gchar     *format)
{
	g_return_if_fail (format != NULL);

	if (!g_settings_is_writable (settings,
					   CUSTOM_FORMAT_KEY))
		return;

	g_settings_set_string (settings,
				 CUSTOM_FORMAT_KEY,
		       		 format);
}

static gchar *
get_time (const gchar* format)
{
  	gchar *out = NULL;
	gchar *out_utf8 = NULL;
  	time_t clock;
  	struct tm *now;
  	size_t out_length = 0;
	gchar *locale_format;

	pluma_debug (DEBUG_PLUGINS);

	g_return_val_if_fail (format != NULL, NULL);

	if (strlen (format) == 0)
		return g_strdup (" ");

	locale_format = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	if (locale_format == NULL)
		return g_strdup (" ");

  	clock = time (NULL);
  	now = localtime (&clock);

	do
	{
		out_length += 255;
		out = g_realloc (out, out_length);
	}
  	while (strftime (out, out_length, locale_format, now) == 0);

	g_free (locale_format);

	if (g_utf8_validate (out, -1, NULL))
	{
		out_utf8 = out;
	}
	else
	{
		out_utf8 = g_locale_to_utf8 (out, -1, NULL, NULL, NULL);
		g_free (out);

		if (out_utf8 == NULL)
			out_utf8 = g_strdup (" ");
	}

  	return out_utf8;
}

static void
configure_dialog_destroyed (GtkWidget *widget,
                            gpointer   data)
{
	TimeConfigureDialog *dialog = (TimeConfigureDialog *) data;

	pluma_debug (DEBUG_PLUGINS);

	g_object_unref (dialog->settings);
	g_slice_free (TimeConfigureDialog, data);
}

static void
choose_format_dialog_destroyed (GtkWidget *widget,
                                gpointer   data)
{
	pluma_debug (DEBUG_PLUGINS);

	g_slice_free (ChooseFormatDialog, data);
}

static GtkTreeModel *
create_model (GtkWidget       *listview,
	      const gchar     *sel_format,
	      PlumaTimePlugin *plugin)
{
	gint i = 0;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	pluma_debug (DEBUG_PLUGINS);

	/* create list store */
	store = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

	/* Set tree view model*/
	gtk_tree_view_set_model (GTK_TREE_VIEW (listview),
				 GTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (listview));
	g_return_val_if_fail (selection != NULL, GTK_TREE_MODEL (store));

	/* there should always be one line selected */
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

	/* add data to the list store */
	while (formats[i] != NULL)
	{
		gchar *str;

		str = get_time (formats[i]);

		pluma_debug_message (DEBUG_PLUGINS, "%d : %s", i, str);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_FORMATS, str,
				    COLUMN_INDEX, i,
				    -1);
		g_free (str);

		if (sel_format && strcmp (formats[i], sel_format) == 0)
			gtk_tree_selection_select_iter (selection, &iter);

		++i;
	}

	/* fall back to select the first iter */
	if (!gtk_tree_selection_get_selected (selection, NULL, NULL))
	{
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	return GTK_TREE_MODEL (store);
}

static void
scroll_to_selected (GtkTreeView *tree_view)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	pluma_debug (DEBUG_PLUGINS);

	model = gtk_tree_view_get_model (tree_view);
	g_return_if_fail (model != NULL);

	/* Scroll to selected */
	selection = gtk_tree_view_get_selection (tree_view);
	g_return_if_fail (selection != NULL);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		GtkTreePath* path;

		path = gtk_tree_model_get_path (model, &iter);
		g_return_if_fail (path != NULL);

		gtk_tree_view_scroll_to_cell (tree_view,
					      path, NULL, TRUE, 1.0, 0.0);
		gtk_tree_path_free (path);
	}
}

static void
create_formats_list (GtkWidget       *listview,
		     const gchar     *sel_format,
		     PlumaTimePlugin *plugin)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;

	pluma_debug (DEBUG_PLUGINS);

	g_return_if_fail (listview != NULL);
	g_return_if_fail (sel_format != NULL);

	/* the Available formats column */
	cell = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
			_("Available formats"),
			cell,
			"text", COLUMN_FORMATS,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (listview), column);

	/* Create model, it also add model to the tree view */
	create_model (listview, sel_format, plugin);

	g_signal_connect (listview,
			  "realize",
			  G_CALLBACK (scroll_to_selected),
			  NULL);

	gtk_widget_show (listview);
}

static void
updated_custom_format_example (GtkEntry *format_entry,
			       GtkLabel *format_example)
{
	const gchar *format;
	gchar *time;
	gchar *str;
	gchar *escaped_time;

	pluma_debug (DEBUG_PLUGINS);

	g_return_if_fail (GTK_IS_ENTRY (format_entry));
	g_return_if_fail (GTK_IS_LABEL (format_example));

	format = gtk_entry_get_text (format_entry);

	time = get_time (format);
	escaped_time = g_markup_escape_text (time, -1);

	str = g_strdup_printf ("<span size=\"small\">%s</span>", escaped_time);

	gtk_label_set_markup (format_example, str);

	g_free (escaped_time);
	g_free (time);
	g_free (str);
}

static void
choose_format_dialog_button_toggled (GtkToggleButton *button,
				     ChooseFormatDialog *dialog)
{
	pluma_debug (DEBUG_PLUGINS);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom)))
	{
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);

		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
	{
		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		return;
	}
}

static void
configure_dialog_button_toggled (GtkToggleButton *button, TimeConfigureDialog *dialog)
{
	pluma_debug (DEBUG_PLUGINS);

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->custom)))
	{
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);

		set_prompt_type (dialog->settings, USE_CUSTOM_FORMAT);
		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
	{
		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		set_prompt_type (dialog->settings, USE_SELECTED_FORMAT);
		return;
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->prompt)))
	{
		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);

		set_prompt_type (dialog->settings, PROMPT_SELECTED_FORMAT);
		return;
	}
}

static gint
get_format_from_list (GtkWidget *listview)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	pluma_debug (DEBUG_PLUGINS);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (listview));
	g_return_val_if_fail (model != NULL, 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (listview));
	g_return_val_if_fail (selection != NULL, 0);

	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
	        gint selected_value;

		gtk_tree_model_get (model, &iter, COLUMN_INDEX, &selected_value, -1);

		pluma_debug_message (DEBUG_PLUGINS, "Sel value: %d", selected_value);

	        return selected_value;
	}

	g_return_val_if_reached (0);
}

static void
configure_dialog_selection_changed (GtkTreeSelection *selection,
                                    TimeConfigureDialog *dialog)
{
	gint sel_format;

	sel_format = get_format_from_list (dialog->list);
	set_selected_format (dialog->settings, formats[sel_format]);
}

static TimeConfigureDialog *
get_configure_dialog (PlumaTimePlugin *plugin)
{
	TimeConfigureDialog *dialog = NULL;
	GtkTreeSelection *selection;
	gchar *data_dir;
	gchar *ui_file;
	GtkWidget *viewport;
	PlumaTimePluginPromptType prompt_type;
	gchar *sf;
	GtkWidget *error_widget;
	gboolean ret;
	gchar *root_objects[] = {
		"time_dialog_content",
		NULL
	};

	pluma_debug (DEBUG_PLUGINS);

	dialog = g_slice_new (TimeConfigureDialog);
	dialog->settings = g_object_ref (plugin->priv->settings);

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "pluma-time-setup-dialog.ui", NULL);
	ret = pluma_utils_get_ui_objects (ui_file,
					  root_objects,
					  &error_widget,
					  "time_dialog_content", &dialog->content,
					  "formats_viewport", &viewport,
					  "formats_tree", &dialog->list,
					  "always_prompt", &dialog->prompt,
					  "never_prompt", &dialog->use_list,
					  "use_custom", &dialog->custom,
					  "custom_entry", &dialog->custom_entry,
					  "custom_format_example", &dialog->custom_format_example,
					  NULL);

	g_free (data_dir);
	g_free (ui_file);

	if (!ret)
	{
		return NULL;
	}

	sf = get_selected_format (plugin);
	create_formats_list (dialog->list, sf, plugin);
	g_free (sf);

	prompt_type = get_prompt_type (plugin);

	g_settings_bind (dialog->settings,
	                 CUSTOM_FORMAT_KEY,
	                 dialog->custom_entry,
	                 "text",
	                 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);

        if (prompt_type == USE_CUSTOM_FORMAT)
        {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->custom), TRUE);

		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);
        }
        else if (prompt_type == USE_SELECTED_FORMAT)
        {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_list), TRUE);

		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);
        }
        else
        {
	        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->prompt), TRUE);

		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);
        }

	updated_custom_format_example (GTK_ENTRY (dialog->custom_entry),
			GTK_LABEL (dialog->custom_format_example));

	/* setup a window of a sane size. */
	gtk_widget_set_size_request (GTK_WIDGET (viewport), 10, 200);

	g_signal_connect (dialog->custom,
			  "toggled",
			  G_CALLBACK (configure_dialog_button_toggled),
			  dialog);
   	g_signal_connect (dialog->prompt,
			  "toggled",
			  G_CALLBACK (configure_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->use_list,
			  "toggled",
			  G_CALLBACK (configure_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->content,
			  "destroy",
			  G_CALLBACK (configure_dialog_destroyed),
			  dialog);
	g_signal_connect (dialog->custom_entry,
			  "changed",
			  G_CALLBACK (updated_custom_format_example),
			  dialog->custom_format_example);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->list));
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (configure_dialog_selection_changed),
			  dialog);

	return dialog;
}

static void
real_insert_time (GtkTextBuffer *buffer,
		  const gchar   *the_time)
{
	pluma_debug_message (DEBUG_PLUGINS, "Insert: %s", the_time);

	gtk_text_buffer_begin_user_action (buffer);

	gtk_text_buffer_insert_at_cursor (buffer, the_time, -1);
	gtk_text_buffer_insert_at_cursor (buffer, " ", -1);

	gtk_text_buffer_end_user_action (buffer);
}

static void
choose_format_dialog_row_activated (GtkTreeView        *list,
				    GtkTreePath        *path,
				    GtkTreeViewColumn  *column,
				    ChooseFormatDialog *dialog)
{
	gint sel_format;
	gchar *the_time;

	sel_format = get_format_from_list (dialog->list);
	the_time = get_time (formats[sel_format]);

	set_prompt_type (dialog->settings, PROMPT_SELECTED_FORMAT);
	set_selected_format (dialog->settings, formats[sel_format]);

	g_return_if_fail (the_time != NULL);

	real_insert_time (dialog->buffer, the_time);

	g_free (the_time);
}

static ChooseFormatDialog *
get_choose_format_dialog (GtkWindow                 *parent,
			  PlumaTimePluginPromptType  prompt_type,
			  PlumaTimePlugin           *plugin)
{
	ChooseFormatDialog *dialog;
	gchar *data_dir;
	gchar *ui_file;
	GtkWidget *error_widget;
	gboolean ret;
	gchar *sf, *cf;
	GtkWindowGroup *wg = NULL;

	if (parent != NULL)
		wg = gtk_window_get_group (parent);

	dialog = g_slice_new (ChooseFormatDialog);
	dialog->settings = plugin->priv->settings;

	data_dir = peas_extension_base_get_data_dir (PEAS_EXTENSION_BASE (plugin));
	ui_file = g_build_filename (data_dir, "pluma-time-dialog.ui", NULL);
	ret = pluma_utils_get_ui_objects (ui_file,
					  NULL,
					  &error_widget,
					  "choose_format_dialog", &dialog->dialog,
					  "choice_list", &dialog->list,
					  "use_sel_format_radiobutton", &dialog->use_list,
					  "use_custom_radiobutton", &dialog->custom,
					  "custom_entry", &dialog->custom_entry,
					  "custom_format_example", &dialog->custom_format_example,
					  NULL);

	g_free (data_dir);
	g_free (ui_file);

	if (!ret)
	{
		GtkWidget *err_dialog;

		err_dialog = gtk_dialog_new_with_buttons (NULL,
			parent,
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			"gtk-ok", GTK_RESPONSE_ACCEPT,
			NULL);

		if (wg != NULL)
			gtk_window_group_add_window (wg, GTK_WINDOW (err_dialog));

		gtk_window_set_resizable (GTK_WINDOW (err_dialog), FALSE);
		gtk_dialog_set_default_response (GTK_DIALOG (err_dialog), GTK_RESPONSE_OK);

		gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (err_dialog))),
				   error_widget);

		g_signal_connect (G_OBJECT (err_dialog),
				  "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_widget_show_all (err_dialog);

		return NULL;
	}

	gtk_window_group_add_window (wg,
			     	     GTK_WINDOW (dialog->dialog));
	gtk_window_set_transient_for (GTK_WINDOW (dialog->dialog), parent);
	gtk_window_set_modal (GTK_WINDOW (dialog->dialog), TRUE);

	sf = get_selected_format (plugin);
	create_formats_list (dialog->list, sf, plugin);
	g_free (sf);

	cf = get_custom_format (plugin);
     	gtk_entry_set_text (GTK_ENTRY(dialog->custom_entry), cf);
	g_free (cf);

	updated_custom_format_example (GTK_ENTRY (dialog->custom_entry),
				       GTK_LABEL (dialog->custom_format_example));

	if (prompt_type == PROMPT_CUSTOM_FORMAT)
	{
        	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->custom), TRUE);

		gtk_widget_set_sensitive (dialog->list, FALSE);
		gtk_widget_set_sensitive (dialog->custom_entry, TRUE);
		gtk_widget_set_sensitive (dialog->custom_format_example, TRUE);
	}
	else if (prompt_type == PROMPT_SELECTED_FORMAT)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->use_list), TRUE);

		gtk_widget_set_sensitive (dialog->list, TRUE);
		gtk_widget_set_sensitive (dialog->custom_entry, FALSE);
		gtk_widget_set_sensitive (dialog->custom_format_example, FALSE);
	}
	else
	{
		g_return_val_if_reached (NULL);
	}

	/* setup a window of a sane size. */
	gtk_widget_set_size_request (dialog->list, 10, 200);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog->dialog),
					 GTK_RESPONSE_OK);

	g_signal_connect (dialog->custom,
			  "toggled",
			  G_CALLBACK (choose_format_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->use_list,
			  "toggled",
			  G_CALLBACK (choose_format_dialog_button_toggled),
			  dialog);
	g_signal_connect (dialog->dialog,
			  "destroy",
			  G_CALLBACK (choose_format_dialog_destroyed),
			  dialog);
	g_signal_connect (dialog->custom_entry,
			  "changed",
			  G_CALLBACK (updated_custom_format_example),
			  dialog->custom_format_example);
	g_signal_connect (dialog->list,
			  "row_activated",
			  G_CALLBACK (choose_format_dialog_row_activated),
			  dialog);

	gtk_window_set_resizable (GTK_WINDOW (dialog->dialog), FALSE);

	return dialog;
}

static void
choose_format_dialog_response_cb (GtkWidget          *widget,
				  gint                response,
				  ChooseFormatDialog *dialog)
{
	switch (response)
	{
		case GTK_RESPONSE_HELP:
		{
			pluma_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_HELP");
			pluma_help_display (GTK_WINDOW (widget),
					    NULL,
					    "pluma-insert-date-time-plugin");
			break;
		}
		case GTK_RESPONSE_OK:
		{
			gchar *the_time;

			pluma_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_OK");

			/* Get the user's chosen format */
			if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->use_list)))
			{
				gint sel_format;

				sel_format = get_format_from_list (dialog->list);
				the_time = get_time (formats[sel_format]);

				set_prompt_type (dialog->settings, PROMPT_SELECTED_FORMAT);
				set_selected_format (dialog->settings, formats[sel_format]);
			}
			else
			{
				const gchar *format;

				format = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));
				the_time = get_time (format);

				set_prompt_type (dialog->settings, PROMPT_CUSTOM_FORMAT);
				set_custom_format (dialog->settings, format);
			}

			g_return_if_fail (the_time != NULL);

			real_insert_time (dialog->buffer, the_time);
			g_free (the_time);

			gtk_widget_destroy (dialog->dialog);
			break;
		}
		case GTK_RESPONSE_CANCEL:
			pluma_debug_message (DEBUG_PLUGINS, "GTK_RESPONSE_CANCEL");
			gtk_widget_destroy (dialog->dialog);
	}
}

static void
time_cb (GtkAction  *action,
	 PlumaTimePlugin *plugin)
{
	PlumaWindow *window;
	GtkTextBuffer *buffer;
	gchar *the_time = NULL;
	PlumaTimePluginPromptType prompt_type;

	pluma_debug (DEBUG_PLUGINS);

	window = PLUMA_WINDOW (plugin->priv->window);
	buffer = GTK_TEXT_BUFFER (pluma_window_get_active_document (window));
	g_return_if_fail (buffer != NULL);

	prompt_type = get_prompt_type (plugin);

        if (prompt_type == USE_CUSTOM_FORMAT)
        {
		gchar *cf = get_custom_format (plugin);
	        the_time = get_time (cf);
		g_free (cf);
	}
        else if (prompt_type == USE_SELECTED_FORMAT)
        {
		gchar *sf = get_selected_format (plugin);
	        the_time = get_time (sf);
		g_free (sf);
	}
        else
        {
		ChooseFormatDialog *dialog;

		dialog = get_choose_format_dialog (GTK_WINDOW (window),
						   prompt_type,
						   plugin);
		if (dialog != NULL)
		{
			dialog->buffer = buffer;
			dialog->settings = plugin->priv->settings;

			g_signal_connect (dialog->dialog,
					  "response",
					  G_CALLBACK (choose_format_dialog_response_cb),
					  dialog);

			gtk_widget_show (GTK_WIDGET (dialog->dialog));
		}

		return;
	}

	g_return_if_fail (the_time != NULL);

	real_insert_time (buffer, the_time);

	g_free (the_time);
}

static GtkWidget *
pluma_time_plugin_create_configure_widget (PeasGtkConfigurable *configurable)
{
	TimeConfigureDialog *dialog;

	dialog = get_configure_dialog (PLUMA_TIME_PLUGIN (configurable));

	return dialog->content;
}

static void
pluma_time_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	PlumaTimePlugin *plugin = PLUMA_TIME_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			plugin->priv->window = GTK_WIDGET (g_value_dup_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
pluma_time_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	PlumaTimePlugin *plugin = PLUMA_TIME_PLUGIN (object);

	switch (prop_id)
	{
		case PROP_OBJECT:
			g_value_set_object (value, plugin->priv->window);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
pluma_time_plugin_class_init (PlumaTimePluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = pluma_time_plugin_finalize;
	object_class->dispose = pluma_time_plugin_dispose;
	object_class->set_property = pluma_time_plugin_set_property;
	object_class->get_property = pluma_time_plugin_get_property;

	g_object_class_override_property (object_class, PROP_OBJECT, "object");

	g_type_class_add_private (object_class, sizeof (PlumaTimePluginPrivate));
}

static void
pluma_time_plugin_class_finalize (PlumaTimePluginClass *klass)
{
	/* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

static void
peas_activatable_iface_init (PeasActivatableInterface *iface)
{
	iface->activate = pluma_time_plugin_activate;
	iface->deactivate = pluma_time_plugin_deactivate;
	iface->update_state = pluma_time_plugin_update_state;
}

static void
peas_gtk_configurable_iface_init (PeasGtkConfigurableInterface *iface)
{
	iface->create_configure_widget = pluma_time_plugin_create_configure_widget;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	pluma_time_plugin_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
	                                            PEAS_TYPE_ACTIVATABLE,
	                                            PLUMA_TYPE_TIME_PLUGIN);

	peas_object_module_register_extension_type (module,
	                                            PEAS_GTK_TYPE_CONFIGURABLE,
	                                            PLUMA_TYPE_TIME_PLUGIN);
}
