# 🌦️ Station Worldwide Weather Watcher (Projet WWW)

> **Prototype de système embarqué pour l'acquisition de données environnementales en milieu maritime.**

Ce projet, réalisé dans le cadre du cursus ingénieur (CESI), répond à la demande de l'Agence Internationale pour la Vigilance Météorologique (AIVM). L'objectif est de fournir une solution robuste, modulaire et économe en énergie pour collecter des données (Température, Pression, Humidité, Luminosité, GPS) afin de prévenir les risques naturels.

---

## 🏗️ Architecture Logicielle

Le projet repose sur une architecture en **C** modulaire, utilisant une **Machine à États Finis** (FSM) pour orchestrer les différents modes de fonctionnement. Cette approche garantit la stabilité du système et facilite la maintenance.

### Structure du Projet
Le code est organisé pour séparer clairement la logique métier (Modes) de la gestion matérielle (Drivers).

```text
/Projet_Meteo.ino
├── En-tête & Déclarations                
│   ├── Inclusions                        # Bibliothèques externes (DHT, SD, RTClib, MicroNMEA...)
│   ├── 1. Config. Matérielle             # Définitions des broches (PIN_CLK, PIN_DHT, etc.)
│   └── 2. États & Variables              # Structures de données (Enums Modes/LEDs) et variables globales
│
├── Couche HAL
│   ├── 3. Interruptions (ISR)            # isr_red(), isr_green() : Détection asynchrone des boutons
│   ├── 4. Driver LEDs                    # led_update() : Traduction des états/erreurs en couleurs
│   ├── 5. Système de Fichiers (SD)       # log_data_to_sd(), archiveFileIfNeeded() : Gestion FAT et révisions
│   ├── 6. Driver Capteurs unifié         # check_sensors() : Acquisition DHT11, Lux, RTC, GPS et gestion erreurs
│   └── 7. Logique Boutons                # read_buttons() : Traitement temporel (appuis longs, doubles clics)
│
├── Couche MÉTIER 
│   └── 8. Logique des Modes & Commandes
│       ├── process_command()             # Parseur UART pour modifier les paramètres EEPROM
│       ├── mode_standard_run()           # Mode par défaut : Acquisition périodique
│       ├── mode_config_run()             # Mode Configuration : Interface série et timeout (30 min)
│       ├── mode_eco_run()                # Mode Économique : Intervalle d'acquisition doublé
│       └── mode_maintenance_run()        # Mode Maintenance : Arrêt SD et affichage LCD en temps réel
│
└── Point d'Entrée 
    └── 9. Setup & Loop
        ├── setup()                       # Initialisation (Serial, Pins, Interruption, LCD, RTC, SD)
        └── loop()                        # Machine à états principale (Switch Case sur currentMode)
```

---

## ⚙️ Modes de Fonctionnement

Le système intègre 4 modes distincts, accessibles via interactions physiques (Boutons) et identifiables via la LED d'état.

| Mode | LED d'État | Description | Condition d'accès |
| :--- | :--- | :--- | :--- |
| **Standard** | 🟢 Verte | **Mode nominal.** Acquisition des données toutes les 10 min (défaut) et enregistrement sur carte SD. | Démarrage normal (aucun bouton pressé). |
| **Configuration** | 🟡 Jaune | **Mode paramétrage.** Modification des seuils via le terminal. Timeout inactivité : 30 min ou **double clic Bouton Rouge** retour en mode standard| Double clic **Bouton Rouge** (depuis Standard). |
| **Économique** | 🔵 Bleue | **Mode éco.** Intervalle d'acquisition doublé. GPS activé 1 cycle sur 2 pour économiser la batterie. | Appui **5s** sur **Bouton Vert** (depuis Standard). |
| **Maintenance** | 🟠 Orange | **Mode technique.** Lecture des données en direct sur le port série. Arrêt de l'écriture SD pour retrait sécurisé . | Appui **5s** sur **Bouton Rouge** (depuis Standard ou Éco). |

---

## 💻 Interfaces Utilisateur (Port Série)

Le système communique via l'UART (Vitesse : 9600 bauds) pour le mode maintenance.

### 1. Interface Mode Configuration
Les paramètres sont stockés en mémoire non volatile (EEPROM). Voici les commandes disponibles :

| Commande | Description | Exemple |
| :--- | :--- | :--- |
| `LOG_INTERVALL=X` | Définit l'intervalle entre 2 mesures (min). | `LOG_INTERVALL=15` |
| `FILE_MAX_SIZE=X` | Taille max d'un fichier log avant archivage (octets). | `FILE_MAX_SIZE=4096` |
| `TIMEOUT=X` | Temps max d'attente d'un capteur (sec). | `TIMEOUT=30` |
| `LUMIN=1` / `0` | Active/Désactive le capteur de luminosité. | `LUMIN=1` |
| `RESET` | Réinitialise tous les paramètres aux valeurs d'usine. | `RESET` |
| `VERSION` | Affiche la version du firmware et le lot. | `VERSION` |

### 2. Interface Mode Maintenance
Ce mode affiche les données brutes en temps réel pour vérifier le bon fonctionnement des capteurs sans remplir la carte SD.

**Exemple de sortie console :**
```text
--- MODE MAINTENANCE ACTIF ---
# SD CARD: STOP (Safe to remove)
# Streaming données capteurs...
 T:22.4C | P:1013hPa | H:45% | Lum:High | GPS:48.85,2.35 T:22.5C | P:1013hPa | H:45% | Lum:High | GPS:48.85,2.35
```

---

## 🚨 Codes d'Erreurs et Diagnostic LED

Le système utilise la LED bicolore pour signaler les anomalies matérielles via des séquences de clignotement spécifiques.

| Séquence (Couleur & Fréquence) | Signification de l'erreur | Action requise |
| :--- | :--- | :--- |
| 🔴 / 🔵 (1 Hz) | **Erreur Horloge RTC** | Vérifier la pile ou la connexion I2C du module RTC. |
| 🔴 / 🟡 (1 Hz) | **Erreur GPS** / Données Incohérentes | Accès aux données satellites refusé ou valeurs hors seuils. |
| 🔴 / 🟢 (1 Hz) | **Erreur Capteur** | Un capteur (Temp/Press/Hygro) ne répond pas (Timeout). |
| 🔴 / ⚪ (1 Hz) | **Carte SD Pleine** | La carte est pleine, archivage impossible. |
| 🔴 / ⚪ (Lent - 2s blanc) | **Erreur Écriture SD** | Carte absente, mal insérée ou corrompue. |

---

## 📂 Gestion des Fichiers (Logs)

Les données sont stockées au format CSV dans des fichiers journaliers.

* **Format du nom :** `AAMMJJ_R.LOG` (Année, Mois, Jour, Révision).
    * Exemple : `200531_0.LOG`
* **Rotation :** Le système écrit toujours dans la révision courante. Si la taille dépasse `FILE_MAX_SIZE` (défaut 2ko ou 4ko), une nouvelle révision est créée (`_0.LOG` -> `_1.LOG`).

---

### Auteurs
**Groupe A2 - CPI A2 S3E**
* WHARTON Néïssa
* LUGAND Pierre
* SALHI Ilès
* SIDIBE Moussa Mamadou
