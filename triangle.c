/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2012, OtherCrashOverride
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// A rotating cube rendered with OpenGL|ES. Three images used as textures on the cube faces.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include "lo/lo.h"
#include "cv.h"

#include "bcm_host.h"

#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#include "cube_texture_and_coords.h"


#include "triangle.h"
#include <pthread.h>


#define PATH "./"

#define IMAGE_SIZE_WIDTH 1920
#define IMAGE_SIZE_HEIGHT 1080

#ifndef M_PI
   #define M_PI 3.141592654
#endif
  

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
   GLuint tex;
// model rotation vector and direction
   GLfloat rot_angle_x_inc;
   GLfloat rot_angle_y_inc;
   GLfloat rot_angle_z_inc;
// current model rotation angles
   GLfloat rot_angle_x;
   GLfloat rot_angle_y;
   GLfloat rot_angle_z;
// current distance from camera
   GLfloat distance;
   GLfloat distance_inc;
} CUBE_STATE_T;

static void init_ogl(CUBE_STATE_T *state);
static void init_model_proj(CUBE_STATE_T *state);
static void reset_model(CUBE_STATE_T *state);
static void redraw_scene(CUBE_STATE_T *state);
static void update_model(CUBE_STATE_T *state);
static void init_textures(CUBE_STATE_T *state);
static void exit_func(void);
static volatile int terminate;
static CUBE_STATE_T _state, *state=&_state;

static void init_osc();
void lo_error(int num, const char *m, const char *path);
int osc_handler_dst_point(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);
int osc_handler_matrix(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);
int osc_handler_camtranslate(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);
static void* eglImage = 0;
static pthread_t thread1;


/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns: void
 *
 ***********************************************************/
static void init_ogl(CUBE_STATE_T *state)
{
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      //EGL_SAMPLES, 4,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   
   EGLConfig config;

   // get an EGL display connection
   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(state->display!=EGL_NO_DISPLAY);

   // initialize the EGL display connection
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);

   // get an appropriate EGL frame buffer configuration
   // this uses a BRCM extension that gets the closest match, rather than standard which returns anything that matches
   result = eglSaneChooseConfigBRCM(state->display, attribute_list, &config, 1, &num_config);
   assert(EGL_FALSE != result);

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, NULL);
   assert(state->context!=EGL_NO_CONTEXT);

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;
      
   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;        

   dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );
         
   dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);
      
   nativewindow.element = dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );
      
   state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
   assert(state->surface != EGL_NO_SURFACE);

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);

   // Set background color and clear buffers
   glClearColor(0.f, 0.f, 0.f, 1.0f);

   // Enable back face culling.
   glEnable(GL_CULL_FACE);

   glMatrixMode(GL_MODELVIEW);
}

/***********************************************************
 * Name: init_model_proj
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the OpenGL|ES model to default values
 *
 * Returns: void
 *
 ***********************************************************/
static void init_model_proj(CUBE_STATE_T *state)
{
   float nearp = 0.5f;
   float farp = 100.0f;
   float hht;
   float hwd;

   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

   glViewport(0, 0, (GLsizei)state->screen_width, (GLsizei)state->screen_height);
      
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   //~ hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   hht = nearp;
   hwd = (float)state->screen_width / (float)state->screen_height * nearp;

   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
   
   printf("window size : %dx%d\n", state->screen_width, state->screen_height);
   
   glEnableClientState( GL_VERTEX_ARRAY );
   glScalef(1.,1.,0.);
   glVertexPointer( 3, GL_BYTE, 0, quadx );

   reset_model(state);
}

/***********************************************************
 * Name: reset_model
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Resets the Model projection and rotation direction
 *
 * Returns: void
 *
 ***********************************************************/
static void reset_model(CUBE_STATE_T *state)
{
    // reset model position
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(perspecMat);
}

/***********************************************************
 * Name: update_model
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Updates model projection to current position/rotation
 *
 * Returns: void
 *
 ***********************************************************/
static void update_model(CUBE_STATE_T *state)
{
    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION); 
    glLoadMatrixf(perspecMat);
}

/***********************************************************
 * Name: redraw_scene
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description:   Draws the model and calls eglSwapBuffers
 *                to render to screen
 *
 * Returns: void
 *
 ***********************************************************/
static void redraw_scene(CUBE_STATE_T *state)
{
   // Start with a clear screen
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   // draw first 4 vertices
   glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);

   eglSwapBuffers(state->display, state->surface);
}

