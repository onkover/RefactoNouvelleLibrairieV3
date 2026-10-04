# LibraryV3 — Plan à suivre, phase G

*État au 4 octobre 2026*

## Point de départ

Le chantier LOD des rochers est **terminé** (étapes 1 à 4d). Il est résumé dans l'annexe A14 (PDF, leçon complète).

**Référence I** — i7-1265U épinglé (`affinity=4`), ceinture, 4 vues, 1536 × 900, `Release` :

| Grandeur | Valeur |
|---|---|
| Render | 1,67 à 1,69 ms |
| Frame | 3,87 ms (≈ 258 fps) |
| Coût par face soumise | ≈ 20 ns |

Répartition de la frame : Render ≈ 43 %, `Clear` ≈ 1,08 ms, `Present` ≈ 0,9 ms (hors budget moteur).

---

## Étape 1 — LOD des sphères (LOD optionnel pour tout mesh)

**Exigence :** tout mesh doit pouvoir avoir ou non une chaîne de LOD, car d'autres objets seront ajoutés plus tard.

Côté moteur, l'exigence est déjà satisfaite : tout mesh est une chaîne, décrite par un `.lod.json` ou implicite (longueur 1) pour un simple `.obj`. La scène décide seule. Ce qui reste concerne la **fabrication des niveaux**.

**Démarrage :**

1. Examiner `sphere_lo`, `sphere_mid` et `sphere_hi`.
2. Mesurer leurs ε, avec la méthode validée sur les rochers (directions ciblées sur les sommets + 100 000 aléatoires, convergence vérifiée, × 1,1 arrondi vers le haut).

**Deux questions à trancher avant tout code :**

- **Méthode de fabrication** pour un mesh qui ne vient d'aucune fonction (vaisseau modélisé, mesh importé) : simplificateur générique de type QEM, exécuté hors ligne par Claude, validé sur les sphères en le comparant au ré-échantillonnage exact.
- **Droit de monter** : une chaîne peut-elle dépasser la résolution choisie par l'auteur de la scène ? (Exemple : Mercure est aujourd'hui en `sphere_mid` ; avec une chaîne, vue de près, elle pourrait passer en `sphere_hi`. L'image deviendrait meilleure mais changerait, et la comparaison « à image identique » avec la Référence I ne tiendrait plus.)

**Cible :** les planètes et lunes restées en L0 portent 88 % des faces encore soumises.

**Fichiers à fournir :** liens bruts des trois `.obj` de sphères, ou confirmation qu'ils sont générés par `gen_meshes.py`.

---

## Étape 2 — `Clear`

Coût limité par la mémoire, indépendant du nombre de faces : aucun LOD ne le réduira. Il pèse désormais presque autant que tout le rendu.

---

## Étape 3 — Les dettes notées

### Serializer

- **Clés optionnelles :** `JsonReader::Read` émet un avertissement « clé absente » pour chaque clé optionnelle non définie (par exemple `depthDisplayRange`, `lodTolerancePx` sur chaque caméra). Correction envisagée : tester `Has()` avant de lire. Décision qui concerne tout le Serializer.
- **Surcharges caméra :** `depthDisplayRange` et `lodTolerancePx` sont testées avec `> 0.0f` dans `BuildViewData` ; une valeur invalide (NaN, négative) écrite dans la scène retombe en silence sur `EngineConfig`. À revoir avec le point précédent.

### ResourceManager

- **Cache de `LoadMeshChecked`** indexé par le chemin seul, sans les `OBJLoadOptions` : charger deux fois le même `.obj` avec des options différentes rend toujours la première version, en silence. Contourné aujourd'hui (la scène et le chargeur de chaînes reçoivent les mêmes options), mais la faiblesse reste.
