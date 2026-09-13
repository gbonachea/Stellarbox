/*
 * stellarbox-desktop - escritorio independiente para Stellarbox.
 *
 * Pinta el fondo de pantalla y muestra los iconos de ~/Desktop sobre cada
 * monitor. No depende de ningun gestor de archivos concreto, por lo que se
 * complementa tanto con stellarbox como con cualquier otro WM y con
 * cualquier gestor de archivos.
 *
 * Config:
 *   - Fondo: ~/.config/stellarbox/wallpaper (ruta absoluta del archivo).
 *     Si no existe, se usa la primera imagen de /usr/share/stellarbox/backgrounds.
 *     Si tampoco hay, se dibuja un degradado neutro.
 *   - Iconos: contenido de ~/Desktop (doble clic para abrir).
 *
 * Licencia: MIT
 */
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

enum {
    COL_PIXBUF,
    COL_NAME,
    COL_URI,
    COL_COUNT
};

#define ICON_SIZE 48

typedef struct {
    GtkListStore *store;
    GFile *desktop;
    char *cfg_file;
    char *bg_dir;
    char *wallpaper;
    GdkPixbuf *wallpaper_cache;
    int cache_w, cache_h;
} SbApp;

typedef struct {
    SbApp *app;
    GtkWidget *window;
    GtkWidget *icon_view;
} SbWin;

typedef struct {
    SbWin *win;
    GFile *file;
} MenuClosure;

static void menu_closure_free(MenuClosure *mc);

/* ------------------------------------------------------------------ */
/*  fondo de pantalla                                                  */
/* ------------------------------------------------------------------ */

static char *read_text_file(const char *path)
{
    gchar *c = NULL;
    if (!g_file_get_contents(path, &c, NULL, NULL))
        return NULL;
    g_strstrip(c);
    if (!*c) {
        g_free(c);
        return NULL;
    }
    return c;
}

static const char *const image_exts[] = { "png", "jpg", "jpeg", "webp", "bmp", NULL };

static int has_image_ext(const char *name)
{
    if (!name)
        return 0;
    const char *dot = strrchr(name, '.');
    if (!dot)
        return 0;
    for (int i = 0; image_exts[i]; i++)
        if (g_ascii_strcasecmp(dot + 1, image_exts[i]) == 0)
            return 1;
    return 0;
}

static char *find_default_wallpaper(const char *bg_dir)
{
    GDir *d = g_dir_open(bg_dir, 0, NULL);
    if (!d)
        return NULL;
    const char *n;
    while ((n = g_dir_read_name(d)))
        if (has_image_ext(n)) {
            char *p = g_build_filename(bg_dir, n, NULL);
            g_dir_close(d);
            return p;
        }
    g_dir_close(d);
    return NULL;
}

static void resolve_wallpaper(SbApp *app)
{
    g_clear_pointer(&app->wallpaper, g_free);
    if (g_file_test(app->cfg_file, G_FILE_TEST_IS_REGULAR)) {
        char *p = read_text_file(app->cfg_file);
        if (p && g_file_test(p, G_FILE_TEST_IS_REGULAR)) {
            app->wallpaper = p;
            return;
        }
        g_free(p);
    }
    app->wallpaper = find_default_wallpaper(app->bg_dir);
}

static void reload_wallpaper(SbApp *app)
{
    g_clear_object(&app->wallpaper_cache);
    resolve_wallpaper(app);
}

