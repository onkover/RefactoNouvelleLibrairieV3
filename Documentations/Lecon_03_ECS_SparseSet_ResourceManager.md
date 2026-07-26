# Leçon 3 — ECS (SparseSet) & ResourceManager : audit et optimisations professionnelles

**Projet :** LibraryV3 (`onkover/LibraryV3`) — moteur 3D CPU, C++23, Visual Studio 2026
**Périmètre :** `Scene/Entity.hpp`, `Scene/SparseSet.hpp`, `Scene/Registry.hpp`, `Scene/System.hpp` / `Systeme.cpp`, `Scene/Components/Component.hpp`, `Ressources/ResourceManager.h/.cpp`, `Ressources/ResourceHandle.h`
**Format :** audit du code réel du dépôt → théorie → correctifs appliqués et validés par compilation/tests

---

## 1. Contexte et objectif du chantier

Suite à l'audit du moteur, l'ECS (Entity-Component-System) à base de SparseSet et le ResourceManager à handles typés étaient déjà des architectures **professionnelles dans leur intention** (le design suit fidèlement le patron d'EnTT), mais souffraient de plusieurs failles structurelles et d'hygiène. Ce chantier, découpé en items **F1 à F6** (plus deux corrections mineures **B3/B6**), les a résolus dans l'ordre suivant :

| Item | Objet | Statut |
|---|---|---|
| B3 | Faute de frappe `ConstainsEntity` → `Contains` | ✅ Fait |
| B6 | `Emplace` avec perfect forwarding (SparseSet) | ✅ Fait |
| **F1** | **Versionnage des entités** (bit-packing génération/index) | ✅ Fait et testé |
| F2 | SparseSet professionnalisé (cascade directe de F1) | ✅ Fait |
| **F3** | `ComponentView` : itérateur proxy utilisant son tuple capturé | ✅ Fait |
| **F6** | Hygiène `Systeme.cpp` (copies inutiles, boucles dangereuses) | ✅ Fait |
| **F4** | `MeshComponent` : possession → référence (handles) | ✅ Fait |
| **F5** | `ResourceManager` : chemins canoniques, unload O(1), erreurs typées | ✅ Fait et testé |

---

## 2. Rappel théorique — ECS et SparseSet

**ECS (Entity-Component-System)** : composition plutôt qu'héritage, données plutôt qu'objets. Une `Entity` est un simple identifiant, un `Component` est une donnée pure sans logique, un `System` est une fonction libre qui itère sur les composants. Bénéfices : localité cache, itération O(N) sur les données utiles, composition libre, parallélisation facilitée. Inconvénients : complexité de mise en œuvre (type erasure, templates), relations hiérarchiques maladroites, requêtes multi-composants coûteuses si mal implémentées.

**SparseSet** : structure à trois tableaux formant une bijection à double sens.
- **`m_Sparse`** — annuaire *identité → adresse* : indexé par `EntityIndex(entity)`, donne la position dans le dense. Permet `Has`/`Get` en O(1).
- **`m_Dense`** — entrepôt compact des composants, sans trous, 100 % cache-friendly (contiguïté + prefetcher matériel).
- **`m_Entities`** — miroir *adresse → identité* : indispensable au swap-and-pop (« qui possède le dernier élément du dense ? »), à l'itération (fournir l'identité pendant le parcours), et à la validation de génération (`m_Entities[i] == entity` compare index ET génération en un seul `==`).

**Résolution type → SparseSet** : chaque type de composant reçoit un ID entier via une **static locale templée** (`ComponentTypeManager::GetTypeID<T>()`), qui sert d'index direct dans `std::vector<unique_ptr<IComponentStorage>>`. O(1), sans RTTI. Règle absolue : **un typeID ne se sérialise jamais** (dépend de l'ordre d'exécution, pas du code).

