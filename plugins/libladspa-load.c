/* search.c

   Free software by Richard W.E. Furse. Do with as you will. No
   warranty. */

/*****************************************************************************/

#include <dirent.h>
#include <glib.h>
#include <gmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/*****************************************************************************/

#include "ladspa.h"
#include "ladspa-utils.h"

/*****************************************************************************/

/* Search just the one directory. */
static void
LADSPADirectoryPluginSearch
(const char * pcDirectory, 
 LADSPAPluginSearchCallbackFunction fCallbackFunction) {

  char * pcFilename;
  GDir * psDirectory;
  LADSPA_Descriptor_Function fDescriptorFunction;
  const char * pcDirectoryEntry;
  GModule * psPluginHandle;

  psDirectory = g_dir_open(pcDirectory, 0, NULL);
  if (!psDirectory)
    return;

  while (1) {

    pcDirectoryEntry = g_dir_read_name(psDirectory);
    if (!pcDirectoryEntry) {
      g_dir_close(psDirectory);
      return;
    }

    pcFilename = g_build_filename( pcDirectory, pcDirectoryEntry, NULL );
    
    psPluginHandle = g_module_open (pcFilename, G_MODULE_BIND_LAZY );
    if (psPluginHandle) {
      /* This is a file and the file is a shared library! */

      //dlerror();
      if( g_module_symbol(psPluginHandle, "ladspa_descriptor", &fDescriptorFunction ) ) {
	/* We've successfully found a ladspa_descriptor function. Pass
           it to the callback function. */
	fCallbackFunction(pcFilename,
			  psPluginHandle,
			  fDescriptorFunction);
	g_free(pcFilename);
      }
      else {
	/* It was a library, but not a LADSPA one. Unload it. */
	g_module_close(psPluginHandle);
	g_free(pcFilename);
      }
    }
  }
}

/*****************************************************************************/

void 
LADSPAPluginSearch(LADSPAPluginSearchCallbackFunction fCallbackFunction) {

  char * pcBuffer;
  const char * pcEnd;
  const char * pcLADSPAPath;
  const char * pcStart;

  pcLADSPAPath = getenv("LADSPA_PATH");
  if (!pcLADSPAPath) {
    fprintf(stderr,
	    "Warning: You do not have a LADSPA_PATH "
	    "environment variable set. Using \"/usr/lib/ladspa:/usr/local/lib/ladspa\"\n");
    pcLADSPAPath = "/usr/lib/ladspa:/usr/local/lib/ladspa";
  }
  
  pcStart = pcLADSPAPath;
  while (*pcStart != '\0') {
    pcEnd = pcStart;
    while (*pcEnd != ':' && *pcEnd != '\0')
      pcEnd++;
    
    pcBuffer = malloc(1 + pcEnd - pcStart);
    if (pcEnd > pcStart)
      strncpy(pcBuffer, pcStart, pcEnd - pcStart);
    pcBuffer[pcEnd - pcStart] = '\0';
    
    LADSPADirectoryPluginSearch(pcBuffer, fCallbackFunction);
    free(pcBuffer);

    pcStart = pcEnd;
    if (*pcStart == ':')
      pcStart++;
  }
}

/*****************************************************************************/

/* EOF */
