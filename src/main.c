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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"

/* The following declaration serves as a normal prototype when compiling for Unix,
   and a DLL-import-declaration when compiling for Win32. */
G_MODULE_IMPORT int galan_main(int argc, char *argv[]);

PUBLIC int main(int argc, char *argv[]) {
  printf("gAlan version " VERSION "\n"
	 "Copyright (C) 1999 Tony Garnock-Jones\n"
         "Copyright (C) 2002 Torben Hohn\n"
	 "gAlan comes with ABSOLUTELY NO WARRANTY; for details, see the file\n"
	 "\"COPYING\" that came with the gAlan distribution.\n"
	 "This is free software, distributed under the GNU General Public\n"
	 "License. Please see \"COPYING\" or http://www.gnu.org/copyleft/gpl.txt\n");

  return galan_main(argc, argv);
}