static GdkPixbuf *wallpaper_pixbuf(SbApp *app, int w, int h)
{
    if (!app->wallpaper)
        return NULL;
    if (app->wallpaper_cache && app->cache_w == w && app->cache_h == h)
        return g_object_ref(app->wallpaper_cache);

    GdkPixbuf *src = gdk_pixbuf_new_from_file(app->wallpaper, NULL);
    if (!src)
        return NULL;

    int sw = gdk_pixbuf_get_width(src);
    int sh = gdk_pixbuf_get_height(src);
    double s = MAX((double) w / sw, (double) h / sh);
    int tw = (int) (sw * s);
    int th = (int) (sh * s);

    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(src, tw, th, GDK_INTERP_BILINEAR);
    g_object_unref(src);

    GdkPixbuf *out = gdk_pixbuf_new(GDK_COLORSPACE_RGB, gdk_pixbuf_get_has_alpha(scaled), 8, w, h);
    int ox = (tw - w) / 2;
    int oy = (th - h) / 2;
    gdk_pixbuf_copy_area(scaled, ox, oy, w, h, out, 0, 0);
    g_object_unref(scaled);

    g_clear_object(&app->wallpaper_cache);
    app->wallpaper_cache = g_object_ref(out);
    app->cache_w = w;
    app->cache_h = h;
    return g_object_ref(out);
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer user_data)
{
    SbApp *app = user_data;
    int ww = gtk_widget_get_allocated_width(w);
    int wh = gtk_widget_get_allocated_height(w);

    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgb(cr, 0.13, 0.14, 0.16);
    cairo_paint(cr);

    GdkPixbuf *pb = wallpaper_pixbuf(app, ww, wh);
    if (pb) {
        gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
        cairo_paint(cr);
        g_object_unref(pb);
    } else {
        cairo_pattern_t *grad = cairo_pattern_create_linear(0, 0, 0, wh);
        cairo_pattern_add_color_stop_rgb(grad, 0.0, 0.23, 0.23, 0.25);
        cairo_pattern_add_color_stop_rgb(grad, 1.0, 0.09, 0.09, 0.11);
        cairo_set_source(cr, grad);
        cairo_paint(cr);
        cairo_pattern_destroy(grad);
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/*  iconos de ~/Desktop                                                */
/* ------------------------------------------------------------------ */

static GdkPixbuf *icon_for_file(GFile *f)
{
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    GdkPixbuf *pb = NULL;

    if (g_file_is_native(f)) {
        GFileInfo *info = g_file_query_info(f, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                            G_FILE_QUERY_INFO_NONE, NULL, NULL);
        const char *ct = info ? g_file_info_get_content_type(info) : NULL;
        g_clear_object(&info);

        if (ct && strcmp(ct, "inode/directory") == 0) {
            pb = gtk_icon_theme_load_icon(theme, "folder", ICON_SIZE,
                                          GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        } else {
            g_autofree char *bn = g_file_get_basename(f);
            if (bn && g_str_has_suffix(bn, ".desktop")) {
                g_autofree char *path = g_file_get_path(f);
                GKeyFile *kf = g_key_file_new();
                if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
                    char *ic = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
                    if (ic) {
                        pb = gtk_icon_theme_load_icon(theme, ic, ICON_SIZE,
                                                      GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
                        g_free(ic);
                    }
                }
                g_key_file_free(kf);
            } else {
                GIcon *gi = ct ? g_content_type_get_icon(ct) : NULL;
                if (G_IS_THEMED_ICON(gi)) {
                    const char *const *names = g_themed_icon_get_names(G_THEMED_ICON(gi));
                    if (names && names[0])
                        pb = gtk_icon_theme_load_icon(theme, names[0], ICON_SIZE,
                                                      GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
                }
                g_clear_object(&gi);
            }
            if (!pb)
                pb = gtk_icon_theme_load_icon(theme, "text-x-generic", ICON_SIZE,
                                              GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        }
    } else {
        pb = gtk_icon_theme_load_icon(theme, "drive-removable-media", ICON_SIZE,
                                      GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    }
    return pb;
}

static gint sort_names(gconstpointer a, gconstpointer b)
{
    const char *sa = *(const char *const *) a;
    const char *sb = *(const char *const *) b;
    return g_utf8_collate(sa, sb);
}

static void refresh_store(SbApp *app)
{
    gtk_list_store_clear(app->store);

    g_autofree char *desktop_path = g_file_get_path(app->desktop);
    GDir *d = g_dir_open(desktop_path, 0, NULL);
    if (!d && g_file_make_directory_with_parents(app->desktop, NULL, NULL))
        d = g_dir_open(desktop_path, 0, NULL);
    if (!d)
        return;

    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    const char *n;
    while ((n = g_dir_read_name(d)))
        g_ptr_array_add(names, g_strdup(n));
    g_dir_close(d);
    g_ptr_array_sort(names, sort_names);

    for (guint i = 0; i < names->len; i++) {
        const char *name = g_ptr_array_index(names, i);
        g_autoptr(GFile) f = g_file_get_child(app->desktop, name);
        g_autoptr(GdkPixbuf) pb = icon_for_file(f);
        g_autofree char *uri = g_file_get_uri(f);
        GtkTreeIter it;
        gtk_list_store_append(app->store, &it);
        gtk_list_store_set(app->store, &it,
                           COL_PIXBUF, pb,
                           COL_NAME, name,
                           COL_URI, uri,
                           -1);
    }
    g_ptr_array_unref(names);
}

static void on_dir_changed(GFileMonitor *mon, GFile *f1, GFile *f2,
                           GFileMonitorEvent ev, gpointer user_data)
{
    SbApp *app = user_data;
    refresh_store(app);
}

/* ------------------------------------------------------------------ */
/*  acciones                                                           */
/* ------------------------------------------------------------------ */

static void launch_uri(GtkWindow *parent, const char *uri)
{
    GError *err = NULL;
    if (!g_app_info_launch_default_for_uri(uri, NULL, &err)) {
        GtkWidget *dlg = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                "No se pudo abrir:\n%s", uri);
        if (err && err->message)
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", err->message);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        g_clear_error(&err);
    }
}

static void on_item_activated(GtkIconView *view, GtkTreePath *path, gpointer user_data)
{
    SbWin *w = user_data;
    GtkTreeIter it;
    GtkTreeModel *model = gtk_icon_view_get_model(view);
    gchar *uri = NULL;
    if (gtk_tree_model_get_iter(model, &it, path))
        gtk_tree_model_get(model, &it, COL_URI, &uri, -1);
    if (uri) {
        launch_uri(GTK_WINDOW(w->window), uri);
        g_free(uri);
    }
}

static void on_open_file(GtkMenuItem *item, gpointer user_data)
{
    MenuClosure *mc = user_data;
    g_autofree char *uri = g_file_get_uri(mc->file);
    launch_uri(GTK_WINDOW(mc->win->window), uri);
}

static void on_trash_file(GtkMenuItem *item, gpointer user_data)
{
    MenuClosure *mc = user_data;
    GError *err = NULL;
    if (!g_file_trash(mc->file, NULL, &err)) {
        GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(mc->win->window), GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                "No se pudo eliminar.");
        if (err && err->message)
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", err->message);
        gtk_dialog_run(GTK_DIALOG(dlg));
        gtk_widget_destroy(dlg);
        g_clear_error(&err);
    }
}

static void on_new_folder(GtkMenuItem *item, gpointer user_data)
{
    SbWin *w = user_data;
    g_autofree char *path = g_file_get_path(w->app->desktop);
    char *name = NULL;
    for (int i = 1; i < 1000; i++) {
        if (i == 1)
            name = g_build_filename(path, "Nueva carpeta", NULL);
        else
            name = g_strdup_printf("%s/Nueva carpeta %d", path, i);
        if (!g_file_test(name, G_FILE_TEST_EXISTS))
            break;
        g_free(name);
        name = NULL;
    }
    if (name) {
        g_mkdir_with_parents(name, 0755);
        g_free(name);
    }
}

static void on_open_manager(GtkMenuItem *item, gpointer user_data)
{
    SbWin *w = user_data;
    g_autofree char *uri = g_file_get_uri(w->app->desktop);
    launch_uri(GTK_WINDOW(w->window), uri);
}

static void on_change_wallpaper(GtkMenuItem *item, gpointer user_data)
{
    SbWin *w = user_data;
    GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
        "Elegir fondo de pantalla", GTK_WINDOW(w->window),
        GTK_FILE_CHOOSER_ACTION_OPEN, "_Aceptar", "_Cancelar");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Imagenes");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/webp");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        g_autofree char *selected = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        g_autofree char *cfg_dir = g_path_get_dirname(w->app->cfg_file);
        g_mkdir_with_parents(cfg_dir, 0755);
        GError *err = NULL;
        if (!g_file_set_contents(w->app->cfg_file, selected, -1, &err)) {
            GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(w->window), GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                                    "No se pudo guardar el fondo.");
            if (err && err->message)
                gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dlg), "%s", err->message);
            gtk_dialog_run(GTK_DIALOG(dlg));
            gtk_widget_destroy(dlg);
            g_clear_error(&err);
        } else {
            reload_wallpaper(w->app);
            gtk_widget_queue_draw(w->window);
        }
    }
    g_object_unref(chooser);
}

