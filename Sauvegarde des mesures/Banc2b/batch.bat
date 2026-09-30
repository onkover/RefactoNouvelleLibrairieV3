   set EXE_A=C:\Banc2b\A\RefactoNouvelleLibrairieV3.exe
   set EXE_B=C:\Banc2b\B\RefactoNouvelleLibrairieV3.exe

   
start "" /wait /affinity 4 "%EXE_B%" --scene=solar_system_v1compat.json --bench=300 --csv=Mesures/jetable.csv
start "" /wait /affinity 4 "%EXE_A%" --scene=solar_system_v1compat.json --bench=300 --csv=Mesures/2B_A1.csv
start "" /wait /affinity 4 "%EXE_B%" --scene=solar_system_v1compat.json --bench=300 --csv=Mesures/2b_B1.csv
start "" /wait /affinity 4 "%EXE_B%" --scene=solar_system_v1compat.json --bench=300 --csv=Mesures/2b_B2.csv
start "" /wait /affinity 4 "%EXE_A%" --scene=solar_system_v1compat.json --bench=300 --csv=Mesures/2b_A2.csv