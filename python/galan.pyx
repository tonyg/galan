

cdef extern from "cobject.h":
   object PyCObject_FromVoidPtr(void *cobj, void *destruct)
   void *PyCObject_AsVoidPtr(object cobject)
   


cdef extern from "glib.h":
   ctypedef struct GHashTable:
     int opaque

   ctypedef struct GList:
     void *data


   ctypedef void *gpointer 
   ctypedef void (*GHFunc)( gpointer key, gpointer value, gpointer user_data )

   gpointer g_hash_table_lookup (GHashTable *hash_table, gpointer key)
   void g_hash_table_foreach (GHashTable *hash_table, GHFunc *func,  gpointer user_data)

   int g_hash_table_size( GHashTable *hash_table )

   GList *g_list_append( GList *list, gpointer data )
   GList *g_list_next( GList *list )

   void g_thread_init   (void      *vtable)


cdef extern from "gtk/gtk.h":
    void gtk_set_locale()
    void gtk_init( int *argc, char **argv)
    void gtk_main()

cdef extern from "gdk/gdk.h":
    void gdk_rgb_init()
    void gdk_threads_init()

cdef extern from "objectstore.h":
    void init_objectstore()
 

cdef extern from "generator.h":

    cdef struct Generator:
      char *name 
      
    cdef struct GeneratorClass:
      char *name
      
#    ctypedef class PyrGeneratorClass [type GeneratorClass]:
#      char *name

    cdef struct EventLink:
      int is_signal
      Generator *src
      int src_q
      Generator *dst
      int dst_q

    Generator *gen_new_generator(GeneratorClass *k, char *name)
    void gen_kill_generator(Generator *g)

    #Generator *gen_unpickle(ObjectStoreItem *item)
    #ObjectStoreItem *gen_pickle(Generator *g, ObjectStore *db)

    EventLink *gen_link(int is_signal, Generator *src, int src_q, Generator *dst, int dst_q)
    EventLink *gen_find_link(int is_signal, Generator *src, int src_q, Generator *dst, int dst_q)
    void gen_unlink(EventLink *lnk)

    void init_generator()
    void init_event()
    void init_clock()
    GHashTable *get_generator_classes()


cdef extern from "plugin.h":
    void init_plugins()

cdef extern from "comp.h":
    void init_comp()

#cdef extern from "clock.h":
#    void init_clock()

cdef extern from "gencomp.h":
    void init_gencomp()

cdef extern from "prefs.h":
    void init_prefs()

cdef extern from "control.h":
    void init_control()

cdef extern from "gui.h":
    void init_gui()




cdef class PyGenClass:
    cdef GeneratorClass *klass

    def __init__( self, PyGenClass k ):
       if k != None:
          self.klass = k.klass
       else:
          self.klass = NULL

cdef class PyGen:
    cdef Generator *g

    def __init__( self, object klass, char *name ):
       cdef GeneratorClass *gc
       #gc = <GeneratorClass *>g_hash_table_lookup( get_generator_classes(), klassname )
       gc = <GeneratorClass *>PyCObject_AsVoidPtr( klass )

       self.g = gen_new_generator( gc, name )

    def link( self, int is_signal, int src_q, PyGen g, int dst_q ):
       gen_link( is_signal, self.g, src_q, g.g, dst_q )


cdef void fill_pylist( gpointer key, gpointer value, object list ):
    list.append( <char *> key )

cdef class PyGenClasses:
    cdef GHashTable *genclasses

    def __init__( self ):
       self.genclasses = get_generator_classes()

    def __getitem__( self, char *name ):
       cdef void *retval
       retval = g_hash_table_lookup( self.genclasses, name )
       return PyCObject_FromVoidPtr( retval, NULL ) 

    def __len__( self ):
       return g_hash_table_size( self.genclasses )

    def keys( self ):
        retval = []	
        g_hash_table_foreach( self.genclasses, <void (*)(void (*),void (*),void (*))> fill_pylist, <void *> retval )
        return retval



def init_all():

    cdef int n
    n=0
    g_thread_init(NULL)

    gdk_threads_init()
    #main_thread = g_thread_self();
    gtk_set_locale()
    gtk_init( &n, NULL )
    gdk_rgb_init()
    
    init_generator()

    init_clock()
    init_event()
    init_control()
    init_gui()
    init_comp()
    init_gencomp()
    init_prefs()
    init_objectstore()
    init_plugins()

def mainloop():
    gtk_main()
