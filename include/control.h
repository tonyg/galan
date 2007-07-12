/* gAlan - Graphical Audio Language
 * Copyright (C) 1999 Tony Garnock-Jones
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

#ifndef Control_H
#define Control_H

/*
  Controls we want to play with:

  - sliders
  - knobs
  - togglebuttons
  - buttons
  - arbitrary others
 */

typedef enum ControlKind ControlKind;
/* moved typedef of ControlDescriptor to generator.h */
/* moved typedef of Control to generator.h */

enum ControlKind {
  CONTROL_KIND_NONE = 0,		/* used to terminate lists of ControlDescriptors */
  CONTROL_KIND_SLIDER,
  CONTROL_KIND_KNOB,
  CONTROL_KIND_TOGGLE,
  CONTROL_KIND_BUTTON,
  CONTROL_KIND_USERDEF,
  CONTROL_KIND_PANEL,

  CONTROL_MAX_KIND
};

typedef void (* ControlMove_cb)(struct Control *);


enum ControlPanelBackgroundType {
    CONTROL_PANEL_BG_DEFAULT,
    CONTROL_PANEL_BG_IMAGE,
    CONTROL_PANEL_BG_COLOR,
    CONTROL_PANEL_BG_GRADIENT
};

struct ControlPanel {
    GtkWidget *scrollwin, *fixedwidget;
    char *name;
    gboolean visible;
    struct sheet *sheet;
    int w,h;
    GtkWidget *sizer_ebox, *sizer_image;

    int sizer_x, sizer_y;
    int sizer_saved_x, sizer_saved_y;
    int sizer_moving;
    int sizer_visible;


    // background rendering...
    //char *current_bg;

    enum ControlPanelBackgroundType  bg_type;
    GdkColor color1;
    GdkColor color2;
    GdkColor frame_color;
    guint16  frame_alpha;
    
    char *bg_image_name;
};

struct ControlDescriptor {
  ControlKind kind;			/* kind of control */
  const char *name;				/* default control name */
  gdouble min, max, step, page;		/* for sliders and knobs */
  int size;				/* vertical size; optional; only for sliders - 0 =>deflt */
  gboolean allow_direct_edit;		/* put in a GtkEntry? - only for sliders, knobs */
  gboolean is_dst_gen;			/* true if g is a target, false if g is a source */
  int queue_number;			/* which output event queue to send from, if gen is a
					   src gen, or event queue to send to, if gen is a
					   dst gen. */

  void (*initialize)(Control *control);
  void (*destroy)(Control *control);

  void (*refresh)(Control *control);
  gpointer refresh_data;
};

struct Control {
  ControlDescriptor *desc;
  ControlPanel *panel;
  char *name;				/* overriding name. Set to NULL to use default. */
  int testbg_active;
  gdouble min, max, step, page;		/* overrides desc's values */

  gboolean frame_visible;
  gboolean name_visible;
  gboolean entry_visible;
  gboolean control_visible;

  int moving, saved_x, saved_y;		/* variables to implement drag-moving of controls */
  int x, y;				/* position within control window */
  gboolean events_flow;			/* TRUE => sends AEvents, FALSE => is silent */
  gboolean kill_me;			/* there is a race in the control deletion with the update thread. 
					   to delete a control remove it from the generator, and set kill_me,
					   the call control_update on it. */
  int update_refcount;

  GtkWidget *widget;			/* the control itself */
  GtkWidget *whole;			/* the control embedded in some wrapping */
  GtkWidget *title_frame;		/* the frame displaying the whole control */
  GtkWidget *title_label;		/* the label displaying the name */
  GtkWidget *entry;			/* The Text field */

  GtkWidget *ebox;			/* An Eventbox for speed */

  ControlPanel *this_panel;
  ControlMove_cb move_callback;
  Generator *g;				/* source for output events; owner of control _OR_
					   target of output events; also owner of control?? */
  void *data;				/* user data (mostly for userdef'd controls) */
};

extern GtkWidget *control_panel;

extern Control *control_new_control(ControlDescriptor *desc, Generator *g, ControlPanel *panel );
extern void control_kill_control(Control *c);

extern Control *control_unpickle(ObjectStoreItem *item);
extern ObjectStoreItem *control_pickle(Control *c, ObjectStore *db);

extern void control_emit(Control *c, gdouble number);
extern void control_update_names(Control *c);
extern void control_update_range(Control *c);
extern void control_update_value(Control *c);
extern void control_set_value(Control *c, gfloat value);
extern void control_moveto(Control *c, int x, int y);
extern void control_update_bg(Control *c);

extern void init_control(void);
extern void init_control_thread(void);
extern Control *control_clone( Control *c, Generator *g, ControlPanel *cp );

extern void show_control_panel(void);
extern void hide_control_panel(void);
extern void reset_control_panel(void);

extern ControlPanel *control_panel_new( char *name, gboolean visible, struct sheet *sheet );
extern void control_panel_register_panel( ControlPanel *panel, char *name, gboolean add_fixed );
extern void control_panel_unregister_panel( ControlPanel *panel );
extern void update_panel_name( ControlPanel *panel );
extern ControlPanel *control_panel_unpickle(ObjectStoreItem *item);
extern ObjectStoreItem *control_panel_pickle(ControlPanel *cp, ObjectStore *db);

/* these functions can go into ControlDescriptor.refresh (with a suitable refresh_data) */
extern void control_int32_updater(Control *c);
extern void control_double_updater(Control *c);

#endif
