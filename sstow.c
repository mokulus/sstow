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

struct dirlist {
	char** data;
	size_t size;
	size_t cap;
};

struct dirlist*
dirlist_make()
{
	struct dirlist* dl = emalloc(sizeof(*dl));
	dl->cap = 32;
	dl->size = 0;
	dl->data = emalloc(32 * sizeof(char*));
	return dl;
}

void
dirlist_delete(struct dirlist* dl)
{
	size_t i;
	for(i = 0; i < dl->size; ++i)
		free(dl->data[i]);
	free(dl->data);
	free(dl);
}

void
dirlist_add(struct dirlist* dl, char* dir)
{
	if(dl->size >= dl->cap) {
		dl->cap = dl->cap * 3 / 2;
		dl->data = realloc(dl->data, dl->cap * sizeof(char*));
		if(!dl->data)
			die("realloc %d bytes", dl->cap * sizeof(char*));
	}

	dl->data[dl->size++] = dir;
}

void
dirlist_fill(struct dirlist* dl, const char* root_name)
{
	size_t i = 0;
	dirlist_add(dl, NULL);
	do {
		char* full_path = join(root_name, dl->data[i]);
		DIR* dir = opendir(full_path);
		if(dir) { 
			struct dirent* d;
			while((d = readdir(dir))) {
				if(!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
					continue;
				dirlist_add(dl, join(dl->data[i], d->d_name));
			}
		}
		free(full_path);
		closedir(dir);
	} while(++i < dl->size);
}

void
create(struct dirlist* dl,
       const char* root_name,
       const char* target_name)
{
	struct stat sb;
	size_t i;
	for(i = 0; i < dl->size; ++i) {
		char* root_path = join(root_name, dl->data[i]);
		char* target_path = join(target_name, dl->data[i]);
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
delete(struct dirlist* dl,
       const char* root_name,
       const char* target_name)
{
	struct stat sb;
	size_t i;
	for(i = 0; i < dl->size; ++i) {
		char* current = dl->data[dl->size - 1 - i];
		char* root_path = join(root_name, current);
		char* target_path = join(target_name, current);
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
	struct dirlist* dl = dirlist_make();
	dirlist_fill(dl, root_name_abs);

	if(delete_flag)
		delete(dl, root_name_abs, target_name_abs);
	if(create_flag)
		create(dl, root_name_abs, target_name_abs);
	free(root_name_abs);
	free(target_name_abs);
	dirlist_delete(dl);
}