static void on_restore_wallpaper(GtkMenuItem *item, gpointer user_data)
{
    SbWin *w = user_data;
    if (remove(w->app->cfg_file) != 0 && errno != ENOENT)
        g_warning("no se pudo eliminar %s", w->app->cfg_file);
    reload_wallpaper(w->app);
    gtk_widget_queue_draw(w->window);
}

static void popup_menu(SbWin *w, GdkEventButton *ev, GFile *item)
{
    GtkWidget *menu = gtk_menu_new();

    if (item) {
        MenuClosure *mc = g_new0(MenuClosure, 1);
        mc->win = w;
        mc->file = g_object_ref(item);

        GtkWidget *open = gtk_menu_item_new_with_label("Abrir");
        g_signal_connect(open, "activate", G_CALLBACK(on_open_file), mc);
        gtk_container_add(GTK_CONTAINER(menu), open);

        GtkWidget *trash = gtk_menu_item_new_with_label("Mover a la papelera");
        g_signal_connect(trash, "activate", G_CALLBACK(on_trash_file), mc);
        gtk_container_add(GTK_CONTAINER(menu), trash);

        g_object_weak_ref(G_OBJECT(menu), (GWeakNotify) menu_closure_free, mc);
    } else {
        GtkWidget *nf = gtk_menu_item_new_with_label("Nueva carpeta");
        g_signal_connect(nf, "activate", G_CALLBACK(on_new_folder), w);
        gtk_container_add(GTK_CONTAINER(menu), nf);

        GtkWidget *sep = gtk_separator_menu_item_new();
        gtk_container_add(GTK_CONTAINER(menu), sep);

        GtkWidget *fm = gtk_menu_item_new_with_label("Abrir con gestor de archivos");
        g_signal_connect(fm, "activate", G_CALLBACK(on_open_manager), w);
        gtk_container_add(GTK_CONTAINER(menu), fm);

        GtkWidget *sep2 = gtk_separator_menu_item_new();
        gtk_container_add(GTK_CONTAINER(menu), sep2);

        GtkWidget *cw = gtk_menu_item_new_with_label("Cambiar fondo de pantalla...");
        g_signal_connect(cw, "activate", G_CALLBACK(on_change_wallpaper), w);
        gtk_container_add(GTK_CONTAINER(menu), cw);

        GtkWidget *rw = gtk_menu_item_new_with_label("Restaurar fondo por defecto");
        g_signal_connect(rw, "activate", G_CALLBACK(on_restore_wallpaper), w);
        gtk_container_add(GTK_CONTAINER(menu), rw);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (const GdkEvent *) ev);
}

