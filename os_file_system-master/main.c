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
#include "fs.h"

int main(){
    char* disk_name = "disk1";
    make_fs(disk_name);
    mount_fs(disk_name);

    printf("calculating %d \n", (1048576 + 4096 -1 )/4096);

    fs_create("file1");
    fs_open("file1");

    fs_create("file2");
    fs_open("file2");
    
    int size = 1048576;
    char buf[size];
    memset(buf, 9, size);

    int i;
    for (i=0; i<15; i++){
        fs_write(0, buf, size);
    }

    for (i=0; i<2; i++){
        fs_write(0, buf, size);
    }

    //     printf("asdf \n \n \n");

    // for (i=0; i<15; i++){
    //     fs_write(1, buf, size);
    // }

    // fs_delete("file2");

    // fs_create("file3");
    // fs_open("file3");
    // for (i=0; i<1; i++){
    //     fs_write(2, buf, size);
    // }

    // fs_lseek(0, 500);
    // memset(buf, 2, size);
    // fs_write(0, buf, 100);

    // char buf2[10];
    // memset(buf2, 0, 10);

    // fs_lseek(0, 500);
    // fs_read(0, buf2, 10);

    // for (i=0; i<10; i++){
    //     printf("read buf %d \n", buf2[i]);
    // }

    /*
    
    // printf("buf[0] %d\n", buf[0]);


 
    fs_lseek(0, 0);
    fs_write(0, buf, 4096 * 4096);
   
    

    int i;
    for (i=0; i<10; i++){
        printf("read buf %d \n", buf2[i]);
    }*/
    /*
    printf("\n \n");
    memset(buf2, 0, 10);

    fs_lseek(0, 500);
    fs_read(0, buf2, 10);*/


    // fs_close(0);
    return 0;
}