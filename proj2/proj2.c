#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

// macro for printing msg to stderr, clearing resources and exiting with 1
#define ERROR(msg)						\
{										\
	fprintf(stderr, "%s\n", msg);	\
	clearResources();				\
	return 1;						\
}

// macro for incrementing a semaphore and shared var by n
#define SIGNAL(sem, n)					\
{										\
	for (int i = 0; i < n; i++){	\
		sem_post(sem);				\
		sharedVar->judgefinish++;	\
	}								\
}

//struct for variables used as arguments
typedef struct
{
	int immiCount_PI;   //PI>=1
	int immi_IG;    //0<=IG<=2000
	int judge_JG;   //0<=JG<=2000
	int cert_IT;    //0<=IT<=2000
	int judgment_JT;    //0<=JT<=2000
} Arguments;

//init
Arguments arguments;

// struct for shared variables needed between processes
typedef struct
{
	int allCntr;
	int immCntr;
	int enteredCntr;
	int checkedCntr;
	int inBuildingCntr;
	int judge;
	int judgefinish;
} sharedVars;

//init
sharedVars *sharedVar;

//size of semaphore
#define SEM_SIZE sizeof(sem_t)

//file to write
FILE *fp;

//used semaphores

//for writing to a file or editing shared variables
sem_t *fileSem;
//controls entering to a building (depends on if judge is inside)
sem_t *nojudge;
//controls checking of immigrants
sem_t *mutex;
//signals if everyone is checked
sem_t *allsigned;
//signals if judge has made the confirmation
sem_t *confirmed;


//help function to clear all resources (sems, memory and file descriptor)
void clearResources()
{
	sem_destroy(fileSem);
	munmap(fileSem, SEM_SIZE);

	sem_destroy(nojudge);
	munmap(nojudge, SEM_SIZE);

	sem_destroy(mutex);
	munmap(mutex, SEM_SIZE);

	sem_destroy(allsigned);
	munmap(allsigned, SEM_SIZE);

	sem_destroy(confirmed);
	munmap(confirmed, SEM_SIZE);

	munmap(sharedVar, sizeof(sharedVars));

	fclose(fp);
}

/*help function for processing args
* takes input args and count and args structure
* returns 1 if ERROR
* stores input args in shared variables
*/
int processArguments(int argc, char *argv[], Arguments *arguments)
{
    //too many or not enough arguments error
	if (argc != 6){
		return 1;
	}
	char *end;

	arguments->immiCount_PI = strtoul(argv[1], &end, 10);
	if (*end){
		return 1;
	}
	if (arguments->immiCount_PI < 1){
		return 1;
	}

	arguments->immi_IG = strtoul(argv[2], &end, 10);
	if (*end){
		return 1;
	}
	if (arguments->immi_IG < 0 || arguments->immi_IG > 2000){
		return 1;
	}

	arguments->judge_JG = strtoul(argv[3], &end, 10);
	if (*end){
		return 1;
	}
	if (arguments->judge_JG < 0 || arguments->judge_JG > 2000){
		return 1;
	}

	arguments->cert_IT = strtoul(argv[4], &end, 10);
	if (*end){
		return 1;
	}
	if (arguments->cert_IT < 0 || arguments->cert_IT > 2000){
		return 1;
	}

	arguments->judgment_JT = strtoul(argv[5], &end, 10);
	if (*end){
		return 1;
	}
	if (arguments->judgment_JT < 0 || arguments->judgment_JT > 2000){
		return 1;
	}

	return 0;
}

