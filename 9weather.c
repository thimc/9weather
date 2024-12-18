#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include <json.h>

enum {
	Emouse,
	Ekeyboard,
	Eresize,
	Etimer,
};

enum {
	Delay = 5*(60*1000),	/* default delay is 5 minutes */
	Imgsize = 100,		/* images from openweathermap are 100x100 pixels */
	Iconsize = 4096,	/* images are limited to 4kB in file size */
};

Mousectl *mctl;
Keyboardctl *kctl;
char *menustr[] = { "exit", 0 };
Menu menu = { menustr };

Image *background, *icon;

int delay = Delay, unitflag;
char *apikey, *zip;
char city[25], description[25], temperature[25], iconid[5];

void
usage(void)
{
	print("usage: %s [-d seconds] [-i] [-z zip,country] [-k apikey]\n", argv0);
	threadexitsall("usage");
}

double
round(double n)
{
	double f, c;
	
	f = floor(n);
	c = ceil(n);
	if(n - f > c - n)
		return f;
	return c;
}

int
webclone(int *conn)
{
	char buf[128];
	int n, fd;

	if((fd = open("/mnt/web/clone", ORDWR)) < 0)
		sysfatal("webclone: couldn't open %s: %r", buf);
	if((n = read(fd, buf, sizeof buf)) < 0)
		sysfatal("webclone: reading clone: %r");
	if(n == 0)
		sysfatal("webclone: short read on clone");
	buf[n] = '\0';
	*conn = atoi(buf);

	return fd;
}

char*
readbody(int c)
{
	char body[Iconsize], buf[256];
	int n, fd;

	snprint(buf, sizeof(buf), "/mnt/web/%d/body", c);
	if((fd = open(buf, OREAD)) < 0)
		sysfatal("open %s: %r", buf);
	if((n = readn(fd, body, sizeof(body))) < 0)
		sysfatal("readn: %r");
	close(fd);
	body[n] = '\0';

	return body;
}

void
polldata(void)
{
	JSON *obj, *desc, *ico, *temp;
	JSONEl *arr;
	char u[256], *buf;
	int conn, ctlfd;

	snprint(u, sizeof(u), "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=%s&mode=json",
		zip, apikey, (unitflag ? "imperial" : "metric"));
	ctlfd = webclone(&conn);
	fprint(ctlfd, "url %s\n", u);
	buf = readbody(conn);
	close(ctlfd);

	if((obj = jsonparse(buf)) == nil)
		sysfatal("jsonparse: %r");

	for(JSONEl *json = obj->first; json != nil; json = json->next){
		if(strcmp(json->name,"name") == 0)
			snprint(city, sizeof(city), json->val->s);
		if(strcmp(json->name, "weather") == 0){
			arr = json->val->first;
			if(desc = jsonbyname(arr->val, "description"))
				snprint(description, sizeof(description), desc->s);
			if(ico = jsonbyname(arr->val, "icon"))
				snprint(iconid, sizeof(iconid), ico->s);
		}
		if(strcmp(json->name, "main") == 0){
			if(temp = jsonbyname(json->val, "temp")){
				snprint(temperature, sizeof(temperature), "%s%d°%s",
					(temp->n > 0 ? "+" : ""), (int)round(temp->n), (unitflag ? "F" : "C"));
			}
		}
	}
	jsonfree(obj);
}

