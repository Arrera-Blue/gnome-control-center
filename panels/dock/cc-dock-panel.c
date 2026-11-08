/* cc-dock-panel.c
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-dock-panel.h"
#include "cc-dock-resources.h"
#include <adwaita.h>
#include <glib/gi18n.h>

typedef enum
{
  FLAVOR_UNKNOWN = 0,
  FLAVOR_DEFAULT,
  FLAVOR_NOAPPMENU,
  FLAVOR_WINDOWS,
  FLAVOR_WINDOWS_ACTIVITY,
  FLAVOR_NEXTSTEP
} FlavorId;

struct _CcDockPanel
{
  CcPanel             parent_instance;

  GSettings          *shell_settings;
  GSettings          *dock_settings;
  GSettings          *topbar_settings;
  GSettings          *appmenu_settings;

  FlavorId            current_flavor;

  AdwToastOverlay    *toast_overlay;
  AdwPreferencesPage *preferences_page;

  GtkWidget          *card_default;
  GtkWidget          *card_noappmenu;
  GtkWidget          *card_windows;
  GtkWidget          *card_windows_activity;
  GtkWidget          *card_nextstep;

  AdwSwitchRow       *autohide_row;
  AdwComboRow        *icon_size_row;
  AdwComboRow        *position_row;

  AdwComboRow        *dock_theme_row;
  AdwComboRow        *topbar_theme_row;
  AdwComboRow        *appmenu_theme_row;
  AdwComboRow        *logo_color_row;
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

/* Réinitialise proprement les extensions GNOME Shell pour éviter les incohérences d'acteurs */
static void
reload_extensions_async (gboolean enable_app_menu)
{
  g_autofree gchar *cmd = g_strdup_printf (
    "/usr/bin/bash -c '"
    "gnome-extensions disable dock@linux.arrera-software.fr 2>/dev/null || true; "
    "gnome-extensions disable top-bar@linux.arrera-software.fr 2>/dev/null || true; "
    "gnome-extensions disable appindicatorsupport@rgcjonas.gmail.com 2>/dev/null || true; "
    "gnome-extensions disable GPaste@gnome-shell-extensions.gnome.org 2>/dev/null || true; "
    "sleep 0.25; "
    "gnome-extensions enable top-bar@linux.arrera-software.fr 2>/dev/null || true; "
    "gnome-extensions enable dock@linux.arrera-software.fr 2>/dev/null || true; "
    "gnome-extensions enable appindicatorsupport@rgcjonas.gmail.com 2>/dev/null || true; "
    "gnome-extensions enable GPaste@gnome-shell-extensions.gnome.org 2>/dev/null || true; "
    "%s'",
    enable_app_menu
      ? "gnome-extensions enable app-menu@linux.arrera-software.fr 2>/dev/null || true;"
      : "gnome-extensions disable app-menu@linux.arrera-software.fr 2>/dev/null || true;"
  );
  g_spawn_command_line_async (cmd, NULL);
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
load_custom_css (void)
{
  static gboolean loaded = FALSE;
  GtkCssProvider *provider;
  GdkDisplay *display;

  if (loaded)
    return;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (
    provider,
    ".flavor-card {\n"
    "  border-radius: 12px;\n"
    "  border: 2px solid transparent;\n"
    "  transition: border-color 200ms ease, box-shadow 200ms ease;\n"
    "}\n"
    ".flavor-card:hover:not(.flavor-selected) {\n"
    "  border-color: alpha(@accent_color, 0.35);\n"
    "}\n"
    ".flavor-card.flavor-selected {\n"
    "  border-color: @accent_color;\n"
    "  box-shadow: 0 0 0 1px @accent_color, 0 0 16px alpha(@accent_color, 0.45);\n"
    "}\n"
  );

  display = gdk_display_get_default ();
  if (display)
    gtk_style_context_add_provider_for_display (
      display,
      GTK_STYLE_PROVIDER (provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

  g_object_unref (provider);
  loaded = TRUE;
}

static gboolean
is_extension_enabled (GSettings *shell_settings, const gchar *uuid)
{
  g_auto(GStrv) enabled = NULL;

  if (!shell_settings)
    return FALSE;

  enabled = g_settings_get_strv (shell_settings, "enabled-extensions");
  if (!enabled)
    return FALSE;

  for (guint i = 0; enabled[i]; i++)
    {
      if (g_strcmp0 (enabled[i], uuid) == 0)
        return TRUE;
    }

  return FALSE;
}

static FlavorId
detect_current_flavor (CcDockPanel *self)
{
  g_autofree gchar *position = NULL;
  gboolean island_mode = FALSE;
  gboolean hide_top_bar = FALSE;
  gboolean always_bar_mode = FALSE;
  gboolean show_activities = FALSE;
  gboolean appmenu_enabled = FALSE;

  if (self->dock_settings)
    {
      GSettingsSchema *schema = NULL;
      g_object_get (self->dock_settings, "settings-schema", &schema, NULL);
      if (schema)
        {
          if (g_settings_schema_has_key (schema, "position"))
            position = g_settings_get_string (self->dock_settings, "position");
          if (g_settings_schema_has_key (schema, "island-mode"))
            island_mode = g_settings_get_boolean (self->dock_settings, "island-mode");
          if (g_settings_schema_has_key (schema, "always-bar-mode"))
            always_bar_mode = g_settings_get_boolean (self->dock_settings, "always-bar-mode");
          if (g_settings_schema_has_key (schema, "show-activities-button"))
            show_activities = g_settings_get_boolean (self->dock_settings, "show-activities-button");
          g_settings_schema_unref (schema);
        }
    }

  if (self->topbar_settings)
    {
      GSettingsSchema *schema = NULL;
      g_object_get (self->topbar_settings, "settings-schema", &schema, NULL);
      if (schema)
        {
          if (g_settings_schema_has_key (schema, "hide-top-bar"))
            hide_top_bar = g_settings_get_boolean (self->topbar_settings, "hide-top-bar");
          g_settings_schema_unref (schema);
        }
    }

  if (island_mode)
    return FLAVOR_NEXTSTEP;

  if (hide_top_bar || always_bar_mode)
    {
      if (show_activities)
        return FLAVOR_WINDOWS_ACTIVITY;
      else
        return FLAVOR_WINDOWS;
    }

  appmenu_enabled = is_extension_enabled (self->shell_settings, "app-menu@linux.arrera-software.fr");
  if (!appmenu_enabled)
    return FLAVOR_NOAPPMENU;

  return FLAVOR_DEFAULT;
}

static void
update_selected_flavor_highlight (CcDockPanel *self)
{
  FlavorId current;
  g_autofree gchar *position = NULL;
  gboolean is_flavor_1_or_2;
  gboolean can_autohide;

  struct {
    GtkWidget *widget;
    FlavorId   flavor;
  } cards[] = {
    { self->card_default,          FLAVOR_DEFAULT },
    { self->card_noappmenu,        FLAVOR_NOAPPMENU },
    { self->card_windows,          FLAVOR_WINDOWS },
    { self->card_windows_activity, FLAVOR_WINDOWS_ACTIVITY },
    { self->card_nextstep,         FLAVOR_NEXTSTEP },
  };

  g_assert (CC_IS_DOCK_PANEL (self));

  if (self->current_flavor != FLAVOR_UNKNOWN)
    current = self->current_flavor;
  else
    current = detect_current_flavor (self);

  self->current_flavor = current;

  for (gsize i = 0; i < G_N_ELEMENTS (cards); i++)
    {
      if (!cards[i].widget)
        continue;

      if (cards[i].flavor == current)
        gtk_widget_add_css_class (cards[i].widget, "flavor-selected");
      else
        gtk_widget_remove_css_class (cards[i].widget, "flavor-selected");
    }

  if (self->dock_settings)
    position = g_settings_get_string (self->dock_settings, "position");

  is_flavor_1_or_2 = (current == FLAVOR_DEFAULT || current == FLAVOR_NOAPPMENU);
  /* Le masquage automatique est débloqué uniquement si on est en disposition 1 ou 2 avec le dock en bas */
  can_autohide = is_flavor_1_or_2 && (g_strcmp0 (position, "bottom") == 0);

  if (self->autohide_row)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->autohide_row), can_autohide);
      if (can_autohide)
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->autohide_row),
                                     _("Masque automatiquement le dock lorsqu'une fenêtre le chevauche"));
      else if (!is_flavor_1_or_2)
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->autohide_row),
                                     _("Verrouillé : non disponible pour cette disposition"));
      else
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->autohide_row),
                                     _("Verrouillé : disponible uniquement avec le dock en bas"));
    }

  if (self->position_row)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->position_row), is_flavor_1_or_2);
      if (!is_flavor_1_or_2)
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->position_row),
                                     _("Verrouillé : fixe pour cette disposition"));
      else
        adw_action_row_set_subtitle (ADW_ACTION_ROW (self->position_row), NULL);
    }
}

