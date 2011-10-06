
#ifndef CLUTTER_GST_SHADERS_H
#define CLUTTER_GST_SHADERS_H

#include <clutter/clutter.h>

/* Copied from test-shaders */

/* These variables are used instead of the standard GLSL variables on
   GLES 2 */
#ifdef COGL_HAS_GLES

#define GLES2_VARS              \
  "precision mediump float;\n"
#define TEX_COORD "cogl_tex_coord_in[0]"
#define COLOR_VAR "cogl_color_in"

#else /* COGL_HAS_GLES */

#define GLES2_VARS ""
#define TEX_COORD "gl_TexCoord[0]"
#define COLOR_VAR "gl_Color"

#endif /* COGL_HAS_GLES */

/* a couple of boilerplate defines that are common amongst all the
 * sample shaders
 */

#define FRAGMENT_SHADER_VARS      \
  GLES2_VARS

/* FRAGMENT_SHADER_END: apply the changed color to the output buffer correctly
 * blended with the gl specified color (makes the opacity of actors work
 * correctly).
 */
#define FRAGMENT_SHADER_END                             \
     "  gl_FragColor = gl_FragColor * " COLOR_VAR ";"

#endif

