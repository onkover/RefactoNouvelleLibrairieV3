@echo off
rem Lance le simulateur V3 et ecrit les predictions dans v3sim_resultats.txt
rem Arguments : OBJ, cameras, largeur, hauteur d'une vue, fins de pieces (A..F, indices de faces cumules)
rem Les images Bench_Cam_*.ppm (profondeur, magenta = pixel non couvert) sont ecrites dans le dossier courant.
v3sim.exe level_fps.obj cameras.txt 768 450 196,4898,14114,61606,64270,64934 > v3sim_resultats.txt
type v3sim_resultats.txt
pause
 