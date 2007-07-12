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

#ifndef Generator_H
#define Generator_H

#include <math.h>

#define SAMPLE_RATE		gen_get_sample_rate()

#if WANT_FLOATING_POINT_SAMPLES


#define SAMPLE float
#define HAVE_FLOAT_SAMPLE 1
#undef HAVE_DOUBLE_SAMPLE

#define SAMPLE_ADD(a,b)		((a)+(b))
#define SAMPLE_MUL(a,b)		((a)*(b))
#define SAMPLE_UNIT		((SAMPLE) +1)
#define SAMPLE_MAX		((SAMPLE) +1)
#define SAMPLE_MIN		((SAMPLE) -1)
#define SAMPLE_ZERO		((SAMPLE) 0)
#else
#error Fixed-point samples not implemented.
#endif

#define CLIP_SAMPLE(x)	((x) > SAMPLE_MAX ? SAMPLE_MAX : ((x) < SAMPLE_MIN ? SAMPLE_MIN : (x)))

typedef gint32 SAMPLETIME;	/* must be signed. */

typedef struct AEvent AEvent;
typedef struct EventLink EventLink;
typedef struct InputSignalDescriptor InputSignalDescriptor;
typedef struct OutputSignalDescriptor OutputSignalDescriptor;
typedef struct GeneratorClass GeneratorClass;
typedef struct Generator Generator;
typedef struct ControlDescriptor ControlDescriptor;
typedef struct Control Control;
typedef struct ControlPanel ControlPanel;
typedef struct AClock AClock;

typedef enum AClockReason {
  CLOCK_DISABLE = 0,	/**< sent to clock when it's deselected as master clock */
  CLOCK_ENABLE,		/**< sent to clock when it's selected as master clock */

  CLOCK_MAX_REASON	/**< end-of-enum marker */
} AClockReason;

/**
 * \brief This is the function type that is called on receive of an Event.
 *
 * \param recip The Generator receiving the event.
 * \param event The AEvent to process.
 */

typedef void (*AEvent_handler_t)(Generator *recip, AEvent *event);

/**
 * \brief This is the function type that is called to turn an AClock on and off.
 *
 * \param clock The Clock receiving the event.
 * \param reason one of CLOCK_ENABLE or CLOCK_DISABLE.
 */

typedef void (*AClock_handler_t)(AClock *clock, AClockReason reason);

/**
 * \brief This function type is an output Generator
 * 
 * \param source The Generator which generates the realtime signal.
 * \param buffer where to put the signal.
 * \param number of samples to generate.
 *
 * \return TRUE if the function generated data. A filter which receives FALSE when reading its input
 *         should return FALSE also and do nothing.
 */

typedef gboolean (*AGenerator_t)(Generator *source, SAMPLE *buffer, int buflen);

/**
 * \brief This is function type used for pickle and unpickle.
 *
 * The generic part of the Generator is allready pickled/unpickled when this function is
 * called. This is why this function also receives the ObjectStoreItem as a Parameter.
 *
 * \param gen The Generator which shall be pickled/unpickled.
 * \param item The ObjectStoreItem associated with this Generator.
 * \param db The ObjectStore used.
 */

typedef void (*AGenerator_pickle_t)(Generator *gen, ObjectStoreItem *item, ObjectStore *db);

/**
 * AEvent can have several types which must be handled by aevent_copy(), the other two...
 */

typedef enum AEventKind {
  AE_ANY = -1,		/**< wildcard, for matching all incoming evts */
  AE_NONE = 0,		/**< 'null event' kind */
  AE_NUMBER,		/**< a numeric message */
  AE_REALTIME,		/**< a realtime-has-elapsed message */
  AE_STRING,		/**< a string message  */
  AE_NUMARRAY,		/**< an array message  */
  AE_DBLARRAY,		/**< a double array message  */
  AE_MIDIEVENT,		/**< a midi event  */

  AE_LAST_EVENT_KIND	/**< end-of-enum marker */
} AEventKind;


/**
 * \brief represents a link between 2 Generators.
 */

struct EventLink {
  int is_signal;		/**< true for signal link, false for event link */
  Generator *src;
  gint src_q;
  Generator *dst;
  gint dst_q;
};

typedef struct arr {
    int len;
    SAMPLE *numbers;
} Array;

typedef struct darr {
    int len;
    double *numbers;
} DArray;

typedef struct midiev {
    char midistring[4];
    int len;
} MidiEv;

struct AEvent {			/**< audio event */
  AEventKind kind;		/**< what kind of event? */
  Generator *src, *dst;
  int src_q, dst_q;
  SAMPLETIME time;		/**< sample number - note at 32-bits allows for only ~27
				     hours of (continuous) play! */

