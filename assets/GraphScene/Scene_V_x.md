Les scènes bench_V_* ne sont pas faites pour jouer. Ce sont des instruments de mesure, sur le même modèle que bench_V_V1 et bench_V_V2, qui n’ont pas non plus de contrôle. Il y a trois raisons à cela.

1. Chaque frame doit être identique. Le bench lance 300 frames et en tire un temps moyen. Si la caméra bouge, le nombre de triangles visibles, de pixels couverts et de blocs change à chaque frame. Le temps mesuré mélange alors le coût du moteur avec l’endroit où vous regardiez. Une différence de 3 % entre deux binaires deviendrait invisible dans ce bruit.
2. Les prédictions sont faites pour un point de vue précis. Les chiffres de l’artefact (53 964 faces avant pour Game, etc.) ne valent que pour la caméra en (0 ; 1,7 ; 3) qui regarde vers −z. Le run R1 vérifie que le moteur retrouve exactement ces compteurs. Avec une caméra mobile, il n’y aurait rien de fixe à comparer.
3. Les comparaisons A/B en dépendent. Pour juger Giesen ou Abrash, on lance le binaire A puis le binaire B dans l’ordre A B B A, sur la même image. Toute différence de temps doit venir du code, pas de la scène.

Les quatre caméras ont été placées pour couvrir des cas différents :

|Caméra  | 	Ce qu’elle mesure                                                       
| :---   |    :----:   |
|Game    |	Le spawn : très grands triangles de près, et la profondeur du niveau à travers les portails |   
|Dbg1    |	Le même endroit décalé de 3 m : beaucoup de triangles en biais, le clip near                |   
|Dbg2    |	Le milieu de la galerie C : triangles moyens, quatre pièces rejetées d’un bloc              |   
|Dbg3    |	Le fond de D, retournée vers l’entrée : beaucoup de triangles de 1 à 4 pixels               |   

## L’objectif de l’étape V3

Le jalon D consiste à choisir s’il faut réécrire le parcours des arêtes du rasterizer, avec Giesen, avec Abrash, ou pas du tout. C’est un gros chantier. Les scènes V1 et V2 ne suffisent pas pour décider :

V1 contient surtout de petits objets ;
V2 est une seule sphère en gros plan.

Aucune des deux ne ressemble à un vrai niveau, avec des murs immenses devant soi, des centaines de petits triangles au fond, des portails et de l’occlusion.

V3 fournit ce cas réaliste et figé. La démarche est la suivante :

On écrit les prédictions.
On mesure.
La règle de décision (étape 10 de l’artefact) dit si Giesen ou Abrash rapporte au moins 5 %.

La simulation a déjà fait apparaître un problème plus urgent que le jalon D : un niveau chargé en un seul mesh passe entièrement par le chemin Intersect, qui n’est pas vectorisé. C’est précisément ce que la scène bench_V_V3_rooms.json sert à mesurer.

## Pour naviguer dans le niveau : une troisième scène, d’exploration

Le Serializer sait déjà lire un composant CameraFPS. Il n’y a donc rien à changer dans le moteur, seulement dans un JSON. Ne modifiez pas les deux scènes de banc. Faites une copie.

Fichier : copiez assets/GraphScene/bench_V_V3.json sous le nom assets/GraphScene/explore_V3.json.

Nœud Bench_Cam_Game, bloc components :

Avant :
```
json
"components": {
  "Transform": {
    "translation": [ 0.0, 1.7, 3.0 ],
    "rotation": [ 0, 0.0, 0 ],
    "scale": [ 1.0, 1.0, 1.0 ]
  },
  "Camera": { ... inchangé ... }
}
```

Après :
```
json
"components": {
  "Transform": {
    "translation": [ 0.0, 1.7, 3.0 ],
    "rotation": [ 0, 0.0, 0 ],
    "scale": [ 1.0, 1.0, 1.0 ]
  },
  "Camera": { ... inchangé ... },
  "CameraFPS": {
    "enabled": true,
    "moveSpeed": 5.0,
    "mouseSensitivity": 0.15,
    "lockVertical": true,
    "pitchLimit": 89.0,
    "sprintMultiplier": 3.0
  }
}
```
Ce sont exactement les six clés que lit ParseCameraFPS. Avec lockVertical: true, vous restez à hauteur d’œil. Les angles initiaux sont repris du Transform, donc la caméra ne saute pas à la première frame.

Lancez-la sans --bench ni --csv, puisque ses temps ne sont pas comparables :
```
--scene=explore_V3.json
```

Comme il n’y a pas de collision, vous pouvez traverser les murs. Si vous sortez d’une pièce, ses murs disparaissent. C’est normal : vus de l’extérieur, ce sont des faces arrière, puisque les coques sont enroulées vers l’intérieur (étape 2 de l’artefact). Vous pouvez aussi faire la même copie avec bench_V_V3_rooms.json pour voir, en Debug, les classes Inside/Intersect changer quand vous passez d’une pièce à l’autre.

Il me manque toujours la taille réelle d’une vue en quad (568×400 d’après votre config.json ?) pour corriger les prédictions avant le run R1.