#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "bbgen.h"

page_t		*pagehead = NULL;

link_t  	*linkhead = NULL;
link_t		null_link = { "", "", "", NULL };

hostlist_t	*hosthead = NULL;
state_t		*statehead = NULL;
col_t   	*colhead = NULL;


link_t *find_link(const char *name)
{
	link_t *l;

	for (l=linkhead; (l && (strcmp(l->name, name) != 0)); l = l->next);

	return (l ? l : &null_link);
}

page_t *init_page(const char *name, const char *title)
{
	page_t *newpage = malloc(sizeof(page_t));

	strcpy(newpage->name, name);
	strcpy(newpage->title, title);
	newpage->color = -1;
	newpage->next = NULL;
	newpage->subpages = NULL;
	newpage->groups = NULL;
	newpage->hosts = NULL;
	return newpage;
}

group_t *init_group(const char *title)
{
	group_t *newgroup = malloc(sizeof(group_t));

	strcpy(newgroup->title, title);
	newgroup->hosts = NULL;
	newgroup->next = NULL;
	return newgroup;
}

host_t *init_host(const char *hostname, const int ip1, const int ip2, const int ip3, const int ip4, const int dialup)
{
	host_t 		*newhost = malloc(sizeof(host_t));
	hostlist_t	*newlist = malloc(sizeof(hostlist_t));

	strcpy(newhost->hostname, hostname);
	sprintf(newhost->ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
	newhost->link = find_link(hostname);
	newhost->entries = NULL;
	newhost->color = -1;
	newhost->dialup = dialup;
	newhost->next = NULL;

	newlist->hostentry = newhost;
	newlist->next = hosthead;
	hosthead = newlist;

	return newhost;
}

host_t *find_host(const char *hostname)
{
	hostlist_t	*l;

	for (l=hosthead; (l && (strcmp(l->hostentry->hostname, hostname) != 0)); l = l->next) ;

	return (l ? l->hostentry : NULL);
}


link_t *init_link(const char *filename, const char *urlprefix)
{
	char *p;
	link_t *newlink = NULL;

	p = strrchr(filename, '.');
	if (p == NULL) return NULL;	/* Filename with no extension - not linkable */

	if ( (strcmp(p, ".php") == 0)   ||
	     (strcmp(p, ".html") == 0)  ||
	     (strcmp(p, ".htm") == 0)) {

		newlink = malloc(sizeof(link_t));
		strcpy(newlink->filename, filename);

		*p = '\0';
		strcpy(newlink->name, filename);  /* Without extension, this time */

		strcpy(newlink->urlprefix, urlprefix);
		newlink->next = NULL;
	}

	return newlink;
}

col_t *find_or_create_column(const char *testname)
{
	col_t	*newcol;

	for (newcol = colhead; (newcol && (strcmp(testname, newcol->name) != 0)); newcol = newcol->next);
	if (newcol == NULL) {
		newcol = malloc(sizeof(col_t));
		strcpy(newcol->name, testname);
		newcol->link = find_link(testname);

		/* No need to maintain this list in order */
		if (colhead == NULL) {
			colhead = newcol;
			newcol->next = NULL;
		}
		else {
			newcol->next = colhead;
			colhead = newcol;
		}
	}

	return newcol;
}

state_t *init_state(const char *filename, int dopurple)
{
	FILE *fd;
	char	*p;
	char	hostname[60];
	char	testname[20];
	state_t *newstate;
	char	l[200];
	host_t	*host;
	struct stat st;
	time_t	now = time(NULL);

	/* Ignore summary files and dot-files */
	if ( (strncmp(filename, "summary.", 8) == 0) || (filename[0] == '.')) {
		return NULL;
	}

	/* Pick out host- and test-name */
	strcpy(hostname, filename);
	p = strrchr(hostname, '.');

	/* Skip files that have no '.' in filename */
	if (p) {
		/* Pick out the testname ... */
		*p = '\0';
		strcpy(testname, p+1);

		/* ... and change hostname back into normal form */
		for (p=hostname; (*p); p++) {
			if (*p == ',') *p='.';
		}
	}
	else {
		return NULL;
	}

	newstate = malloc(sizeof(state_t));
	newstate->entry = malloc(sizeof(entry_t));
	newstate->next = NULL;

	newstate->entry->column = find_or_create_column(testname);
	newstate->entry->color = -1;

	host = find_host(hostname);
	stat(filename, &st);
	fd = fopen(filename, "r");

	if (fd && fgets(l, sizeof(l), fd)) {
		if (strncmp(l, "green ", 6) == 0) {
			newstate->entry->color = COL_GREEN;
		}
		else if (strncmp(l, "yellow ", 7) == 0) {
			newstate->entry->color = COL_YELLOW;
		}
		else if (strncmp(l, "red ", 4) == 0) {
			newstate->entry->color = COL_RED;
		}
		else if (strncmp(l, "blue ", 5) == 0) {
			newstate->entry->color = COL_BLUE;
		}
		else if (strncmp(l, "clear ", 6) == 0) {
			newstate->entry->color = COL_CLEAR;
		}
		else if (strncmp(l, "purple ", 7) == 0) {
			newstate->entry->color = COL_PURPLE;
		}
	}

	if (st.st_mtime <= now) {
		/* PURPLE test! */
		if (host && host->dialup) {
			/* Dialup hosts go clear, not purple */
			newstate->entry->color = COL_CLEAR;
		}
		else {
			/* Not in bb-hosts, or logfile too old */
			newstate->entry->color = COL_PURPLE;
		}

		/* FIXME : Need to send a bbd update of status to purple */
	}

	while (fgets(l, sizeof(l), fd) && (strncmp(l, "Status unchanged in ", 20) != 0)) ;
	if (strncmp(l, "Status unchanged in ", 20) == 0) {
		char *p = strchr(l, '\n');
		*p = '\0';

		strcpy(newstate->entry->age, l+20);
		newstate->entry->oldage = (strstr(l+20, "days") != NULL);
	}
	else {
		strcpy(newstate->entry->age, "");
		newstate->entry->oldage = 0;
	}

	fclose(fd);


	if (host) {
		newstate->hostname = host->hostname;
		newstate->entry->next = host->entries;
		host->entries = newstate->entry;
	}
	else {
		/* No host for this test - must be missing from bb-hosts */
		newstate->entry->next = NULL;

		/* Need to malloc() room for the hostname */
		newstate->hostname = malloc(strlen(hostname)+1);
		strcpy(newstate->hostname, hostname);
	}

	return newstate;
}

void getnamelink(char *l, char **name, char **link)
{
	char *p;

	*name = "";
	*link = "";

	/* Find first space and skip spaces */
	for (p=strchr(l, ' '); (p && (isspace (*p))); p++) ;

	*name = p; p = strchr(*name, ' ');
	if (p) {
		*p = '\0'; /* Null-terminate pagename */
		for (p++; (isspace(*p)); p++) ;
		*link = p;
	}
}


void getgrouptitle(char *l, char **title)
{
	char *p;

	*title = "";

	if (strncmp(l, "group-only", 10) == 0) {
		/* Find first space and skip spaces */
		for (p=strchr(l, ' '); (p && (isspace (*p))); p++) ;
		/* Find next space and skip spaces */
		for (p=strchr(p, ' '); (p && (isspace (*p))); p++) ;
		*title = p;
	}
	else if (strncmp(l, "group", 5) == 0) {
		/* Find first space and skip spaces */
		for (p=strchr(l, ' '); (p && (isspace (*p))); p++) ;

		*title = p;
	}
}

link_t *load_links(const char *directory, const char *urlprefix)
{
	DIR		*bblinks;
	struct dirent 	*d;
	link_t		*curlink, *toplink, *newlink;

	toplink = curlink = NULL;
	bblinks = opendir(directory);
	if (!bblinks) {
		perror("Cannot read directory");
		exit(1);
	}

	while ((d = readdir(bblinks))) {
		newlink = init_link(d->d_name, urlprefix);
		if (newlink) {
			if (toplink == NULL) {
				toplink = newlink;
			}
			else {
				curlink->next = newlink;
			}
			curlink = newlink;
		}
	}
	closedir(bblinks);
	return toplink;
}

link_t *load_all_links(void)
{
	link_t *l, *head1, *head2;
	char dirname[200];
	char *p;

	strcpy(dirname, getenv("BBNOTES"));
	head1 = load_links(dirname, "/notes");

	/* Change xxx/xxx/xxx/notes into xxx/xxx/xxx/help */
	p = strrchr(dirname, '/'); *p = '\0'; strcat(dirname, "/help");
	head2 = load_links(dirname, "/help");

	if (head1) {
		/* Append help-links to list of notes-links */
		for (l = head1; (l->next); l = l->next) ;
		l->next = head2;
	}
	else {
		/* /notes was empty, so just return the /help list */
		head1 = head2;
	}

	return head1;
}


page_t *load_bbhosts(void)
{
	FILE 	*bbhosts;
	char 	l[200];
	char 	*name, *link;
	char 	hostname[65];
	page_t 	*toppage, *curpage, *cursubpage;
	group_t *curgroup;
	host_t	*curhost;
	int	ip1, ip2, ip3, ip4;
	char	*p;


	bbhosts = fopen(getenv("BBHOSTS"), "r");
	if (bbhosts == NULL)
		exit(1);

	curpage = toppage = init_page("*TOP*", "");
	while (fgets(l, sizeof(l), bbhosts)) {
		p = strchr(l, '\n'); if (p) { *p = '\0'; };

		if (strncmp(l, "page", 4) == 0) {
			getnamelink(l, &name, &link);
			curpage->next = init_page(name, link);
			curpage = curpage->next;
			cursubpage = NULL;
			curgroup = NULL;
			curhost = NULL;
		}
		else if (strncmp(l, "subpage", 7) == 0) {
			getnamelink(l, &name, &link);
			if (cursubpage == NULL) {
				cursubpage = curpage->subpages = init_page(name, link);
			}
			else {
				cursubpage = cursubpage->next = init_page(name, link);
			}
			curgroup = NULL;
			curhost = NULL;
		}
		else if (strncmp(l, "group", 5) == 0) {
			getgrouptitle(l, &link);
			if (curgroup == NULL) {
				curgroup = init_group(link);
				if (cursubpage == NULL) {
					/* We're on a main page */
					curpage->groups = curgroup;
				}
				else {
					/* We're in a subpage */
					cursubpage->groups = curgroup;
				}
			}
			else {
				curgroup->next = init_group(link);
				curgroup = curgroup->next;
			}
			curhost = NULL;
		}
		else if (sscanf(l, "%3d.%3d.%3d.%3d %s", &ip1, &ip2, &ip3, &ip4, hostname) == 5) {
			int dialup = 0;
			char *p = strchr(l, '#');

			if (p && strstr(p, " dialup")) dialup=1;

			if (curhost == NULL) {
				curhost = init_host(hostname, ip1, ip2, ip3, ip4, dialup);
				if (curgroup != NULL) {
					curgroup->hosts = curhost;
				} else if (cursubpage != NULL) {
					cursubpage->hosts = curhost;
				}
				else if (curpage != NULL) {
					curpage->hosts = curhost;
				}
				else {
					/* Should not happen! */
					printf("Nowhere to put the host %s\n", hostname);
				}
			}
			else {
				curhost = curhost->next = init_host(hostname, ip1, ip2, ip3, ip4, dialup);
			}
		}
		else {
		};
	}

	fclose(bbhosts);
	return toppage;
}


state_t *load_state(void)
{
	DIR		*bblogs;
	struct dirent 	*d;
	state_t		*newstate, *topstate;
	int		dopurple;
	struct stat	st;

	chdir(getenv("BBLOGS"));
	if (stat(".bbstartup", &st) == -1) {
		/* Do purple if no ".bbstartup" file */
		dopurple = 1;
	}
	else {
		/* Don't do purple hosts ("avoid purple explosion on startup") */
		dopurple = 0;
		remove(".bbstartup");
	}

	topstate = NULL;
	bblogs = opendir(getenv("BBLOGS"));
	if (!bblogs) {
		perror("No logs!");
		exit(1);
	}

	while ((d = readdir(bblogs))) {
		if (d->d_name[0] != '.') {
			newstate = init_state(d->d_name, dopurple);
			if (newstate) {
				newstate->next = topstate;
				topstate = newstate;
			}
		}
	}

	closedir(bblogs);

	return topstate;
}


void calc_hostcolors(hostlist_t *head)
{
	int		color;
	hostlist_t 	*h;
	entry_t		*e;

	for (h = head; (h); h = h->next) {
		color = 0;

		for (e = h->hostentry->entries; (e); e = e->next) {
			if (e->color > color) color = e->color;
		}

		/* Blue and clear is not propageted upwards */
		if ((color == COL_CLEAR) || (color == COL_BLUE)) color = COL_GREEN;

		h->hostentry->color = color;
	}
}

void calc_pagecolors(page_t *phead, char *indent1)
{
	page_t 	*p, *toppage;
	group_t *g;
	host_t  *h;
	int	color;
	char 	indent2[80];

	strcpy(indent2, indent1);
	strcat(indent2, "  >");

	for (toppage=phead; (toppage); toppage = toppage->next) {
		/* printf("%s Color: page=%s\n", indent1, toppage->name); */

		/* Start with the color of immediate hosts */
		color = (toppage->hosts ? toppage->hosts->color : -1);
		/* printf("%s Hostcolor %d\n",indent1 , color); */

		/* Then adjust with the color of hosts in immediate groups */
		for (g = toppage->groups; (g); g = g->next) {
			for (h = g->hosts; (h); h = h->next) {
				if (h->color > color) color = h->color;
			}
		}
		/* printf("%s Host+group color: %d\n", indent1, color); */

		/* Then adjust with the color of subpages, if any.  */
		/* These must be calculated first!                  */
		if (toppage->subpages) {
			/* printf("%s Calling calc of subpages\n", indent1); */
			calc_pagecolors(toppage->subpages, indent2);
		}

		/* printf("%s Subpages checked: ", indent1); */
		for (p = toppage->subpages; (p); p = p->next) {
			/* printf("%s ", p->name); */
			if (p->color > color) color = p->color;
		}
		/* printf("\n"); */

		toppage->color = color;
		/* printf("%s pagecolor: %d\n", indent1, color); */
	}
}


void dumplinks(link_t *head)
{
	link_t *l;

	for (l = head; l; l = l->next) {
		printf("Link for host %s, URL/filename %s/%s\n", l->name, l->urlprefix, l->filename);
	}
}


void dumphosts(host_t *head, char *prefix)
{
	host_t *h;
	entry_t *e;
	char	format[80];

	strcpy(format, prefix);
	strcat(format, "Host: %s, ip: %s, color: %d, link: %s\n");

	for (h = head; (h); h = h->next) {
		printf(format, h->hostname, h->ip, h->color, h->link->filename);
		for (e = h->entries; (e); e = e->next) {
			printf("\t\t\t\t\tTest: %s, state %d, age: %s, oldage: %d\n", 
				e->column->name, e->color, e->age, e->oldage);
		}
	}
}

void dumpgroups(group_t *head, char *prefix, char *hostprefix)
{
	group_t *g;
	char    format[80];

	strcpy(format, prefix);
	strcat(format, "Group: %s\n");

	for (g = head; (g); g = g->next) {
		printf(format, g->title);
		dumphosts(g->hosts, hostprefix);
	}
}

void dumphostlist(hostlist_t *head)
{
	hostlist_t *h;

	for (h=head; (h); h=h->next) {
		printf("Hostlist entry: Hostname %s\n", h->hostentry->hostname);
	}
}


void dumpstatelist(state_t *head)
{
	state_t *s;

	for (s=statehead; (s); s=s->next) {
		printf("Host: %s, test:%s, state: %d, oldage: %d, age: %s\n",
			s->hostname,
			s->entry->column->name,
			s->entry->color,
			s->entry->oldage,
			s->entry->age);
	}
}

int main(int argc, char *argv[])
{
	page_t *p, *q;

	linkhead = load_all_links();
	/* dumplinks(linkhead); */

	pagehead = load_bbhosts();
	/* dumphostlist(hosthead); */

	statehead = load_state();
	/* dumpstatelist(statehead); */

	calc_hostcolors(hosthead);

	calc_pagecolors(pagehead, "");
	/* For the ultimate top-page */
	for (p=pagehead; (p); p = p->next) {
		if (p->color > pagehead->color) pagehead->color = p->color;
	}


	for (p=pagehead; p; p = p->next) {
		printf("Page: %s, color: %d, title=%s\n", p->name, p->color, p->title);
		for (q = p->subpages; (q); q = q->next) {
			printf("\tSubpage: %s, color=%d, title=%s\n", q->name, q->color, q->title);
			dumpgroups(q->groups, "\t\t", "\t\t    ");
			dumphosts(q->hosts, "\t    ");
		}

		dumpgroups(p->groups, "\t","\t    ");
		dumphosts(p->hosts, "    ");
	}
	dumphosts(pagehead->hosts, "");

	return 0;
}

