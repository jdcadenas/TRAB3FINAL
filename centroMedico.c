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
sem_t sem_fila_medicos[NUM_MEDICOS];
sem_t sem_atencion_medicos[NUM_MEDICOS];

sem_t sem_fila_cajeros;
sem_t sem_atencion_cajeros[NUM_CAJEROS];

int pacientes_vip_por_medico[NUM_MEDICOS] = {0};
int contador_pacientes_regulares[NUM_MEDICOS] = {0};
int contador_pacientes = 0;

int hay_paciente_en_fila[NUM_MEDICOS] = {0}; // Indica si hay pacientes en la fila de cada médico
int hay_paciente_vip = 0;                    // Indica si hay pacientes en la fila VIP

sem_t sem_fila_vip;
sem_t mutex;

int terminar;

void *medico(void *arg)
{
    int id_medico = *(int *)arg;
    int contador_pacientes = 0;
    int hay_paciente_regular = 0;

    int filadelmedico;
    int atencion_medicos;

    while (1)
    {

        // Verificar si hay pacientes en la fila del médico
        if (hay_paciente_en_fila[id_medico] > 0)
        {
            printf("hay %d pacientes en la fila del medico %d\n", hay_paciente_en_fila[id_medico], id_medico);
            // sem_wait(&sem_atencion_medicos[id_medico]);
            printf("Médico %d atendiendo a un paciente regular.\n", id_medico);
            sleep(1);
            sem_wait(&mutex);                  // Proteger el acceso a hay_paciente_en_fila
            hay_paciente_en_fila[id_medico]--; // Actualizar variable de estado de la fila
            contador_pacientes_regulares[id_medico]++;
            contador_pacientes++;
            sem_post(&mutex);

            sem_post(&sem_atencion_medicos[id_medico]);
            sem_post(&sem_fila_medicos[id_medico]);
        }
        else if ((contador_pacientes >= 3) && (hay_paciente_vip > 0)) // Verificar si hay pacientes VIP y si es el turno de atender
        {
            printf("Médico %d atendiendo a un paciente VIP.\n", id_medico);
            sem_wait(&mutex);
            pacientes_vip_por_medico[id_medico]++;
            contador_pacientes = 0; // Reiniciar el contador
            sem_post(&mutex);

            sleep(1); // Simular tiempo de atención
            sem_post(&sem_atencion_medicos[id_medico]);
            sem_post(&sem_fila_vip);
        }
        else
        {
            // Si no hay pacientes, dormir
            printf("Médico %d durmiendo una siesta.\n", id_medico);
            sleep(2); // Simular siesta
            continue;
        }

        sem_post(&sem_fila_cajeros);
    }
}

void *paciente(void *arg)
{
    int fila;
    int value;
    int value2;
    int capacidad;
    int fila_cajeros;
    while (1)
    {
        printf("**** Paciente Entrando al centro médico\n");

        sem_getvalue(&capacidad_consultorio, &capacidad);
        printf("capacidad del centro: %d\n", capacidad);

        sem_wait(&capacidad_consultorio);
        printf("Cliente esperando ser atendido\n");
        // Generar un número aleatorio entre 0 y 4 para establecer la fila del paciente
        // si es fila 4 entonces es fila VIP
        fila = rand() % (NUM_FILAS + 1);

        printf("cliente para la fila %d\n", fila);

        if (fila != FILA_VIP)
        {
            printf("cliente regular\n");
            sem_wait(&mutex);
            hay_paciente_en_fila[fila]++; // Indicar que hay un paciente en la fila del médico
            contador_pacientes_regulares[fila]++;
            sem_post(&mutex); // Salir de la sección crítica

            sem_wait(&sem_fila_medicos[fila]); // Esperar en la fila del médico
            printf("Paciente regular en consulta\n");

            // **Esperar a que el médico esté disponible**
            sem_wait(&sem_atencion_medicos[fila]);

            // **Simular la consulta**
            printf("Paciente está siendo atendido por el médico %d.\n", fila);
            // sleep(1);

            // **Liberar al médico**
            sem_post(&sem_atencion_medicos[fila]);
            // sem_post(&sem_fila_medicos[fila]);
        }
        else
        {
            printf("cliente VIP\n");
            sem_wait(&mutex);
            hay_paciente_vip++; // Indicar que hay un paciente en la fila VIP
            pacientes_vip_por_medico[fila]++;
            sem_post(&mutex);        // Salir de la sección crítica
            sem_wait(&sem_fila_vip); // Esperar en la fila VIP
            printf(" paciente esperando fila vip\n");

            // **Buscar un médico disponible para atender al paciente VIP**
            for (int i = 0; i < NUM_MEDICOS; i++)
            {
                if (sem_trywait(&sem_atencion_medicos[i]) == 0)
                {
                    printf("Paciente VIP está siendo atendido por el médico %d.\n", i);
                    // **Simular la consulta VIP**
                    sleep(1);
                    pacientes_vip_por_medico[i]++;
                    sem_post(&sem_atencion_medicos[i]);
                    break;
                }
            }
            sem_post(&sem_fila_vip);
        }
        // **Esperar en la fila de cajeros**
        sem_wait(&sem_fila_cajeros);
        printf("cliente esperando en la fila del cajero\n");
        // **Buscar un cajero disponible**
        for (int i = 0; i < NUM_CAJEROS; i++)
        {
            if (sem_trywait(&sem_atencion_cajeros[i]) == 0)
            {
                printf("Paciente está pagando al cajero %d.\n", i);

                // **Simular el pago**
                sleep(1);

                // **Liberar al cajero**
                sem_post(&sem_atencion_cajeros[i]);
                break;
            }
        }

        // **Indicar que ha salido de la fila de cajeros**
        sem_post(&sem_fila_cajeros);

        // **Salir del centro médico**
        sem_post(&capacidad_consultorio);
        printf("Paciente salió del centro médico.\n");
        // pthread_exit(NULL);
    }
}

