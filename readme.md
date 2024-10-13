
# Centro médico VIP

# Traajo No 3 Ejercicio 3.1

## Mario Roman TRaversaro

El problema a modelar, se trata de un consultorio médico, con atención a pacientes VIP.

## Paciente

    Obtiene un no de fila y se dirige a la fila correspondiente.
    Espera su turno utilizando el semáforo del médico.
    Una vez atendido, se dirige a la fila de la caja.
    Espera su turno en la caja y abona.
    Libera un cupo en el centro médico.

## Médico

    Espera a que haya un paciente en su fila o en la fila VIP.
    Atiende al paciente.
    Cada tres pacientes, verifica si hay pacientes VIP esperando y los atiende.
    Si no hay pacientes, se duerme.

## Cajero

    Espera a que haya un paciente en la fila de la caja.
    Cobra al paciente.

Instrucciones

compilar con:
 gcc centroMedico.c -o centroMedico.exe -lpthread
Ejecutar:
./centroMedico.exe

Detener con:
CTRL + C