/***********************************************************
 * Name: init_textures
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description:   Initialise OGL|ES texture surfaces to use image
 *                buffers
 *
 * Returns: void
 *
 ***********************************************************/
static void init_textures(CUBE_STATE_T *state)
{
   //// load three texture buffers but use them on six OGL|ES texture surfaces
   glGenTextures(1, &state->tex);

   glBindTexture(GL_TEXTURE_2D, state->tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_SIZE_WIDTH, IMAGE_SIZE_HEIGHT, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


   /* Create EGL Image */
   eglImage = eglCreateImageKHR(
                state->display,
                state->context,
                EGL_GL_TEXTURE_2D_KHR,
                (EGLClientBuffer)state->tex,
                0);
    
   if (eglImage == EGL_NO_IMAGE_KHR)
   {
      printf("eglCreateImageKHR failed.\n");
      exit(1);
   }

   // Start rendering
   pthread_create(&thread1, NULL, video_decode_test, eglImage);

   // setup overall texture environment
   glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glEnable(GL_TEXTURE_2D);

   // Bind texture surface to current vertices
   glBindTexture(GL_TEXTURE_2D, state->tex);
}

/***********************************************************
 * Name: init_osc
 *
 * Arguments:
 *       none
 *
 * Description:   initialize OSC server
 *
 * Returns: void
 *
 ***********************************************************/
static void init_osc(){
     /* start a new server on port 7770 */
    lo_server_thread osc_server = lo_server_thread_new("7770", lo_error);
    
    /* add a function to wait for /quad message */
    lo_server_thread_add_method(osc_server, "/dst_point", "ffffffff", osc_handler_dst_point, NULL);
    lo_server_thread_add_method(osc_server, "/map_matrix", "fffffffff", osc_handler_matrix, NULL);
    
    lo_server_thread_start(osc_server);

}

/***********************************************************
 * Name: lo_error
 *
 * Arguments:
 *       num : error number
 *       msg : error message
 *       path : message path that causes error
 *
 * Description:   handle and print out OSC error
 *
 * Returns: void
 *
 ***********************************************************/
void lo_error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}

/***********************************************************
 * Name: quad_handler
 *
 * Arguments:
 *       refer to liblo documentation
 *
 * Description:   handle OSC message
 *
 * Returns: void
 *
 ***********************************************************/
int osc_handler_dst_point(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    CvPoint2D32f src[8], dst[8];
    CvMat *map_matrix, *src_mat, *dst_mat;
    int i;
    float *pt,*pt2;
    
	map_matrix=cvCreateMat(3, 3, CV_32FC1 );
	dst_mat=cvCreateMat(4, 1, CV_32FC2 );
	src_mat=cvCreateMat(4, 1, CV_32FC2 );
    
    for (i=0;i<4;i++){
        src[i].x=srcPoint[i*2];
        src[i].y=srcPoint[i*2+1];
    }
  
    for (i=0;i<4;i++){
        dst[i].x=argv[i*2]->f;
        dst[i].y=argv[i*2+1]->f;
    }
    
    //~ printf("\t src point \t\t dst point \n");
    //~ for (i=0;i<4;i++){
        //~ printf("\t%.3f\t%.3f\t\t%.3f\t%.3f",src[i].x, src[i].y, dst[i].x, dst[i].y);
        //~ printf("\n");
    //~ }

    cvGetPerspectiveTransform(src, dst, map_matrix);
    
    pt=dst_mat->data.fl;
    pt2=src_mat->data.fl;
    for ( i=0;i<4;i++) {
        *(pt++)=dst[i].x;
        *(pt++)=dst[i].y;
        *(pt2++)=src[i].x;
        *(pt2++)=src[i].y;
    }

        
    cvPerspectiveTransform(dst_mat, src_mat, map_matrix);
    //~ printf("src2 matrix :\n");
    //~ pt=src_mat->data.fl;
    //~ for (i=0;i<4;i++){
        //~ printf("%.3f\t%.3f\n",*(pt++), *(pt++));
    //~ }
    //~ printf("-----------------\n");
    
    //~ pt=map_matrix->data.fl;
    //~ for (i=0;i<9;i++){
        //~ printf("%.3f\t",*(pt++));
    //~ }
    //~ printf("\n");
//~ 
    //~ pt=map_matrix->data.fl;
    //~ perspecMat[0]=*(pt++);
    //~ perspecMat[1]=*(pt++);
    //~ perspecMat[2]=0.;    
    //~ perspecMat[3]=*(pt++);
    //~ 
    //~ perspecMat[4]=*(pt++);
    //~ perspecMat[5]=*(pt++);
    //~ perspecMat[6]=0.;
    //~ perspecMat[7]=*(pt++);
    //~ 
    //~ perspecMat[8]=0.;
    //~ perspecMat[9]=0.;
    //~ perspecMat[10]=1.;
    //~ perspecMat[11]=0.;
    //~ 
    //~ perspecMat[12]=*(pt++);
    //~ perspecMat[13]=*(pt++);
    //~ perspecMat[14]=0.;
    //~ perspecMat[15]=*(pt++);

    pt=map_matrix->data.fl;
    perspecMat[0]=*(pt++);
    perspecMat[4]=*(pt++);
    perspecMat[8]=0.;    
    perspecMat[12]=*(pt++);
    
    perspecMat[1]=*(pt++);
    perspecMat[5]=*(pt++);
    perspecMat[9]=0.;
    perspecMat[13]=*(pt++);
    
    perspecMat[2]=0.;
    perspecMat[6]=0.;
    perspecMat[10]=1.;
    perspecMat[14]=0.;
    
    perspecMat[3]=*(pt++);
    perspecMat[7]=*(pt++);
    perspecMat[11]=0.;
    perspecMat[15]=*(pt++);
    
    //~ printf("perspecMat\n");
    //~ for (i=0;i<16;i++){
        //~ if ( i%4 ==0 ) printf("\n");
        //~ //printf("%.3f\t%.3f\t%.3f\t%.3f\n", perspecMat[i*4], perspecMat[i*4+1], perspecMat[i*4+2], perspecMat[i*4+3]);
        //~ printf("\t%.3f",perspecMat[i]);
    //~ }
    //~ printf("\n");
    
    return 0;
}

