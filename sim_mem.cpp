#include "sim_mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;
char main_memory[MEMORY_SIZE];

int frame;       //the index of the page that we are looking for and if i looking for the next page to remove from the RAM
int counterOfPagesInRam; //numbers of pages that got into the RAM

sim_mem::sim_mem(char exe_file_name1[], char exe_file_name2[], char swap_file_name[],
                 int text_size,       //constractor to build the class
                 int data_size, int bss_size, int heap_stack_size,
                 int num_of_pages, int page_size, int num_of_process) {

    this->text_size = text_size;
    this->data_size = data_size;
    this->bss_size = bss_size;
    this->heap_stack_size = heap_stack_size;
    this->num_of_pages = num_of_pages;
    this->page_size = page_size;
    this->num_of_proc = num_of_process;

    int pagesOfText = text_size / page_size;  //numbers of pages that belong to the text

    if ((this->program_fd[0] = open(exe_file_name1, O_RDWR, 0)) == -1) {    //open file1 for read only
        perror("open file1 failed");
    }
    if (this->num_of_proc == 2) {
        if ((this->program_fd[1] = open(exe_file_name2, O_RDWR, 0)) == -1) { //open file2 for read only
            perror("open file2 failed");
        }
    }
    this->swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR); //open swap file
    if (this->swapfile_fd == -1) {
        perror("open swap_file failed");
    }

    this->page_table = (page_descriptor **) malloc(num_of_process * sizeof(page_descriptor *));
    this->page_table[0] = (page_descriptor *) malloc(num_of_pages * sizeof(page_descriptor));
    for (int j = 0; j < num_of_pages; j++) {
        page_table[0][j].V = 0;
        page_table[0][j].frame = -1;
        page_table[0][j].D = 0;
        page_table[0][j].swap_index = -1;
        if (j < pagesOfText) {
            page_table[0][j].P = 0;
        } else {
            page_table[0][j].P = 1;
        }
    }
    if (this->num_of_proc == 2){   //if we have 2 process we have to make 2 page table
        this->page_table[1] = (page_descriptor *) malloc(num_of_pages * sizeof(page_descriptor));
        for (int j = 0; j < num_of_pages; j++) {
            page_table[1][j].V = 0;
            page_table[1][j].frame = -1;
            page_table[1][j].D = 0;
            page_table[1][j].swap_index = -1;
            if (j < pagesOfText) {
                page_table[1][j].P = 0;
            } else {
                page_table[1][j].P = 1;
            }
        }
    }
    for (int i = 0; i < (bss_size+heap_stack_size+data_size) * num_of_proc; i++) {  //init for the swap file
        write(this->swapfile_fd, "0", 1);
    }
    for (int k = 0; k < MEMORY_SIZE; k++) {  //init for the main memory
        main_memory[k] = '0';
    }
    counterOfPagesInRam = 0;  //global var
    frame = 0;               //global var
}

sim_mem::~sim_mem() {                             //distractor, close the program, free mallocs/new
    for (int i = 0; i < num_of_proc; i++) {
        free(page_table[i]);
    }
    free(page_table);
    close(program_fd[0]);
    close(program_fd[1]);
    close(swapfile_fd);
}

