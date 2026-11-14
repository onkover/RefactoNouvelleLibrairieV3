# Annexe A8 — Le drapeau `dirty` volé par l'affichage de debug

> Annexe transversale, rattachée au contrat du drapeau `m_dirty` (Annexe A5, Règle R11) et au couple `LocalTransformSystem` / `WorldTransformSystem` (Leçon 03 — ECS).
> Découverte en marge de l'Annexe A7 (zoom caméra) — **sans lien de cause avec elle**. Les deux chantiers ne se recouvrent pas ; ce document existe séparément pour que la recherche de l'un ne pollue pas l'historique de l'autre.

---

## 1. Le symptôme

Plus aucun objet visible dans la vue de droite (`Overview_Camera`, perspective), hormis le gizmo. En orthographique (`Top_Camera`), les objets restaient visibles mais dans un cadrage incohérent (« lointain »). La vue de gauche (`FPS_Camera`), pilotée, restait parfaitement correcte.

## 2. Fausses pistes écartées

| Hypothèse | Verdict | Preuve |
|---|---|---|
| H1 — `wheelDelta` non remis à zéro | Écartée | Confirmé à zéro par Onky ; de toute façon `CameraZoomSystem` sort à sa première ligne si `wheelDelta == 0` |
| H2 — `Overview_Camera` porte un `FPSControllerComponent` par erreur | Écartée | JSON vérifié : aucun bloc `CameraFPS` sur `Overview_Camera` ni `Top_Camera` |
| H3 — régression localisée à la scène du L06 | Confirmée mais mal formulée | Le bug touche *toute* caméra sans contrôleur, dans *toute* scène — H3 n'était pas fausse, seulement trop étroite |

`CameraZoomSystem` (Annexe A7) est donc formellement innocentée : sa boucle ne peut matériellement pas s'exécuter dans les conditions du symptôme.

## 3. Le diagnostic

`Scene/System.cpp`, `DebugDisplaySystemRecursive` — la fonction qui produit l'arbre de debug consulté pour l'investigation :

```cpp
auto& transform = registry.getComponent<TransformComponent>(entity);
...
Vec3f worldPosition = Vec3f(transform.m_worldMatrix[3][0], ...);
Vec3f localPosition = Vec3f(transform.m_localMatrix[3][0], ...);

transform.m_dirty = false; // Reset dirty flag after displaying
```

Une fonction d'affichage — pure lecture par nature — mute `m_dirty`. Ce drapeau est un contrat exclusif entre `ParseTransform` (qui le lève, à la lecture du JSON) et `LocalTransformSystem` (seul autorisé à le baisser, et seulement après avoir réellement recalculé `m_localMatrix`). En le baissant sans avoir « cuisiné » la donnée, `DebugDisplaySystemRecursive` fait croire à `LocalTransformSystem` que le travail est déjà fait — qui passe alors indéfiniment sur cette entité (`if (!tr.m_dirty) continue;`).

## 4. Le mécanisme exact

1. `ParseTransform` lève `m_dirty = true` au chargement — correct, vérifié.
2. Si `DebugDisplaySystem` s'exécute avant que `LocalTransformSystem` n'ait eu l'occasion de composer la matrice pour cette entité, il consomme le drapeau à sa place. `m_localMatrix` reste à sa valeur d'initialisation par défaut du `TransformComponent` — position `(0,0,0)`, rotation identité.
3. `WorldTransformSystem` / `PropagateWorld` composent ensuite un monde correct... à partir d'un local resté faux. `m_worldMatrix` hérite du défaut.
4. **`FPS_Camera` (et toute entité contrôlée) est épargnée** : `CameraFPSControllerSystem` relève `tr.m_dirty = true` *inconditionnellement, chaque frame*, qu'il y ait mouvement ou non. Le vol est réparé soixante fois par seconde, sans que personne ne le remarque.
5. `Overview_Camera` et `Top_Camera` n'ont ni `CameraFPS` ni `CameraFollow` : rien ne relève jamais leur drapeau après le premier vol. Elles se figent à l'identité, définitivement.

## 5. La preuve dans les relevés d'Onky

Deux extraits de l'arbre de debug, pris à quelques instants d'écart. `Cube1`/`Cube2` (animés, redirtyés chaque frame par `AnimationSystem`) bougent très légèrement entre les deux relevés. `Overview_Camera` reste **exactement** à `(0, 0, 0)` dans les deux — la signature d'un état gelé une fois pour toutes, pas d'un calcul qui dérive.

## 6. Le correctif

```cpp
Vec3f worldPosition = Vec3f(transform.m_worldMatrix[3][0], transform.m_worldMatrix[3][1], transform.m_worldMatrix[3][2]);
Vec3f localPosition = Vec3f(transform.m_localMatrix[3][0], transform.m_localMatrix[3][1], transform.m_localMatrix[3][2]);

// SUPPRIMÉ : transform.m_dirty = false;
```

Une ligne retirée. Confirmé par Onky : `Overview_Camera` affiche `World(0, 0, 120)` dès la première frame après correction.

## 7. La règle généralisée

R11 (Annexe A5) énonçait la moitié du contrat : *« écrire dans la source sans lever le drapeau introduit un bug différé »*. Ce bug est le miroir exact de l'autre moitié, jamais énoncée jusqu'ici :

> **R11bis — Le drapeau `dirty` a deux écrivains, jamais un troisième.** Seul celui qui modifie `m_local` a le droit de lever `m_dirty`. Seul `LocalTransformSystem` — après avoir réellement recomposé `m_localMatrix` — a le droit de le baisser. Toute autre fonction, y compris une fonction de lecture pure comme un affichage de debug, n'a de droit sur ce champ que celui de le *lire*.

**Invariant proposé pour `CheckSceneInvariants`** (à ajouter, non encore implémenté) :

```cpp
// INVARIANT 5 : une entite sans controleur qui redirty chaque frame
// ne doit jamais montrer m_dirty == false alors que m_localMatrix
// ne correspond pas a m_local — signe qu'un tiers a vole le drapeau
// avant que LocalTransformSystem n'ait pu le consommer legitimement.
```

Le corps exact reste à écrire — comparer `m_localMatrix` à `m_local.ToLocalMatrix()` à chaque frame coûterait cher (on recalculerait ce que le drapeau existe justement pour éviter de recalculer) ; une piste moins coûteuse est un compteur de « frames depuis dernière composition réelle », incrémenté par `LocalTransformSystem` et jamais par quiconque d'autre.

## 8. Numérotation

Ce bug attend un numéro dans ton journal global — je n'ai pas de vue sur le dernier numéro attribué à travers l'ensemble de tes documents (le journal continue au moins jusqu'aux bugs 26/27 mentionnés en dette de L04 P2, et au moins quatre bugs supplémentaires ont été trouvés et corrigés pendant les tests v1compat du L06). À toi de l'insérer à sa place exacte.

---

*Annexe A8 — clôturée.*
