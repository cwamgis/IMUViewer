#include "tableaudebord.h"

TableauDeBord::TableauDeBord()
{
    _nbEch=0;
}

TableauDeBord::TableauDeBord(const char* fichiercsv)
{
    // Lecture du fichier des données centrale
    CSV donneesCentrale;
    //_fichiercsv="amjad_marche_cheville.out";
    double **donneesBrutes   = donneesCentrale.readCSV(fichiercsv);

    // Nombre de lignes du fichier
    _nbEch = donneesCentrale.getNbLines();

    // Cree un vecteur de signaux avec toutes les données et classifie le mouvement
    creeVecteurSignauxEtClassifie(donneesBrutes,freqFiltreGravite,freqEch);
    calculeFenetreCentrale();

    // Initialisation de l'indice de parcours du signal et des capteurs
    reInitialiseCapteursCentraleEtProgressionSignal();

    // Initialisation du QTime à maintenant
    setLastTimeToCurrentTime();
}


TableauDeBord::~TableauDeBord()
{
    if (_classif) delete _classif;
    // On vide le QVector de signaux

    for (int i=0;i<_signaux.size();i++)
    {
        delete _signaux[i];
    }
}


// Calcule la fenête d'évolution de la centrale en simulant son évolution
void TableauDeBord::calculeFenetreCentrale()
{
    double x,y,z=0.0;
    _coinInferieur.append(INFINITY);
    _coinInferieur.append(INFINITY);
    _coinInferieur.append(INFINITY);
    _coinSuperieur.append(-INFINITY);
    _coinSuperieur.append(-INFINITY);
    _coinSuperieur.append(-INFINITY);


    // Parcours de toutes les données
    for (int i=0;i<_nbEch-2;i++)
    {
        x = _signaux[9]->getSignalDoubleIntegre(i);
        y = _signaux[10]->getSignalDoubleIntegre(i);
        z = _signaux[11]->getSignalDoubleIntegre(i);

        if (x<_coinInferieur[0])_coinInferieur[0]=x;
        if (y<_coinInferieur[1])_coinInferieur[1]=y;
        if (z<_coinInferieur[2])_coinInferieur[2]=z;

        if (x>_coinSuperieur[0])_coinSuperieur[0]=x;
        if (y>_coinSuperieur[1])_coinSuperieur[1]=y;
        if (z>_coinSuperieur[2])_coinSuperieur[2]=z;
    }
}

QVector<double> TableauDeBord::getCoinInferieur()
{
    return _coinInferieur;
}

QVector<double> TableauDeBord::getCoinSuperieur()
{
    return _coinSuperieur;
}

void TableauDeBord::creeVecteurSignauxEtClassifie(double** donneesBrutes,  FrequencyType uneFreqFiltre, FrequencyType uneFreqEch)
{


    // Données de l'accéléro : récupération et suppression de l'effet de gravité
    // _signaux[0-1-2];
    QVector<Signal*> _signauxTmp;
    for (int i=2;i<=4;i++)
    {
        Signal *signalBrut = new Signal(donneesBrutes,_nbEch,0,i,tailleFenetreStats);
        signalBrut->regulariseEchantillonage(uneFreqEch);
        signalBrut->calculStats();

        Signal gravite(*signalBrut);
        gravite.passeBas(uneFreqFiltre,uneFreqEch,false);

        Signal *signalSansGravite = *signalBrut - gravite;
        _signaux.append(signalSansGravite);
        _signauxTmp.append(signalBrut);
    }

    // Classification
    _classif = new Classifieur(&_signauxTmp,tailleFenetreStats);
    _classif->classe();

    // Données du gyroscope : récupération et intégration
    // _signaux[3-4-5];
    for (int i=6;i<=8;i++)
    {
        Signal *signalBrut = new Signal(donneesBrutes,_nbEch,0,i,tailleFenetreStats);
        signalBrut->regulariseEchantillonage(uneFreqEch);
        signalBrut->integre();
        _signaux.append(signalBrut);
    }

    // Données du magnéto : récupération
    // _signaux[6-7-8];
    for (int i=10;i<=12;i++)
    {
        Signal *signalBrut = new Signal(donneesBrutes,_nbEch,0,i,tailleFenetreStats);
        signalBrut->regulariseEchantillonage(uneFreqEch);
        signalBrut->passeBas(uneFreqFiltre,uneFreqEch,false);
        _signaux.append(signalBrut);
    }

    // Données de l'accéléromètre : changement de repère (repère central => repère absolu)
    // et récupération des nouveaux signaux correspondants
    // _signaux[9-10-11];
    for (int i = 0;i<=2;i++)// Copie signaux
    {
        Signal *signalAccelero = new Signal(*_signaux[i]);
        _signaux.append(signalAccelero);
    }
    Signal::changeRepere(_signaux[9],_signaux[10],_signaux[11],*_signaux[3],*_signaux[4],*_signaux[5]);

    // ON double intègre l'accéléro repère absolu
    for (int i = 9;i<=11;i++)_signaux[i]->doubleIntegre();



   _nbEch=_signaux[0]->getTaille();
}

