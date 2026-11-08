# Leçon 3 — ECS (SparseSet) & ResourceManager
## Support de cours complet — théorie, audit, implémentation

**Projet :** LibraryV3 (`onkover/LibraryV3`) — moteur 3D CPU, C++23, Visual Studio 2026
**Prérequis :** notions de base en C++ (templates, `std::vector`, RAII), Leçons 1 (Config/Enums) et 2 (Mathématiques)
**Objectif de la leçon :** comprendre en profondeur l'architecture d'un ECS à base de SparseSet — pas seulement l'utiliser, mais savoir pourquoi chaque décision de design existe, ce qu'elle coûte, et ce qu'elle achète.

---

## Sommaire

- **Partie I** — Pourquoi un ECS ? Théorie et paradigme
- **Partie II** — Anatomie du SparseSet (sparse, dense, miroir, cache CPU)
- **Partie III** — Résolution de type et itération multi-composants
- **Partie IV** — F1 : le versionnage des entités (le problème ABA)
- **Partie V** — F3 : l'itérateur proxy du ComponentView
- **Partie VI** — F6 : hygiène systémique
- **Partie VII** — F4 : composants et ressources — référencer, ne jamais posséder
- **Partie VIII** — F5 : ResourceManager professionnel
- **Partie IX** — RAII et cycle de vie
- **Partie X** — Synthèse des règles dictatoriales
- **Partie XI** — Exercices d'auto-évaluation
- **Annexe** — Glossaire

---

# Partie I — Pourquoi un ECS ? Théorie et paradigme

## 1.1 Le problème que l'ECS résout

Dans un moteur naïf orienté objet, un objet de scène est modélisé par une classe : `GameObject`, dont héritent `Ship`, `Camera`, `Light`, etc. Ce modèle tombe malade rapidement pour deux raisons.

**Maladie n°1 — l'explosion combinatoire de l'héritage.** Un vaisseau qui émet de la lumière et joue un son doit-il hériter de `Renderable`, `LightEmitter` et `AudioSource` ? L'héritage multiple devient vite un plat de spaghettis : chaque nouvelle combinaison de comportements exige soit une nouvelle classe, soit une hiérarchie de plus en plus profonde et fragile.

**Maladie n°2 — le massacre du cache CPU.** Les objets sont alloués sur le tas, dispersés en mémoire. Une boucle `for (auto* obj : scene) obj->Update();` provoque un cache miss par objet, plus un appel virtuel (indirection supplémentaire). Sur un moteur 100 % CPU comme LibraryV3, où chaque cycle compte pour le rasterizer, ce coût est rédhibitoire.

## 1.2 Le paradigme ECS

L'ECS renverse la logique : **composition plutôt qu'héritage, données plutôt qu'objets**.

- **Entity** : un simple identifiant numérique. Aucune donnée, aucun comportement propre.
- **Component** : une structure de données pures, sans logique (`TransformComponent`, `LightComponent`...).
- **System** : une fonction libre qui itère sur tous les composants d'un type donné et applique la logique (`AnimationSystem`, `TriggerSystem`...).

C'est l'architecture d'Unity DOTS, d'Unreal (Mass), et de la référence open-source du domaine, **EnTT** (utilisée notamment par Minecraft). LibraryV3 suit fidèlement ce design.

## 1.3 Bénéfices et inconvénients

