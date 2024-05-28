
/*
 *******************************************************************************
 *
 *   © 2016 and later: Unicode, Inc. and others.
 *   License & terms of use: http://www.unicode.org/copyright.html#License
 *
 *******************************************************************************
 ****************************************************************************** *
 *
 *   Copyright (C) 1999-2007, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 ****************************************************************************** *
 *   file name:  gnomelayout.cpp
 *
 *   created on: 09/04/2001
 *   created by: Eric R. Mader
 */

#include <gnome.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "unicode/ustring.h"
#include "unicode/uscript.h"

#include "GnomeFontInstance.h"

#include "paragraph.h"

#include "GnomeGUISupport.h"
#include "GnomeFontMap.h"
#include "UnicodeReader.h"
#include "ScriptCompositeFontInstance.h"

#define ARRAY_LENGTH(array) (sizeof array / sizeof array[0])

struct Context
{
    long width;
    long height;
    Paragraph *paragraph;
};

static FT_Library engine;
static GnomeGUISupport *guiSupport;
static GnomeFontMap *fontMap;
static ScriptCompositeFontInstance *font;

static GSList *appList = NULL;

GtkWidget *newSample(const gchar *fileName);
void       closeSample(GtkWidget *sample);

void showabout(GtkWidget */*widget*/, gpointer /*data*/)
{
    GtkWidget *aboutBox;
    const gchar *documentedBy[] = {NULL};
    const gchar *writtenBy[] = {
        "Eric Mader",
        NULL
    };

    aboutBox = gnome_about_new("Gnome Layout Sample",
                               "0.1",
                               "Copyright (C) 1998-2006 By International Business Machines Corporation and others. All Rights Reserved.",
                               "A simple demo of the ICU LayoutEngine.",
                               writtenBy,
                               documentedBy,
                               "",
                               NULL);

    gtk_widget_show(aboutBox);
}

void notimpl(GtkObject */*object*/, gpointer /*data*/)
{
    gnome_ok_dialog("Not implemented...");
}

gchar *prettyTitle(const gchar *path)
{
  const gchar *name  = g_basename(path);
  gchar *title = g_strconcat("Gnome Layout Sample - ", name, NULL);

  return title;
}

void openOK(GtkObject */*object*/, gpointer data)
{
  GtkFileSelection *fileselection = GTK_FILE_SELECTION(data);
  GtkWidget *app = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(fileselection), "app"));
  Context *context = (Context *) gtk_object_get_data(GTK_OBJECT(app), "context");
  gchar *fileName  = g_strdup(gtk_file_selection_get_filename(fileselection));
  Paragraph *newPara;

  gtk_widget_destroy(GTK_WIDGET(fileselection));

  newPara = Paragraph::paragraphFactory(fileName, font, guiSupport);

  if (newPara != NULL) {
    gchar *title = prettyTitle(fileName);
    GtkWidget *area = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(app), "area"));

    if (context->paragraph != NULL) {
      delete context->paragraph;
    }

    context->paragraph = newPara;
    gtk_window_set_title(GTK_WINDOW(app), title);

    gtk_widget_hide(area);
    context->paragraph->breakLines(context->width, context->height);
    gtk_widget_show_all(area);

    g_free(title);
  }

  g_free(fileName);
}

