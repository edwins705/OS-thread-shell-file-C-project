#include<stdio.h>
#include<stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"

#define MAX_FILDES 32
#define MAX_FILE 64
#define MAX_F_NAME 15
#define DATA_SIZE 4096
// #define DATA_SIZE 6144
#define ENTRY_SIZE 64

struct super_block {
    int fat_idx; // First block of the FAT 
    int fat_len; // Length of FAT in blocks 
    int dir_idx; // First block of directory 
    int dir_len; // Length of directory in blocks 
    int data_idx; // First block of file-data
};

struct dir_entry {
    int used; // Is this file-”slot” in use 
    char name [MAX_F_NAME + 1]; // DOH!
    int size; // file size in bytes
    int head; // first data block of file 
    int ref_cnt;
    // how many open file descriptors are there?
    // ref_cnt > 0 -> cannot delete file
};

struct file_descriptor {
    int used; // fd in use
    int file; // the first block of the file
    // (f) to which fd refers too
    int offset; // position of fd within f
};

//belongs in the memory
struct super_block fs;
struct file_descriptor fildes[MAX_FILDES]; // 32
int *FAT; // Will be populated with the FAT data
struct dir_entry *DIR;

static int first=0;
static char diskn[16];
static int mount=0;

char* dname;

int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);

int fs_open(char *name);
int fs_close(int fildes);
int fs_create(char *name);
int fs_delete(char *name);

int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);

int fs_get_filesize(int fildes);
int fs_listfiles(char ***files);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

//what is the head and ref_count and size

int make_fs(char *disk_name){ // make fs in disk
    if(make_disk(disk_name)){
        printf("error making disk");
        return -1;
    }
    if(open_disk(disk_name)){
        printf("opening disk fails stupid");
        return -1;
    }

    strcpy(diskn, disk_name);

    char* buf=malloc(BLOCK_SIZE);
    
    //writing super block meta data
    int fat_length = DATA_SIZE * 4 / BLOCK_SIZE;  

    memset(buf, 0, BLOCK_SIZE);
    struct super_block* sp = malloc(sizeof(struct super_block));
    sp->fat_idx=2;
    sp->fat_len=fat_length;
    sp->dir_idx=1;
    sp->dir_len=1;
    sp->data_idx=DISK_BLOCKS - DATA_SIZE;
    memcpy(buf, sp, sizeof(struct super_block));
    block_write(0, buf);

    //writing directory meta data
    memset(buf, 0, BLOCK_SIZE);
    struct dir_entry* DE = malloc(ENTRY_SIZE * sizeof(struct dir_entry));

    int i;
    for(i=0; i<ENTRY_SIZE; i++){
        DE[i].used=0;
        memset(DE[i].name, 0, sizeof(DE[i].name));
        DE[i].size=0;
        DE[i].head=0;
        DE[i].ref_cnt=0;
    }
    memcpy(buf, DE, ENTRY_SIZE*sizeof(struct dir_entry));
    block_write(1, buf);
    
    //writing FAT
    int F[DISK_BLOCKS]={0};//said it was DISK_BLOCKS
    for (i=0; i<DISK_BLOCKS; i++){
        F[i]=-2;//set all entries as free
    }
    for (i=0; i<10; i++){
       F[i]=-3; //reserved
    }
    
    // writing FAT into disk
    int meta = sp->dir_len + sp->fat_len;
    for (i=0; i<meta; i++){
        memset(buf, 0, BLOCK_SIZE);
        memcpy(buf, &F[i*1024], BLOCK_SIZE);
        block_write(2+i, buf);
    }
    
    first = 1;

    if(close_disk(disk_name)){
        return -1;
    }
    return 0;
}

