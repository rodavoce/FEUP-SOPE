#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FIFO_NAME_SIZE 15
#define OG_RW_PERMISSION 0660

static clock_t startTime = 0;
static int endTime = 0;
static int parkingSpace;
static int currentOcupation = 0;
static pthread_mutex_t parker_lock = PTHREAD_MUTEX_INITIALIZER;

void closeEntryControllers() {
  int fd;
  vehicle finish;
  finish.id = 0;
  finish.access = 'S';
  finish.t_parking = 0;

  if ((fd = open("/tmp/fifoN", O_WRONLY)) == -1) {
    perror("Vehicle thread Cfifo: ");
  } else {
    write(fd, &finish, sizeof(vehicle));
    close(fd);
  }

  if ((fd = open("/tmp/fifoE", O_WRONLY)) == -1) {
    perror("Vehicle thread Cfifo: ");
  } else {
    write(fd, &finish, sizeof(vehicle));
    close(fd);
  }

  if ((fd = open("/tmp/fifoS", O_WRONLY)) == -1) {
    perror("Vehicle thread Cfifo: ");
  } else {
    write(fd, &finish, sizeof(vehicle));
    close(fd);
  }

  if ((fd = open("/tmp/fifoO", O_WRONLY)) == -1) {
    perror("Vehicle thread Cfifo: ");
  } else {
    write(fd, &finish, sizeof(vehicle));
    close(fd);
  }
}

void *arrumador(void *arg) {

  // RECEBER CARRO

  int enters = 0;
  pthread_detach(pthread_self());

  pthread_mutex_lock(&parker_lock);
  // ARRUMADOR
  if (currentOcupation < parkingSpace) {
    currentOcupation++;
    enters = 1;
  } else {
    // CARRO NAO ENTRA
  }

  pthread_mutex_unlock(&parker_lock);
  if (enters) {
    // ESPERA POR O TEMPO PASSAR

    // SAIR
    pthread_mutex_lock(&parker_lock);
    currentOcupation--;

    pthread_mutex_unlock(&parker_lock);
  }

  // FREE todos os recursos;
  // fechar fifo veiculo;
  return NULL;
}
/*
criar o seu FIFO próprio (identificado por “fifo?”, onde '?' será ou  ou E,
 ou S ou O);
receber pedidos de acesso através do seu FIFO; cada pedido inclui os seguintes
dados:
-->porta de entrada;
-->tempo de estacionamento (em clock ticks);
-->número identificador único da viatura (inteiro);
-->nome do FIFO privado para a comunicação com o thread“viatura” do programa
Gerador.
 --> criar     um     thread “arrumador”  para  cada  pedido  de  entrada
recebido e  passar-lhe  a  informação  correspondente a esse pedido;
 estar  atento  a  uma  condição  de  terminação  (correspondendo  à  passagem
do T_ABERTURA  do  Parque) e, nessa altura, ler todos os pedidos pendentes no
seu FIFO e fechá-lo para que potenciais novos  clientes  de  estacionamento
sejam notificados  do  encerramento  do  Parque;  encaminhar  os  últimos
pedidos a correspondentes thread“arrumador”.
*/
void *controlador(void *arg) {

  printf("Thread %s \n", (char *)arg);

  char *fifoPath = malloc(sizeof(char) * FIFO_NAME_SIZE);
  sprintf(fifoPath, "/tmp/fifo%c", (*(char *)arg));

  int desFifo, red;

  if (mkfifo(fifoPath, OG_RW_PERMISSION) != 0) {
    perror("FIFO CONTROLER: ");
    return NULL;
  }
  // TODO REVER OPEN DO FIFO
  if ((desFifo = open(fifoPath, O_RDONLY | O_NONBLOCK)) == -1) {
    perror("FIFO OPEN FAILED: ");
    unlink(fifoPath);
    return NULL;
  }

  while (1) {

    vehicle nova;

    if ((red = read(desFifo, &nova, sizeof(nova)) > 0)) {
      printf("LEU \n");
      if (nova.id == 0) {
        printf("END READ CICLE %s \n", fifoPath);
        break;
      }
      printf("CARRO: %d\n Time: %d\n acesso: %c\n path:  \n\n", nova.id,
             nova.t_parking, nova.access);
    } else if (red == -1) {
      perror("Reading Controller error");
      break;
    }
  }
  printf("sai controlador \n");
  unlink(fifoPath); // TODO CHECK UNLINK
  free(fifoPath);
  return NULL;
}
int main(int argc, char const *argv[]) {

  if (argc != 3) {
    printf("Wrong number of arguements.\n Usage: parque <N_lugares> "
           "<T_abertura> \n");
    return 1;
  }

  errno = 0;
  double worktime = strtol(argv[2], NULL, 10);
  if (errno == ERANGE || errno == EINVAL) {
    perror("convert working time failed");
  }

  errno = 0;
  parkingSpace = strtol(argv[1], NULL, 10);
  if (errno == ERANGE || errno == EINVAL) {
    perror("convert parking space failed");
  }
  if ((sem1 = initSem(SEMNAME)) == SEM_FAILED) {
    exit(3);
  }
  // TODO
  //

  pthread_t N, S, E, O;

  // \DUVIDA Ao "matar" os threads exit() vs pthread_cancel TODO
  if (pthread_create(&N, NULL, controlador, "N") != 0) {
    perror("Thread N: ");
    pthread_exit(0);
  }
  if (pthread_create(&S, NULL, controlador, "S") != 0) {
    perror("Thread S: ");
    pthread_cancel(N);
    pthread_exit(0);
  }
  if (pthread_create(&E, NULL, controlador, "E") != 0) {
    perror("Thread E: ");
    pthread_cancel(N);
    pthread_cancel(S);
    pthread_exit(0);
  }
  if (pthread_create(&O, NULL, controlador, "O") != 0) {
    perror("Thread O: ");
    pthread_cancel(N);
    pthread_cancel(S);
    pthread_cancel(E);
    pthread_exit(0);
  }

  startTime = clock();

  sleep(worktime);
  printf("passa sleep \n");
  sem_wait(sem1);

  closeEntryControllers();
  printf("work %d\n", (int)worktime);
  // End Time
  if (pthread_join(N, NULL) != 0) {
    perror("threadN : ");
  }
  if (pthread_join(S, NULL) != 0) {
    perror("threadS : ");
  }
  if (pthread_join(E, NULL) != 0) {
    perror("threadE : ");
  }
  if (pthread_join(O, NULL) != 0) {
    perror("threadO : ");
  }

  sem_post(sem1);
  printf("%d\n", endTime);
  //  printf("%f\n", elapsed);
  printf("%s\n", "End Main");

  closeSem(sem1, SEMNAME);
  exit(0);
}
