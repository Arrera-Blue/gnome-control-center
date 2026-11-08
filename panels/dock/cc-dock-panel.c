/* cc-dock-panel.c
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-dock-panel.h"
#include "cc-dock-resources.h"
#include <adwaita.h>
#include <glib/gi18n.h>

struct _CcDockPanel
{
  CcPanel             parent_instance;

  GSettings          *shell_settings;
  GSettings          *dock_settings;
  GSettings          *topbar_settings;
  GSettings          *appmenu_settings;

  AdwToastOverlay    *toast_overlay;
  AdwPreferencesPage *preferences_page;
};

CC_PANEL_REGISTER (CcDockPanel, cc_dock_panel)

typedef struct
{
  const gchar *uuid;
  gboolean     enable;
} ExtensionConfig;

/* Extensions par défaut (saveurs 1, 3, 4, 5) */
static const ExtensionConfig flavor_default_extensions[] = {
  { "dock@linux.arrera-software.fr", TRUE },
  { "top-bar@linux.arrera-software.fr", TRUE },
  { "app-menu@linux.arrera-software.fr", TRUE },
  { "org.gnome.shell.extensions.spot", FALSE },
  { "forge@jmmaranan.com", TRUE },
  { "GPaste@gnome-shell-extensions.gnome.org", TRUE },
  { "appindicatorsupport@rgcjonas.gmail.com", TRUE },
  { "dash-to-dock@micxgx.gmail.com", FALSE },
  { "nightthemeswitcher@romainvigier.fr", TRUE },
  { "restartto@tiagoporsch.github.io", FALSE },
  { "apps-menu@gnome-shell-extensions.gcampax.github.com", FALSE },
  { "background-logo@fedorahosted.org", TRUE },
  { "gsconnect@andyholmes.github.io", FALSE },
  { "launch-new-instance@gnome-shell-extensions.gcampax.github.com", FALSE },
  { "places-menu@gnome-shell-extensions.gcampax.github.com", FALSE },
  { "window-list@gnome-shell-extensions.gcampax.github.com", FALSE },
};

/* Saveur 2 : Menu d'application GNOME (sans app-menu Arrera) */
static const ExtensionConfig flavor_noappmenu_extensions[] = {
  { "dock@linux.arrera-software.fr", TRUE },
  { "top-bar@linux.arrera-software.fr", TRUE },
  { "app-menu@linux.arrera-software.fr", FALSE },
  { "org.gnome.shell.extensions.spot", FALSE },
  { "forge@jmmaranan.com", TRUE },
  { "GPaste@gnome-shell-extensions.gnome.org", TRUE },
  { "appindicatorsupport@rgcjonas.gmail.com", TRUE },
  { "dash-to-dock@micxgx.gmail.com", FALSE },
  { "nightthemeswitcher@romainvigier.fr", TRUE },
  { "restartto@tiagoporsch.github.io", FALSE },
  { "apps-menu@gnome-shell-extensions.gcampax.github.com", FALSE },
  { "background-logo@fedorahosted.org", TRUE },
  { "gsconnect@andyholmes.github.io", FALSE },
  { "launch-new-instance@gnome-shell-extensions.gcampax.github.com", FALSE },
  { "places-menu@gnome-shell-extensions.gcampax.github.com", FALSE },
  { "window-list@gnome-shell-extensions.gcampax.github.com", FALSE },
};

static void
set_bool_if_valid (GSettings *settings, const gchar *key, gboolean val)
{
  GSettingsSchema *schema = NULL;

  if (!settings)
    return;

  g_object_get (settings, "settings-schema", &schema, NULL);
  if (schema)
    {
      if (g_settings_schema_has_key (schema, key))
        g_settings_set_boolean (settings, key, val);
      g_settings_schema_unref (schema);
    }
}

static void
set_string_if_valid (GSettings *settings, const gchar *key, const gchar *val)
{
  GSettingsSchema *schema = NULL;

  if (!settings)
    return;

  g_object_get (settings, "settings-schema", &schema, NULL);
  if (schema)
    {
      if (g_settings_schema_has_key (schema, key))
        g_settings_set_string (settings, key, val);
      g_settings_schema_unref (schema);
    }
}