int osc_handler_matrix(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
     int i = 0;
    
    perspecMat[0]=argv[i++]->f;
    perspecMat[4]=argv[i++]->f;
    perspecMat[8]=0.;    
    perspecMat[12]=argv[i++]->f;
    
    perspecMat[1]=argv[i++]->f;
    perspecMat[5]=argv[i++]->f;
    perspecMat[9]=0.;
    perspecMat[13]=argv[i++]->f;
    
    perspecMat[2]=0.;
    perspecMat[6]=0.;
    perspecMat[10]=1.;
    perspecMat[14]=0.;
    
    perspecMat[3]=argv[i++]->f;
    perspecMat[7]=argv[i++]->f;
    perspecMat[11]=0.;
    perspecMat[15]=argv[i++]->f;
    
    //~ printf("perspecMat\n");
    //~ for (i=0;i<16;i++){
        //~ if ( i%4 ==0 ) printf("\n");
        //~ //printf("%.3f\t%.3f\t%.3f\t%.3f\n", perspecMat[i*4], perspecMat[i*4+1], perspecMat[i*4+2], perspecMat[i*4+3]);
        //~ printf("\t%.3f",perspecMat[i]);
    //~ }
    //~ printf("\n");
    
    return 0;
}

int osc_handler_camtranslate(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data){
    state->distance=argv[0]->f;
    return 0;
}
//------------------------------------------------------------------------------

static void exit_func(void)
// Function to be passed to atexit().
{
   if (eglImage != 0)
   {
      if (!eglDestroyImageKHR(state->display, (EGLImageKHR) eglImage))
         printf("eglDestroyImageKHR failed.");
   }

   // clear screen
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   // Release OpenGL resources
   eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
   eglDestroySurface( state->display, state->surface );
   eglDestroyContext( state->display, state->context );
   eglTerminate( state->display );

   printf("\ncube closed\n");
} // exit_func()



//==============================================================================

int main ()
{
   bcm_host_init();
   printf("Note: ensure you have sufficient gpu_mem configured\n");

   // Clear application state
   memset( state, 0, sizeof( *state ) );
      
   // Start OGLES
   init_ogl(state);

   // Setup the model world
   init_model_proj(state);

   // initialise the OGLES texture(s)
   init_textures(state);
   
   // initialize OSC server
   init_osc();

   while (!terminate)
   {
      update_model(state);
      redraw_scene(state);
   }
   exit_func();
   return 0;
}