int mount_fs(char *disk_name){//read fs into memory
    if(!first){
    printf("finished making");

        return -1;
    }

    if(strcmp(disk_name, diskn)!=0){
        return -1;
    }

    if(open_disk(disk_name)){
        return -1;
    }

    mount=1;

    char buf[BLOCK_SIZE];
    
    //mounting the superblock
    memset(buf, 0, BLOCK_SIZE);
    block_read(0, buf);
    struct super_block* sp = malloc(sizeof(struct super_block));
    memcpy(sp, buf, sizeof(struct super_block));
    fs.fat_idx = sp->fat_idx;
    fs.fat_len = sp->fat_len;
    fs.dir_idx = sp->dir_idx;
    fs.dir_len = sp->dir_len;
    fs.data_idx = sp->data_idx;
    
    //mounting the directory
    memset(buf, 0, BLOCK_SIZE);
    block_read(fs.dir_idx, buf);
    struct dir_entry* DE = malloc(ENTRY_SIZE* sizeof(struct dir_entry));
    memcpy(DE, buf, ENTRY_SIZE* sizeof(struct dir_entry));
    DIR=(struct dir_entry *)DE;
            

    //mounting the FAT
    int* F = malloc(DISK_BLOCKS*sizeof(int));
    int i=0;

    int meta = fs.dir_len + fs.fat_len;

    while(i<meta){
        memset(buf, 0, BLOCK_SIZE);
        block_read(fs.fat_idx+i, buf);
        memcpy(&(F[i*1024]), buf, BLOCK_SIZE);
        i++;
    }
            
    FAT=F;

    return 0;
}
int umount_fs(char *disk_name){//write memory into disk
    if(close_disk(disk_name)){
        return -1;
    }
    if(first==0){
        return -1;
    }

    if(strcmp(disk_name, diskn)!=0){
        return -1;
    }

    if(mount!=1){
        return -1;
    }
    if(disk_name==NULL){
        return -1;
    }

    if(open_disk(disk_name)){
        return -1;
    }

    char buf[BLOCK_SIZE];

    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &fs, sizeof(struct super_block));
    block_write(0, buf);

    memset(buf, 0, BLOCK_SIZE);

    memcpy(buf, DIR, ENTRY_SIZE*sizeof(struct dir_entry));
    block_write(1, buf);

    int meta = fs.dir_len + fs.fat_len;

    char buf2[meta*BLOCK_SIZE];
    memset(buf2, 0, meta*BLOCK_SIZE);
    memcpy(buf2, FAT, meta*BLOCK_SIZE);

    int i;
    for (i=0; i<meta; i++){
        block_write(2+i, &buf2[i*BLOCK_SIZE]);
    }

    if(close_disk(disk_name)){
        return -1;
    }

    mount=0;

    return 0;
}

int fs_open(char *name){
    if (first==0){
        return -1;
    }
    if(name==NULL){
        return -1;
    }

    int found=0;
    int i;
    for(i=0; i<MAX_FILDES;i++){
        if(strcmp(DIR[i].name, name)==0){
            found =1;
            break;
        }
    }
    if(found==0){
        return -1;
    }

    DIR[i].ref_cnt++;

    int found_id=0;
    int j;
    for(j=0; j<MAX_FILDES;j++){
        if(fildes[j].used==0){
            found_id=1;
            break;
        }
    }

    if(found_id==0){
        return -1;
    }
    printf("fd %i\n", j);

    fildes[j].used=1;
    fildes[j].file = DIR[i].head;
    fildes[j].offset = 0;

    printf("create file desc in block %d for directory %d \n", fildes[j].file , i);

    return j;
}
int fs_close(int fd){
    if(!first){
        return -1;
    }
    if(fd>31 || fd < 0){
        return -1;
    }

    if(fildes[fd].used == 0){
        return -1;
    }

    fildes[fd].used=0;
    fildes[fd].offset=0;

    int k;
	for(k = 0; k<ENTRY_SIZE; k++)
	{
		// If the file is open, return an error
		if(fildes[fd].file==DIR[k].head)
		{
            
			break;
		}
	}
    if(k==ENTRY_SIZE){
        return -1;
    }

    fildes[fd].file=0;

    if(DIR[k].ref_cnt>0){
        printf("fd %d ref_count going down\n", fd);
        DIR[k].ref_cnt--;
    }
    return 0;
}
int fs_create(char *name){
    if(!first){
        return -1;
    }
    if (name==NULL){
        return -1;
    }
    if(strlen(name)>MAX_F_NAME){
        return -1;
    }
    int found=0;
    int i;
    for(i = 0; i<ENTRY_SIZE; i++)
	{
	    if(strcmp(DIR[i].name, name)==0){
            return -1;
        }

	    if(DIR[i].used==0){
            found=1;
            break;
        }
	}
    printf("dir entry %d \n", i);

    if(!found){
        return -1;
    }

    DIR[i].used=1;
    strcpy(DIR[i].name, name);
    DIR[i].size=0;

    int found_fat=0;
    int j;
    for (j=fs.data_idx; j<DISK_BLOCKS; j++){
        if(FAT[j]==-2){
            found_fat=1;
            break;
        }
    }
    if(!found_fat){
        return -1;
    }

    printf("create file in block %d \n", j);

    FAT[j] = -1; //changed form j

    DIR[i].head = j;
    DIR[i].ref_cnt=0;

    return 0;
}
int fs_delete(char *name){
    if (!first){
        return -1;
    }
    if (name==NULL){
        return -1;
    }

    int found=0;
    int i;
    for(i = 0; i<ENTRY_SIZE; i++)
	{
        if(strcmp(DIR[i].name, name) == 0)
		{
            found=1;
			break;
		}
	}

    if(!found){
        return -1;
    }

    int j;
	for(j = 0; j<MAX_FILDES; j++)
	{
		if(fildes[j].file==DIR[i].head)
		{
			return -1;
		}
	}

    DIR[i].used=0;
    memset(DIR[i].name, 0, sizeof(DIR[i].name));
    DIR[i].size=0;
    DIR[i].ref_cnt=0;

    int current_block = DIR[i].head;
    int remove=0;
    do{
        printf("FAT[%d]=%d \n", current_block, FAT[current_block]);
        remove=current_block;
        current_block = FAT[current_block];
        FAT[remove]=-2;
    }
    while(current_block>0);


    current_block = DIR[i].head;
    do{
        printf("FAT[%d]=%d \n", current_block, FAT[current_block]);
        current_block = FAT[current_block];
    }
    while(current_block>0);

    DIR[i].head = 0;
    return 0;
}

