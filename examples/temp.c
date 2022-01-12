#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    char s[20] = "Ioannidis";
    int counter = 0, i = 0;
    while(s[i] != '\0'){
        counter += s[i];
        i++;
    }
    int *binary;
    binary = (int *) malloc(10*sizeof(int));
    for(i=0;i<10;i++){
        binary[i] = counter%2;
        counter = counter/2;
    }
    for(i=9;i>=0;i--){
        printf("%d",binary[i]);
    }
    printf("\n");

    return 0;
}