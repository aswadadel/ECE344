#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

void copyFile(char src[], char dest[]){
//open src
	int fdsrc;
	fdsrc = open(src, O_RDONLY);
	if(fdsrc < 0) syserror(open,src);

//create file
    int fddest = creat(dest, 0700);
	if(fddest < 0) syserror(creat, dest);
    
//read and write loop
    int n = 8;
    char buf[n];

	int readReturn = read(fdsrc, buf, n);
    if(readReturn < 0) syserror(read, dest);

    do{
        int wrReturn = write(fddest, buf, readReturn);
        if(wrReturn < 0) syserror(write, dest);

        readReturn = read(fdsrc, buf, n);
    	if(readReturn < 0) syserror(read, src);

    } while(readReturn != 0);

//set permissions
    struct stat statbuf;
    int stats = stat(src, &statbuf);
    if(stats < 0) syserror(stat, src);

	int modReturn = chmod(dest, statbuf.st_mode);
	if(modReturn == -1) syserror(chmod, dest);

//close both files
    int fdsrcClose, fddestClose;

    fdsrcClose = close(fdsrc);
    if(fdsrcClose < 0) syserror(close, src);

    fddestClose = close(fddest);
    if(fddestClose < 0) syserror(close, dest);
}

void formatPath(char *path, char *parent, char *file){
	strcpy(path, parent);
	strcat(path, "/");
	strcat(path, file);
}

void copyDir(char src[], char dest[]){
//make new directory
	int make = mkdir(dest, 0700);
	if(make == -1) syserror(mkdir, dest);

//open
	DIR *directory = opendir(src);
	if(directory == NULL) syserror(opendir, src);
	
//skip first 2 entries
	struct dirent *entry;
	for(int i = 0; i < 3; i++){
		entry = readdir(directory);
		if(errno != 0) syserror(readdir, src);
	}
	
	if(entry == NULL){
		int close = closedir(directory);
		if(close == -1) syserror(closedir, src);
		return;
	}

//traverse directory
	char destPath[512];
	char srcPath[512];
	struct stat fileStat;
	do{
		formatPath(srcPath, src, entry->d_name);
		formatPath(destPath, dest, entry->d_name);

		int temp = stat(srcPath, &fileStat);
		if(temp < 0) syserror(temp, srcPath);
		//printf("%s\n", destPath);
		if(S_ISDIR(fileStat.st_mode)){
			copyDir(srcPath, destPath);
		} else{
			copyFile(srcPath, destPath);
		}	

		entry = readdir(directory);
		if(errno != 0) syserror(readdir, src);
	} while(entry != NULL);

//set permissions
	struct stat statbuf;
    int stats = stat(src, &statbuf);
    if(stats < 0) syserror(stat, src);

	int modReturn = chmod(dest, statbuf.st_mode);
	if(modReturn == -1) syserror(chmod, dest);

//close
	int close = closedir(directory);
	if(close == -1) syserror(closedir, src);
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}
	
	copyDir(argv[1], argv[2]);
	
	//int make = mkdir("newTest", 0001);
	//if(make == -1) syserror(mkdir, "newTest");

	return 0;
}