char sim_mem::load(int process_id, int address) {  //to return the char in the address index

    process_id--;                //if process id = 1 i want him to be page table[0]
    int pageNum = address / page_size;   //for exemple 50/5 = 10 => the page of the adrees is page 10
    int offset = address % page_size;   // for exmple 22%10 = 2 => the offset is 2
    if (page_table[process_id][pageNum].V == 1) {       // if valid == 1 we take it from the physical memory address and update the page table

        frame = page_table[process_id][pageNum].frame;  //update
        return main_memory[(frame * page_size) + offset];           //the position of return value
    } else if (page_table[process_id][pageNum].V == 0) {
        if (page_table[process_id][pageNum].P == 0)   // the valid = 0, p = 0 => this is page of text => copy page from exec
        {
            if (counterOfPagesInRam < MEMORY_SIZE / page_size) {
                frame = fromExec(process_id, pageNum);
                return main_memory[(frame * page_size) + offset];           //the position of return value
            } else {
                memoryIsFull(process_id);
                lseek(program_fd[process_id], (pageNum * page_size), SEEK_SET);
                read(program_fd[process_id], &main_memory[frame * page_size],page_size);   //read page size of chars into the RAM(main memory)
                page_table[process_id][pageNum].V = 1;                      //update
                page_table[process_id][pageNum].frame = frame;              //update
                counterOfPagesInRam++;
                return main_memory[(frame * page_size) + offset];           //the position of return value
            }
        }

        if (page_table[process_id][pageNum].P == 1) {  // the valid = 0, p = 1 => this is not page of text => this page can be change
            if (page_table[process_id][pageNum].D == 1) {  // bring from the swap
                if (counterOfPagesInRam < MEMORY_SIZE / page_size) {
                    bringFromSwap(process_id, pageNum);
                    counterOfPagesInRam++;
                    return main_memory[(frame * page_size) + offset];           //the position of return value
                } else {
                    memoryIsFull(process_id); //move the relevant page to the swap
                    bringFromSwap(process_id, pageNum);  //copy from swap to maim memory
                    return main_memory[(frame * page_size) + offset];           //the position of return value
                }

            } else if (page_table[process_id][pageNum].D == 0) {   //the page is clean
                if (isData(address) == 1) {  //data area
                    if (counterOfPagesInRam < MEMORY_SIZE / page_size) {
                        frame = fromExec(process_id, pageNum);
                        return main_memory[(frame * page_size) + offset];           //the position of return value
                    } else {
                        memoryIsFull(process_id);                      //remove page from the RAM to the swap
                        frame = fromExec(process_id, pageNum);         //read the page from the exec into the main memory
                        return main_memory[(frame * page_size) + offset];           //the position of return value
                    }
                } else { //stack/heap/bss area
                    if (isBss(address) == 1) { //bss area
                        if (counterOfPagesInRam < MEMORY_SIZE / page_size) {
                            initNewPage(process_id, pageNum);                        //make new page that full by zeros
                            counterOfPagesInRam++;
                            return main_memory[(frame * page_size) + offset];           //the position of return value

                        } else {
                            memoryIsFull(process_id);                     //remove page from the RAM to the swap
                            initNewPage(process_id,pageNum);              //make new page that full by zeros
                            counterOfPagesInRam++;
                            return main_memory[(frame * page_size) + offset];           //the position of return value
                        }

                    } else {//heap/stack area
                        perror("HEAP/STACK- ERROR!");
                        return '\0';
                    }
                }
            }
        }
    }
    return '\0';
}

