/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>


#include <X11/Xlib.h>

char *tzmoscow = "Europe/Moscow";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	return smprintf("%s", line);
}

static const char *baticons[]  = { "", "", "", "", ""};

int 
mapToRange(int input) {
    if (input >= 0 && input <= 5) {
        return 0;
    } else if (input >= 6 && input <= 30) {
        return 1;
    } else if (input >= 31 && input <= 50) {
        return 2;
	} else if (input >= 51 && input <= 80) {
        return 3;
    } else if (input >= 81 && input <= 100) {
        return 4;
    } else {
        return -1;
    }
}


char *
getbattery(char *base)
{
	char *co;
	char *status = "";
	int  cap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "capacity");
	if (co == NULL) {
		return smprintf("");
	}
	sscanf(co, "%d", &cap);
	free(co);

	co = readfile(base, "status");
	if (co == NULL) {
		return smprintf("");
	}
	if (!strncmp(co,"Charging",8)){
		status="󱐋";
	}
	free(co);

	const char *icon = baticons[mapToRange(cap)];

	return smprintf("%d%% %s  %s", cap, icon, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");
	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
execscript(char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;

	memset(retval, 0, sizeof(retval));

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");
	retval[strlen(retval)-1] = '\0';

	return smprintf("%s", retval);
}


const char *
get_popen() {
    FILE *pf;
    char command[42];
    char data[512];

    // Execute a process listing
    sprintf(command, "pamixer --get-volume-human | tr -d '\\n'"); 

    // Setup our pipe for reading and execute our command.
    pf = popen(command,"r"); 

    // Error handling

    // Get the data from the process execution
    fgets(data, 512 , pf);

    // the data is now in 'data'

    if (pclose(pf) != 0)
        fprintf(stderr," Error: Failed to close command stream \n");
	
    if (!strncmp(data,"muted",5))
		return "  ";
	return "";
}

char *
wifi(const char *scriptPath){
    FILE *fp = popen(scriptPath, "r");
    if (fp == NULL) {
        perror("Ошибка при выполнении скрипта");
        return NULL;
    }

    // Создаем буфер для чтения вывода
    char buffer[128];
    char *result = malloc(1); // Динамический буфер для сохранения результата
    result[0] = '\0'; // Изначально пустая строка

    // Считываем вывод скрипта и сохраняем его в строку
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result = realloc(result, strlen(result) + strlen(buffer) + 1);
        strcat(result, buffer);
    }

    // Закрываем поток
    pclose(fp);

	if(strcmp(result, "")){
		return smprintf("[ %s ]", result);
	}

    return result;
}

int
main(void)
{
	char *status;
	char *bat;
	char *tmbln;
	char *kbmap;
	char *wifiname;
	const char *mute;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}


	for (;;sleep(5)) {
		bat = getbattery("/sys/class/power_supply/BAT1");
		tmbln = mktimes("%H:%M", tzmoscow);

		wifiname = wifi("/home/nk/test/wifi.sh");
		mute = get_popen();
		kbmap = execscript("setxkbmap -query | grep layout | cut -d':' -f 2- | tr -d ' '");
		status = smprintf("%s %s [ %s ] [ %s] [ %s ]",
			mute, wifiname, kbmap, bat, tmbln);

		setstatus(status);

		free(kbmap);
		free(bat);
		free(tmbln);
		free(wifiname);
		free(status);
	}
	

	XCloseDisplay(dpy);

	return 0;
}