int fs_write(int fd, void *buf, size_t nbyte){

    if(!first){
        return -1;
    }
    if (fd>31 || fd <0){
        return -1;
    }
    if(fildes[fd].used==0){
        return -1;
    }
    if(nbyte <=0){
        return 0;
    }

    int k;
	for(k = 0; k<ENTRY_SIZE; k++)
	{
		if(fildes[fd].file==DIR[k].head)
		{
			break;
		}
	}
    if(k==ENTRY_SIZE){
        return -1;
    } 

    if( (fildes[fd].offset + nbyte) > DATA_SIZE * BLOCK_SIZE){
        nbyte = (DATA_SIZE * BLOCK_SIZE)-(fildes[fd].offset) ;
        printf("YEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE %ld \n", nbyte/BLOCK_SIZE/256);

    }

    int numblocks=(nbyte + fildes[fd].offset + BLOCK_SIZE-1)/BLOCK_SIZE;
    
    char* buffer = malloc(numblocks*BLOCK_SIZE);
    memset(buffer, 0, numblocks*BLOCK_SIZE);

    int i;
        int read_block=fildes[fd].file;
        for(i=0; i<numblocks; i++){
            if(read_block>=fs.data_idx){
                block_read(read_block, buffer+i*BLOCK_SIZE);
                read_block = FAT[read_block];
            }
        }

    memcpy(buffer+fildes[fd].offset, buf, nbyte);
    
    int current_block=fildes[fd].file;

    
    for (i=0; i<numblocks; i++){
    
        block_write(current_block, buffer+i*BLOCK_SIZE); // rearranged
       
        int idx;
        if (FAT[current_block]<0){
            
            int j;
            for (j=fs.data_idx; j<DISK_BLOCKS; j++){
                if(FAT[j]==-2 ){
                    break;
                }
            }

            if (j==DISK_BLOCKS){
               
                FAT[current_block]=-1;
                
                break;
            }
            FAT[current_block]=j;
            FAT[j]=-1;
            idx=j;
            
            
        }
       
        if(i==numblocks){
            FAT[current_block]=-1;
            FAT[idx]=-2;
        }
        else{
            current_block = FAT[current_block];
        }
        
    }   

    if(DIR[k].size <= (nbyte+fildes[fd].offset)){
        DIR[k].size=nbyte+fildes[fd].offset;
    }

    fildes[fd].offset = nbyte+fildes[fd].offset;

    return nbyte;
}

