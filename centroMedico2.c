#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <signal.h>

#define NUM_PACIENTES 28 // Capacidad del centro médico
#define NUM_MEDICOS 4  // Cantidad de médicos
#define NUM_CAJEROS 2   // Cantidad de cajeros

sem_t sem_puerta_entrada;    // Semáforo para controlar el acceso al centro médico
sem_t sem_puerta_salida;     // Semáforo para controlar la salida del centro médico
sem_t sem_pacientes_espera;   // Semáforo para los pacientes que esperan afuera
sem_t sem_fila_medicos[NUM_MEDICOS]; // Semáforos para las filas de cada médico
sem_t sem_fila_vip;        // Semáforo para la fila VIP
sem_t sem_atencion_medicos[NUM_MEDICOS]; // Semáforos para la atención de cada médico
sem_t sem_fila_cajeros;      // Semáforo para la fila de los cajeros
sem_t sem_atencion_cajeros[NUM_CAJEROS]; // Semáforos para la atención de cada cajero
sem_t mutex;
int pacientes_atendidos_total = 0; // Contador de pacientes atendidos en total
int pacientes_atendidos_medico[NUM_MEDICOS] = {0}; // Contador de pacientes por médico


// Estructura para representar un paciente
typedef struct {
    int id;          // Identificador del paciente
    int tipo;        // 0: normal, 1: VIP
    int medico_asignado; // Médico al que debe atender el paciente
} Paciente;

// Función para el hilo de paciente
void *paciente(void *args) {
    Paciente *p = (Paciente *)args;

    // Esperar a que haya espacio en el centro médico
    sem_wait(&sem_pacientes_espera);

    // Entrar al centro médico
    sem_wait(&sem_puerta_entrada);
    printf("Paciente %d (%s) entró al centro médico.\n", p->id, p->tipo ? "VIP" : "Normal");

    // Esperar en la fila del médico o en la fila VIP
    if (p->tipo == 0) {
        sem_wait(&sem_fila_medicos[p->medico_asignado]);
    } else {
        sem_wait(&sem_fila_vip);
    }

    // Esperar a que el médico esté disponible
    if (p->tipo == 0) {
        sem_wait(&sem_atencion_medicos[p->medico_asignado]);
        printf("Paciente %d está siendo atendido por el médico %d.\n", p->id, p->medico_asignado);
        pacientes_atendidos_medico[p->medico_asignado]++;
    } else {
        // Buscar un médico disponible para atender al paciente VIP
        for (int i = 0; i < NUM_MEDICOS; i++) {
            if (sem_trywait(&sem_atencion_medicos[i]) == 0) {
                printf("Paciente VIP %d está siendo atendido por el médico %d.\n", p->id, i);
                pacientes_atendidos_medico[i]++;
                break;
            }
        }
    }
    
    sleep(1); // Simular tiempo de atención

    // Liberar al médico
    if (p->tipo == 0) {
        sem_post(&sem_atencion_medicos[p->medico_asignado]);
        sem_post(&sem_fila_medicos[p->medico_asignado]);
    } else {
        for (int i = 0; i < NUM_MEDICOS; i++) {
            if (sem_trywait(&sem_atencion_medicos[i]) == 0) {
                sem_post(&sem_atencion_medicos[i]);
                break;
            }
        }
        sem_post(&sem_fila_vip);
    }

    // Esperar en la fila de cajeros
    sem_wait(&sem_fila_cajeros);

    // Esperar a que un cajero esté disponible
    for (int i = 0; i < NUM_CAJEROS; i++) {
        if (sem_trywait(&sem_atencion_cajeros[i]) == 0) {
            printf("Paciente %d está pagando al cajero %d.\n", p->id, i);
            break;
        }
    }

    sleep(1); // Simular tiempo de pago

    // Liberar al cajero
    for (int i = 0; i < NUM_CAJEROS; i++) {
        if (sem_trywait(&sem_atencion_cajeros[i]) == 0) {
            sem_post(&sem_atencion_cajeros[i]);
            break;
        }
    }
    sem_post(&sem_fila_cajeros);

    // Salir del centro médico
    sem_post(&sem_puerta_salida);
    printf("Paciente %d salió del centro médico.\n", p->id);

    // Liberar espacio en el centro médico
    sem_post(&sem_pacientes_espera);

    pthread_exit(NULL);
}

