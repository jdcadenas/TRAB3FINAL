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

//****** semaforos
sem_t capacidad_consultorio;
sem_t medicos_sem[NUM_MEDICOS];
sem_t paciente_listo[NUM_MEDICOS];
sem_t paciente_salir_consultorio[NUM_MEDICOS];
sem_t cajero_libre;
sem_t consulta_abonada;
sem_t consulta_terminada;
sem_t mutex;
sem_t fila_vip;

volatile sig_atomic_t terminar = 0;

int pacientes_por_medico[NUM_MEDICOS] = {0};
int pacientes_vip_por_medico[NUM_MEDICOS] = {0};
bool hay_paciente_vp = false;
bool hay_paciente[NUM_MEDICOS] = {false};

void *medico(void *arg)
{
    int id_medico = *(int *)arg;
    int contador_pacientes_regulares[NUM_MEDICOS] = {0};

    while (1)
    {
        // verificar si hay pacientes VIP
        if (hay_paciente_vp)
        {
            sem_wait(&fila_vip);
            printf(">>>> Médico %d atendiendo a un paciente VIP\n", id_medico);

            sem_wait(&mutex); // Sección crítica
                pacientes_vip_por_medico[id_medico]++; //contador de pacientes vip
                hay_paciente_vp = false;
                contador_pacientes_regulares[id_medico] = 0; // Reiniciar contador
            sem_post(&mutex);

            sem_post(&consulta_terminada);
            sem_wait(&paciente_salir_consultorio[id_medico]);
            sem_wait(&medicos_sem[id_medico]);
        }
        else
        {
            sem_post(&paciente_listo[id_medico]); // llama a un paciente listo
            sem_wait(&mutex); // Sección crítica
                printf("el %d médico atiende a paciente\n", id_medico);
                pacientes_por_medico[id_medico]++; // Actualizar el contador de pacientes
                contador_pacientes_regulares[id_medico]++;
                printf("contador de pacientes regulares %d \n", contador_pacientes_regulares[id_medico]);

                if (contador_pacientes_regulares[id_medico] == 3)
                {
                    contador_pacientes_regulares[id_medico] = 0;
                    sem_post(&fila_vip); // espera a un paciente VIP
                }
            sem_post(&mutex);

            sem_post(&consulta_terminada);
            sem_wait(&paciente_salir_consultorio[id_medico]);
            sem_wait(&medicos_sem[id_medico]);
            // Liberar el semáforo para que otro paciente pueda ser atendido
        }
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

        // Generar un número aleatorio entre 0 y 4 para establecer la fila del paciente
        // si es fila 4 entonces es fila VIP
        fila = rand() % (5);
        printf("cliente para la fila %d\n", fila);
        sem_wait(&mutex);
            if (fila == FILA_VIP)
            {
                // es un paciente VIP
                hay_paciente_vp = true;
                printf("llegando paciente vip\n");
                sem_post(&fila_vip);                        // incrementar semaforo cola vip
                printf("entrar a consulta VIP \n ");
                sem_wait(&consulta_terminada);              // Esperar a que la consulta termine
            }
            else
            {
                // es un paciente regular 
                sem_wait(&medicos_sem[fila]);               // Esperar a que haya un espacio en la consulta
                printf("Paciente regularen consulta\n");
                // Simular la consulta
                sleep(1);
                sem_post(&paciente_listo[fila]);             // Indicar que el paciente está listo para ser atendido
                sem_wait(&consulta_terminada);               // Esperar a que la consulta termine
                sem_post(&paciente_salir_consultorio[fila]); // Liberar el espacio en la consulta para otros pacientes
            }
        sem_post(&mutex);
        sem_post(&cajero_libre);                            // Esperar a que haya un cajero libre
        printf("Cliente abonando la consulta\n");
        sem_wait(&consulta_abonada);                        // Cliente abonando la consulta
        sem_post(&capacidad_consultorio);                   // Liberar el cupo
    }
}

void *cajero(void *arg)
{
    while (1)
    {
        sem_wait(&cajero_libre);                            // Esperar a que haya un cajero libre
        printf("Cliente atendido cajero\n");
        sem_post(&consulta_abonada);
    }
}

/**** Manejo de señales  ****/

void handle_signal(int sig)
{
    int total_clientes = 0;
    if (sig == SIGINT || sig == SIGTERM)
    {
        terminar = 1;
        printf("\n\nResultado de consultorio\n");
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
    sem_init(&cajero_libre, 0, 2);
    sem_init(&consulta_abonada, 0, 0);

    int rc;

    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_init(&medicos_sem[i], 0, 1);
        sem_init(&paciente_listo[i], 0, 0);
        sem_init(&paciente_salir_consultorio[i], 0, 0);
    }

    sem_init(&consulta_terminada, 0, 0);
    sem_init(&mutex, 0, 1);
    sem_init(&fila_vip, 0, 0);

    /*********Creacion Hilos*********/

    while (!terminar) // para controlar la señal
    {
        //************ Crear hilos de médicos
        for (int i = 0; i < NUM_MEDICOS; i++)
        {
            int *id = malloc(sizeof(int)); // Asignar memoria para el id
            *id = i;
            printf("Creando hilo de medico %d \n", i); // Asignar el valor de i
            if (pthread_create(&h_medicos[i], NULL, medico, (void *)id) != 0)
            {
                printf("Error Creando hilo médicos\n");
                exit(-1);
            }
        }

        //*********** Creación de hilo Cajero
        for (int i = 0; i < NUM_CAJEROS; i++)
        {
            printf("Creando hilo de cajero %d \n", i);
            if (pthread_create(&h_cajeros[i], NULL, cajero, NULL) != 0)
            {
                printf("Error Creando cajero\n");
                exit(-1);
            }
        }

        //********* Creación de hilos pacientes
        while (1)
        {
            printf("Creando hilo de paciente \n");
            if (pthread_create(&h_pacientes, NULL, paciente, NULL))
            {
                printf("Error Creando paciente\n");
                exit(-1);
            }

            sleep(3);
        }

        //******  Esperar a que todos los hilos de médicos terminen

        for (int i = 0; i < NUM_MEDICOS; i++)
            pthread_join(h_medicos[i], NULL);

        for (int i = 0; i < NUM_CAJEROS; i++)
            pthread_join(h_cajeros[i], NULL);

        // pthread_join(h_pacientes, NULL);

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

    sem_destroy(&cajero_libre);
    sem_destroy(&consulta_terminada);
    sem_destroy(&mutex);

    return 0;
}
