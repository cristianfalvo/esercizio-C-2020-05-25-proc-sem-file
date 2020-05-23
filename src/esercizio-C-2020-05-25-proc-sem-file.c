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

void soluzione_A();
void soluzione_B();
void child_process_A(int i);
void child_process_B(int i);

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

	printf("Soluzione A:\n");
	soluzione_A();

	printf("Soluzione B:\n");
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
	int fd = open(file_name_A, O_CREAT | O_RDWR, S_IRWXU);
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

	for(int i = 0; i < N; i++){
		s = wait(NULL);
		CHECK_ERR(s, "wait");
	}
	return;
}

void child_process_A(int i){
	int s;
	char * buffer = malloc(sizeof(char));
	int found = 1;
	char own_char = 'A' + i;

	int fd = open(file_name_A, O_RDWR);
	while(found){
		//tutti i processi aspettano il "verde" dal padre su proc_sem; non faranno sem_post(proc_sam) alla fine
		s = sem_wait(proc_sem);
		CHECK_ERR(s, "sem_wait");

		//La scrittura su file è concorrente e quindi va gestita opportunamente (ad es. con un mutex).
		s = sem_wait(mutex);
		CHECK_ERR(s, "sem_wait");

		//qui siamo in area critica, posso lavorare sulle variabili condivise

		//mi metto all'inizio del file
		s = lseek(fd, 0, SEEK_SET);
		CHECK_ERR(s, "lseek");


		while((found = read(fd, buffer, sizeof(char *))) > 0 && *buffer != 0);
		//if(*buffer == 0){
		//o abbiamo trovato uno zero, o siamo a EOF
		if(found > 0){
			//lseek(fd, pos, SEEK_SET);
			//trovato uno zero, scrivo
			write(fd, &own_char, sizeof(char));
		} else if (found == 0){
			//siamo a EOF, il ciclo termina comunque
			printf("Trovato EOF, dentro al while\n");
		}

		s = sem_post(mutex);
		CHECK_ERR(s, "sem_post");
	}


	//siamo a EOF
	printf("Siamo arrivati a EOF!\nIl processo figlio %d termina.\n", i);
	exit(EXIT_SUCCESS);
}

void soluzione_B(){
	return;
}


void child_process_B(int i){
	return;
}
