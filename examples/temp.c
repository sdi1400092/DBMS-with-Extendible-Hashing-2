#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    int *a, b;
    a = (int *) malloc(10*sizeof(int));
    for(int i=0 ; i<10 ; i++){
        a[i] = i;
    }
    for(int i=0 ; i<10 ; i++){
        b = i+1;
        memcpy(&(a[i]), &b, sizeof(int));
    }
    for(int i=0 ; i<10 ; i++){
        printf("%d\n", a[i]);
    }
}