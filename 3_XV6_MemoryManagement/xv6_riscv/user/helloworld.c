#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"
#include "ustack.h"

void test1()
{
    printf("helloworld\n");

    // fork test
    int pid = fork();
    if (pid == 0){
        printf("child\n");

        int mallo = 18;
        char *a;
        for (int i = 0; i < mallo; i++)
        {
            a = malloc(4096);
            // fill in the memory with some data
            for (int j = 0; j < 4096; j++)
            {
                a[j] = 'a';
            }
            sleep(10);
            printf("malloc 4096\n");
            free(a);
            printf("free 4096\n");
        }
        exit(0);
    }

    else{
        printf("parent\n");
        wait(0);
    }
}

void test2(){
    char *a = malloc(4096);
    char *b = malloc(4096);
    char *c = malloc(4096);
    char *d = malloc(4096);

    for (int j = 0; j < 4096; j++){
        a[j] = 'a';
    }

    for (int j = 0; j < 4096; j++){
        b[j] = 'b';
    }

    for (int j = 0; j < 4096; j++){
        c[j] = 'c';
    }

    for (int j = 0; j < 4096; j++){
        d[j] = 'd';
    }
    
    free(a);
    free(b);
    free(c);
    free(d);
}

void test3(){
    //fork test
    int pid = fork();
    if(pid == 0){
        printf("child\n");

        int mallo = 20;

        for(int i = 0; i < mallo; i++){
            for(int m = 0; m < 8; m++){//512 * 8 = 4096
                char *a = ustack_malloc(512);
                //fill in the memory with some data
                for(int j = 0; j < 512; j++){
                    a[j] = 'a';
                }
                printf("malloc 4096\n");
            }
            
        }

        //free the memory
        for(int i = 0; i < mallo; i++){
            for(int j = 0; j < 8 ;j++){
                ustack_free();
            }
        }
    
        exit(0);
    }
    else{
        printf("parent\n");
        //sleep(150);
        wait(0);
    }
}

void test4() //emanuel
{
    int pid1 = fork();
    int pid2 = fork();
    int *arr = malloc(20 * PGSIZE);
    free(arr);
    arr = malloc(16 * PGSIZE);
    for (int i = 0; i < 1024; i++)
    {
        arr[i] = i;
    }
    for (int i = 0; i < 1024; i++)
    {
        if (pid1 & pid2)
        {
            printf("%d\n", arr[i]);
        }
    }
    free(arr);
}

void test5() { //emanuel
    char *arr[40];
    for (int i = 0; i < 40; i++) {
        printf("Writing: %d\n", i);
        arr[i] = ustack_malloc(512);
        for (int j = 0; j < 512; j++) {
            arr[i][j] = i;
        }
    }
    if (fork()) {
        sleep(10);
    }
    for (int i = 0; i < 40; i++) {
        printf("Testing: %d\n", i);
        for (int j = 0; j < 512; j++) {
            if (arr[i][j] != i) {
                printf("Error!\n");
                exit(0);
            }
            printf("%c", arr[i][j]);
        }
    }
    if (fork()) {
        sleep(20);
    }
    for (int i = 0; i < 40; i++) {
        printf("Freeing: %d\n", i);
        ustack_free();
    }
    exit(0);
}

void test6(){
    char *a = ustack_malloc(512);
    printf("this is the va of a = %p\n", a);
    for(int j = 0; j < 512; j++){
        a[j] = 'a';
    }

    int mallo = 18;

    for(int i = 0; i < mallo; i++){
        for(int m = 0; m < 8; m++){//512 * 8 = 4096
            char *b = ustack_malloc(512);
            //fill in the memory with some data
            for(int j = 0; j < 512; j++){
                b[j] = 'b';
            }
        }        
    }

    // Print the contents of the 'a' buffer
    printf("starting to print from user\n");
    for (int m = 0; m < 512; m++) {
        // if(a[m] != 'a'){
        //     printf("error!\n");
        // }
        // else{
        //     printf("ok!\n");
        //     printf("%c", a[m]);
        // }
        
        printf("%c", a[m]);
    }
    printf("\n\nend print from user\n");

    //free the memory
    for(int i = 0; i < mallo; i++){
        for(int j = 0; j < 8 ;j++){
            ustack_free();
        }
    }
    ustack_free();
    exit(0);
}

void
test7()
{
    printf("helloworld\n");

    //fork test
    int pid = fork();
    if(pid == 0){
        printf("child\n");
 
        int mallo = 20;
        char *arr[mallo];
        for(int i = 0; i < mallo; i++){
            char *a = malloc(4096);
            //fill in the memory with some data
            //keep a in an array
            arr[i] = a;

            for(int j = 0; j < 4096; j++){
                a[j] = 'a';
            }
            printf("malloc 4096\n");
        }

        //free the memory
        for(int i = 0; i < mallo; i++){
            free(arr[i]);
        } 
        printf("free 4096\n");

    }
    else{
        printf("parent\n");
        wait(0);
    }
    
exit(0);
}


void test8()
{
    int num_pages = 25;
    char* all_addr[num_pages];
    for(int i = 0; i < num_pages; i++){ //allocate num_pages pages
        all_addr[i] = sbrk(4096);
        for(int j = 0; j < 4096; j++){
            all_addr[i][j] = i;
        }
    }

    sleep(30);

    for(int t = 0; t < num_pages; t++){
        for(int m = 0; m < 4096; m++){
            // printf("%d", all_addr[t][m]);
        }
    }

    for(int i = 0; i < num_pages; i++){
        sbrk(-4096);
    }
}
int main(int argc, char *argv[])
{
    printf("helloworld\n");
    test1();
    // test2();
    // test3();
    // test4();
    // test5();
    // test6();
    // test7();
    // test8(); //with sbrk
    
    exit(0);
}