static void
apply_extensions_config (GSettings *shell_settings, const ExtensionConfig *configs, gsize n_configs)
{
  g_auto(GStrv) enabled_list = NULL;
  g_auto(GStrv) disabled_list = NULL;
  GHashTable *enabled_map;
  GHashTable *disabled_map;
  GPtrArray *new_enabled;
  GPtrArray *new_disabled;
  GHashTableIter iter;
  gpointer key;

  if (!shell_settings)
    return;

  enabled_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  disabled_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  enabled_list = g_settings_get_strv (shell_settings, "enabled-extensions");
  disabled_list = g_settings_get_strv (shell_settings, "disabled-extensions");

  for (guint i = 0; enabled_list && enabled_list[i]; i++)
    g_hash_table_add (enabled_map, g_strdup (enabled_list[i]));

  for (guint i = 0; disabled_list && disabled_list[i]; i++)
    g_hash_table_add (disabled_map, g_strdup (disabled_list[i]));

  for (gsize i = 0; i < n_configs; i++)
    {
      const gchar *uuid = configs[i].uuid;
      if (configs[i].enable)
        {
          g_hash_table_add (enabled_map, g_strdup (uuid));
          g_hash_table_remove (disabled_map, (gpointer) uuid);
        }
      else
        {
          g_hash_table_remove (enabled_map, (gpointer) uuid);
          g_hash_table_add (disabled_map, g_strdup (uuid));
        }
    }

  new_enabled = g_ptr_array_new_with_free_func (g_free);
  new_disabled = g_ptr_array_new_with_free_func (g_free);

  g_hash_table_iter_init (&iter, enabled_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_ptr_array_add (new_enabled, g_strdup ((const gchar *) key));
  g_ptr_array_add (new_enabled, NULL);

  g_hash_table_iter_init (&iter, disabled_map);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    g_ptr_array_add (new_disabled, g_strdup ((const gchar *) key));
  g_ptr_array_add (new_disabled, NULL);

  g_settings_set_strv (shell_settings, "enabled-extensions", (const gchar * const *) new_enabled->pdata);
  g_settings_set_strv (shell_settings, "disabled-extensions", (const gchar * const *) new_disabled->pdata);

  g_hash_table_destroy (enabled_map);
  g_hash_table_destroy (disabled_map);
  g_ptr_array_free (new_enabled, TRUE);
  g_ptr_array_free (new_disabled, TRUE);
}

/* Réglages par défaut (Saveurs 1 et 2) */
static void
apply_default_dock_settings (CcDockPanel *self)
{
  if (!self->dock_settings)
    return;

  set_bool_if_valid (self->dock_settings, "autohide", TRUE);
  set_bool_if_valid (self->dock_settings, "extend-on-maximize", TRUE);
  set_bool_if_valid (self->dock_settings, "always-bar-mode", FALSE);
  set_string_if_valid (self->dock_settings, "bar-icons-alignment", "center");
  set_string_if_valid (self->dock_settings, "icon-size", "medium");
  set_string_if_valid (self->dock_settings, "theme-mode", "expressive");
  set_string_if_valid (self->dock_settings, "position", "bottom");
  set_bool_if_valid (self->dock_settings, "show-quick-settings", FALSE);
  set_bool_if_valid (self->dock_settings, "show-date-menu", FALSE);
  set_bool_if_valid (self->dock_settings, "show-activities-button", FALSE);
  set_string_if_valid (self->dock_settings, "logo-color", "colored");
  set_string_if_valid (self->dock_settings, "clock-position", "top");
  set_string_if_valid (self->dock_settings, "date-format", "default");
  set_bool_if_valid (self->dock_settings, "dark-tiles", FALSE);
  set_bool_if_valid (self->dock_settings, "island-mode", FALSE);
  set_string_if_valid (self->dock_settings, "vertical-alignment", "top");
  set_bool_if_valid (self->dock_settings, "disable-overview-at-startup", FALSE);
}

static void
apply_default_topbar_settings (CcDockPanel *self)
{
  if (!self->topbar_settings)
    return;

  set_bool_if_valid (self->topbar_settings, "hide-top-bar", FALSE);
  set_string_if_valid (self->topbar_settings, "theme", "vanilla");
  set_string_if_valid (self->topbar_settings, "invisible-elements-color", "auto");
  set_string_if_valid (self->topbar_settings, "appindicator-position", "right");
  set_string_if_valid (self->topbar_settings, "logo-color", "colored");
  set_bool_if_valid (self->topbar_settings, "launch-custom-app", FALSE);
  set_string_if_valid (self->topbar_settings, "custom-app-id", "");
  set_bool_if_valid (self->topbar_settings, "keep-activities-button", FALSE);
}

static void
apply_default_appmenu_settings (CcDockPanel *self)
{
  if (!self->appmenu_settings)
    return;

  set_bool_if_valid (self->appmenu_settings, "super-key-opens-launcher", TRUE);
  set_bool_if_valid (self->appmenu_settings, "replace-gnome-app-menu", TRUE);
  set_bool_if_valid (self->appmenu_settings, "compact-mode", FALSE);
  set_string_if_valid (self->appmenu_settings, "theme-mode", "expressive");
  set_string_if_valid (self->appmenu_settings, "menu-position", "center");
}

/* Réglages Windows 10/11 (Saveurs 3 & 4) */
static void
apply_windows_dock_settings (CcDockPanel *self)
{
  if (!self->dock_settings)
    return;

  set_bool_if_valid (self->dock_settings, "autohide", FALSE);
  set_bool_if_valid (self->dock_settings, "extend-on-maximize", TRUE);
  set_bool_if_valid (self->dock_settings, "always-bar-mode", TRUE);
  set_string_if_valid (self->dock_settings, "bar-icons-alignment", "left");
  set_string_if_valid (self->dock_settings, "icon-size", "medium");
  set_string_if_valid (self->dock_settings, "theme-mode", "expressive");
  set_string_if_valid (self->dock_settings, "position", "bottom");
  set_bool_if_valid (self->dock_settings, "show-quick-settings", TRUE);
  set_bool_if_valid (self->dock_settings, "show-date-menu", TRUE);
  set_bool_if_valid (self->dock_settings, "show-activities-button", FALSE);
  set_string_if_valid (self->dock_settings, "logo-color", "colored");
  set_string_if_valid (self->dock_settings, "clock-position", "bottom");
  set_string_if_valid (self->dock_settings, "date-format", "default");
  set_bool_if_valid (self->dock_settings, "dark-tiles", FALSE);
  set_bool_if_valid (self->dock_settings, "island-mode", FALSE);
  set_string_if_valid (self->dock_settings, "vertical-alignment", "top");
  set_bool_if_valid (self->dock_settings, "disable-overview-at-startup", TRUE);
}

static void
apply_windows_topbar_settings (CcDockPanel *self)
{
  if (!self->topbar_settings)
    return;

  set_bool_if_valid (self->topbar_settings, "hide-top-bar", TRUE);
  set_string_if_valid (self->topbar_settings, "theme", "vanilla");
  set_string_if_valid (self->topbar_settings, "invisible-elements-color", "auto");
  set_string_if_valid (self->topbar_settings, "appindicator-position", "right");
  set_string_if_valid (self->topbar_settings, "logo-color", "colored");
  set_bool_if_valid (self->topbar_settings, "launch-custom-app", FALSE);
  set_string_if_valid (self->topbar_settings, "custom-app-id", "");
  set_bool_if_valid (self->topbar_settings, "keep-activities-button", FALSE);
}

static void
apply_windows_appmenu_settings (CcDockPanel *self)
{
  if (!self->appmenu_settings)
    return;

  set_bool_if_valid (self->appmenu_settings, "super-key-opens-launcher", TRUE);
  set_bool_if_valid (self->appmenu_settings, "replace-gnome-app-menu", TRUE);
  set_bool_if_valid (self->appmenu_settings, "compact-mode", TRUE);
  set_string_if_valid (self->appmenu_settings, "theme-mode", "expressive");
  set_string_if_valid (self->appmenu_settings, "menu-position", "bottom-left");
}

/* Réglages NeXTStep (Saveur 5) */
static void
apply_nextstep_dock_settings (CcDockPanel *self)
{
  if (!self->dock_settings)
    return;

  set_bool_if_valid (self->dock_settings, "autohide", FALSE);
  set_bool_if_valid (self->dock_settings, "extend-on-maximize", FALSE);
  set_bool_if_valid (self->dock_settings, "always-bar-mode", FALSE);
  set_string_if_valid (self->dock_settings, "bar-icons-alignment", "left");
  set_string_if_valid (self->dock_settings, "icon-size", "medium");
  set_string_if_valid (self->dock_settings, "theme-mode", "expressive");
  set_string_if_valid (self->dock_settings, "position", "right");
  set_bool_if_valid (self->dock_settings, "show-quick-settings", TRUE);
  set_bool_if_valid (self->dock_settings, "show-date-menu", TRUE);
  set_bool_if_valid (self->dock_settings, "show-activities-button", TRUE);
  set_string_if_valid (self->dock_settings, "logo-color", "colored");
  set_string_if_valid (self->dock_settings, "clock-position", "bottom");
  set_string_if_valid (self->dock_settings, "date-format", "default");
  set_bool_if_valid (self->dock_settings, "dark-tiles", TRUE);
  set_bool_if_valid (self->dock_settings, "island-mode", TRUE);
  set_string_if_valid (self->dock_settings, "vertical-alignment", "bottom");
  set_bool_if_valid (self->dock_settings, "disable-overview-at-startup", TRUE);
}

static void
on_apply_flavor_default_clicked (CcDockPanel *self)
{
  g_assert (CC_IS_DOCK_PANEL (self));

  /* 1. Extensions GNOME */
  apply_extensions_config (self->shell_settings,
                           flavor_default_extensions,
                           G_N_ELEMENTS (flavor_default_extensions));

  /* 2. Arrera Dock */
  apply_default_dock_settings (self);

  /* 3. Arrera Top Bar */
  apply_default_topbar_settings (self);

  /* 4. Arrera App Menu */
  apply_default_appmenu_settings (self);

  /* 5. Notification Toast */
  if (self->toast_overlay)
    {
      adw_toast_overlay_add_toast (self->toast_overlay,
                                   adw_toast_new (_("Saveur « Arrera par défaut » appliquée !")));
    }
}

static void
on_apply_flavor_noappmenu_clicked (CcDockPanel *self)
{
  g_assert (CC_IS_DOCK_PANEL (self));

  /* 1. Extensions GNOME */
  apply_extensions_config (self->shell_settings,
                           flavor_noappmenu_extensions,
                           G_N_ELEMENTS (flavor_noappmenu_extensions));

  /* 2. Arrera Dock */
  apply_default_dock_settings (self);

  /* 3. Arrera Top Bar */
  apply_default_topbar_settings (self);

  /* 4. Arrera App Menu */
  apply_default_appmenu_settings (self);

  /* 5. Notification Toast */
  if (self->toast_overlay)
    {
      adw_toast_overlay_add_toast (self->toast_overlay,
                                   adw_toast_new (_("Saveur « Menu d'application GNOME » appliquée !")));
    }
}

static void
on_apply_flavor_windows_clicked (CcDockPanel *self)
{
  g_assert (CC_IS_DOCK_PANEL (self));

  /* 1. Extensions GNOME */
  apply_extensions_config (self->shell_settings,
                           flavor_default_extensions,
                           G_N_ELEMENTS (flavor_default_extensions));

  /* 2. Arrera Dock */
  apply_windows_dock_settings (self);

  /* 3. Arrera Top Bar */
  apply_windows_topbar_settings (self);

  /* 4. Arrera App Menu */
  apply_windows_appmenu_settings (self);

  /* 5. Notification Toast */
  if (self->toast_overlay)
    {
      adw_toast_overlay_add_toast (self->toast_overlay,
                                   adw_toast_new (_("Saveur « Disposition Windows 10/11 » appliquée !")));
    }
}

static void
on_apply_flavor_windows_activity_clicked (CcDockPanel *self)
{
  g_assert (CC_IS_DOCK_PANEL (self));

  /* 1. Extensions GNOME */
  apply_extensions_config (self->shell_settings,
                           flavor_default_extensions,
                           G_N_ELEMENTS (flavor_default_extensions));

  /* 2. Arrera Dock */
  apply_windows_dock_settings (self);
  if (self->dock_settings)
    set_bool_if_valid (self->dock_settings, "show-activities-button", TRUE);

  /* 3. Arrera Top Bar */
  apply_windows_topbar_settings (self);

  /* 4. Arrera App Menu */
  apply_windows_appmenu_settings (self);

  /* 5. Notification Toast */
  if (self->toast_overlay)
    {
      adw_toast_overlay_add_toast (self->toast_overlay,
                                   adw_toast_new (_("Saveur « Disposition Windows 10/11 avec activity button » appliquée !")));
    }
}

static void
on_apply_flavor_nextstep_clicked (CcDockPanel *self)
{
  g_assert (CC_IS_DOCK_PANEL (self));

  /* 1. Extensions GNOME */
  apply_extensions_config (self->shell_settings,
                           flavor_default_extensions,
                           G_N_ELEMENTS (flavor_default_extensions));

  /* 2. Arrera Dock */
  apply_nextstep_dock_settings (self);

  /* 3. Arrera Top Bar */
  apply_windows_topbar_settings (self);

  /* 4. Arrera App Menu */
  apply_windows_appmenu_settings (self);

  /* 5. Notification Toast */
  if (self->toast_overlay)
    {
      adw_toast_overlay_add_toast (self->toast_overlay,
                                   adw_toast_new (_("Saveur « Bureau NeXTStep » appliquée !")));
    }
}

static void
cc_dock_panel_finalize (GObject *object)
{
  CcDockPanel *self = CC_DOCK_PANEL (object);

  g_clear_object (&self->shell_settings);
  g_clear_object (&self->dock_settings);
  g_clear_object (&self->topbar_settings);
  g_clear_object (&self->appmenu_settings);

  G_OBJECT_CLASS (cc_dock_panel_parent_class)->finalize (object);
}

static void
cc_dock_panel_class_init (CcDockPanelClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_dock_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/dock/cc-dock-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, preferences_page);

  gtk_widget_class_bind_template_callback (widget_class, on_apply_flavor_default_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_apply_flavor_noappmenu_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_apply_flavor_windows_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_apply_flavor_windows_activity_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_apply_flavor_nextstep_clicked);
}

