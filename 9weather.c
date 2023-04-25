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
Mousectl *mctl;
Keyboardctl *kctl;

Image* icon;
Image* background;
Image* csun;
Font* specialfont;

#define ICONDIM 100
#define MAXSIZ 4096

int unitflag;
int delay;

char *apikey;
char *zip;

char city[25];
char description[25];
char temperature[25];
char iconid[5];

void usage(void);
void ekeyboard(Rune k);
double round(double n);
int webclone(int *conn);
void writeurl(int fd, char* url);
char *readbody(int c);
void polldata(void);
void mkiconfile(void);
void timerproc(void *c);
void eresized(int new);
void redraw(void);


void
usage(void)
{
	print("usage: %s [-d delay] [-i] [-z zip,country] [-f font] [-k apikey]\n", argv0);
	threadexitsall("usage");
}

void
ekeyboard(Rune k)
{
	switch(k){
	case 'q':
	case Kdel:
		threadexitsall(nil);
		break;
	}
}

double
round(double n)
{
	double f, c;
	
	f = floor(n);
	c = ceil(n);
	if(n-f > c-n)
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
	if((n = read(fd, buf, sizeof buf - 1)) < 0)
		sysfatal("webclone: reading clone: %r");
	if(n == 0)
		sysfatal("webclone: short read on clone");
	buf[n] = '\0';
	*conn = atoi(buf);

	return fd;
}

void
writeurl(int fd, char* url)
{
	char buf[256];
	int n;

	snprint(buf, sizeof(buf), "url %s", url);
	n = strlen(buf);
	if(write(fd, buf, n) != n)
		sysfatal("write: %r");
}

char*
readbody(int c)
{
	char buf[256], body[MAXSIZ];
	int fd;

	snprint(buf, sizeof(buf), "/mnt/web/%d/body", c);
	if((fd = open(buf, OREAD)) < 0)
		sysfatal("open %s %r", buf);
	if(readn(fd, body, MAXSIZ) <= 0)
		sysfatal("readn: %r");
	close(fd);

	return body;
}

void
polldata(void)
{
	JSON *obj, *desc, *ico, *temp;
	JSONEl *arr;
	char *buf, *u;
	int conn, ctlfd;

	/* Get the API response */
	u = malloc(sizeof(char) * 256);
	if(u == nil)
		sysfatal("malloc: %r");

	snprint(u, 256, "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=%s&mode=json",
		zip, apikey, (unitflag ? "imperial" : "metric"));
	ctlfd = webclone(&conn);
	writeurl(ctlfd, u);
	buf = readbody(conn);
	close(ctlfd);
	free(u);

	/* Parse the JSON */
	obj = jsonparse(buf);
	if(obj == nil)
		sysfatal("jsonparse: %r");

	for(JSONEl *json = obj->first; json != nil; json = json->next){
		if(strcmp(json->name,"name")==0)
			snprint(city, sizeof city, json->val->s);
		if(strcmp(json->name, "weather") == 0){
			arr = json->val->first;
			if(desc = jsonbyname(arr->val, "description"))
				snprint(description, sizeof description, desc->s);
			if(ico = jsonbyname(arr->val, "icon"))
				snprint(iconid, sizeof iconid, ico->s);
		}
		if(strcmp(json->name, "main") == 0){
			if(temp = jsonbyname(json->val, "temp")){
				snprint(temperature, sizeof temperature, "%s%d°%s",
					(temp->n > 0 ? "+" : "-"),
					(int)round(temp->n),
					(unitflag ? "F" : "C"));
			}
		}
	}
	jsonfree(obj);
}

void
mkiconfile(void)
{
	char *body, *url, *arg[4], *cmd;
	int conn, ctlfd, f, pid, ifd;

	url = malloc(50 * sizeof(char));
	if(url == nil)
		sysfatal("malloc: %r");
	snprint(url, 50, "http://openweathermap.org/img/wn/%s@2x.png", iconid);

	ctlfd = webclone(&conn);
	writeurl(ctlfd, url);
	body = readbody(conn);
	close(ctlfd);

	if((f = create("icon.png", OWRITE, 0666)) <= 0)
		sysfatal("create %r");
	write(f, body, MAXSIZ);
	close(f);

	cmd = malloc(30 * sizeof(char));
	if(cmd == nil)
		sysfatal("malloc: %r");
	snprint(cmd, 30, "png -c icon.png > icon");
	arg[0] = "rc";
	arg[1] = "-c";
	arg[2] = cmd;
	arg[3] = nil;

	pid = fork();
	if(pid == 0){
		exec("/bin/rc", arg);
		exits("fork");
	}
	wait();
	free(cmd);

	if(icon)
		freeimage(icon);
	ifd = open("icon", OREAD);
	if(ifd < 0)
		sysfatal("open icon: %r");
	icon = readimage(display, ifd, 0);
	close(ifd);

	if(remove("icon.png") < 0)
		sysfatal("remove png %r");
	if(remove("icon") < 0)
		sysfatal("remove icon %r");
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

void
eresize(void)
{
	redraw();
	flushimage(display, 1);
}

void
redraw(void)
{
	Point p;
	draw(screen, screen->r, background, nil, ZP);
	p = Pt(screen->r.min.x, screen->r.min.y);

	string(screen, Pt(p.x+ICONDIM+1, p.y+20), display->black, ZP, display->defaultfont, city);
	string(screen, Pt(p.x+ICONDIM+1, p.y+40), display->black, ZP, display->defaultfont, description);
	string(screen, Pt(p.x+ICONDIM+1, p.y+60), display->black, ZP, display->defaultfont, temperature);
	draw(screen, Rect(p.x, p.y, p.x+ICONDIM, p.y+ICONDIM), display->transparent, icon, ZP);
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
	char *font;

	unitflag = 0;
	delay = 5 * (60 * 1000);
	font = "/lib/font/bit/dejavusansbi/unicode.18.font";

	ARGBEGIN {
	case 'd':
		delay = atoi(EARGF(usage())) * 1000;
		break;
	case 'f':
		font = EARGF(usage());
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
	} ARGEND;

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
	specialfont = openfont(display, font);
	goto timer;

	for(;;){
		switch(alt(alts)){
		case Ekeyboard:
			ekeyboard(k);
			break;
		case Eresize:
			if(getwindow(display, Refnone) < 0)
				sysfatal("getwindow: %r");
			eresize();
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
