**je ne saisis pas l'intérêt du fichier V3sim.cpp. Que dois-je en faire ?**



Vous n’avez rien à en faire pour avancer. v3sim.cpp ne fait pas partie du moteur : il ne va ni dans LibraryV3 ni dans l’EXE.



À quoi il a servi. C’est le petit programme qui m’a donné tous les chiffres de l’artefact :



* faces soumises, avant, arrière et dessinées ;
* blocs de 8 pixels, pixels couverts ;
* triangles masqués ;
* tuiles 8×8 pour Abrash ;
* écart de la virgule fixe pour Giesen.



Il relit level\_fps.obj et refait le pipeline du moteur en version simplifiée : projection, clip near, test de face avant (aire négative), boîte, couverture, test de profondeur. Pour chaque caméra, il compte ce que le moteur devrait compter. Ce sont donc les « prédictions écrites avant la mesure ».



Pourquoi je vous l’ai donné. Pour la traçabilité : une prédiction dont on ne peut pas refaire le calcul ne vaut pas grand-chose. Il servira dans deux cas.



1. Le run R1 ne tombe pas dans les fourchettes. Supposons que le moteur donne 41 000 faces avant au lieu de 53 964 pour la caméra Game. Le simulateur sert alors de référence indépendante. Soit le moteur a un bug (winding, culling, clip), soit c’est ma simulation qui se trompe. On compare compteur par compteur, caméra par caméra, pour savoir lequel.

2\.  Vous modifiez la scène (taille des cellules, position des caméras, nouvelle pièce). Il faut alors recalculer les prédictions avant de mesurer, sinon la règle « prédire avant de mesurer » n’est plus respectée.





Si vous voulez l’utiliser un jour. Il se compile comme un projet console C++ séparé, sans dépendance. Il prend quatre choses en entrée :

* l’OBJ ;
* un fichier de caméras ;
* la taille du viewport ;
* les indices de fin de chaque pièce dans l’OBJ.



Je ne vous ai pas envoyé le fichier de caméras (cameras.txt). Je peux le faire, avec la ligne de commande exacte, si vous le voulez.



Ma recommandation : rangez-le dans un dossier Outils/ à côté du générateur, ou ignorez-le. On n’y reviendra que si R1 diverge.





**oui envoie moi les fichiers qui manquent. Même s'ils ne servent pas au moteur, c'est une entrée à ce fichier pour qu'il fonctionne**



Je vous ai envoyé les quatre fichiers qui manquaient pour faire tourner v3sim.cpp. J’ai relancé le simulateur avec ces entrées : il redonne exactement les chiffres de l’artefact.



* cameras.txt : les 4 caméras du bench. Une ligne par caméra : nom, x, y, z, lacet en degrés (0 = regarde −z).
* roomend.txt : les indices de fin de chaque pièce dans level\_fps.obj, en faces cumulées de A à F (196, 4898, 14114, 61606, 64270, 64934). Ils permettent de ventiler les compteurs par pièce. Mon ancien fichier de travail portait des valeurs périmées (66 250 faces, une version antérieure du niveau). La version envoyée est corrigée.
* run\_v3sim.bat : lance v3sim.exe avec les bons arguments et écrit v3sim\_resultats.txt. Il écrit aussi une image de profondeur .ppm par caméra, où le magenta marque un pixel non couvert.
* v3sim\_reference.txt : la sortie de référence. Si vous relancez le simulateur sans rien changer, vous devez retrouver ce fichier à l’identique.



Pour l’utiliser :



* Compilez v3sim.cpp en projet console séparé (C++17, sans dépendance) pour obtenir v3sim.exe.
* Mettez dans le même dossier v3sim.exe, level\_fps.obj, cameras.txt et run\_v3sim.bat.
* Double-cliquez sur le .bat.





Si vous modifiez la scène avec le générateur, les fins de pièces changent. Il faudra alors me redonner les nouveaux comptes pour mettre roomend.txt à jour.