void openfile(GtkObject */*object*/, gpointer data)
{
  GtkWidget *app = GTK_WIDGET(data);
  GtkWidget *fileselection;
  GtkWidget *okButton;
  GtkWidget *cancelButton;

  fileselection =
    gtk_file_selection_new("Open File");

  gtk_object_set_data(GTK_OBJECT(fileselection), "app", app);

  okButton =
    GTK_FILE_SELECTION(fileselection)->ok_button;

  cancelButton =
    GTK_FILE_SELECTION(fileselection)->cancel_button;

  gtk_signal_connect(GTK_OBJECT(fileselection), "destroy",
		     GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

  gtk_signal_connect(GTK_OBJECT(okButton), "clicked",
		     GTK_SIGNAL_FUNC(openOK), fileselection);

  gtk_signal_connect_object(GTK_OBJECT(cancelButton), "clicked",
		     GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(fileselection));

  gtk_window_set_modal(GTK_WINDOW(fileselection), TRUE);
  gtk_widget_show(fileselection);
  gtk_main();
}

void newapp(GtkObject */*object*/, gpointer /*data*/)
{
  GtkWidget *app = newSample("Sample.txt");

  gtk_widget_show_all(app);
}

void closeapp(GtkWidget */*widget*/, gpointer data)
{
  GtkWidget *app = GTK_WIDGET(data);

  closeSample(app);
}

void shutdown(GtkObject */*object*/, gpointer /*data*/)
{
    gtk_main_quit();
}

GnomeUIInfo fileMenu[] =
{
  GNOMEUIINFO_MENU_NEW_ITEM((gchar *) "_New Sample",
			    (gchar *) "Create a new Gnome Layout Sample",
			    newapp, NULL),

  GNOMEUIINFO_MENU_OPEN_ITEM(openfile, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CLOSE_ITEM(closeapp, NULL),
  GNOMEUIINFO_MENU_EXIT_ITEM(shutdown, NULL),
  GNOMEUIINFO_END
};

GnomeUIInfo helpMenu[] =
{
    // GNOMEUIINFO_HELP("gnomelayout"),
    GNOMEUIINFO_MENU_ABOUT_ITEM(showabout, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo mainMenu[] =
{
    GNOMEUIINFO_SUBTREE(N_((gchar *) "File"), fileMenu),
    GNOMEUIINFO_SUBTREE(N_((gchar *) "Help"), helpMenu),
    GNOMEUIINFO_END
};

gint eventDelete(GtkWidget *widget, GdkEvent */*event*/, gpointer /*data*/)
{
  closeSample(widget);

  // indicate that closeapp  already destroyed the window 
  return TRUE;
}

gint eventConfigure(GtkWidget */*widget*/, GdkEventConfigure *event, Context *context)
{
  if (context->paragraph != NULL) {
    context->width  = event->width;
    context->height = event->height;

    if (context->width > 0 && context->height > 0) {
        context->paragraph->breakLines(context->width, context->height);
    }
  }

  return TRUE;
}

gint eventExpose(GtkWidget *widget, GdkEvent */*event*/, Context *context)
{
  if (context->paragraph != NULL) {
    gint maxLines = context->paragraph->getLineCount() - 1;
    gint firstLine = 0, lastLine = context->height / context->paragraph->getLineHeight();
    GnomeSurface surface(widget);

    context->paragraph->draw(&surface, firstLine, (maxLines < lastLine)? maxLines : lastLine);
  }

  return TRUE;
}

GtkWidget *newSample(const gchar *fileName)
{
  Context   *context = new Context();

  context->width  = 600;
  context->height = 400;
  context->paragraph = Paragraph::paragraphFactory(fileName, font, guiSupport);

  gchar *title = prettyTitle(fileName);
  GtkWidget *app = gnome_app_new("gnomeLayout", title);

  gtk_object_set_data(GTK_OBJECT(app), "context", context);

  gtk_window_set_default_size(GTK_WINDOW(app), 600 - 24, 400);

  gnome_app_create_menus_with_data(GNOME_APP(app), mainMenu, app);

  gtk_signal_connect(GTK_OBJECT(app), "delete_event",
		     GTK_SIGNAL_FUNC(eventDelete), NULL);

  GtkWidget *area = gtk_drawing_area_new();
  gtk_object_set_data(GTK_OBJECT(app), "area", area);

  GtkStyle *style = gtk_style_copy(gtk_widget_get_style(area));

  for (int i = 0; i < 5; i += 1) {
    style->fg[i] = style->white;
  }
    
  gtk_widget_set_style(area, style);

  gnome_app_set_contents(GNOME_APP(app), area);

  gtk_signal_connect(GTK_OBJECT(area),
		     "expose_event",
		     GTK_SIGNAL_FUNC(eventExpose),
		     context);

  gtk_signal_connect(GTK_OBJECT(area),
		     "configure_event",
		     GTK_SIGNAL_FUNC(eventConfigure),
		     context);

  appList = g_slist_prepend(appList, app);

  g_free(title);

  return app;
}

void closeSample(GtkWidget *app)
{
  Context *context = (Context *) gtk_object_get_data(GTK_OBJECT(app), "context");

  if (context->paragraph != NULL) {
    delete context->paragraph;
  }

  delete context;

  appList = g_slist_remove(appList, app);

  gtk_widget_destroy(app);

  if (appList == NULL) {
    gtk_main_quit();
  }
}

int main (int argc, char *argv[])
{
    LEErrorCode   fontStatus = LE_NO_ERROR;
    poptContext   ptctx;
    GtkWidget    *app;

    FT_Init_FreeType(&engine);

    gnome_init_with_popt_table("gnomelayout", "0.1", argc, argv, NULL, 0, &ptctx);

    guiSupport = new GnomeGUISupport();
    fontMap    = new GnomeFontMap(engine, "FontMap.Gnome", 24, guiSupport, fontStatus);
    font       = new ScriptCompositeFontInstance(fontMap);

    if (LE_FAILURE(fontStatus)) {
        FT_Done_FreeType(engine);
        return 1;
    }

    const char  *defaultArgs[] = {"Sample.txt", NULL};
    const char **args = poptGetArgs(ptctx);
    
    if (args == NULL) {
        args = defaultArgs;
    }

    for (int i = 0; args[i] != NULL; i += 1) {
       app = newSample(args[i]);
           
       gtk_widget_show_all(app);
    }
    
    poptFreeContext(ptctx);
    
    gtk_main();

    delete font;
    delete guiSupport;

    FT_Done_FreeType(engine);

    exit(0);
}
