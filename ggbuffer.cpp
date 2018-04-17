//
//program: ggbuffer
//author:  Gordon Griesel
//date:    2016, 2018
//
//render pixels to OpenGL frame buffer.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>

//X Windows variables
Display *dpy;
Window win;
GLXContext glc;

typedef float Flt;
typedef Flt Vec[3];
typedef unsigned char cVec[3];

void showFrameRate();
void setup_screen_res(const int w, const int h);
void initXWindows(int w, int h);
void init_opengl();
void cleanupXWindows();
void check_resize(XEvent *e);
void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void physics();
void render();

int xres=640, yres=480;
int xres3, xres4;
unsigned char *screendata = NULL;
Flt pos[3]={20.0,200.0,0.0};
Flt vel[3]={3.0,0.0,0.0};
int showOpengl = 0;

//Setup timers-----------------------------------------------------------------
const double oothousand = 1.0 / 1000.0;
struct timeval gamestarttv;
//My version of GetTickCount() or SDL_GetTicks()
int xxGetTicks() {
	struct timeval end;
	gettimeofday(&end, NULL);
	//long seconds  = end.tv_sec  - gamestarttv.tv_sec;
	//long useconds = end.tv_usec - gamestarttv.tv_usec;
	//long mtime = (seconds*1000 + useconds*oothousand) + 0.5;
	//return (int)mtime;
	//code above compressed...
	return (int)((end.tv_sec - gamestarttv.tv_sec) * 1000 +
		(end.tv_usec - gamestarttv.tv_usec) * oothousand) + 0.5;
}


int main(int argc, char *argv[])
{
	int x=0, y=0;
	if (argc > 2) {
		x = atoi(argv[1]);
		y = atoi(argv[2]);
	}
	if (argc > 3)
		showOpengl = 1;
	initXWindows(x, y);
	setup_screen_res(xres, yres);
	init_opengl();
	int done = 0;
	gettimeofday(&gamestarttv, NULL);
	while (!done) {
		while (XPending(dpy)) {
			XEvent e;
			XNextEvent(dpy, &e);
			check_resize(&e);
			check_mouse(&e);
			done = check_keys(&e);
		}
		physics();
		render();
		glXSwapBuffers(dpy, win);
		showFrameRate();
	}
	cleanupXWindows();
	if (screendata)
		delete [] screendata;
	return 0;
}

void cleanupXWindows(void)
{
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

void set_title()
{
	//Set the window title bar.
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "Draw only to frame buffer");
}

void showFrameRate()
{
	static int count=0;
	static int lastt = xxGetTicks();
	if (++count >= 32) {
		int diff = xxGetTicks() - lastt;
		//32 frames took diff 1/1000 seconds.
		//how much did 1 frame take?
		float secs = (float)diff / 1000.0;
		//frames per second...
		float fps = (float)count / secs;
		printf("frame rate: %f\n", fps);
		count = 0;
		lastt = xxGetTicks();
	}
}

void setup_screen_res(const int w, const int h)
{
	xres = w;
	yres = h;
	xres3 = xres * 3;
	xres4 = xres * 4;
	if (screendata)
		delete [] screendata;
	screendata = new unsigned char[yres * xres4];
}

void initXWindows(int w, int h)
{
	//Fullscreen implementation
	//usage:
	//   for fullscreen call: initXWindows(0, 0);
	//   for windowed call: initXWindows(640, 480);
	//                      initXWindows(1024, 768);
	//                      etc.
	//
	GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
	dpy = XOpenDisplay(NULL);
	//Screen *s = DefaultScreenOfDisplay(dpy);
	if (dpy == NULL) {
		printf("ERROR: cannot connect to X server!\n"); fflush(stdout);
		exit(EXIT_FAILURE);
	}
	Window root = DefaultRootWindow(dpy);
	//for fullscreen
	XWindowAttributes getWinAttr;
    XGetWindowAttributes(dpy, root, &getWinAttr);
	int fullscreen = 0;
	xres = w;
	yres = h;
	if (!w && !h) {
		//Go to fullscreen.
		xres = getWinAttr.width;
		yres = getWinAttr.height;
		printf("getWinAttr: %i %i\n", w, h); fflush(stdout);
		//When window is fullscreen, there is no client window
		//so keystrokes are linked to the root window.
		XGrabKeyboard(dpy, root, False,
			GrabModeAsync, GrabModeAsync, CurrentTime);
		fullscreen = 1;
	}
	XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
	if (vi == NULL) {
		printf("ERROR: no appropriate visual found\n"); fflush(stdout);
		exit(EXIT_FAILURE);
	} 
	Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
	XSetWindowAttributes swa;
	swa.colormap = cmap;
	//List your desired events here.
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
		StructureNotifyMask | SubstructureNotifyMask;
	unsigned int winops = CWBorderPixel|CWColormap|CWEventMask;
	if (fullscreen) {
		winops |= CWOverrideRedirect;
		swa.override_redirect = True;
	}
	printf("2 getWinAttr: %i %i\n", w, h); fflush(stdout);
	win = XCreateWindow(dpy, root, 0, 0, xres, yres, 0,
		vi->depth, InputOutput, vi->visual, winops, &swa);
	glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
	glXMakeCurrent(dpy, win, glc);
	set_title();
}

