#include "../include/shell.h"
#include "../include/process.h"


int main (void){
    shell_init();
    shell_run();
    shell_cleanup();
}
