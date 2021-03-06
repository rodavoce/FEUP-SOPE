#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FIFO_NAME_SIZE 15
#define OG_RW_PERMISSION 0660
#define FILENAME_PARK "parque.log"

static clock_t startTime = 0;

static int parkingSpace;
static int currentOcupation = 0;
static pthread_mutex_t parker_lock = PTHREAD_MUTEX_INITIALIZER;
char *ORIENTATION_S[CONTROLLERS] = {"N", "S", "E", "O"};
static FILE *fp_park;

void closeEntryControllers() {
  int fd;
  vehicle finish;
  finish.id = 0;
  finish.access = 'S';
  finish.t_parking = 0;

  int i = 0;
  for (; i < CONTROLLERS; i++) {
    if ((fd = open(CONTROLADORES_FIFOS[i], O_WRONLY)) == -1) {
      perror("Vehicle thread Cfifo: ");
    } else {
      write(fd, &finish, sizeof(vehicle));
      close(fd);
    }
  }
}

void write_park(int currOcupation, vehicle *veh, const char *vehicleS) {

  char line[LINE_SIZE];

  sprintf(line, "%8ld ; %4d ; %7d ; %s\n", clock() - startTime,
          currentOcupation, veh->id, vehicleS);

  fprintf(fp_park, "%s", line);
}

void *arrumador(void *arg) {

  // RECEBER CARRO
  vehicle *carro = (vehicle *)arg;
  int enters = 0;
  pthread_detach(pthread_self());
  char vehicleNameFifo[200];
  sprintf(vehicleNameFifo, "/tmp/fifo%d", carro->id);
  int fd_vehicle;
  if ((fd_vehicle = open(vehicleNameFifo, O_WRONLY)) == -1) {
    perror("Can't open vehicle fifo from arrumador");
    free(carro);
    return NULL;
  }

  pthread_mutex_lock(&parker_lock);

  statusVehicle s_vehicle;

  if (currentOcupation < parkingSpace) {
    write_park(currentOcupation, carro, ESTAC);

    currentOcupation++;
    enters = 1;

  } else {

    strcpy(s_vehicle.stat, CHEIO);
    write(fd_vehicle, &s_vehicle, sizeof(s_vehicle));

    write_park(currentOcupation, carro, CHEIO);
  }

  pthread_mutex_unlock(&parker_lock);

  if (enters) {
    strcpy(s_vehicle.stat, ENTRADA);
    write(fd_vehicle, &s_vehicle, sizeof(s_vehicle));

    waitTime(carro->t_parking);

    strcpy(s_vehicle.stat, SAIDA);
    write(fd_vehicle, &s_vehicle, sizeof(s_vehicle));

    write_park(currentOcupation, carro, SAIDA);

    pthread_mutex_lock(&parker_lock);
    currentOcupation--;
    pthread_mutex_unlock(&parker_lock);
  }

  // FREE todos os recursos;
  close(fd_vehicle);
  free(carro);
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

  //  printf("Thread %s \n", (char *)arg);

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
    pthread_t vi;
    int arrive = 0;
    vehicle *nova = (vehicle *)malloc(sizeof(vehicle));

    if ((red = read(desFifo, nova, sizeof(*nova)) > 0)) {

      arrive = 1;
      if (nova->id == 0) {
        break;
      }

      if (pthread_create(&vi, NULL, arrumador, nova) != 0) {
        perror("Error creating arrumador thread: ");
        break;
      }

    } else if (red == -1) {
      perror("Reading Controller error");
      break;
    }
    if (!arrive)
      free(nova);
  }
  unlink(fifoPath);
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

  if ((fp_park = fopen(FILENAME_PARK, "w")) == NULL) {
    fprintf(stderr, "Error opening parque.log.\n");
    exit(2);
  }

  fprintf(fp_park, "t(ticks) ; nlug ; id_viat ; observ\n");

  if ((sem1 = initSem(SEMNAME)) == SEM_FAILED) {
    exit(3);
  }

  pthread_t controllers[CONTROLLERS];

  int i = 0;

  for (; i < CONTROLLERS; i++) {
    if (pthread_create(&controllers[i], NULL, controlador, ORIENTATION_S[i]) !=
        0) {
      perror("Thread Creator: ");
    }
  }

  startTime = clock();

  sleep(worktime);

  sem_wait(sem1);

  closeEntryControllers();

  // End Time
  if (pthread_join(controllers[0], NULL) != 0) {
    perror("threadN : ");
  }
  if (pthread_join(controllers[1], NULL) != 0) {
    perror("threadS : ");
  }
  if (pthread_join(controllers[2], NULL) != 0) {
    perror("threadE : ");
  }
  if (pthread_join(controllers[3], NULL) != 0) {
    perror("threadO : ");
  }

  sem_post(sem1);

  pthread_exit(0);
}