static void
on_apply_flavor_default_clicked (CcDockPanel *self)
{
  g_assert (CC_IS_DOCK_PANEL (self));

  self->current_flavor = FLAVOR_DEFAULT;

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

  /* 5. Synchronisation des extensions Shell */
  reload_extensions_async (TRUE);

  /* 6. Mise à jour de la mise en valeur de la saveur sélectionnée */
  update_selected_flavor_highlight (self);

  /* 7. Notification Toast */
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

  self->current_flavor = FLAVOR_NOAPPMENU;

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

  /* 5. Synchronisation des extensions Shell */
  reload_extensions_async (FALSE);

  /* 6. Mise à jour de la mise en valeur de la saveur sélectionnée */
  update_selected_flavor_highlight (self);

  /* 7. Notification Toast */
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

  self->current_flavor = FLAVOR_WINDOWS;

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

  /* 5. Synchronisation des extensions Shell */
  reload_extensions_async (TRUE);

  /* 6. Mise à jour de la mise en valeur de la saveur sélectionnée */
  update_selected_flavor_highlight (self);

  /* 7. Notification Toast */
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

  self->current_flavor = FLAVOR_WINDOWS_ACTIVITY;

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

  /* 5. Synchronisation des extensions Shell */
  reload_extensions_async (TRUE);

  /* 6. Mise à jour de la mise en valeur de la saveur sélectionnée */
  update_selected_flavor_highlight (self);

  /* 7. Notification Toast */
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

  self->current_flavor = FLAVOR_NEXTSTEP;

  /* 1. Extensions GNOME */
  apply_extensions_config (self->shell_settings,
                           flavor_default_extensions,
                           G_N_ELEMENTS (flavor_default_extensions));

  /* 2. Arrera Dock */
  apply_nextstep_dock_settings (self);

  /* 3. Arrera Top Bar */
  apply_windows_topbar_settings (self);

  /* 4. Arrera App Menu */
  apply_default_appmenu_settings (self);

  /* 5. Synchronisation des extensions Shell */
  reload_extensions_async (TRUE);

  /* 6. Mise à jour de la mise en valeur de la saveur sélectionnée */
  update_selected_flavor_highlight (self);

  /* 7. Notification Toast */
  if (self->toast_overlay)
    {
      adw_toast_overlay_add_toast (self->toast_overlay,
                                   adw_toast_new (_("Saveur « Bureau NeXTStep » appliquée !")));
    }
}