  /** message body */
  union {
    gdouble number;		/**< AE_NUMBER */
    gint32 integer;		/**< AE_REALTIME and AE_MASTER_PLAY */
    gchar *string;		/**< AE_STRING */
    Array array;		/**< AE_NUMARRAY */
    DArray darray;		/**< AE_DBLARRAY */
    MidiEv midiev;		/**< AE_MIDIEVENT */
  } d;
};

struct AClock {
  Generator *gen;
  char *name;
  AClock_handler_t handler;
  gboolean selected;
};

#define SIG_FLAG_REALTIME	0x00000001
#define SIG_FLAG_RANDOMACCESS	0x00000002
#define SIG_FLAG_OPENGL		0x00000004

struct InputSignalDescriptor {
  const char *name;
  guint flags;
};

struct OutputSignalDescriptor {
  const char *name;
  guint32 flags;
  struct {
    AGenerator_t realtime;
    struct {
      SAMPLETIME (*get_range)(Generator *generator, OutputSignalDescriptor *sig);
      gboolean (*get_samples)(Generator *generator, OutputSignalDescriptor *sig,
			      SAMPLETIME offset, SAMPLE *buffer, int length);
    } randomaccess;
    gboolean (*render_gl)(Generator *generator );
  } d;
};

#define GEN_DOUBLE_TO_INT(x)	((gint32) floor((x) + 0.5))

struct GeneratorClass {

  char *name;				/* human-readable name - and also
					   tag for save/load, so MUST be unique */
  char *tag;				/* this is the new tag for save/load 
					   it is needed because of the ladspas */

  /* Event inputs */
  gint in_count;
  char **in_names;
  AEvent_handler_t *in_handlers;

  /* Event outputs */
  gint out_count;
  char **out_names;

  /* Signal inputs */
  gint in_sig_count;
  InputSignalDescriptor *in_sigs;

  /* Signal outputs */
  gint out_sig_count;
  OutputSignalDescriptor *out_sigs;

  /* Control descriptors */
  ControlDescriptor *controls;
  int numcontrols;

  /* Instance methods */
  int (*initialize_instance)(Generator *g);
  void (*destroy_instance)(Generator *g);

  AGenerator_pickle_t unpickle_instance;
  AGenerator_pickle_t pickle_instance;
};

struct Generator {
  GeneratorClass *klass;
  char *name;

  /* Links to other Generators */
  GList **in_events;		/* array of GLists of EventLinks */
  GList **out_events;
  GList **in_signals;
  GList **out_signals;

  //EventQ *input_events;
  /* Sample caching for multiple (realtime) reads */
  /* note that we don't cache randomaccess signal generators */
  SAMPLETIME *last_sampletime;
  SAMPLE **last_buffers;
  int *last_buflens;
  gboolean *last_results;

  /* Controls */
  GList *controls;

  /* per-class data */
  void *data;
};

/*=======================================================================*/
/* Managing AEvents */
extern void gen_init_aevent(AEvent *e, AEventKind kind,
			    Generator *src, int src_q,
			    Generator *dst, int dst_q, SAMPLETIME time);

/*=======================================================================*/
/* Managing GeneratorClasses */
extern GeneratorClass *gen_new_generatorclass(const char *name, gboolean prefer,
					      gint count_event_in, gint count_event_out,
					      InputSignalDescriptor *input_sigs,
					      OutputSignalDescriptor *output_sigs,
					      ControlDescriptor *controls,
					      gboolean (*initializer)(Generator *),
					      void (*destructor)(Generator *),
					      AGenerator_pickle_t unpickle_instance,
					      AGenerator_pickle_t pickle_instance);

extern GeneratorClass *gen_new_generatorclass_with_different_tag(const char *name, const char *tag, gboolean prefer,
					      gint count_event_in, gint count_event_out,
					      InputSignalDescriptor *input_sigs,
					      OutputSignalDescriptor *output_sigs,
					      ControlDescriptor *controls,
					      gboolean (*initializer)(Generator *),
					      void (*destructor)(Generator *),
					      AGenerator_pickle_t unpickle_instance,
					      AGenerator_pickle_t pickle_instance);

extern void gen_kill_generatorclass(GeneratorClass *g);
extern void gen_configure_event_input(GeneratorClass *g, gint index,
				      const char *name, AEvent_handler_t handler);
extern void gen_configure_event_output(GeneratorClass *g, gint index, const char *name);

/*=======================================================================*/
/* Managing Generators */
extern Generator *gen_new_generator(GeneratorClass *k, char *name);
extern void gen_kill_generator(Generator *g);

