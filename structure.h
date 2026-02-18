STRUCTURE Configuration_Systeme
    Intervalle_Logs : Entier (Défaut 10 min)
    Taille_Max_Fichier : Entier (Défaut 2048 octets)
    Timeout_Capteur : Entier (Défaut 30s)
    Seuils_Capteurs : (Min/Max pour Temp, Lum, etc.)
FIN STRUCTURE

ENUM Mode_Systeme
    STANDARD, CONFIGURATION, ECONOMIQUE, MAINTENANCE
FIN ENUM

ENUM Etat_LED
    VERTE_FIXE, JAUNE_FIXE, BLEUE_FIXE, ORANGE_FIXE,
    ERREUR_RTC, ERREUR_GPS, ERREUR_CAPTEUR, ERREUR_SD
FIN ENUM