static gboolean
icon_size_get_mapping (GValue   *value,
                       GVariant *variant,
                       gpointer  user_data)
{
  const gchar *str = g_variant_get_string (variant, NULL);
  guint index = 1; /* medium par défaut */

  if (g_strcmp0 (str, "small") == 0)
    index = 0;
  else if (g_strcmp0 (str, "medium") == 0)
    index = 1;
  else if (g_strcmp0 (str, "large") == 0)
    index = 2;

  g_value_set_uint (value, index);
  return TRUE;
}

static GVariant *
icon_size_set_mapping (const GValue       *value,
                       const GVariantType *expected_type,
                       gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  const gchar *str = "medium";

  switch (index)
    {
    case 0:
      str = "small";
      break;
    case 1:
      str = "medium";
      break;
    case 2:
      str = "large";
      break;
    default:
      break;
    }

  return g_variant_new_string (str);
}

static gboolean
position_get_mapping (GValue   *value,
                      GVariant *variant,
                      gpointer  user_data)
{
  const gchar *str = g_variant_get_string (variant, NULL);
  guint index = 0; /* bottom par défaut */

  if (g_strcmp0 (str, "bottom") == 0)
    index = 0;
  else if (g_strcmp0 (str, "left") == 0)
    index = 1;
  else if (g_strcmp0 (str, "right") == 0)
    index = 2;

  g_value_set_uint (value, index);
  return TRUE;
}

static GVariant *
position_set_mapping (const GValue       *value,
                      const GVariantType *expected_type,
                      gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  const gchar *str = "bottom";

  switch (index)
    {
    case 0:
      str = "bottom";
      break;
    case 1:
      str = "left";
      break;
    case 2:
      str = "right";
      break;
    default:
      break;
    }

  return g_variant_new_string (str);
}