/* main logic function
* processes from generators enter and continue by their name
* takes proc. name and shared action counters
*/
void jurisdiction_process(char *name, int *ac, int *ic, int *ne, int *nc, int *nb)
{
	int random; //variable for random sleep
	//if process is JUDGE
	if(strcmp(name,"JUDGE")==0){

		sem_wait(nojudge);	//no one will enter/leave till JUDGE leaves
		sem_wait(mutex);	//mutex is protecting the enter and also checking of IMM
		sem_wait(fileSem);
		sharedVar->allCntr++;
		sharedVar->inBuildingCntr++;
		sharedVar->judge=1;
		fprintf(fp,"%d: JUDGE : enters: %d: %d: %d\n",*ac,*ne,*nc,*nb);
		sem_post(fileSem);

		//JUDGE has to wait for everybody to be checked
		if(ne>nc){
			
			sem_wait(fileSem);
			sharedVar->allCntr++;
			fprintf(fp,"%d: JUDGE : waits for imm: %d: %d: %d\n",*ac,*ne,*nc,*nb);
			sem_post(fileSem);
			sem_post(mutex);
			//IMMs will signal him that they are all checked and good to get a confirmation
			sem_wait(allsigned);
			
		}

		//if everybody is checked then JUDGE proceeds
		sem_wait(fileSem);
   		sharedVar->allCntr++;
    	fprintf(fp,"%d: JUDGE: starts confirmation: %d: %d: %d\n", *ac,*ne,*nc,*nb);
    	sem_post(fileSem);

    	//confirmation lasts for JT
    	if(arguments.judgment_JT>0){
        	random = rand() % arguments.judgment_JT;
    		usleep(random * 1000);
    	}

       	//JUDGE will announce finish of the confirmation
       	sem_wait(fileSem);
   		sharedVar->allCntr++;
   		sharedVar->enteredCntr=0;
   		SIGNAL(confirmed,sharedVar->checkedCntr);  //he will signal that everyone is confirmed (for the amount of entered IMMs)
		sharedVar->checkedCntr=0;
    	fprintf(fp,"%d: JUDGE: ends confirmation: %d: %d: %d\n", *ac,*ne,*nc,*nb);
    	sem_post(fileSem);

		if(arguments.judgment_JT>0){
        	random = rand() % arguments.judgment_JT;
    		usleep(random * 1000);
    	}
		
		sem_wait(fileSem); //and he will leave
		sharedVar->allCntr++;
		sharedVar->inBuildingCntr--;
		sharedVar->judge=0;  //set judge to 0
		fprintf(fp,"%d: JUDGE: leaves: %d: %d: %d\n",*ac,*ne,*nc,*nb);
		sem_post(fileSem);
		
		sem_post(mutex);  //and allow IMMs to be free about checking
		sem_post(nojudge);  //and entering/leaving

		//if JUDGE succesfully confirmed every IMM, he can finish (var is incremented by SIGNAL)
		if(arguments.immiCount_PI==sharedVar->judgefinish){

			sem_wait(fileSem);
			sharedVar->allCntr++;
			fprintf(fp,"%d: JUDGE: finishes\n", *ac);
			sem_post(fileSem);
			exit(0);
		}
		else{  //but if somebody didn't make it, he has to enter again	

			if(arguments.judge_JG>0){
        		random = rand() % arguments.judge_JG;
    			usleep(random * 1000);
    		}

			sem_wait(fileSem);
			sharedVar->allCntr++;
			fprintf(fp,"%d: JUDGE: wants to enter\n", *ac);
            sem_post(fileSem);
			jurisdiction_process("JUDGE", ac, ic, ne, nc, nb);
		}
	}

	//if process is IMM
	if(strcmp(name,"IMM")==0)
	{	
		
    	sem_wait(nojudge);  //IMM has to be sure that JUDGE is not inside (and also they should enter one at a time)

    	sem_wait(fileSem);
    	sharedVar->allCntr++;
    	sharedVar->enteredCntr++;
    	sharedVar->inBuildingCntr++;
    	fprintf(fp,"%d: IMM %d: enters: %d: %d: %d\n",*ac,*ic,*ne,*nc,*nb);
    	sem_post(fileSem);
		
    	sem_post(nojudge); //he will signal next IMM, that he is free to enter
    	sem_wait(mutex); //also they should check one at a time

    	sem_wait(fileSem);
    	sharedVar->allCntr++;
    	sharedVar->checkedCntr++;
    	fprintf(fp,"%d: IMM %d: checks: %d: %d: %d\n",*ac,*ic,*ne,*nc,*nb);
		sem_post(fileSem);
   	
   		//if everyone has checked and JUDGE is inside, they will ask him to start confirmation
    	if(sharedVar->judge==1 && ne==nc){	

    		sem_post(allsigned);
   		}
    	else{	//else they just continue in checking

    		sem_post(mutex);
    	}
    	
    	sem_wait(confirmed); //when he gets confirmation, he is free to get a certificate
    	
    	sem_wait(fileSem);
    	sharedVar->allCntr++;
    	fprintf(fp,"%d: IMM %d: wants certificate: %d: %d: %d\n",*ac,*ic,*ne,*nc,*nb);
    	sem_post(fileSem);

    	//but it takes some time to get one
    	if(arguments.cert_IT>0){
        	random = rand() % arguments.cert_IT;
    		usleep(random * 1000);
    	}
    	
    	sem_wait(fileSem);
		sharedVar->allCntr++;
    	fprintf(fp,"%d: IMM %d: got certificate: %d: %d: %d\n",*ac,*ic,*ne,*nc,*nb);
    	sem_post(fileSem);
	
    	sem_wait(nojudge); //only waiting for JUDGE to leave remains

    	sem_wait(fileSem);
    	sharedVar->allCntr++;
    	sharedVar->inBuildingCntr--;
    	fprintf(fp,"%d: IMM %d: leaves: %d: %d: %d\n",*ac,*ic,*ne,*nc,*nb); //and he can leave now
    	sem_post(fileSem);

    	sem_post(nojudge);
    }
   
}