static GSettings *
load_settings_safely (const gchar *schema_id, const gchar *fallback_dir_name)
{
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  g_autoptr(GSettingsSchema) schema = NULL;

  if (source)
    schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  if (!schema && fallback_dir_name)
    {
      const gchar *data_dir = g_get_user_data_dir ();
      g_autofree gchar *schema_dir = g_build_filename (data_dir, "gnome-shell", "extensions", fallback_dir_name, "schemas", NULL);
      if (g_file_test (schema_dir, G_FILE_TEST_IS_DIR))
        {
          g_autoptr(GSettingsSchemaSource) ext_source =
            g_settings_schema_source_new_from_directory (schema_dir, source, FALSE, NULL);
          if (ext_source)
            schema = g_settings_schema_source_lookup (ext_source, schema_id, TRUE);
        }
    }

  if (schema)
    return g_settings_new_full (schema, NULL, NULL);

  return NULL;
}

static void
cc_dock_panel_init (CcDockPanel *self)
{
  g_resources_register (cc_dock_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (self));

  self->shell_settings = load_settings_safely ("org.gnome.shell", NULL);
  self->dock_settings = load_settings_safely ("org.gnome.shell.extensions.dock", "dock@linux.arrera-software.fr");
  self->topbar_settings = load_settings_safely ("org.gnome.shell.extensions.top-bar", "top-bar@linux.arrera-software.fr");
  self->appmenu_settings = load_settings_safely ("org.gnome.shell.extensions.app-menu", "app-menu@linux.arrera-software.fr");
}
