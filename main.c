#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define SHM_SIZE 104 

/* Union semun */
union semun {
    int val;                  /* value for SETVAL */
    struct semid_ds *buf;     /* buffer for IPC_STAT, IPC_SET */
    unsigned short *array;    /* array for GETALL, SETALL */
};

void free_resources(int shm_id, int sem_write, int sem_read, int sem_wait) { 
    /* Delete the shared memory segment */
    shmctl(shm_id,IPC_RMID,NULL);
    /* Delete the semaphores */
    semctl(sem_write, 0, IPC_RMID, 0);
    semctl(sem_read, 0, IPC_RMID, 0);
    semctl(sem_wait, 0, IPC_RMID, 0);
}

/* Semaphore P - down operation, using semop */
int sem_P(int sem_id) {
    struct sembuf sem_d;
  
    sem_d.sem_num = 0;
    sem_d.sem_op = -1;
    sem_d.sem_flg = 0;
    if (semop(sem_id, &sem_d, 1) == -1) {
        perror("# Semaphore down (P) operation ");
        return -1;
    }
    return 0;
}

/* Semaphore V - up operation, using semop */
int sem_V(int sem_id) {
    struct sembuf sem_d;

    sem_d.sem_num = 0;
    sem_d.sem_op = 1;
    sem_d.sem_flg = 0;
    if (semop(sem_id, &sem_d, 1) == -1) {
        perror("# Semaphore up (V) operation ");
        return -1;
    }
    return 0;
}

/* Semaphore Init - set a semaphore's value to val */
int sem_Init(int sem_id, int val) {
    union semun arg;

    arg.val = val;
    if (semctl(sem_id, 0, SETVAL, arg) == -1) {
        perror("# Semaphore setting value ");
        return -1;
    }
    return 0;
}


typedef struct shared_memory {
    char line[100];
    int lineid;
}shm;

int main (int argc, char *argv[]) { 
    
    int K = atoi(argv[1]);                  //child processes
    int N = atoi(argv[2]);                  //child loops

    //open file and make the strArray 
    FILE *fp = fopen(argv[3], "r");
    char linelength[100];
    int num_of_lines = 0;

    while (fgets(linelength, 100, fp) != NULL) {
        num_of_lines++; 
    }

    fclose(fp);
    fp = fopen(argv[3], "r");

    char strArray[num_of_lines][100];
    
    int i = 0;
    while (fgets(linelength, 100, fp) != NULL) {
        strcpy(strArray[i], strtok(linelength, "\n"));
        i++; 
    }

    fclose(fp);

    //shared memory and semaphores
    int shm_id;
    int sem_write;
    int sem_read;
    int sem_wait;
    shm *sh;

    /* Create a new shared memory segment */
    shm_id = shmget(IPC_PRIVATE, sizeof(shm), IPC_CREAT | 0660);
    if (shm_id == -1) {
        perror("Shared memory creation");
        exit(EXIT_FAILURE);
    }

    /* Create a new semaphore id */
    sem_write = semget(IPC_PRIVATE, 1, IPC_CREAT | 0660);
    if (sem_write == -1) {
        perror("Semaphore creation ");
        shmctl(shm_id, IPC_RMID, (struct shmid_ds *) NULL);
        exit(EXIT_FAILURE);
    }

    sem_read = semget(IPC_PRIVATE, 1, IPC_CREAT | 0660);
    if (sem_read == -1) {
        perror("Semaphore creation ");
        shmctl(shm_id, IPC_RMID, (struct shmid_ds *) NULL);
        exit(EXIT_FAILURE);
    }

    sem_wait = semget(IPC_PRIVATE, 1, IPC_CREAT | 0660);
    if (sem_wait == -1) {
        perror("Semaphore creation ");
        shmctl(shm_id, IPC_RMID, (struct shmid_ds *) NULL);
        exit(EXIT_FAILURE);
    }

    /* Set the value of the write semaphore to 0 */
    if (sem_Init(sem_write, 0) == -1) { 
        free_resources(shm_id, sem_write,sem_read,sem_wait);
        exit(EXIT_FAILURE);
    }

    /* Set the value of the write semaphore to 1 */
    if (sem_Init(sem_read, 1) == -1) { 
        free_resources(shm_id, sem_write, sem_read,sem_wait);
        exit(EXIT_FAILURE);
    }

    /* Set the value of the write semaphore to 0 */
    if (sem_Init(sem_wait, 0) == -1) { 
        free_resources(shm_id, sem_write,sem_read,sem_wait);
        exit(EXIT_FAILURE);
    }

    /* Attach the shared memory segment */
    sh = (struct shared_memory *) shmat(shm_id, NULL, 0);
    if (sh == NULL) {
        perror("Shared memory attach ");
        free_resources(shm_id, sem_write, sem_read,sem_wait);
        exit(EXIT_FAILURE);
    }


    /* New processes */
    int pid;

    for (int i = 0; i < K; i++) {
        if ((pid = fork()) == -1) { 
            perror("fork");
            free_resources(shm_id, sem_write, sem_read,sem_wait);
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { 
            /* Child process */
            printf("# I am the child process with process id: %d\n", getpid());
            
            srand(getpid());

            double sum = 0.0;
            clock_t start, end, total;
            
            for (int j = 0; j < N; j++) {
                start = clock();

                sem_P(sem_read);    //semread--
                sh->lineid = rand() % num_of_lines;
                printf("Child %d requesting %d line.\n", getpid(), sh->lineid + 1);
                sem_V(sem_write);   //semwrite++
            
                sem_P(sem_wait);    //semwait--
                printf("%s\n", sh->line);

                end = clock();
                total = (double)(end - start) / CLOCKS_PER_SEC;
                sum += total;
            }
            sum = sum/N;

            printf("child id %d exiting with avarage process time = %f\n", getpid(), sum);
            exit(EXIT_SUCCESS);
        } 
    }

    if (pid != 0) {
        /* Parent process */
        printf("# I am the parent process with process id: %d\n", getppid());
        
        for (int j = 0; j < K*N; j++) {
            sem_P(sem_write);   //semwrite--
            strncpy(sh->line, strArray[sh->lineid], SHM_SIZE);
            sem_V(sem_wait);    //semwait++
            sem_V(sem_read);    //semread++
        }
    }

    /* Wait for childs process */
    for(int i = 0; i < K; i++){
        wait(NULL);
    }

    /* Clear recourses */
    free_resources(shm_id, sem_write, sem_read,sem_wait);

    return 0;
}