void sim_mem::store(int process_id, int address, char value) { //to put the value in the address index
    process_id--;                //if process id = 1 i want him to be page table[0]
    int pageNum = address / page_size;            //for exemple 50/5 = 10 => the page of the adrees is page 10
    int offset = address % page_size;             // for exmple 22%10 = 2 => the offset is 2

    if (page_table[process_id][pageNum].P == 0) {
        fprintf(stderr, "ERROR-NO PERMISSION IT'S A TEXT AREA! ");
        return;
    }

    if (page_table[process_id][pageNum].V == 1) {       // if valid == 1 we put it on the physical memory's address and update the page table
        frame = page_table[process_id][pageNum].frame;
        main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;          //acting of store, put the value in the right position
        page_table[process_id][pageNum].D = 1;         //update to dirty, made store
        return;
    }
    if (page_table[process_id][pageNum].P == 1) {  // the valid = 0, p = 1 => this is not page of text => this page can be change
        if (page_table[process_id][pageNum].D == 1)
        {  // bring from the swap
            if (counterOfPagesInRam < MEMORY_SIZE / page_size){
                bringFromSwap(process_id, pageNum);   //copy from swap to maim memory
                counterOfPagesInRam++;
                main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;   //acting of store, put the value in the right position
                page_table[process_id][pageNum].D = 1;  //update to dirty, made store
                return;
            } else {
                memoryIsFull(process_id); //move the relevant page to the swap
                bringFromSwap(process_id, pageNum);  //copy from swap to maim memory
                counterOfPagesInRam++;
                main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;   //acting of store, put the value in the right position
                page_table[process_id][pageNum].D = 1;  //copy from swap to maim memory
                return;
            }

        } else if (page_table[process_id][pageNum].D == 0)
        {   //the page is clean
            if (isData(address) == 1) {  //data area
                if (counterOfPagesInRam < MEMORY_SIZE / page_size) {
                    frame = fromExec(process_id, pageNum);   //read the page from the exec into the main memory
                    main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;  //acting of store, put the value in the right position
                    page_table[process_id][pageNum].D = 1;  //update to dirty, made store
                    return;
                } else {
                    memoryIsFull(process_id);
                    frame = fromExec(process_id, pageNum); //read the page from the exec into the main memory
                    main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;  //acting of store, put the value in the right position
                    page_table[process_id][pageNum].D = 1;  //update to dirty, made store
                    return;
                }

            } else
            { //stack/heap/bss area
                if (counterOfPagesInRam < MEMORY_SIZE / page_size) {
                    initNewPage(process_id, pageNum);              //make new page that full by zeros
                    page_table[process_id][pageNum].frame = frame;      //update
                    main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;  //acting of store, put the value in the right position
                    counterOfPagesInRam++;                     //added page to the RAM
                    page_table[process_id][pageNum].D = 1;   //update to dirty, made store
                    return;
                } else {
                    memoryIsFull(process_id);           //the RAM is full, remove page to the swap
                    initNewPage(process_id,pageNum);   // make new page
                    page_table[process_id][pageNum].frame = frame; // update
                    main_memory[(page_table[process_id][pageNum].frame) * page_size + offset] = value;   //acting of store, put the value in the right position
                    counterOfPagesInRam++;            //added page to the RAM
                    page_table[process_id][pageNum].D = 1;  //update to dirty, made store
                    return;
                }
            }
        }
    }
}

void sim_mem::print_memory() {
    int i;
    printf("\n Physical memory\n");
    for (i = 0; i < MEMORY_SIZE; i++) {
        printf("[%c]", main_memory[i]);
        if((i + 1) % page_size == 0)
            printf("\n");
    }
}

void sim_mem::print_swap() {
    char *str = (char *) malloc(this->page_size * sizeof(char));
    int i;
    printf("\n Swap memory\n");
    lseek(swapfile_fd, 0, SEEK_SET); // go to the start of the file
    while (read(swapfile_fd, str, this->page_size) == this->page_size) {
        for (i = 0; i < page_size; i++) {
            printf("%d - [%c]\t", i, str[i]);
        }
        printf("\n");
    }
    free(str);
}

void sim_mem::print_page_table() {
    int i;
    for (int j = 0; j < num_of_proc; j++) {
        printf("\n page table of process: %d \n", j);
        printf("Valid\t Dirty\t Permission \t Frame\t Swap index\n");
        for (i = 0; i < num_of_pages; i++) {
            printf("[%d]\t[%d]\t[%d]\t[%d]\t[%d]\n",
                   page_table[j][i].V,
                   page_table[j][i].D,
                   page_table[j][i].P,
                   page_table[j][i].frame,
                   page_table[j][i].swap_index);
        }
    }
}

int sim_mem::fromExec(int process_id, int pageNum) {
    frame = counterOfPagesInRam % (MEMORY_SIZE / page_size);                 //update the frame
    lseek(program_fd[process_id], (pageNum * page_size), SEEK_SET);
    read(program_fd[process_id], &main_memory[frame * page_size],page_size);   //read page size of chars from the file into the RAM(main memory)
    page_table[process_id][pageNum].frame = frame;          //update the page frame
    page_table[process_id][pageNum].V = 1;
    counterOfPagesInRam++;                    //how many pages in the RAM
    return frame;
}