static void menu_closure_free(MenuClosure *mc)
{
    if (mc->file)
        g_object_unref(mc->file);
    g_free(mc);
}

static gboolean on_button_press(GtkWidget *view, GdkEventButton *ev, gpointer user_data)
{
    SbWin *w = user_data;
    if (ev->button != 3)
        return FALSE;

    GtkTreePath *path = NULL;
    if (gtk_icon_view_get_item_at_pos(GTK_ICON_VIEW(view), ev->x, ev->y, &path, NULL)) {
        GtkTreeModel *model = gtk_icon_view_get_model(GTK_ICON_VIEW(view));
        GtkTreeIter it;
        gchar *uri = NULL;
        GFile *f = NULL;
        if (gtk_tree_model_get_iter(model, &it, path)) {
            gtk_tree_model_get(model, &it, COL_URI, &uri, -1);
            if (uri) {
                f = g_file_new_for_uri(uri);
                g_free(uri);
            }
        }
        gtk_tree_path_free(path);
        popup_menu(w, ev, f);
        g_clear_object(&f);
    } else {
        popup_menu(w, ev, NULL);
    }
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  arrastrar y soltar archivos sobre el escritorio                    */
/* ------------------------------------------------------------------ */

static const GtkTargetEntry dnd_targets[] = { { "text/uri-list", 0, 0 } };

static GFile *unique_target(SbApp *app, const char *basename)
{
    g_autofree char *base = g_strdup(basename ? basename : "archivo");
    g_autofree char *path = g_file_get_path(app->desktop);
    GFile *f = g_file_get_child(app->desktop, base);
    if (!g_file_query_exists(f, NULL))
        return f;
    g_object_unref(f);

    const char *dot = strrchr(base, '.');
    char *root = NULL;
    const char *ext = "";
    if (dot) {
        root = g_strndup(base, dot - base);
        ext = dot;
    } else {
        root = g_strdup(base);
    }
    for (int i = 1; i < 1000; i++) {
        g_autofree char *name = g_strdup_printf("%s %d%s", root, i, ext);
        f = g_file_get_child(app->desktop, name);
        if (!g_file_query_exists(f, NULL)) {
            g_free(root);
            return f;
        }
        g_object_unref(f);
    }
    g_free(root);
    return g_file_get_child(app->desktop, base);
}

static void on_dnd_data(GtkWidget *widget, GdkDragContext *ctx, gint x, gint y,
                        GtkSelectionData *sel, guint info, guint time, gpointer user_data)
{
    SbApp *app = user_data;
    gchar **uris = gtk_selection_data_get_uris(sel);
    for (int i = 0; uris && uris[i]; i++) {
        g_autoptr(GFile) src = g_file_new_for_uri(uris[i]);
        g_autofree char *bn = g_file_get_basename(src);
        g_autoptr(GFile) dst = unique_target(app, bn);
        GError *err = NULL;
        if (!g_file_copy(src, dst, G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, &err))
            g_clear_error(&err);
    }
    g_strfreev(uris);
    gtk_drag_finish(ctx, TRUE, FALSE, time);
}

/* ------------------------------------------------------------------ */
/*  ventanas                                                           */
/* ------------------------------------------------------------------ */

static void apply_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "GtkIconView { background-color: transparent; }\n"
        "GtkIconView.view { background-color: transparent; padding: 12px; color: #ffffff; "
        "text-shadow: 1px 1px 2px rgba(0,0,0,0.8); }\n"
        "GtkIconView.view.selection { background-color: alpha(#1e64d0, 0.45); color: #ffffff; }\n"
        "GtkIconView.view:hover { background-color: alpha(#ffffff, 0.12); }\n",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void create_desktop_window(SbApp *app, GdkMonitor *monitor)
{
    GdkRectangle geo;
    gdk_monitor_get_geometry(monitor, &geo);

    SbWin *w = g_new0(SbWin, 1);
    w->app = app;
    w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    GtkWindow *win = GTK_WINDOW(w->window);

    gtk_window_set_type_hint(win, GDK_WINDOW_TYPE_HINT_DESKTOP);
    gtk_window_set_decorated(win, FALSE);
    gtk_window_set_accept_focus(win, FALSE);
    gtk_window_set_focus_on_map(win, FALSE);
    gtk_window_set_skip_taskbar_hint(win, TRUE);
    gtk_window_set_skip_pager_hint(win, TRUE);
    gtk_window_set_keep_below(win, TRUE);
    gtk_window_set_resizable(win, FALSE);
    gtk_window_move(win, geo.x, geo.y);
    gtk_window_set_default_size(win, geo.width, geo.height);
    gtk_widget_set_app_paintable(w->window, TRUE);
    g_signal_connect(w->window, "draw", G_CALLBACK(on_draw), app);

    w->icon_view = gtk_icon_view_new();
    gtk_icon_view_set_model(GTK_ICON_VIEW(w->icon_view), GTK_TREE_MODEL(app->store));
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(w->icon_view), COL_PIXBUF);
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(w->icon_view), COL_NAME);
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(w->icon_view), GTK_SELECTION_MULTIPLE);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(w->icon_view), 120);
    gtk_icon_view_set_columns(GTK_ICON_VIEW(w->icon_view), -1);
    gtk_icon_view_set_spacing(GTK_ICON_VIEW(w->icon_view), 10);
    gtk_icon_view_set_activate_on_single_click(GTK_ICON_VIEW(w->icon_view), FALSE);

    g_signal_connect(w->icon_view, "item-activated", G_CALLBACK(on_item_activated), w);
    g_signal_connect(w->icon_view, "button-press-event", G_CALLBACK(on_button_press), w);

    gtk_drag_dest_set(w->icon_view, GTK_DEST_DEFAULT_ALL,
                      (GtkTargetEntry *) dnd_targets, 1, GDK_ACTION_COPY);
    g_signal_connect(w->icon_view, "drag-data-received", G_CALLBACK(on_dnd_data), app);

    gtk_container_add(GTK_CONTAINER(w->window), w->icon_view);
    gtk_widget_show_all(w->window);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    SbApp app = { 0 };
    app.cfg_file = g_build_filename(g_get_user_config_dir(), "stellarbox", "wallpaper", NULL);
    app.bg_dir = g_build_filename("/usr/share", "stellarbox", "backgrounds", NULL);
    resolve_wallpaper(&app);

    app.desktop = g_file_new_for_path(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));

    g_autofree char *cfg_dir = g_path_get_dirname(app.cfg_file);
    g_mkdir_with_parents(cfg_dir, 0755);

    app.store = gtk_list_store_new(COL_COUNT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    refresh_store(&app);

    GFileMonitor *mon = g_file_monitor_directory(app.desktop, G_FILE_MONITOR_NONE, NULL, NULL);
    if (mon)
        g_signal_connect(mon, "changed", G_CALLBACK(on_dir_changed), &app);

    apply_css();

    GdkDisplay *display = gdk_display_get_default();
    int n = gdk_display_get_n_monitors(display);
    for (int i = 0; i < n; i++)
        create_desktop_window(&app, gdk_display_get_monitor(display, i));

    gtk_main();
    return 0;
}