void reshape_window(int width, int height)
{
	//window has been resized.
	setup_screen_res(width, height);
	//
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, xres, 0, yres, -1, 1);
	set_title();
}

void init_opengl(void)
{
	//OpenGL initialization
	glViewport(0, 0, xres, yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//This sets 2D mode (no perspective)
	glOrtho(0, xres, 0, yres, -1, 1);
	//Clear the screen
	glClearColor(1.0, 1.0, 1.0, 1.0);
	//glClear(GL_COLOR_BUFFER_BIT);
}

void check_resize(XEvent *e)
{
	//The ConfigureNotify is sent by the
	//server if the window is resized.
	if (e->type != ConfigureNotify)
		return;
	XConfigureEvent xce = e->xconfigure;
	if (xce.width != xres || xce.height != yres) {
		//Window size did change.
		reshape_window(xce.width, xce.height);
	}
}

void check_mouse(XEvent *e)
{
	//Did the mouse move?
	//Was a mouse button clicked?
	static int savex = 0;
	static int savey = 0;
	//
	if (e->type == ButtonRelease) {
		return;
	}
	if (e->type == ButtonPress) {
		if (e->xbutton.button==1) {
			//Left button is down
		}
		if (e->xbutton.button==3) {
			//Right button is down
		}
	}
	if (savex != e->xbutton.x || savey != e->xbutton.y) {
		//Mouse moved
		savex = e->xbutton.x;
		savey = e->xbutton.y;
	}
}

int check_keys(XEvent *e)
{
	//Was there input from the keyboard?
	if (e->type == KeyPress) {
		int key = XLookupKeysym(&e->xkey, 0);
		if (key == XK_Escape) {
			return 1;
		}
	}
	return 0;
}

void physics(void)
{
	int addgrav = 1;
	//Update position
	pos[0] += vel[0];
	pos[1] += vel[1];
	//Check for collision with window edges
	if ((pos[0] < 20.0          && vel[0] < 0.0) ||
		(pos[0] >= (float)xres-20 && vel[0] > 0.0)) {
		vel[0] = -vel[0];
		addgrav = 0;
	}
	if ((pos[1] < 20.0          && vel[1] < 0.0) ||
		(pos[1] >= (float)yres+20 && vel[1] > 0.0)) {
		vel[1] = -vel[1];
		addgrav = 0;
	}
	//Gravity
	if (addgrav)
		vel[1] -= 0.4;
}

void renderViewport(const int y, const int w, const int h)
{
	//x: column, is always 0
	//y: row
	//w: width of area to be drawn
	//h: height of area to be drawn
	unsigned char *p = screendata + (y * xres4);
	//Log("render_viewport(%i %i %i %i)\n", 0, y, w, h);
	glRasterPos2i(0, y);
	glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, (const GLvoid *)p);
}

inline void setpixel(const int x, const int y, cVec rgb)
{
	//Log("setpixel(%i %i : %u %u %u)...\n",x,y,rgb[0],rgb[1],rgb[2]);
	int offset = y * xres4 + (x << 2);
	*(screendata+offset  ) = rgb[0];
	*(screendata+offset+1) = rgb[1];
	*(screendata+offset+2) = rgb[2];
}

inline void clearBuffer(cVec col)
{
	const int n = xres * yres;
	unsigned char *p = screendata;
	for (int i=0; i<n; i++) {
		*(p+0) = col[0];
		*(p+1) = col[1];
		*(p+2) = col[2];
		p += 4;
	}
}

void render(void)
{
	//draw pixels to memory buffer.
	cVec r = {0, 0, 0};
	clearBuffer(r);
	cVec rgb = {255, 0, 0};
	for (int i=0; i<20; i++) {
		for (int j=0; j<20; j++) {
			setpixel(i, j, rgb);
		}
	}
	rgb[1] = 255;
	for (int i=-10; i<10; i++) {
		for (int j=-10; j<10; j++) {
			setpixel(pos[0]+i, pos[1]+j, rgb);
		}
	}
	//render memory buffer to frame buffer.
	renderViewport(0, xres, yres);
	//
	if (showOpengl) {
		glClear(GL_COLOR_BUFFER_BIT);
		float wid = 40.0f;
		glColor3ub(30,60,90);
		glPushMatrix();
		glTranslatef(pos[0], pos[1], pos[2]);
		glBegin(GL_QUADS);
			glVertex2i(-wid,-wid);
			glVertex2i(-wid, wid);
			glVertex2i( wid, wid);
			glVertex2i( wid,-wid);
		glEnd();
		glPopMatrix();
	}
}

