static gboolean
dock_theme_get_mapping (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  const gchar *str = g_variant_get_string (variant, NULL);
  guint index = 0; /* default expressive */

  if (g_strcmp0 (str, "expressive") == 0)
    index = 0;
  else if (g_strcmp0 (str, "black-outline") == 0)
    index = 1;
  else if (g_strcmp0 (str, "vanilla-gnome") == 0)
    index = 2;
  else if (g_strcmp0 (str, "black") == 0)
    index = 3;
  else if (g_strcmp0 (str, "white") == 0)
    index = 4;

  g_value_set_uint (value, index);
  return TRUE;
}

static GVariant *
dock_theme_set_mapping (const GValue       *value,
                        const GVariantType *expected_type,
                        gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  const gchar *str = "expressive";

  switch (index)
    {
    case 0:
      str = "expressive";
      break;
    case 1:
      str = "black-outline";
      break;
    case 2:
      str = "vanilla-gnome";
      break;
    case 3:
      str = "black";
      break;
    case 4:
      str = "white";
      break;
    default:
      break;
    }

  return g_variant_new_string (str);
}

static gboolean
topbar_theme_get_mapping (GValue   *value,
                          GVariant *variant,
                          gpointer  user_data)
{
  const gchar *str = g_variant_get_string (variant, NULL);
  guint index = 0; /* default vanilla */

  if (g_strcmp0 (str, "vanilla") == 0)
    index = 0;
  else if (g_strcmp0 (str, "invisible") == 0)
    index = 1;
  else if (g_strcmp0 (str, "tinted-dark") == 0)
    index = 2;
  else if (g_strcmp0 (str, "tinted-light") == 0)
    index = 3;

  g_value_set_uint (value, index);
  return TRUE;
}

static GVariant *
topbar_theme_set_mapping (const GValue       *value,
                          const GVariantType *expected_type,
                          gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  const gchar *str = "vanilla";

  switch (index)
    {
    case 0:
      str = "vanilla";
      break;
    case 1:
      str = "invisible";
      break;
    case 2:
      str = "tinted-dark";
      break;
    case 3:
      str = "tinted-light";
      break;
    default:
      break;
    }

  return g_variant_new_string (str);
}

static gboolean
appmenu_theme_get_mapping (GValue   *value,
                           GVariant *variant,
                           gpointer  user_data)
{
  const gchar *str = g_variant_get_string (variant, NULL);
  guint index = 0; /* default expressive */

  if (g_strcmp0 (str, "expressive") == 0)
    index = 0;
  else if (g_strcmp0 (str, "black-outline") == 0)
    index = 1;
  else if (g_strcmp0 (str, "vanilla-gnome") == 0)
    index = 2;

  g_value_set_uint (value, index);
  return TRUE;
}

static GVariant *
appmenu_theme_set_mapping (const GValue       *value,
                           const GVariantType *expected_type,
                           gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  const gchar *str = "expressive";

  switch (index)
    {
    case 0:
      str = "expressive";
      break;
    case 1:
      str = "black-outline";
      break;
    case 2:
      str = "vanilla-gnome";
      break;
    default:
      break;
    }

  return g_variant_new_string (str);
}

static gboolean
logo_color_get_mapping (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  const gchar *str = g_variant_get_string (variant, NULL);
  guint index = 0; /* default colored */

  if (g_strcmp0 (str, "colored") == 0)
    index = 0;
  else if (g_strcmp0 (str, "white") == 0)
    index = 1;
  else if (g_strcmp0 (str, "black") == 0)
    index = 2;

  g_value_set_uint (value, index);
  return TRUE;
}

static GVariant *
logo_color_set_mapping (const GValue       *value,
                        const GVariantType *expected_type,
                        gpointer            user_data)
{
  guint index = g_value_get_uint (value);
  const gchar *str = "colored";

  switch (index)
    {
    case 0:
      str = "colored";
      break;
    case 1:
      str = "white";
      break;
    case 2:
      str = "black";
      break;
    default:
      break;
    }

  return g_variant_new_string (str);
}