//check this later for the nbytes
int fs_read(int fd, void *buf, size_t nbyte){

    if(!first){
        return -1;
    }

    if(fd> 31 || fd <0){
        return -1;
    }

    if(fildes[fd].used==0){
       return -1;
    }

    int k;
	for(k = 0; k<ENTRY_SIZE; k++)
	{
		if(fildes[fd].file==DIR[k].head)
		{
			break;
		}
	}
    if(k==ENTRY_SIZE){
        return -1;
    }

    if(fildes[fd].offset >= DIR[k].size+fildes[fd].offset){
        return -1;
    }

    if(nbyte+fildes[fd].offset > DIR[k].size){
        nbyte = DIR[k].size - fildes[fd].offset;
    }

    int total_size = DIR[k].size;
    int total_block = (total_size + BLOCK_SIZE- 1) /BLOCK_SIZE;

    int current_block = fildes[fd].file;
    char buffer[total_block*BLOCK_SIZE];
    memset(buffer, 0, total_block*BLOCK_SIZE);
    int i;
    for(i=0; i<total_block; i++){
        
        printf("CB: %d \n", current_block);
        block_read(current_block, buffer+i*BLOCK_SIZE);
        current_block = FAT[current_block];
    }

    memcpy(buf, buffer+fildes[fd].offset, nbyte);

    fildes[fd].offset=nbyte+fildes[fd].offset;

    current_block = fildes[fd].file;
    do{
        printf("FAT[%d]=%d \n", current_block, FAT[current_block]);
        current_block = FAT[current_block];
    }
    while(current_block>0);

    return nbyte;
}

int fs_get_filesize(int fd){
    if (!first){
        return -1;
    }
    if (fd>31 || fd < 0){
        return -1;
    }
    if(fildes[fd].used==0){
        return -1;
    }

    int k;
	for(k = 0; k<ENTRY_SIZE; k++)
	{
		if(fildes[fd].file==DIR[k].head)
		{
			break;
		}
	}
    if(k==ENTRY_SIZE){
        return -1;
    }    

    return DIR[k].size;
}

int fs_listfiles(char ***files){

    if (first==0){
        return -1;
    }

    char** farray= (char** ) malloc(ENTRY_SIZE*sizeof(char*));
    int i;
    for (i=0; i<ENTRY_SIZE; i++){
        farray[i] = (char *) malloc(16* sizeof(char));
    }
    int cnt=0;

    for (i=0; i<ENTRY_SIZE; i++){
        if (DIR[i].used==1){
            strcpy(farray[cnt], DIR[i].name);
            cnt++;
        }
    }
    farray[cnt]=NULL;

    *files=farray;

    return 0;
}

int fs_lseek(int fd, off_t offset){
    if (first==0){
        return -1;
    }
    if (fd > 31 || fd <0){
        return -1;
    }
    if(fildes[fd].used==0){
        return -1;
    }
    if (offset<0){
        return -1;
    }
    int size= fs_get_filesize(fd);
    if (size < offset){
        return -1;
    }
    
    fildes[fd].offset = offset;

    return 0;
}

int fs_truncate(int fd, off_t length){
    if (first==0){
        return -1;
    }
    if (fd > 31 || fd <0){
        return -1;
    }
    if(fildes[fd].used==0){
        return -1;
    }
    if(length < 0){
        return -1;
    }
    int size = fs_get_filesize(fd);
    if (size < length){
        return -1;
    }

    if (fildes[fd].offset > length){
        fildes[fd].offset=length;
    }

    int k;
	for(k = 0; k<ENTRY_SIZE; k++)
	{
		if(fildes[fd].file==DIR[k].head)
		{
			break;
		}
	}
    if(k==ENTRY_SIZE){
        return -1;
    }

    int num_blocks = (DIR[k].size+ BLOCK_SIZE - 1)/BLOCK_SIZE; 
    DIR[k].size=length;
    
    int trunc_blocks = (length + BLOCK_SIZE - 1)/BLOCK_SIZE; 

    int current_block = fildes[fd].file;
    int term;
    int i;
    for (i=0; i<num_blocks; i++){
        if (i==trunc_blocks-1){
            term = current_block;
            current_block = FAT[current_block];
            FAT[term]=-1;
        }
        if(i>trunc_blocks){
            term = current_block;
            current_block = FAT[current_block];
            FAT[term]=-2;
        }
        current_block = FAT[current_block];
    }

    return 0;
}