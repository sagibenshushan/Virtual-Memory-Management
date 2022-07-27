#include <iostream>
#include "sim_mem.h"
int main() {
    char val;
    sim_mem mem_sm((char*)"exec_file1", (char*)"exec_file2", (char*)"swap_file" ,25, 50, 25,25, 25, 5,2);
    mem_sm.store(1, 98,'X');
    val = mem_sm.load (1, 98);
    printf("\nval: %c \n", val);
    mem_sm.store(2, 98,'X');
    val = mem_sm.load (2, 98);
    printf("\nval: %c \n", val);

    val = mem_sm.load (1, 23);
    printf("\nval: %c \n", val);

    mem_sm.store(1, 23,'T');
    mem_sm.store(1, 35,'P');

    val = mem_sm.load (1, 35);
    printf("\nval: %c \n", val);

    mem_sm.store(1, 36,'Q');
    val = mem_sm.load (1, 36);
    printf("\nval: %c \n", val);

    mem_sm.store(1, 13,'7');
    val = mem_sm.load (1, 13);
    printf("\nval: %c \n", val);

    mem_sm.store(1, 33,'Y');
    val = mem_sm.load (1, 33);
    printf("\nval: %c \n", val);

    mem_sm.store(1, 50,'N');
    mem_sm.store(1, 97,'M');

    mem_sm.print_memory();
    mem_sm.print_swap();
    mem_sm.print_page_table();

    return 0;
}

