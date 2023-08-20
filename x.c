#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "util.h"

Display *d;
Window w;
XEvent e;
KeySym k;
XFixesCursorImage *ci;
Atom del;
int curs;

int
close(void)
{
	return 1;
}

void
quit(void)
{
	if(close()){
		vkhalt();
		exits("closing");
	}
}

void
fscr(void)
{
	XWMHints hints;
	hints.flags = 2;
	Atom s = XInternAtom(d,"_NET_WM_STATE",1);
	Atom f = XInternAtom(d,"_NET_WM_STATE_FULLSCREEN",1);
	Atom n = XInternAtom(d,"_NET_WM_STATE_STICKY",1);
	XChangeProperty(d,w,s,XA_ATOM,32,PropModeReplace,(unsigned char *)&f,1);
}

void
showcurs(int y)
{
	if(y && curs==0){
		XFixesShowCursor(d,DefaultRootWindow(d));
		curs=1;
	}
	if(!y && curs==1){
		XFixesHideCursor(d,DefaultRootWindow(d));
		curs=0;
	}
}

void
resized(int w, int h)
{
	printf("resized: %d %d\n",w,h);
}

void
btndown(int b)
{
	showcurs(0);
	printf("button down %d\n",b);
}

void
btnup(int b)
{
	printf("button up %d\n",b);
}

void
cmotion(int x, int y)
{
	printf("cursor motion %d %d\n",x,y);
}

void
keydown(int k)
{
	showcurs(1);
	printf("key down %x\n",k);
	if(k==0xff1b) /* no keysyms for now */
		quit();
}

void
keyup(int k)
{
	printf("key up %x\n",k);
}

void
mkwin(int wi, int he, char *l)
{
	int full;
	puts("xlib");

	curs=1;
	if(wi==0||he==0){
		wi=800;
		he=600;
		full=1;
	}else
		full=0;

	d = XOpenDisplay(0);
	if(d==0)
		exits("can't connect to display");
	w = XCreateWindow(d,DefaultRootWindow(d),0,0,wi,he,0,0,InputOutput,0,0,0);
	if(w==0)
		exits("can't create window");

	XSelectInput(d,w,KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask|
		EnterWindowMask|LeaveWindowMask|ExposureMask|PointerMotionMask|FocusChangeMask);
	XAutoRepeatOff(d);
	XStoreName(d,w,"test");
	if(full)
		fscr();
	XMapWindow(d,w);

	del = XInternAtom(d,"WM_DELETE_WINDOW",0);
	XSetWMProtocols(d,w,&del,1);
} 

void
readev(void)
{
	if(XPending(d)>0){
		XNextEvent(d,&e);
		if(XFilterEvent(&e,0))
			return;
		switch(e.type){
		case KeyPress:
			k = XkbKeycodeToKeysym(d,e.xkey.keycode,0,0);
			keydown(k);
		break;
		case KeyRelease:
			k = XkbKeycodeToKeysym(d,e.xkey.keycode,0,0);
			keyup(k);
		break;
		case ButtonPress:
			btndown(e.xbutton.button);
		break;
		case ButtonRelease:
			btnup(e.xbutton.button);
		break;
		case MotionNotify:
			ci = XFixesGetCursorImage(d);
			cmotion(ci->x,ci->y);
		break;
		case Expose:
			resized(e.xexpose.width,e.xexpose.height);
		break;
		case ClientMessage:
			if(e.xclient.data.l[0]==del){
				quit();
			}
		}
	}
}