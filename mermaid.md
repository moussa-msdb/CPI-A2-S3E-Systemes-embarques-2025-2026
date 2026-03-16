```mermaid
flowchart TD

    %% ==========================================
    %% DÉFINITION DES STYLES (COULEURS)
    %% ==========================================
    classDef titre fill:#2C3E50,color:#FFFFFF,font-weight:bold,font-size:16px,stroke:none;
    classDef start fill:#98FB98,stroke:#2ECC71,stroke-width:2px,color:#000000;
    
    %% Styles pour les LEDs et États
    classDef green fill:#2ECC71,color:#FFFFFF,stroke:none;
    classDef orange fill:#E67E22,color:#FFFFFF,stroke:none;
    classDef blue fill:#3498DB,color:#FFFFFF,stroke:none;
    classDef yellow fill:#F1C40F,color:#000000,stroke:none;
    
    %% Styles pour les Erreurs et Branches 'Non'
    classDef error fill:#E74C3C,color:#FFFFFF,stroke:none;
    classDef redGreen fill:#FFD700,color:#000000,stroke:#E74C3C,stroke-width:2px;
    classDef redBlue fill:#9B59B6,color:#FFFFFF,stroke:#E74C3C,stroke-width:2px;
    classDef redYellow fill:#FF8C00,color:#000000,stroke:#E74C3C,stroke-width:2px;
    classDef redWhite fill:#ECF0F1,color:#000000,stroke:#E74C3C,stroke-width:2px;

    %% ==========================================
    %% 1. DÉMARRAGE DU SYSTÈME
    %% ==========================================
    T1["1. DÉMARRAGE DU SYSTÈME"]:::titre ~~~ Dem_Start((Début)):::start
    
    Dem_Start --> Dem_L1[Démarrage du système en mode standard automatiquement]:::green

    %% ==========================================
    %% 2. MODE STANDARD
    %% ==========================================
    T2["2. MODE STANDARD"]:::titre ~~~ Std_Start((Début)):::start
    
    Std_Start --> Std_L1[LED verte allumée]:::green
    Std_L1 --> Std_Boucle(( ))
    
    Std_Boucle --> Std_VerifBtn{Action boutons ?}
    Std_VerifBtn -->|Appui 5s Vert| Std_MEco[Aller en Mode Économie]:::blue
    Std_VerifBtn -->|Appui 5s Rouge| Std_MMaint[Aller en Mode Maintenance]:::orange
    Std_VerifBtn -->|Double clic Rouge| Std_MConf[Aller en Mode Configuration]:::yellow
    
    Std_VerifBtn -->|Aucune action bouton| Std_S1[Recevoir le capteur de données]
    
    Std_S1 --> Std_C1{Capteur répond /<br>Données cohérentes ?}
    Std_C1 -->|Non| Std_Err1[Écrire NA dans le fichier<br>+ LED Rouge/Vert]:::redGreen --> Std_S2
    Std_C1 -->|Oui| Std_S2[Recevoir les données de l'horloge]
    
    Std_S2 --> Std_C2{Horloge accessible ?}
    Std_C2 -->|Non| Std_Err2[Erreur accès<br>+ LED Rouge/Bleu]:::redBlue --> Std_S3
    Std_C2 -->|Oui| Std_S3[Recevoir les données GPS]
    
    Std_S3 --> Std_C3{GPS accessible ?}
    Std_C3 -->|Non| Std_Err3[Erreur accès<br>+ LED Rouge/Jaune]:::redYellow --> Std_S4
    Std_C3 -->|Oui| Std_S4[Données enregistrées dans le fichier]
    
    Std_S4 --> Std_C4{Le fichier a atteint<br>FILE_MAX_SIZE ?}
    Std_C4 -->|Oui| Std_Arch[Archiver le fichier et ecrire dans un nouveau fichier]:::error --> Std_C5{Erreur écriture ou carte SD pleine ?}
    Std_C4 -->|Non| Std_C5:::error
    
    Std_C5 -->|Oui| Std_Err4[LED Rouge/Blanc]:::redWhite --> Std_Wait
    Std_C5 -->|Non| Std_Wait[Attendre LOG_INTERVALLE et recommencer le processus]
    
    

    %% ==========================================
    %% 3. MODE MAINTENANCE
    %% ==========================================
    T3["3. MODE MAINTENANCE"]:::titre ~~~ Maint_Start((Début)):::start
    
    Maint_Start --> Maint_L1[Allumer LED Orange]:::orange
    Maint_L1 --> Maint_Boucle(( ))
    
    Maint_Boucle --> Maint_A1[Arrêt d'enregistrement des données]
    Maint_A1 --> Maint_A2[Affichage des données sur le port série]
    Maint_A2 --> Maint_C1{Appuyer 5s sur le<br>bouton rouge ?}
    
    
    Maint_C1 -->|Oui| Maint_C2{Le mode précédent était<br>le mode standard ?}
    Maint_C1 -->|Non| Maint_A3[Rester en mode maintenance]:::error

    Maint_C2 -->|Oui| Maint_M1[Aller au Mode standard]:::green
    Maint_C2 -->|Non| Maint_M2[Aller au Mode économie]:::blue

    %% ==========================================
    %% 4. MODE CONFIGURATION
    %% ==========================================
    T4["4. MODE CONFIGURATION"]:::titre ~~~ Conf_Start((Début)):::start
    
    Conf_Start --> Conf_L1[Allumer LED jaune]:::yellow
    Conf_L1 --> Conf_Boucle(( ))
    
    Conf_Boucle --> Conf_C1{30 min d'inactivité ou LED verte appuyée ?}
    Conf_C1 -->|Oui| Conf_Fin[Retour en mode standard]:::green
    
    Conf_C1 -->|Non| Conf_C2{Réinitialiser le système ?}
    Conf_C2 -->|Oui| Conf_A2[Remise des paramètres à défaut] --> Conf_Merge(( ))
    
    Conf_C2 -->|Non| Conf_C3{Mettre à jour l'horloge ?}
    Conf_C3 -->|Oui| Conf_A3[Remise à l'heure de l'horloge] --> Conf_Merge
    
    Conf_C3 -->|Non| Conf_C4{Étalonner les seuils ?}
    Conf_C4 -->|Oui| Conf_A4[Calibrer les capteurs aux bons seuils] --> Conf_Merge
    
    Conf_C4 -->|Non| Conf_C5{Configurer l'appareil ?}
    Conf_C5 -->|Oui| Conf_A5[Paramétrer le système] --> Conf_Merge
    
    Conf_C5 -->|Non| Conf_Merge
    
    Conf_Merge --> Conf_Sauvegarde[Enregistrer les modifications<br>dans l'EEPROM]
    
    %% ==========================================
    %% 5. MODE ÉCONOMIE
    %% ==========================================
    T5["5. MODE ÉCONOMIE"]:::titre ~~~ Eco_Start((Début)):::start
    
    Eco_Start --> Eco_L1[Allumer LED bleue]:::blue
    Eco_L1 --> Eco_Boucle(( ))
    
    Eco_Boucle --> Eco_VerifBtn{Action boutons ?}
    Eco_VerifBtn -->|Appui 5s Vert| Eco_MStd[Aller en Mode Standard]:::green
    Eco_VerifBtn -->|Appui 5s Rouge| Eco_MMaint[Aller en Mode Maintenance]:::orange
    
    Eco_VerifBtn -->|Aucune action bouton| Eco_S1[Recevoir les données des capteurs]
    
    Eco_S1 --> Eco_C1{TIMEOUT dépassé ?}
    Eco_C1 -->|Oui| Eco_Err1[Recevoir 'NA'<br>+ LED Rouge/Vert]:::redGreen --> Eco_S2
    Eco_C1 -->|Non| Eco_S2[Recevoir les données de l'horloge RTC]
    
    Eco_S2 --> Eco_C2{Horloge accessible ?}
    Eco_C2 -->|Non| Eco_Err2[Erreur accès<br>+ LED Rouge/Bleu]:::redBlue --> Eco_CGPS
    Eco_C2 -->|Oui| Eco_CGPS{GPS déjà récupéré<br>au tour d'avant ?}
    
    Eco_CGPS -->|Non| Eco_S3[Recevoir les données du GPS]
    Eco_CGPS -->|Oui| Eco_S4
    
    Eco_S3 --> Eco_C3{GPS accessible ?}
    Eco_C3 -->|Non| Eco_Err3[Erreur accès<br>+ LED Rouge/Jaune]:::redYellow --> Eco_S4
    Eco_C3 -->|Oui| Eco_S4[Données enregistrées dans le fichier]
    
    Eco_S4 --> Eco_C4{Le fichier a atteint<br>FILE_MAX_SIZE ?}
    Eco_C4 -->|Oui| Eco_Arch[Archiver le fichier et ecrire dans un nouveau fichier]:::error --> Eco_C5{Erreur écriture ou carte SD pleine ?}
    Eco_C4 -->|Non| Eco_C5:::error

    Eco_C5 -->|Oui| Eco_Err4[LED Rouge/Blanc]:::redWhite --> Eco_Wait
    Eco_C5 -->|Non| Eco_Wait[Attendre 2x LOG_INTERVALLE et recommencer le processus]
