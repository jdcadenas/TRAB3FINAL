/*Programa que implementa modelar un centro medico VIP*/
/*Mario Roman TRaversaro*/
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define NUM_MEDICOS 4         // atienden 4 medicos
#define NUM_CAJEROS 2         // 2 cajeros
#define CAPACIDAD_CENTRO 28   // 28 pacientes dentro del consultorio
#define NUM_FILAS NUM_MEDICOS // una fila para cada medico
#define FILA_VIP 4            // indica fila vip

// semaforos
sem_t capacidad_consultorio;
sem_t medicos_sem[NUM_MEDICOS];
sem_t cajeros[NUM_CAJEROS];
sem_t abonar_consulta;
sem_t consulta_terminada;
sem_t paciente_listo, esperando_paciente_regular;
sem_t mutex, mutex1;
sem_t fila_vip;

volatile sig_atomic_t terminar = 0;

int pacientes_por_medico[NUM_MEDICOS] = {0};
int pacientes_vip_por_medico[NUM_MEDICOS] = {0};
bool hay_paciente_vp = false;

void handle_signal(int sig)
{
    int total_clientes = 0;
    if (sig == SIGINT || sig == SIGTERM)
    {
        terminar = 1;
        printf("Resultado de consultorio\n");
        printf("Handle signal %d\n", sig);
        for (int i = 0; i < NUM_MEDICOS; i++)
        {
            printf("El medico  %d atendio %d pacientes", i + 1, pacientes_por_medico[i]);
            printf("y %d Pacientes VIP\n", pacientes_vip_por_medico[i]);
            total_clientes += pacientes_por_medico[i] + pacientes_vip_por_medico[i];
        }
        printf("________________________________________________\n");
        printf("Total de clientes atendidos : %d \n", total_clientes);
    }
    exit(0);
}

void *medico(void *arg)
{
    int id_medico = *(int *)arg;
    while (1)
    {
        sem_wait(&mutex1); // Proteger acceso a recursos compartidos
        // verificar si hay pacientes VIP
        if (hay_paciente_vp || (pacientes_por_medico[id_medico] % 3 == 0))
        {
            sem_wait(&fila_vip);
            printf("Médico %d atendiendo a un paciente VIP\n", id_medico);
            sem_wait(&mutex);
            pacientes_vip_por_medico[id_medico]++;
            hay_paciente_vp = false;
            sem_post(&mutex);
            sem_post(&consulta_terminada);
        }
        else
        {
            sem_wait(&esperando_paciente_regular);
            //  esperar por un paciente regular
            printf("Médico %d esperando  a un paciente regular\n", id_medico);
            sem_wait(&medicos_sem[id_medico]);

            printf("Médico %d llama a un paciente regular\n", id_medico);

            sem_wait(&mutex);
            pacientes_por_medico[id_medico]++;
            sem_post(&mutex);
            sem_post(&consulta_terminada);
        }
        sem_post(&mutex1);
        sem_post(&abonar_consulta);
    }
}

void *paciente(void *arg)
{
    int fila;
    while (1)
    {
        printf("**** Paciente Entrando al centro médico\n");
        sem_wait(&capacidad_consultorio);
        printf("Cliente esperando ser atendido\n");

        fila = rand() % (NUM_FILAS + 1);
        printf("cliente para la fila %d\n", fila);

        if (fila == FILA_VIP)
        {
            sem_wait(&mutex);
            hay_paciente_vp = true;
            sem_post(&mutex);
            printf("llegando paciente vip\n");
            sem_post(&fila_vip);
        }
        else
        {

            sem_post(&esperando_paciente_regular);
            sem_post(&medicos_sem[fila]);

            printf("paciente regular Saliendo \n");
            sem_post(&consulta_terminada); // notificar que la consulta ha terminado
        }

        sem_wait(&consulta_terminada); // Esperar a que el médico termine
    }
}

void *cajero(void *arg)
{
    while (1)
    {
        sem_wait(&abonar_consulta);
        printf("Cliente abonando la consulta\n");
        sem_post(&capacidad_consultorio);
    }
}

int main()
{
    /*Rejistrar manjador de senales */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    /*Inicializacion semilla num aleatorios*/
    srand(time(NULL));

    /*Declaracion de Hilos*/
    pthread_t h_medicos[NUM_MEDICOS];
    pthread_t h_cajeros[NUM_CAJEROS];
    pthread_t h_pacientes;

    /*Inicializacion de semaforos*/
    sem_init(&capacidad_consultorio, 0, 28);
    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_init(&medicos_sem[i], 0, 0);
    }

    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_init(&cajeros[i], 0, 0);
    }
    sem_init(&abonar_consulta, 0, 0);
    sem_init(&consulta_terminada, 0, 0);
    sem_init(&mutex, 0, 1);
    sem_init(&mutex1, 0, 1);
    sem_init(&fila_vip, 0, 0);
    sem_init(&paciente_listo, 0, 0);
    sem_init(&esperando_paciente_regular, 0, 0);

    /*Creacion Hilos*/

    while (!terminar)
    {
        for (int i = 0; i < NUM_MEDICOS; i++)
        {
            int *id = malloc(sizeof(int)); // Asignar memoria para el id
            *id = i;                       // Asignar el valor de i
            pthread_create(&h_medicos[i], NULL, medico, (void *)id);

            printf("Creando hilo de medico %d \n", i);
        }
        for (int i = 0; i < NUM_CAJEROS; i++)
        {
            pthread_create(&h_cajeros[i], NULL, cajero, NULL);
            printf("Creando hilo de cajero %d \n", i);
        }

        pthread_create(&h_pacientes, NULL, paciente, NULL);
        printf("Creando hilo de paciente \n");

        //******  Esperar a que todos los hilos de médicos terminen

        for (int i = 0; i < NUM_MEDICOS; i++)
        {
            pthread_join(h_medicos[i], NULL);
        }

        for (int i = 0; i < NUM_CAJEROS; i++)
        {
            pthread_join(h_cajeros[i], NULL);
        }

        pthread_join(h_pacientes, NULL);

        sleep(1);
    }
    /* eliminación de semáforos*/
    sem_destroy(&capacidad_consultorio);
    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_destroy(&medicos_sem[i]);
    }
    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_destroy(&cajeros[i]);
    }

    sem_destroy(&abonar_consulta);
    sem_destroy(&consulta_terminada);
    sem_destroy(&mutex);
    sem_destroy(&mutex1);
    sem_destroy(&paciente_listo);
    return 0;
}
