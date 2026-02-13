# 🌦️ Station Météo Embarquée - Projet Système embarqué (WWW)

Ce projet consiste en la conception et le développement du logiciel embarqué pour une station météo maritime autonome. Commanditée par l'Agence Internationale pour la Vigilance Météorologique (AIVM), cette station collecte des données environnementales pour aider à la prévention des catastrophes naturelles.

---

## 🏗️ Architecture du Projet

Le code est structuré de manière modulaire pour séparer la logique métier des drivers matériels.

### Structure des fichiers
- **`main.c`** : Point d'entrée et boucle principale (Machine à états).
- **`config.h`** : Constantes globales (Pins, Timeouts).
- **`structures.h`** : Définitions des structures de données.
- **`drivers/`** : Gestion bas niveau (LED, Capteurs, GPS, SD).
- **`modes/`** : Logique métier (Standard, Config, Eco, Maintenance).

---

## ⚙️ Modes de Fonctionnement

| Mode | LED d'État | Description | Accès |
| :--- | :--- | :--- | :--- |
| **Standard** | 🟢 Verte | Acquisition périodique (10 min) et stockage SD. | Démarrage par défaut. |
| **Configuration** | 🟡 Jaune | Modification des seuils via Port Série. | Démarrage avec **Bouton Rouge**. |
| **Économique** | 🔵 Bleue | Intervalle doublé, GPS 1x sur 2. | Appui **5s Bouton Vert**. |
| **Maintenance** | 🟠 Orange | Lecture capteurs en direct, arrêt SD. | Appui **5s Bouton Rouge**. |

---

## 🚨 Codes d'Erreur (LED)

| Code Couleur (Clignotement 1Hz) | Signification |
| :--- | :--- |
| 🔴 / 🔵 | **Erreur Horloge RTC** |
| 🔴 / 🟢 | **Erreur GPS** ou **Capteur** |
| 🔴 / 🟡 | **Données Incohérentes** |
| 🔴 / ⚪ | **Carte SD Pleine** |
| 🔴 / ⚪ (Lent) | **Erreur Écriture SD** |

---


