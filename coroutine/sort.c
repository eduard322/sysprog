#define _XOPEN_SOURCE /* Mac compatibility. */
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>

/*Initializing the main coroutine and the array of coroutine. Size of the array will be defined in main func*/
static ucontext_t uctx_main, *CONTEXTS;
/*Initializing the array of time spent for each coroutine. Size of the array will be defined in main func*/
double* TIME_TAKEN;
#define handle_error(msg) \
   do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define stack_size 1024 * 1024


/*Max size of numbers from the file. It can be changed*/
#define N 100000


/*Reading file... (w\o async)*/
int read_file(char* name, int* a){
    FILE *file;
    int i = 0;
    file = fopen(name, "r");
    while(fscanf(file, "%d", &a[i]) != EOF){
        i++;
    }
    fclose(file);
    return i;
}

/*Swap func for bubble sorting*/
void swap(int* a, int i, int j){
    if(i != j){
        int K;
        K = a[i];
        a[i] = a[j];
        a[j] = K;
    }
}

/*Bubble sorting itself*/
void bubble(int* a, int size){
    int key;
    do{
        key = 0;
        for(int i = 0; i < size - 1; i++){
            if(a[i] > a[i+1]){ 
                swap(a, i, i + 1);
                key = 1;
            }   
        }
    } while(key);
}


/*Coroutine switcher. It returns time spent on swaping contexts */
clock_t SWAP_C(int id, int ID_MAX, double* time_taken_cor){
    clock_t t;
    t = clock();
    //Searching coroutine that is not finished...
    for(int i = id + 1; ; i++){
        if(i >= ID_MAX + 1) i = 0;
        if(i == id) break;
        if(time_taken_cor[i] == 0.0){
                printf("SWAPCONTEXT %d -> %d \n", id, i);         
                swapcontext(&CONTEXTS[id], &CONTEXTS[i]);
                t = clock() - t;
                break;
        }
    }
    return t;
}


/*A function that every coroutine starts.*/ 
static void full_sort(char* text_name, int id, int ID_MAX, int* A, int* SIZE, char* time_1, int* NUM){
    // Initializing clock_t variables and read some func arguments
    printf("sort %d: started\n", id);
    clock_t t, t_cor; 
    t_cor = clock();
    double t0;
    double time_taken; 
    TIME_TAKEN[id] = 0;
    double TIME = atof(time_1)/1000;
    NUM[id] = 0;
    t = clock(); 


    // Reading file
    printf("READING %s \n", text_name);
    *SIZE = read_file(text_name, A);

    // Counting time spent on reading. If more than target_latency/Num_of_coroutines then swapping the context
    time_taken = ((double)(clock() - t))/CLOCKS_PER_SEC; // in s 
    //printf("Time taken: %f ms; Time max: %f ms \n", time_taken*1000, TIME*1000/(ID_MAX + 1));
    if(time_taken > TIME/(ID_MAX + 1)){
     NUM[id] += 1;
     t0 = SWAP_C(id, ID_MAX, TIME_TAKEN);
     t = clock();
     t_cor -= t0;
    }

    //printf(" **** %f ms ***** \n", ((double)(clock() - t_cor)*1000)/CLOCKS_PER_SEC);
    // Bubble sort
    printf("Bubbling... \n");
    bubble(A, *SIZE);


    // Counting time spent on sorting. If more than target_latency/Num_of_coroutines then swapping the context
    time_taken = ((double)(clock() - t))/CLOCKS_PER_SEC; // in s
    //printf("Time taken: %f ms; Time max: %f ms \n", time_taken*1000, TIME*1000/(ID_MAX + 1));
    if(time_taken > TIME/(ID_MAX + 1)){ 
        NUM[id] += 1;
      t0 = SWAP_C(id, ID_MAX, TIME_TAKEN);
      t = clock();
      t_cor -= t0;
    }
    //printf(" **** %f ms ***** \n", ((double)(clock() - t_cor)*1000)/CLOCKS_PER_SEC);


    printf("sort %d: finished\n", id);
    t_cor = clock() - t_cor; 
    TIME_TAKEN[id] = ((double)t_cor*1000)/CLOCKS_PER_SEC;
    NUM[id] += 1;
    t0 = SWAP_C(id, ID_MAX, TIME_TAKEN);




}



static void *
allocate_stack_sig()
{
	void *stack = malloc(stack_size);
	stack_t ss;
	ss.ss_sp = stack;
	ss.ss_size = stack_size;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);
	return stack;
}


int main(int argc, char * argv[]){

    
    clock_t begin = clock();

    int numofcor = argc - 2; // number of coroutines 
    CONTEXTS = (ucontext_t*)malloc((numofcor) * sizeof(ucontext_t));
    char* func_stacks[numofcor];

    int ARRAY[numofcor][N], SIZE[numofcor], TOTSIZE = 0, NUM[numofcor]; // ARRAY is numbers from the files, SIZE is an array of sizes for each file

    double time_spent = 0;
    TIME_TAKEN = (double*)malloc((numofcor) * sizeof(double));

    for(int i = 1; i < argc - 1; i++){
        func_stacks[i-1] = allocate_stack_sig();
        if (getcontext(&CONTEXTS[i-1]) == -1)
		    handle_error("getcontext");
        CONTEXTS[i-1].uc_stack.ss_sp = func_stacks[i-1];
	    CONTEXTS[i-1].uc_stack.ss_size = stack_size;
    }

    for(int i = 1; i < numofcor; i++){
        CONTEXTS[i-1].uc_link = &uctx_main;
	    makecontext(&CONTEXTS[i-1], full_sort, 7, argv[i+1], i-1, numofcor - 1, ARRAY[i-1], &(SIZE[i-1]), argv[1], NUM);
    }

    CONTEXTS[numofcor-1].uc_link = &uctx_main;
	makecontext(&CONTEXTS[numofcor-1], full_sort, 7, argv[argc-1], numofcor - 1, numofcor - 1, ARRAY[numofcor-1], &(SIZE[numofcor-1]), argv[1], NUM);


    // Here it starts
    printf("main: swapcontext(&uctx_main, &CONTEXTS[0])\n");
	if (swapcontext(&uctx_main, &CONTEXTS[0]) == -1)
		handle_error("swapcontext");





    // Calculating the total size of merged array
    for(int i = 0; i < numofcor; i++)
        TOTSIZE += SIZE[i];

    // Filling the total array
    int TOTARRAY[TOTSIZE], j = 0, k;
    for(int i = 0; i < numofcor; i++){
        for(k = 0; k < SIZE[i]; k++){
            TOTARRAY[j] = ARRAY[i][k];
            j++;
        }
    }

    // Sorting it
    bubble(TOTARRAY, TOTSIZE);


    printf("OUTPUT FOR TASK:\n");
    printf("\nEach coroutine spent: \n");
    for(int i = 0; i < numofcor; i++){
        printf("cor_%d: time %f ms and %d switchings \n", i, TIME_TAKEN[i], NUM[i]);
        //time_spent += TIME_TAKEN[i];
    }

    clock_t end = clock();

    time_spent += (double)(end - begin) / CLOCKS_PER_SEC;

    printf("\n Total time spent: %f ms \n", time_spent*1000);

    return 0;








}
