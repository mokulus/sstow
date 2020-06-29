#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

void
die(const char *errstr, ...)
{
	va_list ap;
	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(1);
}

char*
join(const char* s1, const char* s2)
{
	size_t len = strlen(s1) + strlen(s2) + 2; /* / and null */
	char* str = malloc(len);
	if(!str)
		die("malloc join");
	strcpy(str, s1);
	strcat(str, "/");
	strcat(str, s2);
	return str;
}

struct dirent
get_dirent(const char* path) {
	DIR* dir = opendir(path);
	if(!dir)
		die("opendir %s", path);
	struct dirent* d;
	struct dirent ret;
	while((d = readdir(dir))) {
		if(!strcmp(d->d_name, ".")) {
			ret = *d;
			break;
		}
	}
	closedir(dir);
	return ret;
}

void
iter_dirs(DIR* root,
	  const char* root_name,
	  const char* target_name,
	  void (*func)(const struct dirent* d,
		       const struct stat* sb,
		       const char* root_d_name,
		       const char* target_d_name))
{
	struct dirent* d;
	struct stat sb;
	while((d = readdir(root))) {
		if(!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		char* root_d_name = join(root_name, d->d_name);
		char* target_d_name = join(target_name, d->d_name);
		if(lstat(root_d_name, &sb) == -1)
			die("lstat %s", d->d_name);
		func(d, &sb, root_d_name, target_d_name);
		free(target_d_name);
		free(root_d_name);
	}
	rewinddir(root);
	while((d = readdir(root))) {
		if(!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		char* root_d_name = join(root_name, d->d_name);
		char* target_d_name = join(target_name, d->d_name);
		if(lstat(root_d_name, &sb) == -1)
			die("lstat %s", d->d_name);
		switch(sb.st_mode & S_IFMT) {
		case S_IFDIR:
			iter_dirs();
		break;
		}
		free(target_d_name);
		free(root_d_name);
	}
}

void
create(const struct dirent* d,
       const struct stat* sb,
       const char* root_d_name,
       const char* target_d_name)
{
	(void)d;
	switch(sb->st_mode & S_IFMT) {
	case S_IFDIR:;
		printf("mkdir %s\n", root_d_name);
	break;
	default:
		printf("%s -> %s\n", root_d_name, target_d_name);
	}
}

int
main(int argc, const char** argv)
{
	if(argc != 3)
		die("usage: %s target package\n", argv[0]);
	const char* target_name = argv[1];
	const char* root_name = argv[2];

	DIR* root = opendir(root_name);
	if(!root)
		die("opendir %s", root_name);
	iter_dirs(root, root_name, target_name, create);
	closedir(root);
}
