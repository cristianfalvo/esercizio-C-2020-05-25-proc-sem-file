/*
 * esercizio-C-2020-05-25-proc-sem-file.c
 *
 *  Created on: 23 mag 2020
 *      Author: utente
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <semaphore.h>


#define FILE_SIZE (1024*1024)
#define N 4
#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
//#define DEBUG_A
#define DEBUG_B

void soluzione_A();
void soluzione_B();
void child_process_A(int i);

sem_t * proc_sem;
sem_t * mutex;
char * file_name_A = "output_A.txt";
char * file_name_B = "output_B.txt";


int main(int argc, char ** argv){

	int s;

	//creo la memoria condivisa per i semafori
	proc_sem = mmap(NULL,
		2*sizeof(sem_t),
		PROT_READ | PROT_WRITE, //accesso alla memory map
		MAP_SHARED | MAP_ANONYMOUS, //tipologia
		-1, //non si appoggia a file
		0); //offset
	if (proc_sem == MAP_FAILED){
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	mutex = &proc_sem[1];

	s = sem_init(proc_sem, 1, 0);
	CHECK_ERR(s, "sem_init");

	s = sem_init(mutex, 1, 1);
	CHECK_ERR(s, "sem_init");

    printf("ora avvio la soluzione_A()...\n");
    soluzione_A();

    printf("ed ora avvio la soluzione_B()...\n");
    soluzione_B();

	s = sem_destroy(proc_sem);
	CHECK_ERR(s, "sem_destroy")

	s = sem_destroy(mutex);
	CHECK_ERR(s, "sem_destroy")

    printf("bye!\n");
    return 0;
}

void soluzione_A(){
	//usare le system call open(), lseek(), write()
	int s;

	//creo il file "output_a.txt" con dimensione data
	int fd = open(file_name_A, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
	CHECK_ERR(fd, "open");
	s = ftruncate(fd, FILE_SIZE);
	CHECK_ERR(s, "ftruncate");
	s = close(fd);
	CHECK_ERR(s, "close");

	for (int i = 0; i < N; i++){
		switch(fork()){
			case -1:
				CHECK_ERR(-1, "fork");
				break;
			case 0:
				//child
#ifdef DEBUG_A
				printf("child %d\n", i);
#endif
				child_process_A(i);
				break;
			default:
				;
		}
	}

	for(int i = 0; i < FILE_SIZE + N; i++){
		s = sem_post(proc_sem);
		CHECK_ERR(s, "sem_post");
	}

	for (int i =0; i<N; i++){
		wait(NULL);
	}
	return;
}

void child_process_A(int i){
	int s;
	char * buffer = malloc(sizeof(char));
	int found = 1;
	char own_char = 'A' + i;
	int cycle_counter = 0;
	int write_counter = 0;
#ifdef DEBUG_A
	int current_pos = 0;
#endif

	int fd = open(file_name_A, O_RDWR);

//	//mi metto all'inizio del file
//	s = lseek(fd, 0, SEEK_SET);
//	CHECK_ERR(s, "lseek");

	while(found){
#ifdef DEBUG_A
		current_pos = lseek(fd, 0, SEEK_CUR);
#endif
		//tutti i processi aspettano il "verde" dal padre su proc_sem; non faranno sem_post(proc_sam) alla fine
		s = sem_wait(proc_sem);
		CHECK_ERR(s, "sem_wait");

		//La scrittura su file è concorrente e quindi va gestita opportunamente (ad es. con un mutex).
		s = sem_wait(mutex);
		CHECK_ERR(s, "sem_wait");

		//qui siamo in area critica, posso lavorare sulle variabili condivise
#ifdef DEBUG_A
		printf("[child %d]cycle_counter = %d, current_pos=%d\n", i, cycle_counter, current_pos);
#endif

		while((found = read(fd, buffer, sizeof(char))) > 0 && *buffer != 0);
		//if(*buffer == 0){
		//o abbiamo trovato uno zero, o siamo a EOF
		if(found > 0){
			//lseek(fd, pos, SEEK_SET);
			//trovato uno zero, torno indietro di uno e scrivo
			s = lseek(fd, -1, SEEK_CUR);
			CHECK_ERR(s, "lseek");
			s = write(fd, &own_char, sizeof(char));
			CHECK_ERR(s, "write");
			write_counter++;
		} else if (found <= 0){
			//siamo a EOF, il ciclo termina comunque
#ifdef DEBUG
			printf("Trovato EOF, dentro al while\n");
#endif
		}

		s = sem_post(mutex);
		CHECK_ERR(s, "sem_post");
		cycle_counter++;
	}

	s = close(fd);
	CHECK_ERR(s, "close");

	//siamo a EOF
#ifdef DEBUG
	printf("Siamo arrivati a EOF!\nIl processo figlio %d termina.\n", i);
#endif
	exit(EXIT_SUCCESS);
}

void soluzione_B(){
	//usare le system call open(), mmap()
	//evidentemente serve mappare il file in una memory map

	int s;
	char * file_mmap;

	//creo il file "output_b.txt" con dimensione data
	int fd = open(file_name_B, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU);
	CHECK_ERR(fd, "open");
	s = ftruncate(fd, FILE_SIZE);
	CHECK_ERR(s, "ftruncate");

//	s = close(fd);
//	CHECK_ERR(s, "close");

	//creo una memory map condivisa per salvare l'intero file; per questo il fd non va chiuso

	file_mmap = mmap(NULL,
			FILE_SIZE,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			0);

	if (file_mmap == MAP_FAILED){
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	char * pointer = file_mmap;
	//creo i processi
	for (int i =0; i<N; i++){
		switch(fork()){
			case -1:
				CHECK_ERR(-1, "fork");
				break;

			case 0:
				//child
				while(1){
					//tutti i processi aspettano il "verde" dal padre su proc_sem; non faranno sem_post(proc_sam) alla fine
					s = sem_wait(proc_sem);
					CHECK_ERR(s, "sem_wait");

					//La scrittura su file è concorrente e quindi va gestita opportunamente (ad es. con un mutex).
					s = sem_wait(mutex);
					CHECK_ERR(s, "sem_wait");

					//zona critica, faccio quello che devo fare
					while(pointer[0] != 0 && (pointer-file_mmap) < FILE_SIZE) pointer++;
					if((pointer-file_mmap) >= FILE_SIZE){
						s = sem_post(mutex);
						CHECK_ERR(s, "sem_post");
						printf("[child %d]EOF, termino\n", i);
						break;
					} else if (pointer[0] == 0){
						pointer[0] = 'A' + i;
					}

					s = sem_post(mutex);
					CHECK_ERR(s, "sem_post");
				}
				printf("[child %d]Finito, termino\n", i);
				exit(EXIT_SUCCESS);
				break;

			default:
				;
		}
	}

	for(int i = 0; i < FILE_SIZE + N; i++){
		s = sem_post(proc_sem);
		CHECK_ERR(s, "sem_post");
	}

	for (int i =0; i<N; i++){
		wait(NULL);
	}
	return;
}