static void
on_dock_logo_color_changed (CcDockPanel *self)
{
  if (self->dock_settings && self->topbar_settings)
    {
      g_autofree gchar *val = g_settings_get_string (self->dock_settings, "logo-color");
      set_string_if_valid (self->topbar_settings, "logo-color", val);
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

  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, card_default);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, card_noappmenu);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, card_windows);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, card_windows_activity);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, card_nextstep);

  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, autohide_row);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, icon_size_row);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, position_row);

  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, dock_theme_row);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, topbar_theme_row);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, appmenu_theme_row);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, logo_color_row);

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

  load_custom_css ();

  self->shell_settings = load_settings_safely ("org.gnome.shell", NULL);
  self->dock_settings = load_settings_safely ("org.gnome.shell.extensions.dock", "dock@linux.arrera-software.fr");
  self->topbar_settings = load_settings_safely ("org.gnome.shell.extensions.top-bar", "top-bar@linux.arrera-software.fr");
  self->appmenu_settings = load_settings_safely ("org.gnome.shell.extensions.app-menu", "app-menu@linux.arrera-software.fr");

  if (self->dock_settings)
    {
      GSettingsSchema *schema = NULL;
      g_object_get (self->dock_settings, "settings-schema", &schema, NULL);
      if (schema)
        {
          if (g_settings_schema_has_key (schema, "autohide"))
            g_settings_bind (self->dock_settings, "autohide",
                             self->autohide_row, "active",
                             G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

          if (g_settings_schema_has_key (schema, "icon-size"))
            g_settings_bind_with_mapping (self->dock_settings, "icon-size",
                                          self->icon_size_row, "selected",
                                          G_SETTINGS_BIND_DEFAULT,
                                          icon_size_get_mapping,
                                          icon_size_set_mapping,
                                          NULL, NULL);

          if (g_settings_schema_has_key (schema, "position"))
            g_settings_bind_with_mapping (self->dock_settings, "position",
                                          self->position_row, "selected",
                                          G_SETTINGS_BIND_DEFAULT,
                                          position_get_mapping,
                                          position_set_mapping,
                                          NULL, NULL);

          if (g_settings_schema_has_key (schema, "theme-mode"))
            g_settings_bind_with_mapping (self->dock_settings, "theme-mode",
                                          self->dock_theme_row, "selected",
                                          G_SETTINGS_BIND_DEFAULT,
                                          dock_theme_get_mapping,
                                          dock_theme_set_mapping,
                                          NULL, NULL);

          if (g_settings_schema_has_key (schema, "logo-color"))
            g_settings_bind_with_mapping (self->dock_settings, "logo-color",
                                          self->logo_color_row, "selected",
                                          G_SETTINGS_BIND_DEFAULT,
                                          logo_color_get_mapping,
                                          logo_color_set_mapping,
                                          NULL, NULL);

          g_settings_schema_unref (schema);
        }

      g_signal_connect_swapped (self->dock_settings, "changed::logo-color",
                                G_CALLBACK (on_dock_logo_color_changed), self);
    }

  if (self->topbar_settings)
    {
      GSettingsSchema *schema = NULL;
      g_object_get (self->topbar_settings, "settings-schema", &schema, NULL);
      if (schema)
        {
          if (g_settings_schema_has_key (schema, "theme"))
            g_settings_bind_with_mapping (self->topbar_settings, "theme",
                                          self->topbar_theme_row, "selected",
                                          G_SETTINGS_BIND_DEFAULT,
                                          topbar_theme_get_mapping,
                                          topbar_theme_set_mapping,
                                          NULL, NULL);

          g_settings_schema_unref (schema);
        }
    }

  if (self->appmenu_settings)
    {
      GSettingsSchema *schema = NULL;
      g_object_get (self->appmenu_settings, "settings-schema", &schema, NULL);
      if (schema)
        {
          if (g_settings_schema_has_key (schema, "theme-mode"))
            g_settings_bind_with_mapping (self->appmenu_settings, "theme-mode",
                                          self->appmenu_theme_row, "selected",
                                          G_SETTINGS_BIND_DEFAULT,
                                          appmenu_theme_get_mapping,
                                          appmenu_theme_set_mapping,
                                          NULL, NULL);

          g_settings_schema_unref (schema);
        }
    }

  self->current_flavor = detect_current_flavor (self);
  update_selected_flavor_highlight (self);

  if (self->dock_settings)
    g_signal_connect_swapped (self->dock_settings, "changed", G_CALLBACK (update_selected_flavor_highlight), self);
  if (self->topbar_settings)
    g_signal_connect_swapped (self->topbar_settings, "changed", G_CALLBACK (update_selected_flavor_highlight), self);
  if (self->shell_settings)
    g_signal_connect_swapped (self->shell_settings, "changed::enabled-extensions", G_CALLBACK (update_selected_flavor_highlight), self);
}