| | ECS |
|---|---|
| **Bénéfices** | Localité cache maximale (composants contigus) ; itération O(N) sur les données réellement utilisées ; composition libre (une entité = n'importe quel assemblage de composants) ; découplage données/logique qui facilite le multithreading (deux systèmes touchant des composants disjoints peuvent tourner en parallèle) ; sérialisation triviale (les composants sont des données pures). |
| **Inconvénients** | Complexité de mise en œuvre (type erasure, templates, itérateurs) ; les relations entre entités (hiérarchie parent/enfant) deviennent maladroites ; débogage moins intuitif qu'un arbre d'objets classique ; les requêtes multi-composants ont un coût caché si mal implémentées. |

Le reste de cette leçon explique, pièce par pièce, comment LibraryV3 matérialise ces bénéfices — et comment éviter les pièges qui font perdre les bénéfices théoriques en pratique.

---

# Partie II — Anatomie du SparseSet

## 2.1 Le problème à résoudre

Un ECS a besoin d'une structure de stockage par type de composant qui offre trois garanties simultanées : test d'existence en O(1), accès direct en O(1), et **itération contiguë** pour le cache CPU. Aucune structure standard de la STL ne coche les trois cases à la fois : un `std::vector<std::optional<T>>` indexé par entité gaspille de la mémoire et casse la contiguïté (des trous partout) ; un `std::unordered_map<Entity, T>` est O(1) en moyenne mais son itération saute d'un nœud à l'autre en mémoire, sans aucune garantie de contiguïté.

La réponse : le **SparseSet**, une structure à **trois tableaux** qui se répartissent les rôles.

## 2.2 Les trois tableaux et leurs rôles

### `m_Sparse` — l'annuaire (identité → adresse)

Un tableau où l'**index** est l'identité de l'entité et la **valeur** est la position du composant dans le tableau dense. C'est un dictionnaire à accès direct : pas de hachage, pas de comparaison d'arbre, juste une lecture mémoire à une adresse calculée.

```
m_Sparse : [ E0 → ∅ ][ E1 → 0 ][ E2 → ∅ ][ E3 → 1 ][ E4 → ∅ ]...
```
Ici, l'entité 1 a son composant à la position 0 du dense, l'entité 3 à la position 1. Les entités 0, 2 et 4 n'ont pas ce composant (`∅` = `INVALID_INDEX`, une valeur sentinelle qui ne peut jamais être une position valide).

**Rôle 1 — traduction d'identité.** `Get(entity)` = deux lectures mémoire : `m_Dense[m_Sparse[EntityIndex(entity)]]`. Zéro hachage, zéro branche imprévisible — plus rapide qu'une `unordered_map` (qui hache et suit un bucket) et incomparablement plus rapide qu'une `map` (arbre éclaté en mémoire, O(log N)).

**Rôle 2 — test d'existence.** `Has(entity)` compare la valeur lue à la sentinelle `INVALID_INDEX`. C'est ce qui rend une itération multi-composants viable : filtrer « cette entité a-t-elle aussi tel composant ? » ne coûte que quelques cycles.

**Rôle 3 — pivot du swap-and-pop** (voir 2.4).

Le prix à payer : ce tableau est dimensionné par le **plus grand index d'entité en circulation**, pas par le nombre de composants réellement stockés. Une scène de 100 000 entités dont 12 seulement ont un `CameraComponent` paie quand même 100 000 × 4 octets de sparse pour indexer 12 composants — d'où le nom « sparse » (creux). C'est un compromis délibéré : mémoire contre temps constant.

### `m_Dense` — l'entrepôt compact

Un `std::vector<ComponentType>` qui contient les composants **empilés sans trous**, dans un ordre qui n'a rien à voir avec l'ordre de création des entités (le swap-and-pop les déplace en permanence). C'est ce tableau que les systèmes parcourent pour l'itération mono-composant — et c'est lui qui rend l'ECS réellement performant.

### `m_Entities` — le miroir (adresse → identité)

Un `std::vector<Entity>` de même taille que `m_Dense` : à la position `i`, il contient l'identité de l'entité propriétaire du composant `m_Dense[i]`. C'est l'**annuaire inverse**.

**Pourquoi ce miroir est indispensable — le swap-and-pop l'exige.** Supprimer un composant au milieu du dense sans laisser de trou nécessite de déplacer le **dernier** élément à sa place, puis de rediriger l'annuaire de l'entité déplacée. Mais qui possède le dernier élément ? Le composant lui-même ne le sait pas (une donnée pure ne connaît pas son propriétaire), et le sparse ne répond que dans le mauvais sens. Sans miroir, il faudrait **scanner tout le sparse** pour trouver quelle entité pointe vers la dernière position — une suppression O(1) deviendrait O(taille du sparse). `m_Entities[dernier_index]` répond en une lecture.

**Deuxième usage — fournir l'identité pendant l'itération.** Un système qui parcourt le dense a besoin de savoir *à qui* appartient chaque composant (pour consulter d'autres pools, appeler `DestroyEntity`, etc.). Le dense ne contient que des données pures ; `m_Entities[i]` est la seule source de cette identité.

**Troisième usage — la validation (voir Partie IV).** Une fois les entités versionnées, `m_Entities` stocke le handle complet (génération incluse), ce qui permet à `Has()` de détecter un handle périmé en un seul `==`.

### Invariant sacré

Pour tout `i` valide : `m_Sparse[EntityIndex(m_Entities[i])] == i`. Les deux tableaux se pointent mutuellement, en boucle fermée. Une méthode de vérification (à activer en Debug) permet de le garantir explicitement :

```cpp
#ifdef LV3_DEBUG
void CheckInvariants() const
{
    assert(m_Dense.size() == m_Entities.size());
    for (std::uint32_t i = 0; i < m_Entities.size(); ++i)
        assert(m_Sparse[EntityIndex(m_Entities[i])] == i && "Bijection sparse/entities rompue");
}
#endif
```

## 2.3 Pourquoi le dense est cache-friendly

### La hiérarchie mémoire, en ordres de grandeur

| Niveau | Latence typique | Équivalent humain |
|---|---|---|
| Registre | 0 cycle | l'idée dans ta tête |
| Cache L1 | ~4 cycles (~1 ns) | le papier sur ton bureau |
| Cache L2 | ~12 cycles | le tiroir |
| Cache L3 | ~40 cycles | l'armoire au fond du bureau |
| **RAM** | **~200 cycles (~100 ns)** | **descendre aux archives du sous-sol** |

Un **cache miss** (donnée absente de tous les caches) coûte l'équivalent de ~200 instructions perdues à attendre. Sur un moteur qui traite des dizaines de milliers de sommets par frame, le nombre de cache misses *est* la performance — bien plus que le nombre d'opérations arithmétiques.

### Les deux super-pouvoirs du tableau dense

**Localité spatiale : un miss en paie plusieurs.** `m_Dense` est un `std::vector` : contiguïté garantie par le standard C++. Si un composant fait 16 octets, une ligne de cache de 64 octets en contient quatre. Le premier accès charge gratuitement les trois suivants.

**Le prefetcher matériel.** Le CPU contient un circuit qui détecte les motifs d'accès linéaires et charge les lignes suivantes **en avance**, pendant que le programme traite les précédentes. Sur un parcours de `std::vector`, les cache misses disparaissent presque totalement après les premières lignes — RAM et CPU travaillent en pipeline. Cette magie matérielle n'opère que sur des **motifs prévisibles**.

### Le contre-exemple : le pointer chasing

Un modèle `std::vector<std::unique_ptr<Component>>` — chaque objet à une adresse arbitraire du tas — subit deux condamnations : une ligne de cache gaspillée par objet, et surtout des chargements **dépendants** : le CPU ne connaît l'adresse de l'objet suivant qu'après avoir fini de charger le pointeur courant. Le prefetcher est aveugle ; les latences RAM s'additionnent **en série**. C'est le *pointer chasing*, le tueur silencieux des moteurs naïfs.

### La nuance de professionnel : « contigu » ne suffit pas seul

Cache-friendliness = contiguïté **×** densité utile de la ligne. Un composant lourd (plusieurs `Matrix44f`) occupe plusieurs lignes de cache ; le prefetcher fonctionne toujours, mais un système qui ne lit qu'un seul champ charge quand même tout le reste. Pire : un composant contenant des `std::string` ou un `std::set` est contigu **en façade** — chaque chaîne, chaque nœud d'arbre est un pointeur vers le tas. Le sparse set achète la contiguïté du tableau ; le contenu du composant peut la revendre. **Règle : un composant chaud est un POD, ou il en paie le prix.**

## 2.4 Le swap-and-pop, pas à pas

Supprimer un composant sans laisser de trou dans le dense :

1. Retrouver sa position via le sparse : `toRemove = m_Sparse[EntityIndex(entity)]`.
2. Si ce n'est pas déjà le dernier élément, déplacer le **dernier** élément du dense à la position `toRemove` (`m_Dense[toRemove] = std::move(m_Dense[lastIdx])`).
3. Mettre à jour le miroir : `m_Entities[toRemove] = lastEntity`.
4. **Rediriger l'annuaire de l'entité déplacée** : `m_Sparse[EntityIndex(lastEntity)] = toRemove`.
5. `pop_back()` sur le dense et le miroir.
6. Invalider le sparse de l'entité supprimée : `m_Sparse[idx] = INVALID_INDEX`.

Point subtil : si l'élément à supprimer est déjà le dernier, l'étape 2 devient un self-move-assignment. Légal en C++ mais inutile — un `if (toRemove != lastIdx)` évite ce travail superflu.

## 2.5 Résolution type → storage

### Le double problème

Un Registry doit stocker `SparseSet<TransformComponent>`, `SparseSet<MeshComponent>`, etc. — des **types sans lien entre eux** (chaque instanciation de template est une classe distincte). Deux obstacles :

**Ranger des types différents ensemble : le type erasure.** Une interface de base non-template (`IComponentStorage`) donne à tous les SparseSets un ancêtre commun. Le conteneur devient `std::vector<std::unique_ptr<IComponentStorage>>` — le type précis est « gommé » derrière le pointeur de base, exactement comme `std::function` ou `std::any`.

**Retrouver le bon storage en O(1) : l'ID entier séquentiel.** La solution naïve (`std::map<std::type_index, ...>`) coûte RTTI + comparaison + O(log N). La solution professionnelle : attribuer à chaque type un entier séquentiel via une **static locale templée** :

```cpp
class ComponentTypeManager
{
public:
    template <typename T>
    static uint32_t GetTypeID()
    {
        static uint32_t id = s_NextID++;   // toute la magie est ici
        return id;
    }
private:
    static inline uint32_t s_NextID = 0;
};
```

Chaque type `T` instancie une fonction distincte, chacune avec **sa propre** variable statique. La sémantique du `static` local garantit une initialisation unique, au premier appel, thread-safe depuis C++11 (magic statics). Premier type appelé → ID 0, deuxième → ID 1, etc. — une caisse enregistreuse qui distribue des tickets numérotés.

**`getStorage<T>()`** combine les deux mécanismes : obtention de l'ID, redimensionnement du vector si nécessaire, création paresseuse du pool s'il n'existe pas encore, puis `static_cast` vers le type concret. Ce `static_cast` (et non `dynamic_cast`) est légitime car **l'invariant du système le garantit** : le slot `typeID` ne peut être rempli que par `getStorage<ComponentType>()`, puisque `typeID` n'est produit que par `GetTypeID<ComponentType>()`. L'ID est à la fois la clé et la preuve du type.

### Trois pièges à connaître

1. **Non-déterminisme inter-exécutions.** L'ID dépend de l'ordre du premier appel *à l'exécution*, pas du code. **Règle absolue : un typeID ne franchit jamais la frontière du processus** — ni fichier de sauvegarde, ni réseau. La sérialisation identifie les composants par nom.
2. **Frontière de module.** Chaque DLL instanciant le template fabrique sa propre static → IDs incohérents entre modules. LibraryV3 est protégé par construction (LIB statique, tout fusionné au link).
3. **Race sur le compteur.** Le pattern actuel n'est pas thread-safe pour l'incrémentation du compteur partagé entre deux *types différents* appelés simultanément — correctible avec `std::atomic<uint32_t>` le jour où le Registry devient multi-thread.

## 2.6 Itération multi-composants et le pivot minimal

### Trois stratégies, une seule professionnelle

- **Stratégie A (le crime)** : parcourir toutes les entités du Registry et tester chaque type. Coût O(E×K), E = nombre total d'entités.
- **Stratégie B (la faute)** : pivoter sur un pool arbitraire. Si le pivot est le plus grand pool, on visite des candidats condamnés d'avance.
- **Stratégie C (la règle d'or)** : **pivoter sur le pool le plus petit**. L'intersection de plusieurs ensembles ne peut jamais dépasser le plus petit d'entre eux. On parcourt son dense (contigu), et pour chaque candidat on consulte les autres pools en O(1) via leur sparse. Coût total : O(M×K), M = taille du plus petit pool.

### Le piège de performance : ignorer ce qu'on a déjà capturé

Un `ComponentView` bien construit capture, à sa création, un `std::tuple<SparseSet<T>*...>` — les pointeurs typés directs vers chaque pool concerné. **L'erreur classique** est de capturer ce tuple puis de continuer à interroger le Registry générique (`hasComponent<T>()`, `getComponent<T>()`) à chaque entité candidate — re-payant `GetTypeID`, bounds check et `static_cast` en boucle, alors que le tuple les évite tous. La correction : utiliser `std::apply` avec une fold expression directement sur les pointeurs capturés :

```cpp
const bool allPresent = std::apply(
    [entity](auto*... storages) { return (storages->Contains(entity) && ...); },
    m_storages_ptr_tuple);
```

### Deux règles d'exécution supplémentaires

1. **Construire la vue une seule fois par système**, jamais dans une boucle interne — la recherche du pivot a un coût, il ne doit pas se répéter par entité.
2. **Mono-composant → dense brut** (`registry.View<T>()`), **multi-composants → vue avec pivot** — ne jamais utiliser une vue là où un simple parcours suffit.

### Sécurité : ne jamais muter pendant l'itération

Ajouter (`Add`, réallocation possible) ou retirer (`Remove`, swap-and-pop qui déplace des éléments) un composant pendant qu'on itère dessus corrompt l'itération en cours, silencieusement. Le pattern professionnel est le **command buffer** : on note les entités à modifier dans un vecteur local, on applique les changements après la boucle.

---

# Partie III — Le proxy iterator (rappel structurel)

Quand `operator*()` **fabrique** sa valeur de retour (un tuple de références assemblées à la volée) plutôt que de retourner une référence vers une donnée qui existe déjà en mémoire, il ne peut pas retourner une référence — il n'y a rien de permanent à référencer. La solution standard, utilisée depuis longtemps par `std::vector<bool>` et officialisée par les concepts d'itérateurs C++20, est l'**itérateur proxy** : `operator*()` retourne sa valeur **par valeur** (`reference = value_type`, pas `value_type&`). Le tuple retourné contient de vraies références vers le dense — seule l'enveloppe qui les regroupe est neuve à chaque appel.

Voir Partie V pour l'application concrète à `ComponentView`.

---

# Partie IV — F1 : le versionnage des entités

## 4.1 Le problème ABA, illustré

Sans information de génération, une `Entity` n'est qu'un indice recyclable. Scénario concret :

```
1. Entity e = CreateEntity();       // e = 5
2. DestroyEntity(e);                // slot 5 libéré
3. Entity a = CreateEntity();       // a = 5 (recyclé)
4. Entity b = CreateEntity();       // b = 5 !! DEUX entités vivantes, même identifiant
```

C'est le **problème ABA**, connu en programmation concurrente lock-free, transposé ici à l'ECS : une valeur (l'index 5) change d'état (A → B → A) et un observateur extérieur (un handle mémorisé ailleurs, par exemple `TriggerComponent::overlapping_entities`) ne peut pas distinguer « toujours la même entité » de « une entité totalement différente qui a hérité du même numéro ». Conséquence : ajouter un composant à `a` écrase celui de `b`, détruire `a` détruit `b` par la bande — un bug qui ne crashe pas, qui **corrompt**.

Deuxième symptôme du même défaut : sans garde, `DestroyEntity` appelé deux fois sur la même entité passe inaperçu et pousse deux fois le même index dans la liste des slots libres — corruption de la structure elle-même.

## 4.2 Le design retenu : bit-packing dans un `uint32_t`

Trois options existaient. Une `struct {index; generation;}` de 8 octets serait lisible mais doublerait la taille de tout ce qui stocke des entités. Un `uint64_t` (32/32) est le luxe des moteurs AAA à très grande échelle. La solution retenue, celle d'EnTT adaptée à l'échelle de LibraryV3, est le **bit-packing dans un seul `uint32_t`** : **24 bits d'index, 8 bits de génération** — 16,7 millions d'entités simultanées, 256 générations par slot.

```
bits 31…24 (génération, 8 bits) | bits 23…0 (index, 24 bits)
        0x01                    |        0x000002
→ index 2, génération 1 → Entity = 0x01000002
```

Compromis assumé : la génération sur 8 bits **boucle** après 256 réutilisations du même slot (débordement volontaire d'un `uint8_t`). Un handle antique aurait alors 1 chance sur 256 de « revalider » par accident. Négligeable à l'échelle d'un moteur pédagogique mono-scène — EnTT accepte un compromis du même ordre avec ses 12 bits de génération.

```cpp
using Entity = std::uint32_t;
inline constexpr std::uint32_t ENTITY_INDEX_BITS = 24u;
inline constexpr std::uint32_t ENTITY_INDEX_MASK = (1u << ENTITY_INDEX_BITS) - 1u;
inline constexpr Entity        NULL_ENTITY       = 0xFFFFFFFFu;

constexpr std::uint32_t EntityIndex(Entity e) noexcept
{ return e & ENTITY_INDEX_MASK; }

constexpr std::uint8_t EntityGeneration(Entity e) noexcept
{ return static_cast<std::uint8_t>(e >> ENTITY_INDEX_BITS); }

constexpr Entity MakeEntity(std::uint32_t index, std::uint8_t generation) noexcept
{ return (static_cast<Entity>(generation) << ENTITY_INDEX_BITS) | (index & ENTITY_INDEX_MASK); }

static_assert(EntityIndex(MakeEntity(2u, 1u)) == 2u);
static_assert(EntityGeneration(MakeEntity(2u, 1u)) == 1u);
static_assert(MakeEntity(2u, 1u) == 0x01000002u);
```

Les `static_assert` sont des tests unitaires **gratuits, exécutés à la compilation** — si quelqu'un modifie le layout des bits, la compilation échoue immédiatement plutôt que de produire un bug latent.

## 4.3 Le Registry — cycle de vie sécurisé

Trois structures indexées par **index brut** (pas par handle) remplacent l'ancien compteur simple :

```cpp
std::vector<std::uint8_t>  m_Generations;  // génération courante de chaque slot
std::vector<bool>          m_Alive;        // slot occupé ? (pour ForEachAlive)
std::vector<std::uint32_t> m_FreeIndices;  // indices recyclables (LIFO)
std::uint32_t              m_AliveCount = 0;
```

`m_Alive` est en un sens redondant pour `IsAlive` seul (la comparaison de génération suffit), mais indispensable pour un besoin différent : itérer tous les slots vivants sans consulter la free-list en O(N).

```cpp
[[nodiscard]] Entity CreateEntity()
{
    std::uint32_t idx;
    if (!m_FreeIndices.empty())
    {
        idx = m_FreeIndices.back();   // LIFO : réutilise le slot le plus "chaud" (encore en cache)
        m_FreeIndices.pop_back();
    }
    else
    {
        idx = static_cast<std::uint32_t>(m_Generations.size());
        m_Generations.push_back(0u);
        m_Alive.push_back(false);
    }
    m_Alive[idx] = true;
    ++m_AliveCount;
    return MakeEntity(idx, m_Generations[idx]);
}

[[nodiscard]] bool IsAlive(Entity e) const noexcept
{
    const std::uint32_t idx = EntityIndex(e);
    return idx < m_Generations.size() && m_Generations[idx] == EntityGeneration(e);
    // ^ LE test qui tue le problème ABA
}

void DestroyEntity(Entity e)
{
    assert(IsAlive(e) && "DestroyEntity : double destruction ou handle périmé");
    if (!IsAlive(e)) return;   // garde-fou silencieux en Release

    // ORDRE SACRÉ : notifier les storages AVANT d'incrémenter la génération.
    // Inverser détruit tout : OnEntityDestroyed(e) recevrait un handle déjà
    // périmé, Has(e) répondrait false, aucun composant ne serait retiré ->
    // composants fantômes. Un bug qui compile parfaitement.
    for (auto& storage : m_Storages)
        if (storage) storage->OnEntityDestroyed(e);

    const std::uint32_t idx = EntityIndex(e);
    ++m_Generations[idx];   // wrap uint8_t 255→0 volontaire et assumé
    m_Alive[idx] = false;
    --m_AliveCount;
    m_FreeIndices.push_back(idx);
}

template <typename Func>
void ForEachAlive(Func&& fn) const
{
    // Remplace TOUTE boucle "for (Entity e = 0; e < N; e++)" — devenue un bug
    // garanti dès qu'un recyclage a eu lieu (génération non nulle).
    for (std::uint32_t idx = 0; idx < m_Generations.size(); ++idx)
        if (m_Alive[idx])
            fn(MakeEntity(idx, m_Generations[idx]));
}

[[nodiscard]] std::uint32_t GetAliveCount() const noexcept { return m_AliveCount; }
```

**Règle de choix entre `GetAliveCount()` et `ForEachAlive()` :** si la valeur sert à *afficher un nombre*, `GetAliveCount()` (O(1), simple compteur). Si elle sert à *fabriquer ou borner des `Entity`*, c'est obligatoirement `ForEachAlive()` — jamais un renommage mécanique de l'un vers l'autre.

## 4.4 La cascade dans le SparseSet

**Règle unique de partage :** le sparse s'indexe par `EntityIndex(entity)` (l'index nu) ; tout le reste — le miroir, les comparaisons, les paramètres de méthode — manipule le handle `entity` **complet**.

| Élément | Indexé/comparé par |
|---|---|
| `m_Sparse[...]` | `EntityIndex(entity)` uniquement |
| `m_Entities[...]` | stocke le handle **complet** |
| `Contains/Get/Add/Remove(entity)` | reçoivent le handle complet en paramètre |
| `m_Entities[dense] == entity` | comparaison de handles complets (index + génération) |

```cpp
bool Contains(Entity entity) const override
{
    const std::uint32_t idx = EntityIndex(entity);
    if (idx >= m_Sparse.size()) return false;
    const std::uint32_t dense = m_Sparse[idx];
    return dense != INVALID_INDEX && dense < m_Dense.size()
        && m_Entities[dense] == entity;   // valide index ET génération en un seul '=='
}

ComponentType& Get(Entity entity)
{
    assert(Contains(entity));
    return m_Dense[m_Sparse[EntityIndex(entity)]];   // extraction obligatoire ici aussi
}

void Add(Entity entity, ComponentType component)
{
    const std::uint32_t idx = EntityIndex(entity);
    EnsureSparseFits(idx);
    if (Contains(entity)) { Get(entity) = std::move(component); return; }

    // Sentinelle inter-module : si ce slot n'est pas vierge alors que Contains()
    // est faux, c'est que DestroyEntity a fauté (composant fantôme d'une génération
    // antérieure jamais nettoyé).
    assert(m_Sparse[idx] == INVALID_INDEX && "Composant fantôme d'une génération antérieure détecté");

    const uint32_t dense_index = static_cast<uint32_t>(m_Dense.size());
    m_Sparse[idx] = dense_index;
    m_Dense.push_back(std::move(component));
    m_Entities.push_back(entity);
}

void Remove(Entity entity)
{
    if (!Contains(entity)) return;
    const std::uint32_t idx      = EntityIndex(entity);
    const std::uint32_t toRemove = m_Sparse[idx];
    const std::uint32_t lastIdx  = static_cast<std::uint32_t>(m_Dense.size() - 1);

    if (toRemove != lastIdx)   // évite le self-move quand on retire le dernier
    {
        const Entity lastEntity = m_Entities[lastIdx];
        m_Dense[toRemove]    = std::move(m_Dense[lastIdx]);
        m_Entities[toRemove] = lastEntity;
        m_Sparse[EntityIndex(lastEntity)] = toRemove;   // EntityIndex, pas l'entité brute !
    }
    m_Dense.pop_back();
    m_Entities.pop_back();
    m_Sparse[idx] = INVALID_INDEX;
}

template<typename... Args>
ComponentType& Emplace(Entity entity, Args&&... args)
{
    // Construction directe à la place finale : zéro objet temporaire, zéro copie/
    // déplacement intermédiaire. Réservé à la CRÉATION (pas la mise à jour).
    const std::uint32_t idx = EntityIndex(entity);
    EnsureSparseFits(idx);
    assert(!Contains(entity) && "Emplace : le composant existe déjà");
    const uint32_t dense_index = static_cast<uint32_t>(m_Dense.size());
    m_Sparse[idx] = dense_index;
    m_Entities.push_back(entity);
    return m_Dense.emplace_back(std::forward<Args>(args)...);
}
```

**Piège d'implémentation rencontré et corrigé pendant ce chantier** : un nommage trompeur du paramètre de `EnsureSparseFits` (appelé `entity` alors qu'il reçoit un index déjà extrait) avait contribué à faire réapparaître, par mimétisme visuel, des `m_Sparse[entity]` bruts dans `Get()` et `Add()` — provoquant un accès hors limites garanti dès qu'une génération non nulle entrait en jeu. Renommer le paramètre en `idx` explicite a éliminé le piège à la source.

## 4.5 Pourquoi `Emplace` (B6) plutôt que `Add` pour les composants lourds

`Add(Entity, ComponentType component)` reçoit son paramètre **par valeur** : un composant contenant des `std::string`/`std::set` (ex. `TriggerComponent`) subit une construction, puis une copie ou un déplacement vers le paramètre, puis un déplacement final vers le dense — jusqu'à trois manipulations pour un objet qui ne devrait naître qu'une fois. `Emplace` transmet les **arguments du constructeur** via un paquet de références universelles (`Args&&... args`) et `std::forward`, jusqu'à `emplace_back`, qui construit l'objet **directement dans le buffer du vector**. Zéro intermédiaire. Règle : `Add` reste légitime quand un objet déjà construit doit être mis à jour ou transféré ; `Emplace` est le réflexe dès qu'un composant possède la moindre indirection interne et qu'on le crée pour la première fois.

## 4.6 Validation — le test unitaire

```cpp
void TestF1_EntityVersioning()
{
    LV3::Registry reg;
    LV3::Entity a = reg.CreateEntity();               // index 0, gen 0
    reg.addComponent(a, LV3::HealthComponent{100,100});
    reg.DestroyEntity(a);
    LV3::Entity b = reg.CreateEntity();               // index 0, gen 1 (même slot recyclé)

    assert(LV3::EntityIndex(a) == LV3::EntityIndex(b));   // même slot...
    assert(a != b);                                        // ...ticket différent
    assert(!reg.IsAlive(a));
    assert( reg.IsAlive(b));
    assert(!reg.hasComponent<LV3::HealthComponent>(a));   // handle périmé -> refusé
    assert(!reg.hasComponent<LV3::HealthComponent>(b));   // nouvelle entité -> vierge

    LV3::Entity held = reg.CreateEntity();
    reg.DestroyEntity(held);
    reg.CreateEntity();                                   // recycle le slot de 'held'
    assert(!reg.IsAlive(held));   // le voisin mémorisé (scénario TriggerComponent) est bien mort
}
```

Test complémentaire, volontairement destructeur : appeler `reg.DestroyEntity(a)` une seconde fois doit déclencher l'`assert` de `DestroyEntity` — un garde-fou jamais vu se déclencher est un garde-fou dont on ignore s'il existe.

---

# Partie V — F3 : l'itérateur proxy du ComponentView

## 5.1 Le diagnostic

Le constructeur de `ComponentView` fait un travail correct et non négligeable : il capture `m_storages_ptr_tuple` (les pointeurs typés vers chaque `SparseSet<T>` concerné) et détermine `m_mainStorage` en choisissant le pool le plus petit comme pivot. Le défaut se situait uniquement dans l'**itérateur**, qui ignorait ce tuple et repassait systématiquement par le Registry générique :

```cpp
// AVANT — re-paye GetTypeID + bounds check + static_cast, PAR composant, PAR entité candidate
if (!m_view->m_registry->template hasComponent<ComponentTypes>(currentEntity)) ...
m_view->m_registry->template getComponent<ComponentTypes>(currentEntity)...
```

## 5.2 La correction

```cpp
void SkipInvalidEntities()
{
    // ... bornes ...
    while (m_currentDenseIndex < mainSize)
    {
        Entity currentEntity = mainEntities[m_currentDenseIndex];

        // Interroge DIRECTEMENT les pointeurs typés capturés — plus de passage
        // par le Registry. Fold expression && : court-circuite au premier échec.
        const bool allPresent = std::apply(
            [currentEntity](auto*... storages) { return (storages->Contains(currentEntity) && ...); },
            m_view->m_storages_ptr_tuple);

        if (allPresent) break;
        ++m_currentDenseIndex;
    }
}

using value_type = std::tuple<Entity, ComponentTypes&...>;
using reference  = value_type;   // itérateur PROXY : retour PAR VALEUR (cf. std::vector<bool>)

reference operator*() const
{
    const Entity currentEntity = m_view->m_mainStorage->GetDenseEntities()[m_currentDenseIndex];
    return std::apply(
        [currentEntity](auto*... storages) { return value_type{ currentEntity, storages->Get(currentEntity)... }; },
        m_view->m_storages_ptr_tuple);
}
```

Le membre `mutable std::optional<value_type> m_currentTuple`, qui servait à prolonger artificiellement la durée de vie d'un tuple temporaire pour pouvoir le référencer, disparaît entièrement — inutile une fois que le retour se fait par valeur.

## 5.3 Conséquence sur tous les appelants

`operator*` retourne désormais une **prvalue** (valeur temporaire), pas une référence. Une prvalue ne se lie pas à `auto&` — toute boucle doit passer en `auto&&` :

```cpp
// AVANT — ne compile plus après la correction
for (auto& [entity, control, transform] : registry.ViewGroup<...>())

// APRÈS — obligatoire
for (auto&& [entity, control, transform] : registry.ViewGroup<...>())
```

`auto&&` (référence universelle) accepte la prvalue ; les références **contenues** dans le tuple (`ComponentTypes&...`) restent de vraies références modifiables vers le dense — écrire `transform.x = ...` modifie bien le composant réel.

---

# Partie VI — F6 : hygiène systémique

| Correction | Avant | Après | Justification |
|---|---|---|---|
| Copie profonde en récursion | `HierarchyComponent children = registry.getComponent<...>(entity);` | `const auto& children = ...;` | Copie un `std::vector<Entity>` à chaque nœud, à chaque frame, dans une récursion — allocation heap évitable. |
| Boucle dangereuse post-F1 | `for (Entity entity = 0; entity < registry.GetAliveCount(); entity++)` | `registry.ForEachAlive([&](Entity e){...});` | `GetAliveCount()` est un nombre, pas une borne d'index valide ; construit des handles de génération 0 qui ne correspondent à rien après un recyclage. |
| Nommage fichier | `Systeme.cpp` | `System.cpp` | Cohérence avec `System.hpp` ; simplifie la recherche de fichiers. |
| Code mort commenté (référence `glm`) | résidu de portage | nettoyé ou remplacé par un `// TODO` explicite | `glm` n'est plus une dépendance du moteur ; un commentaire qui y fait référence induit en erreur. |

Point vérifié et jugé sain : la matrice racine transmise à `WorldTransformSystem` n'est, dans le code réel, jamais modifiée en place par `UpdateWorldTransforms` (seulement multipliée pour produire `m_worldTransform`) — le risque de rotation cumulative frame après frame, redouté à l'audit initial, ne s'est pas matérialisé. Vigilance à maintenir si le code évolue : ce paramètre devrait idéalement être `const Matrix44f&` pour l'interdire à la compilation plutôt que par discipline.

---

# Partie VII — F4 : composants et ressources — référencer, ne jamais posséder

## 7.1 Le principe

**Règle dictatoriale : un composant ne possède jamais une ressource. Il la référence par handle.** C'est ainsi que fonctionnent Unity (assets référencés par GUID), Unreal, Godot. Un composant qui possède une ressource (`std::shared_ptr`) crée un **second système de propriété**, concurrent de celui du ResourceManager — deux sources de vérité pour une même donnée, qui finissent par diverger.

## 7.2 Étude de cas : MeshComponent

**Diagnostic initial.** `MeshComponent` possédait `std::shared_ptr<MeshClass> m_mesh` et `std::string m_texture`, en parallèle du `ResourceManager` qui gère déjà tout via des handles typés (`MeshHandle`, `MaterialHandle`, `TextureHandle`). Conséquences concrètes de cette duplication : comptage de références atomique à chaque copie de composant (y compris pendant le swap-and-pop du SparseSet) ; impossible de savoir qui possède réellement quoi ; `UnloadAll()` du ResourceManager ne libère rien tant qu'un `shared_ptr` externe garde une référence.

**Preuve empirique du danger, trouvée dans le code réel.** Dans `Serializer::ParseMesh`, un `MeshHandle hMesh = ctx.pRM.LoadMesh(...)` était calculé et validé — puis **jamais assigné** au composant. Le mesh chargé était systématiquement perdu, silencieusement, à chaque entité chargée depuis un JSON. Ce n'est pas une hypothèse d'audit : c'est un bug actif, découvert en relisant le code, qui illustre exactement pourquoi deux systèmes de propriété concurrents créent des trous — le point de couture entre les deux mondes (handle chargé / composant qui devrait le porter) n'existait tout simplement pas.

**Vérification architecturale avant de trancher.** Lecture de `SubMesh.h` : chaque sous-maillage porte déjà son propre `MaterialHandle material` (avec `HasMaterial()`). Lecture de `Material.h` : chaque matériau porte déjà ses `TextureHandle` (diffuse, spéculaire, normale, ambiante, opacité, émissive). **Conclusion : le pipeline mesh → submesh → matériau → texture est déjà entièrement construit sur des handles**, en amont de `MeshComponent`. Ajouter un `MaterialHandle` sur le composant lui-même aurait été de la **généralité spéculative** (YAGNI) : aucun système ne l'aurait consommé, puisque le matériau se résout déjà via le sous-maillage du mesh.

**MeshComponent final :**
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

Bénéfice mesurable et concret : le composant redevient trivialement copiable. Le swap-and-pop du SparseSet, qui faisait auparavant un déplacement de `shared_ptr` (décrément atomique potentiel), devient un simple `memcpy` de quelques octets.

## 7.3 RenderSystem : résolution au point d'usage

Le système consommateur doit désormais recevoir le `ResourceManager` et résoudre le handle **à l'instant du besoin**, jamais avant :

```cpp
void RenderSystem(Registry& registry, Entity activeCamera, ResourceManager& resourceManager)
{
    // ... itération sur les Transforms ...
    auto& meshComp = registry.getComponent<MeshComponent>(Entities[tr]);
    const MeshClass* mesh = resourceManager.GetMesh(meshComp.m_mesh);
    if (!mesh)
    {
        // Handle invalide ou mesh déchargé entre-temps : on ignore cette entité
        // plutôt que de déréférencer un pointeur nul.
        continue;
    }
    // monMoteur->dessiner(*mesh, transform.m_worldTransform, viewMatrix);
    // chaque SubMesh de *mesh porte déjà son MaterialHandle, résolu à son tour
    // via resourceManager.GetMaterial(submesh.material) au moment du dessin.
}
```

Différence de contrat par rapport à l'ancien `shared_ptr` : avec un handle, `GetMesh()` peut légitimement renvoyer `nullptr` (mesh déchargé depuis). L'ancien `shared_ptr` garantissait une durée de vie automatique tant qu'une référence existait ; le composant à handle n'offre plus cette garantie — le système doit gérer le cas explicitement, ce qui est le prix acceptable de ne plus dupliquer la propriété.

---

# Partie VIII — F5 : ResourceManager professionnel

## 8.1 Canonicalisation des chemins

**Le problème.** `m_pathToMesh` (`unordered_map<std::string, MeshHandle>`) compare des chaînes caractère par caractère. `"assets/x.obj"`, `"Assets/x.obj"`, `"assets\x.obj"` désignent le même fichier physique mais sont quatre clés distinctes selon la casse et les séparateurs employés — un même mesh peut être chargé plusieurs fois, silencieusement, sans erreur ni log.

**La solution :**
```cpp
std::string CanonicalKey(const std::string& filepath)
{
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(filepath, ec);
    // weakly_canonical, contrairement à canonical(), n'exige pas que le fichier
    // existe déjà — important puisque LoadMesh doit encore pouvoir échouer
    // proprement sur un chemin absent plutôt que lever une exception avant même
    // de vérifier son existence.
    return ec ? filepath : canonical.generic_string();
    // generic_string() force les '/' quelle que soit la plateforme.
}
```

Appliquée de façon cohérente dans `LoadMeshChecked`, `FindMesh`, `IsMeshLoaded` — toute méthode qui lit ou écrit dans le cache doit utiliser la même normalisation, sinon l'incohérence se reforme entre lecture et écriture.

## 8.2 UnloadMesh en O(1) — le principe du miroir, appliqué au ResourceManager

**Le problème.** `m_pathToMesh` va de chemin vers handle. Pour décharger un mesh, on dispose du handle mais on doit retrouver le **chemin** correspondant pour le retirer du cache — sans structure d'appui, la seule solution est un scan complet de la map, comparant chaque valeur. Exactement le même défaut structurel qu'un SparseSet sans miroir `m_Entities`.

**La solution — une map inverse, entretenue en parallèle :**
```cpp
std::unordered_map<uint32_t, std::string> m_meshIdToPath;   // le miroir

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
`m_pathToMesh` (chemin→handle) et `m_meshIdToPath` (id→chemin) forment les deux moitiés d'une même bijection, chacune en O(1) — c'est le principe du miroir de la Partie II, transposé du SparseSet au ResourceManager. `UnloadAll()` doit également vider `m_meshIdToPath`, sous peine de fuite logique.

## 8.3 Erreurs typées avec std::expected

**Le problème.** `MeshHandle::Invalid()` en retour d'échec ne porte qu'une information binaire (ça a marché ou pas). Impossible de distinguer fichier introuvable, parsing échoué, ou mesh vide — un `Logger::error` générique ne dit jamais *pourquoi*.

**La solution, honnête sur ses limites réelles :**
```cpp
enum class EMeshLoadError : std::uint8_t { FileNotFound, ParseFailed };

std::expected<MeshHandle, EMeshLoadError>
ResourceManager::LoadMeshChecked(const std::string& filepath, const OBJLoadOptions& opt)
{
    const std::string key = CanonicalKey(filepath);
    if (auto it = m_pathToMesh.find(key); it != m_pathToMesh.end()) return it->second;

    if (!fs::exists(filepath))
        return std::unexpected(EMeshLoadError::FileNotFound);   // distinction fiable

    MeshHandle h = OBJLoader::Load(filepath, *this, opt);
    if (!h.IsValid())
        return std::unexpected(EMeshLoadError::ParseFailed);    // fusionne "vide" et "malformé"

    m_pathToMesh.emplace(key, h);
    m_meshIdToPath.emplace(h.id, key);
    return h;
}

// LoadMesh() devient un adaptateur mince, pour compatibilité ascendante :
MeshHandle ResourceManager::LoadMesh(const std::string& filepath, const OBJLoadOptions& opt)
{
    auto result = LoadMeshChecked(filepath, opt);
    return result.has_value() ? *result : MeshHandle::Invalid();
}
```

**Pourquoi `std::expected` plutôt qu'une exception ?** Le fichier utilise déjà `noexcept` sur plusieurs méthodes (`AllocateMeshHandle`, etc.) — une exception romprait cette promesse et introduirait un coût de désenroulement de pile pour un cas qui n'a rien d'exceptionnel (un fichier manquant en développement est courant). `std::expected` documente l'échec **dans la signature elle-même** : quiconque lit le type de retour comprend immédiatement que l'échec fait partie du contrat.

**Limite honnête, documentée plutôt que dissimulée.** `OBJLoader::ParseFile` retourne un simple `bool` qui fusionne déjà en interne « fichier vide » et « aucune face valide ». `ParseFailed` regroupe donc ces deux causes distinctes ; les séparer réellement nécessiterait de faire remonter l'information depuis `OBJLoader` lui-même — chantier volontairement laissé de côté ici pour ne pas élargir le périmètre de F5. Un résidu de code mort a également été repéré à cette occasion dans `OBJLoader::Load` (un test `if (rawFaces.empty())` qui ne peut jamais être vrai à cet endroit, puisque `ParseFile` garantit déjà l'inverse s'il retourne `true`) — sans impact fonctionnel, noté pour un futur passage.

## 8.4 Test de validation (TNR)

`TestF5_ResourceManager_UnloadMesh()` charge plusieurs meshes, décharge le premier de la liste et vérifie : la bijection chemin↔handle est cohérente avant et après suppression ; les meshes non ciblés ne subissent aucun effet de bord (garde contre une erreur d'indexation dans la map inverse) ; un double-unload reste un no-op silencieux ; recharger le même chemin après déchargement fonctionne et attribue un **nouvel** id (`AllocateMeshHandle` ne recycle jamais ses ids, contrairement aux entités qui recyclent leurs index — deux politiques différentes, à connaître).

---

# Partie IX — RAII et cycle de vie

## 9.1 Pourquoi UnloadMesh n'est appelée nulle part aujourd'hui

Ce n'est pas un oubli : `UnloadMesh` répond à un besoin de déchargement **sélectif en cours de session** (changement de niveau, contrainte mémoire, hot-reload d'assets pendant le développement). Le moteur, à ce stade de sa progression, charge une scène une fois via le `Serializer` et tourne jusqu'à la fin — aucun système de gestion de niveaux n'existe encore pour déclencher un tel appel. C'est une pièce d'API complète, écrite en avance de son usage.

## 9.2 RAII : le nettoyage de fin de programme existe déjà, autrement

Deux mécanismes distincts se cachent derrière l'idée de « nettoyage automatique » :

**Mécanisme A — le RAII du C++ (Resource Acquisition Is Initialization).** Quand un objet sort de sa portée (fin de fonction, destruction de son propriétaire), son destructeur s'exécute **automatiquement, garanti par le langage**, à un point précis et déterministe. C'est ce mécanisme qui explique que `~ResourceManager()` appelle déjà `UnloadAll()` :
```cpp
ResourceManager::~ResourceManager() { UnloadAll(); }
```
`UnloadAll()` vide chaque `unordered_map<uint32_t, std::unique_ptr<MeshClass>>` via `.clear()` — ce qui détruit chaque `unique_ptr` contenu, donc chaque `MeshClass`, en cascade, par les règles standards du C++. C'est délibérément plus efficace qu'appeler `UnloadMesh()` élément par élément : quand **tout** doit disparaître, un `.clear()` global évite le travail de maintenance croisée du miroir qui n'a de sens que pour une suppression sélective.

**Mécanisme B — la récupération mémoire par l'OS à la fin du processus.** L'OS reprend toutes les pages mémoire allouées au processus quand celui-ci se termine, que les destructeurs C++ aient été appelés ou non.

**Pourquoi la distinction compte.** S'appuyer sur le mécanisme B plutôt que sur A ferait perdre trois garanties : le RAII s'exécute même en cas de sortie anticipée (return prématuré, exception qui remonte la pile) — l'OS ne nettoie qu'à la toute fin ; le RAII libère aussi des ressources **non-mémoire** (le jour où `MeshClass` encapsulera un identifiant GPU ou un descripteur de fichier, seul le code C++ sait parler au driver pour le libérer proprement) ; et surtout, le RAII est ce qui permettrait un rechargement de niveau **sans redémarrer le processus** — aucune sortie de processus n'a lieu dans ce cas, donc seul le mécanisme A peut fonctionner.

**Règle : ne jamais s'appuyer sur le nettoyage de l'OS pour une logique applicative.** Le RAII doit toujours être la garantie invoquée, jamais la fin du processus.

---

# Partie X — Synthèse des règles dictatoriales de la Leçon 3

1. Une `Entity` est un ticket daté, jamais un indice brut — tout accès tableau passe par `EntityIndex(e)`.
2. `IsAlive` est la comparaison de génération, rien d'autre — pas de recherche dans la free-list.
3. Dans `DestroyEntity` : notifier les storages d'abord, incrémenter la génération ensuite — l'inversion crée des composants fantômes.
4. Le sparse s'indexe par `EntityIndex(entity)` ; le miroir stocke toujours le handle complet, génération incluse.
5. Un itérateur qui fabrique sa valeur la retourne par valeur (itérateur proxy), jamais par référence vers un temporaire.
6. Ne jamais refaire un travail déjà capturé : si des pointeurs typés existent, on les utilise, on ne repasse pas par une résolution générique.
7. Un composant chaud est un POD : il référence les ressources par handle, il ne les possède jamais.
8. Un chemin de ressource est canonique avant d'être une clé de cache.
9. Une map à sens unique s'accompagne de son miroir dès que l'inverse est nécessaire — sinon la suppression ciblée redevient O(N).
10. Un typeID runtime ne se sérialise jamais — il dépend de l'ordre d'exécution, pas du code.
11. Un objet qui va finir dans un conteneur ne devrait naître qu'une fois, à sa place finale (`Emplace` plutôt que `Add` pour les composants avec indirection).
12. Le RAII est déterministe : ne jamais s'appuyer sur le nettoyage de l'OS en fin de processus pour une logique applicative.
13. Tout invariant binaire se verrouille par `static_assert` ; un assert inter-module (ex. dans `Add`) vaut mieux qu'un commentaire — il détonne au bon endroit si l'invariant est rompu ailleurs.

---

# Partie XI — Exercices d'auto-évaluation

1. Pourquoi `m_Sparse[entity]` (handle brut) plutôt que `m_Sparse[EntityIndex(entity)]` provoque-t-il un accès hors limites, et pas simplement un résultat incorrect ? Calcule la valeur décimale d'un handle d'index 2, génération 3, et compare-la à `MAX_ENTITIES_INIT`.
2. Un système garde une `Entity` en cache d'une frame à l'autre pour éviter de la relire. Quelle vérification doit-il faire avant de l'utiliser, et pourquoi cette vérification est-elle O(1) ?
3. Pourquoi `UnloadAll()` n'appelle-t-il pas `UnloadMesh()` sur chaque handle plutôt que de vider directement les conteneurs ?
4. Un composant contient un `std::vector<float>` de taille variable. Reste-t-il "trivialement copiable" au sens de `std::is_trivially_copyable_v` ? Que cela implique-t-il pour le coût du swap-and-pop qui le concerne ?
5. Pourquoi un itérateur qui retourne `value_type&` plutôt que `value_type` par valeur poserait-il un problème de durée de vie dès lors que sa valeur est fabriquée à la volée ? Que se passerait-il concrètement si `operator*` était appelé deux fois de suite sans stocker le résultat entre-temps ?
6. Le `ResourceManager` charge un mesh via un chemin non canonisé deux fois de suite avec une casse différente. Décris précisément la séquence d'événements en mémoire (combien de `MeshClass` existent, combien d'entrées dans chaque map).
7. Pourquoi la solution retenue pour l'erreur de chargement (`std::expected`) est-elle jugée préférable à une exception dans ce fichier précis, et pas dans l'absolu ?

---

# Annexe — Glossaire

- **ABA problem** : un état numérique repasse par la même valeur après un cycle de changements, trompant un observateur qui n'a mémorisé que la valeur et pas son "époque".
- **Cache miss** : donnée absente de tous les niveaux de cache CPU, nécessitant un accès RAM (~200 cycles).
- **ECS** : Entity-Component-System, paradigme de composition de données plutôt que d'héritage d'objets.
- **Itérateur proxy** : itérateur dont `operator*()` retourne une valeur fabriquée à la volée plutôt qu'une référence vers une donnée déjà stockée.
- **Prefetcher** : circuit matériel du CPU qui anticipe les accès mémoire linéaires et charge les lignes de cache suivantes en avance.
- **RAII** : Resource Acquisition Is Initialization — principe C++ liant la durée de vie d'une ressource à la durée de vie d'un objet, via le constructeur/destructeur.
- **Swap-and-pop** : technique de suppression dans un tableau compact, consistant à déplacer le dernier élément à la place de celui supprimé puis à réduire la taille du tableau.
- **Type erasure** : technique consistant à masquer le type concret d'un objet derrière une interface commune, pour pouvoir le stocker dans un conteneur homogène.
- **YAGNI** : *You Aren't Gonna Need It* — principe déconseillant d'ajouter une fonctionnalité tant qu'aucun besoin réel ne la justifie.