void sim_mem::memoryIsFull(int process_id) {
    frame = counterOfPagesInRam % (MEMORY_SIZE / page_size);
    int positionToMove = -1;
    int procToMove = -1;
    for(int k = 0; k < 2; k++) {
        for (int j = 0; j < this->num_of_pages; j++) {                         //check all the pages in the page table
            if (page_table[k][j].frame == frame) {        //frame is the page that have to remove from the main memory
                positionToMove = j;                                //found the page number to move from the page table
                procToMove = k;
            }
        }
    }
    if (page_table[procToMove][positionToMove].D == 1){              //if the page is dirty move the page to the swap but first find the specific place in swap
        page_table[procToMove][positionToMove].swap_index = checkAvialibleInSwap()*page_size;            //update the swap index
        lseek(swapfile_fd, page_table[procToMove][positionToMove].swap_index, SEEK_SET);
        write(swapfile_fd, &main_memory[frame * page_size],page_size);             //write all the page into swap file from main memory
    }
    page_table[procToMove][positionToMove].V = 0;               //update the valid to 0
    page_table[procToMove][positionToMove].frame = -1;         // update the frame to -1
}

int sim_mem::checkAvialibleInSwap() {     //where there is empty place in swap file
    char *str;
    str = (char *) malloc(this->page_size * sizeof(char));
    int i;
    for (int k = 0; k < (bss_size+heap_stack_size+data_size)*num_of_proc; k++) {                  //k < size of swap file
        lseek(swapfile_fd, k * page_size, SEEK_SET);                     // go to the position of the start of the new page
        read(swapfile_fd, str, this->page_size);                          //read page size chars into str
        for (i = 0; i < page_size; i++) {
            if (str[i] != '0') {
                break;
            }
            if (i == page_size - 1) {
                free(str);
                return k;
            }
        }
    }
    free(str);
    return -1;
}

int sim_mem::isData(int address) {
    if (address >= text_size && address <= text_size + data_size) {         //data area
        return 1;
    }
    return 0; // heap/stack/bss area
}

int sim_mem::isBss(int address) {
    if (address >= text_size + data_size && address <= text_size + data_size + bss_size) {   //bss area
        return 1;
    }
    return 0;  // heap/stack area
}

void sim_mem::initNewPage(int process_id, int pageNum) {
    frame = counterOfPagesInRam % (MEMORY_SIZE / page_size);    //update the frame
    for (int j = 0; j < page_size; j++) {
        main_memory[(frame * page_size) + j] = '0';             //put pageSize of '0' in the RAM
    }
    page_table[process_id][pageNum].frame = frame;    //update
    page_table[process_id][pageNum].V = 1;    //update
    page_table[process_id][pageNum].swap_index = -1;    // update
}

void sim_mem::bringFromSwap(int process_id, int pageNum) {
    frame = counterOfPagesInRam % (MEMORY_SIZE / page_size);            //update the frame
    lseek(swapfile_fd, page_table[process_id][pageNum].swap_index, SEEK_SET);  //put the index in start of swapIndex * pageSize
    read(swapfile_fd, &main_memory[frame * page_size], page_size);//read from the swap file into the RAM
    lseek(swapfile_fd, page_table[process_id][pageNum].swap_index, SEEK_SET); //put the index in start of swapIndex * pageSize
    for (int k = 0; k < page_size; k++) {
        write(swapfile_fd, "0", 1);              //write page size of '0' in the position of lseek till the (position + page size-1)
    }
    page_table[process_id][pageNum].swap_index = -1;             //update
    page_table[process_id][pageNum].V = 1;                      //update
    page_table[process_id][pageNum].frame = frame;              //update
}