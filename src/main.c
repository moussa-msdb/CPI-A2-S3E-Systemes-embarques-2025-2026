VARIABLE GLOBALE Mode_Actuel = STANDARD
VARIABLE GLOBALE Parametres = Charger_Depuis_EEPROM()

FONCTION Setup()
    Initialiser_Drivers(LED, Boutons, SD, Capteurs, GPS, RTC)
    
    SI Bouton_Rouge est PRESSÉ ALORS
        Mode_Actuel = CONFIGURATION
    SINON
        Mode_Actuel = STANDARD
    FIN SI
FIN FONCTION

FONCTION Loop()
    // Mise à jour continue des tâches de fond (Non-bloquant)
    Mettre_A_Jour_LED_Clignotement()

    // Machine à États Principale
    SELON Mode_Actuel FAIRE
        CAS STANDARD:
            Executer_Mode_Standard()
        CAS CONFIGURATION:
            Executer_Mode_Config()
        CAS ECONOMIQUE:
            Executer_Mode_Eco()
        CAS MAINTENANCE:
            Executer_Mode_Maintenance()
    FIN SELON
FIN FONCTION
