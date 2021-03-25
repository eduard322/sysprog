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


void write_file(int* a, int size){
    FILE *file;
    file = fopen("Sorted_array.txt", "w");
    for(int i = 0; i < size; i++){
        fprintf(file, "%d ", a[i]);
    }
}


/*Swap func for quicksort sorting*/

void swap(int* a, int i, int j){
    if(i != j){
        int K;
        K = a[i];
        a[i] = a[j];
        a[j] = K;
    }
}


void quicksort(int* a,int first,int size){
   int i, j, pivot, temp;

   if(first<size){
      pivot=first;
      i=first;
      j=size;

      while(i<j){
         while(a[i]<=a[pivot]&&i<size)
            i++;
         while(a[j]>a[pivot])
            j--;
         if(i<j) swap(a, i, j);
      }

      temp=a[pivot];
      a[pivot]=a[j];
      a[j]=temp;
      quicksort(a,first,j-1);
      quicksort(a,j+1,size);

   }
}

/*Coroutine switcher. It returns time spent on swaping contexts */
/*
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
*/

/*Coroutine switcher. It returns time spent on swaping contexts */
void SWAP_C(int id, int ID_MAX, double* time_taken_cor){
    //Searching coroutine that is not finished...
    //printf("CHECK");   
    for(int i = id + 1; i <= 2*ID_MAX; i++){
        if(i >= ID_MAX + 1) i = 0;
        if(i == id) break;
        if(time_taken_cor[i] <= 0.0){
                printf("SWAPCONTEXT %d -> %d \n", id, i);         
                swapcontext(&CONTEXTS[id], &CONTEXTS[i]);
                break;
        }
    }
}

/*A function that every coroutine starts.*/ 
static void full_sort(char* text_name, int id, int ID_MAX, int* A, int* SIZE, char* time_1, int* NUM){
    // Initializing clock_t variables and read some func arguments
    printf("sort %d: started\n", id);
    double t, t_cor; 
    t_cor = clock();
    double t0, T = 0;
    double time_taken; 
    TIME_TAKEN[id] = 0.0;
    double TIME = atof(time_1)/1000;
    NUM[id] = 0;
    t = clock(); 


    // Reading file
    printf("READING %s \n", text_name);
    *SIZE = read_file(text_name, A);

    // Counting time spent on reading. If more than target_latency/Num_of_coroutines then swapping the context
    time_taken = ((double)(clock() - t))/CLOCKS_PER_SEC; // in s 
    printf("Time taken: %f ms; Time max: %f ms \n", time_taken*1000, TIME*1000/(ID_MAX + 1));
    if(time_taken > TIME/(ID_MAX + 1)){
      printf("CHECK");   
     NUM[id] += 1;
     //t0 = SWAP_C(id, ID_MAX, TIME_TAKEN);
     t0 = clock();
     SWAP_C(id, ID_MAX, TIME_TAKEN);
     t = clock();
     T += clock() - t0;
     //printf("%d : %f\n", id, t0);
     //t_cor -= t0;
    }

    //printf(" **** %f ms ***** \n", ((double)(clock() - t_cor)*1000)/CLOCKS_PER_SEC);
    // quicksort sort
    printf("Quick sorting... \n");
    quicksort(A, 0, *SIZE - 1);


    // Counting time spent on sorting. If more than target_latency/Num_of_coroutines then swapping the context
    time_taken = ((double)(clock() - t))/CLOCKS_PER_SEC; // in s
    printf("Time taken: %f ms; Time max: %f ms \n", time_taken*1000, TIME*1000/(ID_MAX + 1));
    if(time_taken > TIME/(ID_MAX + 1)){ 
        NUM[id] += 1;
      //t0 = SWAP_C(id, ID_MAX, TIME_TAKEN);
     t0 = clock();
    SWAP_C(id, ID_MAX, TIME_TAKEN);
     t = clock();
     T += clock() - t0;
     //printf("%d : %f\n", id, t0);
     //t_cor -= t0;
    }
    //printf(" **** %f ms ***** \n", ((double)(clock() - t_cor)*1000)/CLOCKS_PER_SEC);


    printf("sort %d: finished\n", id);
    t_cor = clock() - t_cor - T; 
    TIME_TAKEN[id] = ((double)t_cor*1000)/CLOCKS_PER_SEC;
    NUM[id] += 1;
    SWAP_C(id, ID_MAX, TIME_TAKEN);




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

    int numofcor = argc - 2; // a of coroutines 
    CONTEXTS = (ucontext_t*)malloc((numofcor) * sizeof(ucontext_t));
    char* func_stacks[numofcor];

    int ARRAY[numofcor][N], SIZE[numofcor], TOTSIZE = 0, NUM[numofcor]; // ARRAY is numbers from the files, SIZE is an array of sizes for each file

    double time_spent = 0 , t;
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
	makecontext(&CONTEXTS[numofcor-1], full_sort, 7, argv[numofcor+1], numofcor - 1, numofcor - 1, ARRAY[numofcor-1], &(SIZE[numofcor-1]), argv[1], NUM);


    // Here it starts
    printf("main: swapcontext(&uctx_main, &CONTEXTS[0])\n");
	if (swapcontext(&uctx_main, &CONTEXTS[0]) == -1)
		handle_error("swapcontext");





    // Calculating the total size of merged array
    for(int i = 0; i < numofcor; i++)
        TOTSIZE += SIZE[i];
/*
    // Filling the total array
    int TOTARRAY[TOTSIZE], j = 0, k;
    for(int i = 0; i < numofcor; i++){
        for(k = 0; k < SIZE[i]; k++){
            TOTARRAY[j] = ARRAY[i][k];
            j++;
        }
    }

    // Sorting it
    quicksort(TOTARRAY, 0, TOTSIZE - 1);
*/
/*
    for(int i = 0; i < TOTSIZE; i++){
        printf("%d ", TOTARRAY[i]);
    }
*/
 //   write_file(TOTARRAY, TOTSIZE);

    printf("\nOUTPUT FOR TASK:\n");
    printf("\nEach coroutine spent: \n");
    for(int i = 0; i < numofcor; i++){
        printf("cor_%d: time %f ms and %d switchings \n", i, TIME_TAKEN[i], NUM[i]);
        time_spent += TIME_TAKEN[i];
    }

    clock_t end = clock();

    t = (double)(end - begin) / CLOCKS_PER_SEC;
    time_spent += t*1000;
    printf("Main func worked for %f ms", t*1000);

    printf("\n Total time spent: %f ms \n", time_spent);

    return 0;








}
