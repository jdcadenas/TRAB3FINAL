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
sem_t paciente_listo[NUM_MEDICOS];
sem_t paciente_salir_consultorio[NUM_MEDICOS];
sem_t cajeros[NUM_CAJEROS];
sem_t cajero_libre;
sem_t cliente_salida;
// sem_t abonar_consulta;
sem_t consulta_terminada;
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
            printf("El medico  %d atendio %d pacientes", i, pacientes_por_medico[i]);
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
        if (hay_paciente_vp)
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
            sem_post(&paciente_listo[id_medico]);

            // Esperar a que haya un paciente listo
            sem_wait(&mutex); // Sección crítica
            // Atender al paciente
            printf("el %d médico atiende a paciente", id_medico);
            pacientes_por_medico[id_medico]++; // Actualizar el contador de pacientes
            printf("El valor del cant paciente es %d\n", pacientes_por_medico[id_medico]);
            sem_post(&mutex);
            sem_post(&consulta_terminada);
            sem_wait(&paciente_salir_consultorio[id_medico]);
            sem_wait(&medicos_sem[id_medico]);
            // Liberar el semáforo para que otro paciente pueda ser atendido
        }
    }
    sem_post(&mutex1);
}

void *paciente(void *arg)
{
    int fila;
    while (1)
    {
        printf("**** Paciente Entrando al centro médico\n");
        sem_wait(&capacidad_consultorio);

        printf("Cliente esperando ser atendido\n");

        sem_wait(&mutex);
        fila = rand() % (NUM_FILAS);
        printf("cliente para la fila %d\n", fila);
        sem_post(&mutex);

        if (fila == FILA_VIP)
        {
            sem_wait(&mutex);
            hay_paciente_vp = true;
            sem_post(&mutex);
            printf("llegando paciente vip\n");
            sem_post(&fila_vip);
            sem_wait(&consulta_terminada);
        }
        else
        {
            sem_wait(&medicos_sem[fila]); // Esperar a que haya un espacio en la consulta
            printf("Paciente en consulta\n");
            // Simular la consulta
            sleep(2);
            sem_post(&paciente_listo[fila]); // Indicar que el paciente está listo para ser atendido
            sem_wait(&consulta_terminada);   // Esperar a que la consulta termine
            sem_post(&paciente_salir_consultorio[fila]);
            // Liberar el espacio en la consulta para otros pacientes
        }
        // Cliente abonando la consulta
        sem_post(&cajero_libre); // Esperar a que haya un cajero libre
        printf("Cliente abonando la consulta\n");
        sem_wait(&cliente_salida);
        // Liberar el cupo
        sem_post(&capacidad_consultorio);
    }
}

void *cajero(void *arg)
{
    while (1)
    {
        sem_wait(&cajero_libre); // Esperar a que haya un cajero libre
        printf("Cliente atendido cajero\n");
        sem_post(&cliente_salida);
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
    sem_init(&capacidad_consultorio, 0, CAPACIDAD_CENTRO);
    sem_init(&cajero_libre, 0, 1);
    sem_init(&cliente_salida, 0, 0);

    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_init(&medicos_sem[i], 0, 1);
        sem_init(&paciente_listo[i], 0, 0);
        sem_init(&paciente_salir_consultorio[i], 0, 0);
    }

    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_init(&cajeros[i], 0, 0);
    }

    sem_init(&consulta_terminada, 0, 0);
    sem_init(&mutex, 0, 1);
    sem_init(&mutex1, 0, 1);
    sem_init(&fila_vip, 0, 0);

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
            pthread_join(h_medicos[i], NULL);

        for (int i = 0; i < NUM_CAJEROS; i++)
            pthread_join(h_cajeros[i], NULL);

        pthread_join(h_pacientes, NULL);

        sleep(2);
    }
    /* eliminación de semáforos*/
    sem_destroy(&capacidad_consultorio);
    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_destroy(&medicos_sem[i]);
        sem_destroy(&paciente_listo[i]);
        sem_destroy(&paciente_salir_consultorio[i]);
    }
    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_destroy(&cajeros[i]);
    }

    sem_destroy(&cajero_libre);
    sem_destroy(&consulta_terminada);
    sem_destroy(&mutex);
    sem_destroy(&mutex1);

    return 0;
}