// Función para el hilo de médico
void *medico(void *args) {
    int id_medico = *(int *)args;

    while (1) {
        // Verificar si hay pacientes en la fila del médico
        if (sem_trywait(&sem_fila_medicos[id_medico]) == 0) {
            sem_post(&sem_fila_medicos[id_medico]);
            sem_wait(&sem_atencion_medicos[id_medico]);
        } else {
            // Verificar si hay pacientes VIP
            if (sem_trywait(&sem_fila_vip) == 0) {
                sem_post(&sem_fila_vip);
                sem_wait(&sem_atencion_medicos[id_medico]);
            } else {
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

    pthread_exit(NULL);
}

// Función para el hilo de cajero
void *cajero(void *args) {
    int id_cajero = *(int *)args;

    while (1) {
        // Verificar si hay pacientes en la fila
        if (sem_trywait(&sem_fila_cajeros) == 0) {
            sem_post(&sem_fila_cajeros);
            sem_wait(&sem_atencion_cajeros[id_cajero]);
        } else {
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

    pthread_exit(NULL);
}

// Manejador de la señal SIGTERM
void manejador_sigterm(int sig) {
    printf("Recibida señal SIGTERM. Terminando la ejecución.\n");
    exit(0);
}

int main() {
    // Inicializar los semáforos
    sem_init(&sem_puerta_entrada, 0, 1);
    sem_init(&sem_puerta_salida, 0, 1);
    sem_init(&sem_pacientes_espera, 0, NUM_PACIENTES); 
    sem_init(&sem_fila_vip, 0, 0); 
    sem_init(&sem_fila_cajeros, 0, 0); 
     sem_init(&mutex, 0, 0); 

    for (int i = 0; i < NUM_MEDICOS; i++) {
        sem_init(&sem_fila_medicos[i], 0, 0);
        sem_init(&sem_atencion_medicos[i], 0, 1);
    }

    for (int i = 0; i < NUM_CAJEROS; i++) {
        sem_init(&sem_atencion_cajeros[i], 0, 1);
    }

    // Crear los hilos de médicos
    pthread_t hilos_medicos[NUM_MEDICOS];
    int id_medicos[NUM_MEDICOS];
    for (int i = 0; i < NUM_MEDICOS; i++) {
        id_medicos[i] = i;
        pthread_create(&hilos_medicos[i], NULL, medico, &id_medicos[i]);
    }

    // Crear los hilos de cajeros
    pthread_t hilos_cajeros[NUM_CAJEROS];
    int id_cajeros[NUM_CAJEROS];
    for (int i = 0; i < NUM_CAJEROS; i++) {
        id_cajeros[i] = i;
        pthread_create(&hilos_cajeros[i], NULL, cajero, &id_cajeros[i]);
    }

    // Crear los hilos de pacientes (ejemplo)
    pthread_t hilos_pacientes[3];
    Paciente pacientes[3];
    for (int i = 0; i < 10; i++) {
        pacientes[i].id = i;
        pacientes[i].tipo = rand() % 2; // Asignar tipo de paciente aleatoriamente
        pacientes[i].medico_asignado = rand() % NUM_MEDICOS; // Asignar médico aleatoriamente
        pthread_create(&hilos_pacientes[i], NULL, paciente, &pacientes[i]);
    }

    // Manejar la señal SIGTERM
    signal(SIGTERM, manejador_sigterm);

    // Esperar a que los hilos terminen
    for (int i = 0; i < 10; i++) {
        pthread_join(hilos_pacientes[i], NULL);
    }

    // Imprimir estadísticas al final
    printf("\n----- Estadísticas -----\n");
    printf("Pacientes atendidos en total: %d\n", pacientes_atendidos_total);
    for (int i = 0; i < NUM_MEDICOS; i++) {
        printf("Pacientes atendidos por el médico %d: %d\n", i, pacientes_atendidos_medico[i]);
    }

    // Destruir los semáforos
    sem_destroy(&sem_puerta_entrada);
    sem_destroy(&sem_puerta_salida);
    sem_destroy(&sem_pacientes_espera);
    sem_destroy(&sem_fila_vip);
    sem_destroy(&sem_fila_cajeros);
    for (int i = 0; i < NUM_MEDICOS; i++) {
        sem_destroy(&sem_fila_medicos[i]);
        sem_destroy(&sem_atencion_medicos[i]);
    }
    for (int i = 0; i < NUM_CAJEROS; i++) {
        sem_destroy(&sem_atencion_cajeros[i]);
    }

    return 0;
}