/* ------------------------------------------------------------------
 * Copyright (C) 2011 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdint.h>

#if defined(_MSC_VER)
#include <getopt.h>
#else
#include <unistd.h>
#endif

#include <stdlib.h>
#include "libAACenc/include/aacenc_lib.h"
#include "wavreader.h"

void usage(const char *name) {
  fprintf(stderr,
          "%s [-r bitrate] [-t aot] [-a afterburner] [-s sbr] [-v vbr] in.wav "
          "out.aac\n",
          name);
  fprintf(stderr, "Supported AOTs:\n");
  fprintf(stderr, "\t2\tAAC-LC\n");
  fprintf(stderr, "\t5\tHE-AAC\n");
  fprintf(stderr, "\t29\tHE-AAC v2\n");
  fprintf(stderr, "\t23\tAAC-LD\n");
  fprintf(stderr, "\t39\tAAC-ELD\n");
}