void *cajero(void *arg)

{
    int id_cajero = *(int *)arg;
    printf("Cajero %d iniciado.\n", id_cajero);

    while (1)
    {
        if (sem_trywait(&sem_fila_cajeros) == 0)
        {
            sem_post(&sem_fila_cajeros);
            sem_wait(&sem_atencion_cajeros[id_cajero]);
        }
        else
        {
            // Si no hay pacientes, dormir
            printf("Cajero %d esperando pacientes.\n", id_cajero);
            sleep(2); // Simular espera
            continue;
        }

        // Cobrar al paciente
        printf("Cajero %d cobrando a un paciente.\n", id_cajero);
        sleep(1); // Simular tiempo de cobro

        // Liberar al paciente
        sem_post(&sem_atencion_cajeros[id_cajero]);
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
            printf("El medico  %d atendio %d pacientes", i, contador_pacientes_regulares[i]);
            printf("y %d Pacientes VIP\n", pacientes_vip_por_medico[i]);
            total_clientes += contador_pacientes_regulares[i] + pacientes_vip_por_medico[i];
        }
        printf("________________________________________________\n");
        printf("Total de clientes atendidos : %d \n", total_clientes);
    }
    exit(0);
}

int main()
{

    /*Registrar manejador de senales */
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

    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_init(&sem_fila_medicos[i], 0, 0); // indica medico
        sem_init(&sem_atencion_medicos[i], 0, 1);
    }

    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_init(&sem_fila_cajeros, 0, 0);
        sem_init(&sem_atencion_cajeros[i], 0, 1);
    }

    sem_init(&mutex, 0, 1);        //
    sem_init(&sem_fila_vip, 0, 0); //

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
            int *idc = malloc(sizeof(int)); // Asignar memoria para el id
            *idc = i;
            printf("Creando hilo de cajero %d \n", i);
            if (pthread_create(&h_cajeros[i], NULL, cajero, (void *)idc) != 0)
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

            sleep(2);
        }

        // sleep(2);
    }
    //******  Esperar a que todos los hilos de médicos terminen

    for (int i = 0; i < NUM_MEDICOS; i++)
        pthread_join(h_medicos[i], NULL);

    for (int i = 0; i < NUM_CAJEROS; i++)
        pthread_join(h_cajeros[i], NULL);

    // pthread_join(h_pacientes, NULL);
    /* eliminación de semáforos*/
    sem_destroy(&capacidad_consultorio);
    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_destroy(&sem_fila_medicos[i]);
        sem_destroy(&sem_atencion_medicos[i]);
    }

    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_destroy(&sem_fila_cajeros);
        sem_destroy(&sem_atencion_cajeros[i]);
    }

    sem_destroy(&mutex);

    return 0;
}