**Itération multi-composants** : on pivote toujours sur le plus petit pool (l'intersection est bornée par son minimum), on consulte les autres pools en O(1) via leurs pointeurs typés capturés à la construction de la vue — jamais en repassant par le Registry à chaque entité candidate.

---

## 3. F1 — Versionnage des entités

### 3.1 Problème résolu

Sans génération, une `Entity` n'était qu'un indice recyclable : après destruction et recréation d'un slot, un handle périmé stocké ailleurs (ex. `TriggerComponent::overlapping_entities`) pouvait redevenir valide par accident (**problème ABA**). De plus, `DestroyEntity` ne vérifiait pas la vivacité de l'entité, permettant une **double destruction silencieuse** menant à deux entités vivantes partageant le même slot.

### 3.2 Design retenu

Bit-packing dans un seul `uint32_t` : **24 bits d'index / 8 bits de génération** (16,7 M d'entités simultanées, 256 générations par slot ; le wrap 255→0 est volontaire et assumé, risque de collision 1/256 après 256 recyclages du même slot — négligeable à l'échelle du projet).

```cpp
// Entity.hpp
using Entity = std::uint32_t;
inline constexpr std::uint32_t ENTITY_INDEX_BITS = 24u;
inline constexpr std::uint32_t ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1u;
inline constexpr Entity        NULL_ENTITY       = 0xFFFFFFFFu;

constexpr std::uint32_t EntityIndex(Entity e) noexcept      { return e & ENTITY_INDEX_MASK; }
constexpr std::uint8_t  EntityGeneration(Entity e) noexcept { return static_cast<std::uint8_t>(e >> ENTITY_INDEX_BITS); }
constexpr Entity        MakeEntity(std::uint32_t index, std::uint8_t generation) noexcept
{ return (static_cast<Entity>(generation) << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK); }
```

### 3.3 Registry — cycle de vie sécurisé

- `CreateEntity()` : recycle via `m_FreeIndices` (LIFO) ou alloue un nouveau slot ; retourne `MakeEntity(idx, m_Generations[idx])`.
- `IsAlive(e)` : **le** test qui tue le problème ABA — `m_Generations[idx] == EntityGeneration(e)`.
- `DestroyEntity(e)` : `assert(IsAlive(e))` (double-destroy détecté immédiatement) ; **ordre sacré** — notification des storages *avant* incrément de génération (sinon composants fantômes).
- `ForEachAlive(fn)` : remplace toute boucle `for (Entity e = 0; e < N; e++)`, désormais un bug garanti dès qu'un recyclage a eu lieu.
- `GetAliveCount()` : compteur O(1), pour affichage/log uniquement — jamais comme borne de boucle.

### 3.4 Cascade dans SparseSet

Règle unique retenue : **le sparse s'indexe par `EntityIndex(entity)` ; tout le reste (miroir, comparaisons, paramètres) manipule le handle `entity` complet.**

Bugs trouvés et corrigés pendant la migration (a posteriori de l'implémentation initiale) :
- `Get()` (const et non-const) indexait encore `m_Sparse[entity]` au lieu de `m_Sparse[EntityIndex(entity)]` → accès hors limites garanti dès génération ≥ 1.
- `Add()` écrivait `m_Sparse[entity] = dense_index` au lieu de `m_Sparse[idx] = dense_index` → même défaut.
- `EnsureSparseFits` renommé pour prendre un `std::uint32_t idx` explicite (le nom `entity` du paramètre original masquait le bug ci-dessus par mimétisme visuel).
- Ajout de l'assert sentinelle dans `Add` : `assert(m_Sparse[idx] == INVALID_INDEX && "Composant fantôme d'une génération antérieure détecté")` — audite `DestroyEntity` depuis l'extérieur.

### 3.5 Test de validation

`TestF1_EntityVersioning()` — vérifie : recyclage d'index avec génération différente, `IsAlive` correct sur périmé/vivant, absence de composant fantôme à travers les générations, survie propre d'un handle mémorisé après destruction de son entité. **Résultat : tous les invariants tiennent.**

---

## 4. F3 — ComponentView : itérateur proxy

### 4.1 Problème résolu

Le constructeur de `ComponentView` capturait déjà `m_storages_ptr_tuple` (pointeurs typés vers chaque `SparseSet<T>`) et calculait correctement le pivot minimal — mais l'itérateur ne s'en servait jamais : `SkipInvalidEntities()` et `operator*()` repassaient par `m_registry->hasComponent<T>()` / `getComponent<T>()`, re-payant `GetTypeID` + bounds check + `static_cast` par composant, par entité candidate, à chaque frame.

### 4.2 Correctifs appliqués

```cpp
// SkipInvalidEntities() — interroge directement les pointeurs capturés
const bool allComponentsPresent = std::apply(
    [currentEntity](auto*... storages) { return (storages->Contains(currentEntity) && ...); },
    m_view->m_storages_ptr_tuple);

// operator*() — construit le tuple directement via Get(), retour PAR VALEUR
reference operator*() const
{
    const Entity currentEntity = m_view->m_mainStorage->GetDenseEntities()[m_currentDenseIndex];
    return std::apply(
        [currentEntity](auto*... storages) { return value_type{ currentEntity, storages->Get(currentEntity)... }; },
        m_view->m_storages_ptr_tuple);
}
```

`reference` devient `value_type` (et non plus `value_type&`) : itérateur **proxy**, comme `std::vector<bool>`. Le membre `mutable std::optional<value_type> m_currentTuple` disparaît — plus besoin de prolonger la durée de vie d'un tuple temporaire.

**Conséquence sur tous les appelants** : `operator*` retournant désormais une prvalue, tout `for (auto& [...] : view)` doit devenir `for (auto&& [...] : view)`. Vérifié et déjà en place dans `Systeme.cpp` (`AnimationSystem`, `CameraSystem`, `TriggerSystem`, `PlayerInputSystem`).

---

## 5. F6 — Hygiène de Systeme.cpp

| Correction | Avant | Après |
|---|---|---|
| Copie profonde dans la récursion | `HierarchyComponent children = registry.getComponent<...>(entity);` | `const auto& children = registry.getComponent<...>(entity);` |
| Boucle dangereuse post-F1 | `for (Entity entity = 0; entity < registry.GetAliveCount(); entity++)` dans `DebugDisplaySystem` | `registry.ForEachAlive([&](Entity entity) { ... });` |
| Signature non protégée | `WorldTransformSystem(Registry&, Matrix44f&)` | vérifier passage en `const Matrix44f&` |
| Nommage fichier | `Systeme.cpp` | renommé `System.cpp` (cohérence avec `System.hpp`) |

Point vérifié et jugé non problématique : la matrice racine n'est en réalité jamais modifiée par `UpdateWorldTransforms` dans le code réel (contrairement à l'hypothèse initiale de l'audit) — vigilance à maintenir si le code évolue.

---

## 6. F4 — MeshComponent : de la possession à la référence

### 6.1 Problème résolu — et bug actif découvert

`MeshComponent` possédait sa ressource via `std::shared_ptr<MeshClass> m_mesh` et `std::string m_texture`, en parallèle du `ResourceManager` qui gère déjà tout via handles typés — deux systèmes de propriété concurrents. **Preuve concrète trouvée dans `Serializer::ParseMesh`** : `MeshHandle hMesh = ctx.pRM.LoadMesh(...)` était calculé, validé, puis **jamais assigné** à `m.m_mesh` — le mesh chargé était systématiquement perdu.

### 6.2 Vérification architecturale (Material.h / SubMesh.h)

Lecture de `SubMesh.h` : chaque sous-maillage porte déjà son propre `MaterialHandle material`. Lecture de `Material.h` : chaque matériau porte déjà ses `TextureHandle` (diffuse, spéculaire, normale, etc.). **Conclusion : le pipeline mesh → submesh → matériau → texture est déjà entièrement construit sur des handles.** `MeshComponent` n'a donc besoin d'aucun champ matériau propre (évite la généralité spéculative) ; `m_texture` (`std::string`) est un vestige sans place légitime dans cette architecture et est supprimé.

### 6.3 MeshComponent final

```cpp
struct MeshComponent
{
    MeshHandle m_mesh;   // résolu via resourceManager.GetMesh(m_mesh) au moment de l'usage

    float m_orbitalSpeed = 0.0f;
    float m_rotationSpeed = 0.0f;
    float m_currentOrbitAngle = 0.0f;
    float m_currentRotationAngle = 0.0f;
};
static_assert(std::is_trivially_copyable_v<MeshComponent>);
```

### 6.4 Serializer::ParseMesh — bug corrigé, migration vers Emplace

Le handle chargé est désormais réellement stocké, via `emplaceComponent<MeshComponent>(entity, hMesh, ...)` (construction directe, cf. B6).

### 6.5 RenderSystem — résolution du handle au point d'usage

Signature étendue : `RenderSystem(Registry&, Entity activeCamera, ResourceManager& resourceManager)`. Résolution `const MeshClass* mesh = resourceManager.GetMesh(meshComp.m_mesh);` **à l'intérieur de la boucle**, avec garde `if (!mesh) continue;` — un handle peut légitimement résoudre à `nullptr` (mesh déchargé entre deux frames), contrairement à l'ancien `shared_ptr` qui garantissait une durée de vie automatique. Répercuter la nouvelle signature dans `System.hpp` et sur tous les sites d'appel.

---

## 7. F5 — ResourceManager : chemins, unload, erreurs

### 7.1 Problème 1 — chemins non normalisés

`m_pathToMesh` utilisait le chemin brut comme clé : `"assets/x.obj"`, `"Assets/x.obj"`, `"assets\x.obj"` sont trois clés distinctes pour le même fichier → chargements dupliqués silencieux.

**Solution** :
```cpp
std::string CanonicalKey(const std::string& filepath)
{
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(filepath, ec);
    return ec ? filepath : canonical.generic_string();
}
```
`weakly_canonical` n'exige pas l'existence du fichier (contrairement à `canonical`) ; `generic_string()` uniformise les séparateurs. Appliqué dans `LoadMeshChecked`, `FindMesh`, `IsMeshLoaded`.

### 7.2 Problème 2 — UnloadMesh en O(N)

Scan linéaire de `m_pathToMesh` pour retrouver le chemin correspondant à un handle. **Solution : miroir inverse** `std::unordered_map<uint32_t, std::string> m_meshIdToPath`, entretenu en parallèle de `m_pathToMesh` (même principe que le miroir `m_Entities` du SparseSet).

```cpp
void ResourceManager::UnloadMesh(MeshHandle h)
{
    if (!h.IsValid()) return;
    if (auto pathIt = m_meshIdToPath.find(h.id); pathIt != m_meshIdToPath.end())
    {
        m_pathToMesh.erase(pathIt->second);
        m_meshIdToPath.erase(pathIt);
    }
    m_meshes.erase(h.id);
}
```
`UnloadAll()` vide également `m_meshIdToPath` (sinon fuite logique). Note : `UnloadAll()` utilise `.clear()` sur les conteneurs plutôt que d'itérer `UnloadMesh()` un par un — plus efficace quand *tout* doit disparaître (RAII : le destructeur `~ResourceManager()` appelle déjà `UnloadAll()` automatiquement).

### 7.3 Problème 3 — erreurs de chargement non typées

**Solution** : canal d'erreur explicite via `std::expected` (C++23), sans casser l'API existante.

```cpp
enum class EMeshLoadError : std::uint8_t { FileNotFound, ParseFailed };

std::expected<MeshHandle, EMeshLoadError>
ResourceManager::LoadMeshChecked(const std::string& filepath, const OBJLoadOptions& opt)
{
    const std::string key = CanonicalKey(filepath);
    if (auto it = m_pathToMesh.find(key); it != m_pathToMesh.end()) return it->second;
    if (!fs::exists(filepath)) return std::unexpected(EMeshLoadError::FileNotFound);
    MeshHandle h = OBJLoader::Load(filepath, *this, opt);
    if (!h.IsValid()) return std::unexpected(EMeshLoadError::ParseFailed);
    m_pathToMesh.emplace(key, h);
    m_meshIdToPath.emplace(h.id, key);
    return h;
}
// LoadMesh() devient un adaptateur mince par-dessus, pour compatibilité ascendante
```

**Limite honnête, documentée** : `OBJLoader::ParseFile` retourne un simple `bool` qui fusionne déjà "fichier vide" et "aucune face valide" — `ParseFailed` regroupe donc ces deux causes. Distinguer plus finement nécessiterait de faire remonter une information depuis `OBJLoader` lui-même (hors périmètre de F5, volontairement non traité).

**Résidu noté, non corrigé** : dans `OBJLoader::Load`, le test `if (parsedDataOBJ.rawFaces.empty())` est du code mort — `ParseFile` garantit déjà `rawFaces` non vide s'il retourne `true`. Sans impact fonctionnel, à nettoyer lors d'un futur passage sur `OBJLoader`.

### 7.4 Test de validation (TNR)

`TestF5_ResourceManager_UnloadMesh()` — charge plusieurs meshes, décharge le premier de la liste, vérifie : invariants avant/après sur la cible (bijection chemin↔handle cohérente dans les deux sens), absence d'effet de bord sur les meshes non ciblés, no-op sûr sur double-unload, id jamais recyclé après rechargement du même chemin. **Résultat : tous les invariants tiennent.**

---

## 8. RAII — pourquoi UnloadMesh n'est pas appelée en fin de programme

Point de compréhension consolidé pendant ce chantier : `~ResourceManager()` appelle déjà `UnloadAll()` **automatiquement**, garanti par le langage (RAII), au moment où l'objet sort de sa portée — pas au moment où l'OS récupère la mémoire du processus. Ce sont deux mécanismes distincts : le RAII est déterministe et s'exécute même en cas de sortie anticipée (return, exception) ; il libère aussi des ressources non-mémoire (futurs handles GPU, descripteurs) que l'OS seul ne pourrait pas nettoyer à la place du driver. `UnloadMesh()` reste réservée à un déchargement **sélectif en cours de session** (changement de niveau, hot-reload) — fonctionnalité écrite en avance de son usage, car aucun système de gestion de niveaux n'existe encore dans le moteur.

---

## 9. Points notés pour des chantiers futurs (non traités ici)

- **`TriggerSystem`** : la détection de collision N² actuelle (double `ViewGroup` imbriqué) fonctionne mais ne satisfait pas Onky — piste retenue : broad-phase spatiale (grille uniforme ou quadtree) avant la narrow-phase sphère/sphère existante. Chantier à part entière, distinct de l'hygiène F6.
- **`OBJLoader::ParseFile`** : granularité d'erreur insuffisante (bool unique) pour distinguer "fichier vide" de "aucune face valide" ; à revoir si un diagnostic plus fin des échecs de parsing devient nécessaire.
- **`OBJLoader::Load`** : test mort (`if (parsedDataOBJ.rawFaces.empty())`) à nettoyer.
- **`MeshComponent::m_material`** : non ajouté (YAGNI) — les matériaux sont déjà portés par `SubMesh`. À réintroduire en une ligne le jour où un vrai besoin d'override de matériau par instance apparaît.

---

## 10. Règles dictatoriales consolidées de la Leçon 3

1. Une `Entity` est un ticket daté, jamais un indice brut — tout accès tableau passe par `EntityIndex(e)`.
2. Dans `DestroyEntity` : notifier les storages d'abord, incrémenter la génération ensuite.
3. Le sparse s'indexe par `EntityIndex(entity)` ; le miroir stocke toujours le handle complet.
4. Un itérateur qui fabrique sa valeur la retourne par valeur (proxy iterator).
5. Un composant chaud est un POD : il référence les ressources par handle, il ne les possède jamais.
6. Un chemin de ressource est canonique avant d'être une clé de cache ; une map à sens unique s'accompagne de son miroir si l'inverse est nécessaire.
7. Un typeID runtime ne se sérialise jamais ; un objet qui va finir dans un conteneur ne devrait naître qu'une fois, à sa place finale (`Emplace`).
8. Le RAII est déterministe : ne jamais s'appuyer sur le nettoyage de l'OS pour une logique applicative.