/* generates 1 judge process
* enters main logic after success
*/
int generateJudge(int interval)
{
    pid_t pid;

    //sleep only if interval is greater than 0
    if(interval>0){
    	int random = rand() % interval;
    	usleep(random * 1000);
    }

    //if somethin bad happens, then return 1
    if ((pid = fork()) < 0){
        return 1;
	}

	//if alles gut, then continue to logic process
    if (pid == 0){
        	
		sem_wait(fileSem);	//wait for write
		sharedVar->allCntr++;
		fprintf(fp,"%d: JUDGE: wants to enter\n", sharedVar->allCntr);	//do write
		sem_post(fileSem);	//continue

		jurisdiction_process("JUDGE", &sharedVar->allCntr, &sharedVar->immCntr, &sharedVar->enteredCntr, &sharedVar->checkedCntr, &sharedVar->inBuildingCntr);	//entering logic function

		clearResources();

		exit(0);
	}
	
	return 0;
}

/* generates a count of immigrant processes
* enters main logic if successful
* waits for all processes to end and clears resources
*/
int generate(int interval)
{
    pid_t pid;
    // generate p processes
    for (int i = 1; i <= arguments.immiCount_PI; i++)
    {
        

        if ((pid = fork()) < 0){
            return 1;
		}

        // code for generator child
        if (pid == 0){
        	if (interval != 0)
        	{
            // sleep process for miliseconds in range specified by an argument (H/S) - generate hacker/serf every n miliseconds
            	if(interval>0){
        		int random = rand() % interval;
    			usleep(random * 1000);
    			}
        	}
            sem_wait(fileSem);	//wait for write
            sharedVar->allCntr++;
            sharedVar->immCntr++;
            fprintf(fp,"%d: IMM %d: starts\n",sharedVar->allCntr,i);	//do it
            sem_post(fileSem);	//go

            jurisdiction_process("IMM", &sharedVar->allCntr, &i, &sharedVar->enteredCntr, &sharedVar->checkedCntr, &sharedVar->inBuildingCntr);	//entering logic function

            clearResources();

            exit(0);
        }
    }

    // wait for every child process of the generator to exit
    while (wait(NULL) > 0){
        ;
	}

    clearResources();

    exit(0);

    return 0;
}




/* MAIN
* all functions are meant to return 1, if there is an error, so their returns are tested and ERROR macro is called when return=1
* clearResources is called many times (because of problems during testing - maybe it is not needed)
*/
int main(int argc, char *argv[])
{	

	//open the output file at first
    fp = fopen("proj2.out", "w");
    setbuf(fp, NULL);

    //process the arguments 
    int returnCode = processArguments(argc, argv, &arguments);
    if (returnCode != 0){
        ERROR("Error processing arguments...");
    }

    // init of shared memory for shared variables
    sharedVar = (sharedVars *)mmap(NULL, sizeof(sharedVars), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sharedVar->allCntr = 0;
    sharedVar->immCntr = 0;
    sharedVar->enteredCntr = 0;
    sharedVar->checkedCntr = 0;
    sharedVar->inBuildingCntr = 0;
    sharedVar->judge = 0;
    sharedVar->judgefinish = 0;

    //map the semaphores (some have initial value of 1)
    fileSem = mmap(NULL, SEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(fileSem, 1, 1);

    nojudge = mmap(NULL, SEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(nojudge, 1, 1);

    mutex = mmap(NULL, SEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(mutex, 1, 1);

    allsigned = mmap(NULL, SEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(allsigned, 1, 0);

    confirmed = mmap(NULL, SEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(confirmed, 1, 0);

    srand(time(NULL));

    pid_t immGenerator;

    // create helper process for immigrants
    if ((immGenerator = fork()) < 0){
        ERROR("Error creating generator for imm processes...")
	}

	//only if its value is 0
    if (immGenerator == 0){
        // generate immigrants
        returnCode = generate(arguments.immi_IG);
        if (returnCode != 0){
            ERROR("Error generating imm processes...");
		}
    }

    //enter function for generating JUDGE
    //fork can be right there, but i moved it to a separate function
    if (immGenerator != 0){
        returnCode = generateJudge(arguments.judge_JG);
        if (returnCode != 0){
            ERROR("Error generating judge process...");
		}
    }

    // wait for both generators to exit
    while (wait(NULL) > 0){
        ;
	}

	//clear everything
    clearResources();

    exit(0);

    clearResources();

    //success returns 0, error 1
    return 0;
}
