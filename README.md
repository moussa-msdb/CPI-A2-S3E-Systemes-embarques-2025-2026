# 🌦️ Station Météo Maritime Embarquée (Projet AIVM)

> **Prototype de système embarqué pour l'acquisition de données environnementales en milieu maritime.**

[cite_start]Ce projet, réalisé dans le cadre du cursus ingénieur (CESI) [cite: 1, 112][cite_start], répond à la demande de l'Agence Internationale pour la Vigilance Météorologique (AIVM)[cite: 122]. [cite_start]L'objectif est de fournir une solution robuste, modulaire et économe en énergie pour collecter des données (Température, Pression, Humidité, Luminosité, GPS) afin de prévenir les risques naturels[cite: 123].

---

## 🏗️ Architecture Logicielle

[cite_start]Le projet repose sur une architecture en **C** modulaire, utilisant une **Machine à États Finis** (FSM) pour orchestrer les différents modes de fonctionnement[cite: 4, 126]. Cette approche garantit la stabilité du système et facilite la maintenance.

### Structure du Projet
Le code est organisé pour séparer clairement la logique métier (Modes) de la gestion matérielle (Drivers).

```text
/Projet_Meteo
├── src/
│   ├── main.c                 # Point d'entrée : Initialisation et Boucle principale (Switch Case)
│   ├── config.h               # Configuration matérielle (Pins) et constantes (Timeouts)
│   ├── structures.h           # Définitions des structures de données (Logs, Settings, Enums)
│   │
│   ├── drivers/               # COUCHE HAL (Hardware Abstraction Layer)
│   │   ├── led.c / .h         # Gestion asynchrone des LEDs (États et erreurs)
│   │   ├── buttons.c / .h     # Gestion des appuis longs (5s) et courts
│   │   ├── sensors.c / .h     # Driver unifié pour capteurs (BME280 + Lux)
│   │   ├── gps.c / .h         # Driver UART pour module GPS (NMEA)
│   │   ├── rtc.c / .h         # Driver Horloge Temps Réel (DS3231)
│   │   └── sd_logger.c / .h   # Gestion du système de fichiers FAT (Logs et Révisions)
│   │
│   └── modes/                 # COUCHE MÉTIER (Logique des Diagrammes d'Activité)
│       ├── mode_standard.c    # Acquisition périodique & Enregistrement
│       ├── mode_config.c      # Interface Série pour paramétrage
│       ├── mode_eco.c         # Mode économie d'énergie (Intervalle x2)
│       └── mode_maint.c       # Mode maintenance (Lecture directe sans SD)
└── README.md
