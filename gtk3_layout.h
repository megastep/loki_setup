#ifndef LOKI_GTK3_LAYOUT_H
#define LOKI_GTK3_LAYOUT_H

#include <stdio.h>
#include <gtk/gtk.h>

typedef GtkBuilder UiLayout;

#define GTK3_CALLBACK(layout, handler) \
    gtk_builder_add_callback_symbol(layout, #handler, G_CALLBACK(handler))

static inline UiLayout *load_gtk3_layout(const char *filename, const char *domain)
{
    GtkBuilder *builder = gtk_builder_new();
    GError *error = NULL;

    gtk_builder_set_translation_domain(builder, domain);
    if (!gtk_builder_add_from_file(builder, filename, &error)) {
        fprintf(stderr, "%s: %s\n", filename, error->message);
        g_error_free(error);
        g_object_unref(builder);
        return NULL;
    }
    return builder;
}

static inline GtkWidget *ui_widget(UiLayout *layout, const char *id)
{
    return layout ? GTK_WIDGET(gtk_builder_get_object(layout, id)) : NULL;
}

#endif
