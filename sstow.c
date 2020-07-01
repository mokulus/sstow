#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "arg.h"

char* argv0;
int verbose = 0;
int dry_run = 0;

void
die(const char *errstr, ...)
{
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

void *
emalloc(size_t n)
{
	void *p = malloc(n);
	if (!p)
		die("emalloc, out of memory");
	return p;
}

char*
join(const char* s1, const char* s2)
{
	char* str;
	if(!s1) {
		str = strdup(s2);
	}
	else if(!s2) {
		str = strdup(s1);
	}
	else {
		size_t len1 = strlen(s1);
		size_t len2 = strlen(s2);
		str = emalloc(len1 + len2 + 2);
		strcpy(str, s1);
		str[len1] = '/';
		strcpy(str + len1 + 1, s2);
	}
	return str;
}

struct dirnode {
	struct dirnode* prev;
	struct dirnode* next;
	char* relpath;
};

struct dirnode*
get_queue(const char* root_name)
{
	struct dirnode* root_dirnode = emalloc(sizeof(*root_dirnode));
	root_dirnode->prev = root_dirnode;
	root_dirnode->next = root_dirnode;
	root_dirnode->relpath = NULL;
	struct dirnode* current = root_dirnode;
	struct dirnode* last = root_dirnode;
	do {
		char* full_path = join(root_name, current->relpath);
		DIR* dir = opendir(full_path);
		if(dir) { 
			struct dirent* d;
			while((d = readdir(dir))) {
				if(!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
					continue;
				struct dirnode* new = emalloc(sizeof(*new));
				struct dirnode* after = last->next;
				last->next = new;
				new->prev = last;
				new->next = after;
				after->prev = new;
				last = new;
				new->relpath = join(current->relpath, d->d_name);
			}
		}
		current = current->next;
		free(full_path);
		closedir(dir);
	} while(current != root_dirnode);

	return root_dirnode;
}

void
free_queue(struct dirnode* queue)
{
	struct dirnode* curr;
	for(curr = queue->next; curr != queue;) {
		free(curr->relpath);
		curr = curr->next;
		free(curr->prev);
	}
	free(queue->relpath);
	free(queue);
}

void
create(struct dirnode* queue,
       const char* root_name,
       const char* target_name)
{
	struct stat sb;
	struct dirnode* curr;
	for(curr = queue->next; curr != queue; curr = curr->next) {
		char* root_path = join(root_name, curr->relpath);
		char* target_path = join(target_name, curr->relpath);
		if(lstat(root_path, &sb) == -1)
			die("lstat %s", root_path);
		if(S_ISDIR(sb.st_mode)) {
			if(verbose)
				printf("mkdir %s\n", target_path);
			if(!dry_run)
				mkdir(target_path, sb.st_mode);
		} else {
			if(verbose)
				printf("%s -> %s\n", root_path, target_path);
			if(!dry_run)
				symlink(root_path, target_path);
		}
		free(root_path);
		free(target_path);
	}
}

void
delete(struct dirnode* queue,
       const char* root_name,
       const char* target_name)
{
	struct stat sb;
	struct dirnode* curr;
	for(curr = queue->prev; curr != queue; curr = curr->prev) {
		char* root_path = join(root_name, curr->relpath);
		char* target_path = join(target_name, curr->relpath);
		if(lstat(root_path, &sb) == -1)
			die("lstat %s", root_path);
		if(S_ISDIR(sb.st_mode)) {
			if(verbose)
				printf("rm -d %s\n", target_path);
			if(!dry_run)
				rmdir(target_path);
		} else {
			if(verbose)
				printf("rm %s\n", target_path);
			if(!dry_run)
				unlink(target_path);
		}
		free(root_path);
		free(target_path);
	}
}

void
usage()
{
	die("usage: %s [-n] [-v] (-S|-D|-R) -t target -d package\n", argv0);
}

char*
process_path(const char* path_name)
{
	if(!path_name)
		usage();
	char* abs = realpath(path_name, NULL);
	if(!abs)
		die("file doesn't exist: %s", path_name);
	struct stat sb;
	if(!(stat(abs, &sb) == 0 && S_ISDIR(sb.st_mode)))
		die("not a directory: %s", abs);
	return abs;
}

int
main(int argc, char* argv[])
{
	(void)argc;
	argv0 = argv[0];
	int create_flag = 0, delete_flag = 0;
	char* target_name = NULL;
	char* root_name = NULL;

	while(*++argv) {
		if(argv[0][0] == '-') {
			if(argv[0][2] != '\0')
				usage();
			switch(argv[0][1]) {
			case 'S':
				create_flag = 1;
				break;
			case 'D':
				delete_flag = 1;
				break;
			case 'R':
				create_flag = 1;
				delete_flag = 1;
				break;
			case 't':
				target_name = argv[1];
				++argv;
				break;
			case 'd':
				root_name = argv[1];
				++argv;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'n':
				dry_run = 1;
				break;
			default:
				usage();
			}
		}
		else usage();
	}

	if(!create_flag && !delete_flag)
		usage();
	char* root_name_abs = process_path(root_name);	
	char* target_name_abs = process_path(target_name);	
	struct dirnode* queue = get_queue(root_name);

	if(delete_flag)
		delete(queue, root_name_abs, target_name_abs);
	if(create_flag)
		create(queue, root_name_abs, target_name_abs);
	free(root_name_abs);
	free(target_name_abs);
	free_queue(queue);
}