///////////////// Début modification à intégrer /////////////////

// Mise à jour des données du capteur de la centrale
void TableauDeBord::majCentrale()
{
    ////////////////// Incrementation iCourant //////////////////
    int pas = incrementeICourant();

    ////////////////// Réinitialisation des capteurs //////////////////
    if (iCourant==0)
    {
        reInitialiseCapteursCentraleEtProgressionSignal();

    }
    //////////////////Mise à jour des indicateurs de position/orientation absolues //////////////////
    else
    {
        // On cumule les valeurs qu'on a sauté...
        for (int i=iCourant;i>iCourant-pas;i--)
        {
            miseenplace(i);
        }
    }
    //////////////////Mise à jour des capteurs temps réel //////////////////

    // Acc
    _IMU._acc[0] = _signaux[0]->getSignal(iCourant);
    _IMU._acc[1] = _signaux[1]->getSignal(iCourant);
    _IMU._acc[2] = _signaux[2]->getSignal(iCourant);
    // Gyro
    _IMU._gyro[0]= _signaux[3]->getSignal(iCourant);
    _IMU._gyro[1]= _signaux[4]->getSignal(iCourant);
    _IMU._gyro[2]= _signaux[5]->getSignal(iCourant);
    // Magnétomètre
    _IMU._magn[0]=_signaux[6]->getSignal(iCourant);
    _IMU._magn[1]=_signaux[7]->getSignal(iCourant);
    _IMU._magn[2]=_signaux[8]->getSignal(iCourant);

}


/// Incremente iCourant et renvoie le pas utilisé
/// Réinitialise également _lastTime
/// Renvoie -1 si iCourant a été réinitialisé à zéro
int TableauDeBord::incrementeICourant()
{
    QTime maintenant = QTime::currentTime();
    // Nb de ms depuis le dernier tour
    int mSecs = abs(maintenant.msecsTo(_lastTime));


    // période d'échantillonage en ms
    double periodeEchantillonage = 1000/freqEch;


    // Incrémentation du pas = division entière (mSecs / période en mS) arrondie à l'entier le plus pres
    int pas = round((mSecs / periodeEchantillonage));

    if ((iCourant+pas) < (_signaux.at(0)->getTaille()))
    {
        iCourant+=pas;
        // Décalage à rajouter à maintenant pour tomber sur le "maintenant" du signal échantilloné
        //(légèrement supérieur ou inférieur)
        _lastTime = _lastTime.addMSecs(pas*periodeEchantillonage);
        return pas;
    }
    else
    {
        iCourant = 0;
        setLastTimeToCurrentTime();
        return -1;
    }
}


QVector<Signal*> TableauDeBord::get_signaux()
{
    return this->_signaux;
}

int TableauDeBord::getiCourant()
{
    return this->iCourant;
}

void TableauDeBord::setiCourant(int unI)
{

    reInitialiseCapteursCentraleEtProgressionSignal();
    this->iCourant = unI;
    // Rejoue le parcours du signal jusqu'au nouveau iCourant



    for (int i=0;i<iCourant;i++)
    {
       miseenplace(i);

    }

}

int TableauDeBord::getnbEch()
{

    return this->_nbEch;
}

void TableauDeBord::setLastTimeToCurrentTime()
{
    _lastTime = QTime::currentTime();
}


// Réinitialise tous les capteurs ainsi que iCourant
void TableauDeBord::reInitialiseCapteursCentraleEtProgressionSignal()
{

    iCourant = 0;
    // Orientation depuis le gyro
    _IMU._orientation[0]=0;
    _IMU._orientation[1]=0;
    _IMU._orientation[2]=0;

    // Postion depuis l'acceleromètre
    _IMU._position[0]=0;
    _IMU._position[1]=0;
    _IMU._position[2]=0;

    // On vide la trajectoire
    _IMU._trajectoire.clear();

}
void TableauDeBord::miseenplace(int i)
{
    // Orientation : cumul des angles obtenus par intégration du signal gyro
    double angleX = _signaux[3]->getSignalIntegre(i);
    double angleY = _signaux[4]->getSignalIntegre(i);
    double angleZ = _signaux[5]->getSignalIntegre(i);
    _IMU._orientation[0]= (_IMU._orientation[0]>=2*M_PI) ? angleX : angleX-2*M_PI;
    _IMU._orientation[1]= (_IMU._orientation[1]>=2*M_PI) ? angleY : angleY-2*M_PI;
    _IMU._orientation[2]= (_IMU._orientation[2]>=2*M_PI) ? angleZ : angleZ-2*M_PI;
    // Position depuis l'acceleromètre
    _IMU._position[0]= _signaux[9]->getSignalDoubleIntegre(i);
    _IMU._position[1]= _signaux[10]->getSignalDoubleIntegre(i);
    _IMU._position[2]= _signaux[11]->getSignalDoubleIntegre(i);
    // On ajoute le point courant de la centrale à la trajectoire
    _IMU._trajectoire.append(_IMU._position);

}

Classifieur* TableauDeBord::getClassifieur()
{
    return _classif;
}