void
mkiconfile(void)
{
	char url[50], cmd[30], *arg[4], *body;
	int conn, ctlfd, f, ifd;

	snprint(url, sizeof(url), "http://openweathermap.org/img/wn/%s@2x.png", iconid);
	ctlfd = webclone(&conn);
	fprint(ctlfd, "url %s\n", url);
	body = readbody(conn);
	close(ctlfd);

	if((f = create("icon.png", OWRITE, 0666)) < 0)
		sysfatal("create: %r");
	write(f, body, Iconsize);
	close(f);
	close(conn);

	snprint(cmd, sizeof(cmd), "png -tc icon.png > icon");
	arg[0] = "rc";
	arg[1] = "-c";
	arg[2] = cmd;
	arg[3] = nil;

	switch(fork()){
	case -1:
		sysfatal("fork: %r");
	case 0:
		exec("/bin/rc", arg);
		sysfatal("exec: %r");
	default:
		waitpid();
	}

	if(icon)
		freeimage(icon);
	if((ifd = open("icon", OREAD)) < 0)
		sysfatal("open icon: %r");
	icon = readimage(display, ifd, 0);
	close(ifd);

	if(remove("icon.png") < 0)
		sysfatal("remove png: %r");
	if(remove("icon") < 0)
		sysfatal("remove icon: %r");
}

void
timerproc(void *c)
{
	threadsetname("timer");
	for(;;){
		sleep(delay);
		sendul(c, 0);
	}
}

int
max(int a, int b, int c)
{
	return (a > b) ? ((a > c) ? a : c) : ((b > c) ? b : c);
}

void
redraw(void)
{
	Rectangle imgr;
	Point p;
	int txtw;

	txtw = max(stringwidth(font, city), stringwidth(font, description), stringwidth(font, temperature)) + font->width;

	imgr = screen->r;
	imgr.min.x += (Dx(screen->r) - txtw - Imgsize) / 2;
	imgr.min.y += (Dy(screen->r) - Imgsize) / 2;
	imgr.max.y = imgr.min.y + Imgsize;
	imgr.max.x = imgr.min.x + Imgsize;

	p = Pt(imgr.max.x, screen->r.min.y + (Dy(screen->r) - font->height) / 2);

	draw(screen, screen->r, background, nil, ZP);
	draw(screen, imgr, icon, nil, ZP);

	string(screen, subpt(p, Pt(0, font->height)), display->black, ZP, font, city);
	string(screen, p, display->black, ZP, font, description);
	string(screen, addpt(p, Pt(0, font->height)), display->black, ZP, font, temperature);
	flushimage(display, 1);
}

void
threadmain(int argc, char *argv[])
{
	Mouse m;
	Rune k;
	Alt alts[] = {
		{ nil, &m,  CHANRCV },
		{ nil, &k,  CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, nil, CHANRCV },
		{ nil, nil, CHANEND },
	};

	apikey = getenv("openweathermap");
	zip = getenv("ZIP");
	ARGBEGIN{
	case 'd': 
		delay = atoi(EARGF(usage())) * 1000; 
		break;
	case 'i': 
		unitflag++; 
		break;
	case 'k': 
		apikey = EARGF(usage()); 
		break;
	case 'z': 
		zip = EARGF(usage()); 
		break;
	case 'h': 
		usage(); 
		break;
	}ARGEND;

	if(!apikey || !zip)
		usage();
	if(initdraw(nil, nil, argv0) < 0)
		sysfatal("initdraw: %r");
	display->locking = 0;
	if((mctl = initmouse(nil, screen)) == nil)
		sysfatal("initmouse: %r");
	if((kctl = initkeyboard(nil)) == nil)
		sysfatal("initkeyboard: %r");

	alts[Emouse].c = mctl->c;
	alts[Ekeyboard].c = kctl->c;
	alts[Eresize].c = mctl->resizec;
	alts[Etimer].c = chancreate(sizeof(ulong), 0);
	proccreate(timerproc, alts[Etimer].c, 1024);

	background = allocimagemix(display, DPalebluegreen, DWhite);
	goto timer;

	for(;;){
		switch(alt(alts)){
		case Ekeyboard:
			switch(k){
			case 'q':
			case Kdel:
				threadexitsall(nil);
				break;
			}
			break;
		case Emouse:
			if(m.buttons & 4){
				switch(menuhit(3, mctl, &menu, nil)){
				case 0:
					threadexitsall(nil);
				}
			}
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			redraw();
			break;
		case Etimer:
timer:
			polldata();
			mkiconfile();
			redraw();
			break;
		}
	}
}