extern Generator *gen_unpickle(ObjectStoreItem *item);
extern ObjectStoreItem *gen_pickle(Generator *g, ObjectStore *db);

extern Generator *gen_clone( Generator *g, ControlPanel *cp );

/*=======================================================================*/
/* Managing links between Generators */
extern EventLink *gen_link(int is_signal, Generator *src, gint src_q, Generator *dst, gint dst_q);
extern EventLink *gen_find_link(int is_signal, Generator *src, gint src_q, Generator *dst, gint dst_q);
extern void gen_unlink(EventLink *lnk);

/*=======================================================================*/
/* Managing controls for Generators */
extern void gen_register_control(Generator *g, Control *c);
extern void gen_deregister_control(Generator *g, Control *c);
extern void gen_update_controls(Generator *g, int index); /* not very elegant */

/*=======================================================================*/
/* Getting audio from Generators and pushing events out event-outputs */
extern gboolean gen_read_realtime_input(Generator *g, gint index, int attachment_number,
					SAMPLE *buffer, int buflen);
					/* use attachment_number == -1 for 'mix all together' */
extern gboolean gen_read_realtime_output(Generator *g, gint index, SAMPLE *buffer, int buflen);

extern SAMPLETIME gen_get_randomaccess_output_range(Generator *g, gint index);
extern SAMPLETIME gen_get_randomaccess_input_range(Generator *g, gint index,
						   int attachment_number);
extern gboolean gen_read_randomaccess_input(Generator *g, gint index, int attachment_number,
					    SAMPLETIME offset, SAMPLE *buffer, int buflen);
					/* attachment -1 is meaningless here */
					/* there's no point in mixing many RA inputs (?) */

extern gboolean gen_render_gl(Generator *g, gint index, int attachment_number );

extern void gen_send_events(Generator *g, gint index, int attachment_number, AEvent *e);
					/* use attachment_number == -1 for 'all' */
					/* does not keep the reference to e */

/*=======================================================================*/
/* Event-loop system (most of these externs reside in event.c) */

extern SAMPLETIME gen_current_sampletime;	/* Don't use directly; use the macro below */
#define gen_get_sampletime()	(gen_current_sampletime)

extern void gen_post_aevent(AEvent *e);		/* queue an event */
						/* does not keep the reference to e */

extern void gen_purge_event_queue_refs(Generator *g);
extern void gen_purge_inputevent_queue_refs(Generator *g);

extern void gen_register_realtime_fn(Generator *g, AEvent_handler_t func);
extern void gen_deregister_realtime_fn(Generator *g, AEvent_handler_t func);
extern void gen_send_realtime_fns(AEvent *e);

/* gint gen_mainloop_once(void)
 *
 * - processes events up to gen_get_sampletime()
 * - returns the number of sample ticks until next event comes due, or MAXIMUM_REALTIME_STEP
 *
 * Note that it does *not* send any AE_REALTIME events. This is the job of the 'master clock'
 * routine that is calling gen_mainloop_once(). So, the master clock's main loop involves:
 *
 * - waiting (interactively) until it's appropriate to get more samples for playback/recording
 * - calling gen_mainloop_once()
 * - sending an AE_REALTIME event to the registered realtime functions
 * - advancing gen_get_sampletime() by calling gen_advance_clock()
 * - playing back the fetched samples
 * - looping
 */
extern gint gen_mainloop_once(void);
extern void gen_advance_clock(gint delta);

extern void gen_mainloop_do_checks(void);

/*=======================================================================*/
/* Master clock system (most of these externs are in clock.c) */

extern int  gen_get_sample_rate(void);

extern void gen_register_clock_listener(GHookFunc hook, gpointer data);
extern void gen_deregister_clock_listener(GHookFunc hook, gpointer data);

extern AClock *gen_register_clock(Generator *gen, char *name, AClock_handler_t handler);
extern void gen_deregister_clock(AClock *clock);

extern void gen_set_default_clock(AClock *clock);

extern AClock **gen_enumerate_clocks(void);
extern void gen_select_clock(AClock *clock);
extern void gen_stop_clock(void);	/* useful? who knows. */

extern void gen_clock_mainloop(void);
extern void gen_clock_mainloop_have_remaining(gint remaining);

extern GHashTable *get_generator_classes( void );

/*=======================================================================*/
/* Module setup/shutdown */

extern void init_generator(void);
extern void init_generator_thread(void);
extern void done_generator(void);
extern void init_clock(void);
extern void done_clock(void);

extern void init_event(void);
#endif
