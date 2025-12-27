# Système d’éclairage intelligent (ESP32 - MQTT - Node-RED)

## Description
Projet IoT pour contrôler un système d’éclairage intelligent
en utilisant ESP32, MQTT et Node-RED.

## Technologies
- ESP32
- MQTT (Mosquitto)
- Node-RED
- Dashboard Node-RED

## Architecture
![Architecture](docs/architecture.jpej)

## Fonctionnement
1. ESP32 publie les données via MQTT
2. Node-RED reçoit et traite les messages
3. Dashboard permet le contrôle à distance
