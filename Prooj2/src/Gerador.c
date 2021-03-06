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
#include <time.h>
#include <unistd.h>

#define FIFO_NAME_BUFF 30
#define OG_RW_PERMISSION 0660
#define LINE_SIZE 1024
#define LIFE_SIZE 100
#define FILENAME_GENERATOR "Gerador.log"

static int id = 1;
static FILE *fp_gerador;
clock_t begin;

vehicle create_vehicle(int uTime) {
  vehicle nova;
  nova.id = id;
  id++;

  char access = ORIENTATION[rand() % 4];

  nova.access = access;
  int r = rand() % 10 + 1;
  nova.t_parking = r * uTime;

  return nova;
};

void write_vehicle(vehicle *veh, statusVehicle status, clock_t life) {

  char line[LINE_SIZE];
  char lifeStr[LIFE_SIZE];

  if (life == 0) {
    strcpy(lifeStr, "?");
  } else {
    sprintf(lifeStr, "%ld", life);
  }

  sprintf(line, "%9ld ; %8d ; %5c   ; %11ld ; %7s ; %s\n", clock() - begin,
          veh->id, veh->access, veh->t_parking, lifeStr, status.stat);
  fprintf(fp_gerador, "%s", line);
}

void *vehicleThread(void *arg) {

  if (pthread_detach(pthread_self()) != 0) {
    perror("Vehicle thread: ");
    free(arg);
  }

  int fd_vehicle;
  vehicle *nova = (vehicle *)(arg);
  char vehicleFifo[200];
  clock_t life_begin, life_end;
  statusVehicle status;
  life_begin = clock();

  sprintf(vehicleFifo, "/tmp/fifo%d", nova->id);

  mkfifo(vehicleFifo, OG_RW_PERMISSION);

  if ((fd_vehicle = open(vehicleFifo, O_RDWR)) == -1) {
    free(nova);
    return NULL;
  }

  sem_wait(sem1);

  int fd_dir;

  switch (nova->access) {
  case 'N':
    if ((fd_dir = open("/tmp/fifoN", O_WRONLY | O_NONBLOCK)) == -1) {
      strcpy(status.stat, FECHADO);
      write_vehicle(nova, status, (clock_t)0);
      free(nova);
      sem_post(sem1);
      unlink(vehicleFifo);
      return NULL;
    }
    break;
  case 'E':
    if ((fd_dir = open("/tmp/fifoE", O_WRONLY | O_NONBLOCK)) == -1) {
      strcpy(status.stat, FECHADO);
      write_vehicle(nova, status, (clock_t)0);
      free(nova);
      sem_post(sem1);
      unlink(vehicleFifo);
      return NULL;
    }
    break;
  case 'S':
    if ((fd_dir = open("/tmp/fifoS", O_WRONLY | O_NONBLOCK)) == -1) {
      strcpy(status.stat, FECHADO);
      write_vehicle(nova, status, (clock_t)0);
      free(nova);
      sem_post(sem1);
      unlink(vehicleFifo);
      return NULL;
    }
    break;
  case 'O':
    if ((fd_dir = open("/tmp/fifoO", O_WRONLY)) == -1) {
      strcpy(status.stat, FECHADO);
      write_vehicle(nova, status, (clock_t)0);
      free(nova);
      sem_post(sem1);
      unlink(vehicleFifo);
      return NULL;
    }
    break;
  }

  if (fd_dir != -1) {
    write(fd_dir, nova, sizeof(*nova));
  }
  sem_post(sem1);

  close(fd_dir);

  if (read(fd_vehicle, &status, sizeof(status)) == -1) {
    perror("read vehicle fifo:");
    close(fd_vehicle);
    free(nova);
    unlink(vehicleFifo);
    return NULL;
  }

  write_vehicle(nova, status, (clock_t)0);

  if (strcmp(status.stat, ENTRADA) == 0) {
    if (read(fd_vehicle, &status, sizeof(status)) == -1) {
      perror("read vehicle fifo");
      close(fd_vehicle);
      free(nova);
      unlink(vehicleFifo);
      return NULL;
    }
    // printf("FIM ENTRADA1\n");
    life_end = clock() - life_begin;
    write_vehicle(nova, status, life_end);
    //    printf("FIM ENTRADA\n");
  }
  close(fd_vehicle);
  free(nova);
  unlink(vehicleFifo);
  return NULL;
}

int main(int argc, char const *argv[]) {

  if (argc != 3) {
    fprintf(stderr, "Wrong number of arguements.\n Usage: parque <T_GERACAO> "
                    "<U_RELOGIO> \n");
    return 1;
  }

  // Guardar variáveis ---------------------------------------------
  srand(time(NULL));

  errno = 0;

  double generatorTime = strtol(argv[1], NULL, 10);
  if (errno == ERANGE || errno == EINVAL) {
    perror("convert working time failed");
  }

  errno = 0;

  int uRelogio = strtol(argv[2], NULL, 10);

  if (errno == ERANGE || errno == EINVAL) {
    perror("convert parking space failed");
  }

  if ((fp_gerador = fopen(FILENAME_GENERATOR, "w")) == NULL) {
    fprintf(stderr, "Can't open gerador.log");
    exit(3);
  }

  fprintf(fp_gerador,
          "t(ticks) ; id_viat ; destin ; t_estacion ; t_vida ; observ\n");

  if ((sem1 = initSem(SEMNAME)) == SEM_FAILED) {
    exit(4);
  }

  // Gerar acesso --------------------------------------------
  begin = clock();
  clock_t end = clock();
  double elapsed = 0;
  while (elapsed < generatorTime) {

    // Gerar tempo de estacionamento e número identificador

    int r = rand() % 10 + 1;

    int waitT = 0;
    if (r > 5 && r <= 8) {
      waitT = uRelogio;
    } else if (r > 8) {
      waitT = 2 * uRelogio;
    }

    pthread_t init;
    vehicle *new = malloc(sizeof(vehicle));
    *new = create_vehicle(uRelogio);
    pthread_create(&init, NULL, vehicleThread, new); // TODO

    waitTime(waitT);

    end = clock();
    elapsed = (double)((end - begin) / CLOCKS_PER_SEC);
  }

  pthread_exit(0);
}
