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
sem_t sem_atencion_cajeros[NUM_CAJEROS];
sem_t sem_fila_cajeros;
sem_t mutex;
sem_t sem_fila_vip;

volatile sig_atomic_t terminar = 0;

int pacientes_por_medico[NUM_MEDICOS] = {0};
int pacientes_vip_por_medico[NUM_MEDICOS] = {0};
bool hay_paciente_vp = false;
bool hay_paciente[NUM_MEDICOS] = {false};
int contador_pacientes_regulares[NUM_MEDICOS] = {0};

void *medico(void *arg)
{
    int id_medico = *(int *)arg;
    int contador_pacientes = 0;

    while (1)
    {
        // Verificar si hay pacientes en la fila del médico
        if (sem_trywait(&sem_fila_medicos[id_medico]) == 0) // Condición de espera
        {
            sem_post(&sem_fila_medicos[id_medico]);
            sem_wait(&sem_atencion_medicos[id_medico]);
            contador_pacientes++; // Incrementar el contador
        }
        else
        {
            // Verificar si hay pacientes VIP y si es el turno de atender
            if (contador_pacientes >= 3 && sem_trywait(&sem_fila_vip) == 0)
            {
                sem_post(&sem_fila_vip);
                sem_wait(&sem_atencion_medicos[id_medico]);
                contador_pacientes = 0; // Reiniciar el contador
            }
            else
            {
                // Si no hay pacientes, dormir
                printf("Médico %d durmiendo una siesta.\n", id_medico);
                sleep(2); // Simular siesta
                continue;
            }
        }

        // Atender al paciente (simulado)
        printf("Médico %d atendiendo a un paciente.\n", id_medico);
        sleep(1); // Simular tiempo de atención

        // Liberar al paciente
        sem_post(&sem_atencion_medicos[id_medico]);
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

            sem_wait(&sem_fila_vip); //
            printf(" paciente esperando filavip\n");

            // Buscar un médico disponible para atender al paciente VIP
            for (int i = 0; i < NUM_MEDICOS; i++)
            {
                if (sem_trywait(&sem_atencion_medicos[i]) == 0)
                {
                    printf("Paciente VIP está siendo atendido por el médico %d.\n", i);
                    pacientes_vip_por_medico[i]++; // contador de pacientes vip
                    sem_post(&sem_atencion_medicos[i]);
                    break;
                }
            }
            sem_post(&sem_fila_vip);
        }
        else
        {
            //  paciente regular esperando en la fila
            sem_wait(&sem_fila_medicos[fila]);
            printf("Paciente regular en consulta\n");

            // Esperar a que el médico esté disponible
            sem_wait(&sem_atencion_medicos[fila]);

            // Simular la consulta
            sleep(1);

            printf("Paciente está siendo atendido por el médico %d.\n", fila);
            contador_pacientes_regulares[fila]++;
            // liberar al médico
            sem_post(&sem_atencion_medicos[fila]);
            sem_post(&sem_fila_medicos[fila]);
            // sem_post(&paciente_salir_consultorio[fila]); // Liberar el espacio en la consulta para otros pacientes
        }
        sem_post(&mutex);
        // Esperar en la fila de cajeros
        sem_wait(&sem_fila_cajeros);

        // Esperar a que un cajero esté disponible
        for (int i = 0; i < NUM_CAJEROS; i++)
        {
            if (sem_trywait(&sem_atencion_cajeros[i]) == 0)
            {
                printf("Pacienteestá pagando al cajero %d.\n", i);
                break;
            }
        }

        sleep(1); // Simular tiempo de pago

        // Liberar al cajero
        for (int i = 0; i < NUM_CAJEROS; i++)
        {
            if (sem_trywait(&sem_atencion_cajeros[i]) == 0)
            {
                sem_post(&sem_atencion_cajeros[i]);
                break;
            }
        }
        sem_post(&sem_fila_cajeros);

        // Salir del centro médico
        sem_post(&capacidad_consultorio); // Liberar el cupo
        printf("Paciente %d salió del centro médico.\n");

        pthread_exit(NULL);
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
        // Cobrar al paciente (simulado)
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
    sem_init(&sem_fila_cajeros, 0, 0);

    int rc;

    for (int i = 0; i < NUM_MEDICOS; i++)
    {
        sem_init(&sem_fila_medicos[i], 0, 0); // indica medico
        sem_init(&sem_atencion_medicos[i], 0, 1);
    }

    for (int i = 0; i < NUM_CAJEROS; i++)
    {
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
        sem_destroy(&sem_fila_medicos[i]);
        sem_destroy(&sem_atencion_medicos[i]);
    }

    for (int i = 0; i < NUM_CAJEROS; i++)
    {
        sem_destroy(&sem_atencion_cajeros[i]);
    }

    sem_destroy(&sem_fila_cajeros);
    sem_destroy(&mutex);

    return 0;
}
