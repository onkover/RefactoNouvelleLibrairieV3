# Annexe A10 — Build portable Debug / Release

**Projets concernés** : `LibraryV3` (LIB) et `RefactoNouvelleLibrairieV3` (EXE)
**Objet** : rendre la configuration `Release|x64` constructible, supprimer toute dépendance à la machine de développement, et séparer les commutateurs de compilation.
**Statut** : appliqué et validé — zéro erreur, zéro avertissement en `Debug|x64` et `Release|x64`, sur deux machines distinctes.

---

## 1. Le point de départ

Avant cette phase, la configuration `Release|x64` n'avait **jamais été construite**. Ce n'était pas un oubli anodin : cinq défauts indépendants l'en empêchaient, chacun suffisant à lui seul.

| # | Défaut | Conséquence |
|---|---|---|
| 1 | `PROJECT_DIR` défini uniquement en `Debug\|x64` | `main.cpp` ne compile pas en Release (`std::string cheminProjet = PROJECT_DIR;`) |
| 2 | `LanguageStandard` = `stdcpp20` en Release, `stdcpp23` en Debug | `std::expected` (F5, `LoadMeshChecked`) invalide en Release |
| 3 | Chemins et bibliothèques SDL2 absents de `Release\|x64` | Ni compilation, ni édition de liens |
| 4 | `_DEBUG` défini en `Release\|x64` (copier-coller depuis Debug) | `_ITERATOR_DEBUG_LEVEL` à 2 côté EXE, 0 côté LIB → `LNK2038` ou corruption silencieuse |
| 5 | `PrecompiledHeader=Create` sur `pch.cpp` conditionné à `Debug\|x64` | Le `.pch` n'est produit dans aucune autre configuration → `C1083` généralisé |

S'ajoutaient deux chemins absolus versionnés (`G:\Projects Visual Studio\...` et `C:\Pascal_Perso\...`, les deux machines codées en dur), un `/arch:AVX` asymétrique, et `WarningLevel3` partout.

### Pourquoi c'était bloquant

La tâche restante de la leçon 06 est une **mesure de performance**. Mesurer en Debug n'a aucun sens : `/Od` supprime l'inlining, l'itérateur de la STL paie ses contrôles de validité, et `ComponentView`, `EdgeFunction` et les opérateurs de `Vec3f` dépendent entièrement de l'inlining. On mesurerait le coût de ne pas inliner, dans des rapports qui ne se transposent pas.

> **Contre-exemple à écarter** : « on profilera en Debug, les proportions restent les mêmes ». Faux. La configuration Release n'est pas une variante de Debug, c'est un programme différent.

---

## 2. Architecture retenue

### 2.1 Trois fichiers, trois natures

```
G:\Projects Visual Studio\
├── LibraryV3\                          (dépôt 1)
│   └── LibraryV3.vcxproj
└── RefactoNouvelleLibrairieV3\         (dépôt 2)
    ├── .gitignore
    ├── Build\
    │   ├── LV3.Common.props            VERSIONNÉ    — réglages d'équipe
    │   ├── LV3.User.props.example      VERSIONNÉ    — modèle
    │   └── LV3.User.props              IGNORÉ       — chemins de la machine
    ├── RefactoNouvelleLibrairieV3.slnx
    └── RefactoNouvelleLibrairieV3.vcxproj
```

**Un seul dossier `Build\`**, dans le dépôt de l'EXE. La LIB ne possède pas les `.props`, elle les désigne par chemin relatif. Deux exemplaires d'un même fichier de configuration finiraient par diverger — c'est le mécanisme du bug 35 (fonction dupliquée LIB/EXE), transposé au build.

**Contrepartie assumée** : `LibraryV3` ne se construit plus seul. Cloné sans `RefactoNouvelleLibrairieV3` à côté, la cible de garde arrête le build avec un message nommé. C'est une dépendance d'**outillage**, pas de code : aucun `#include` de la LIB ne pointe vers l'EXE. À remplacer par un dépôt `LV3.Build` (ou un sous-module) le jour où la LIB devra être construite isolément.

### 2.2 Le partage des rôles

| Fichier | Contient | Versionné |
|---|---|---|
| `LV3.Common.props` | Standard C++, conformité, niveau d'avertissement, options de ligne de commande | Oui |
| `LV3.User.props` | Uniquement des chemins de machine (`SDL2_DIR`, `SDL2_TTF_DIR`, `LIBRARYV3_DIR`) | **Non** |
| `<ItemDefinitionGroup>` sans `Condition` | Ce qui doit être identique dans toutes les configurations (includes, bibliothèques) | — |
| `<ItemDefinitionGroup Condition=...>` | Ce qui distingue **légitimement** Debug de Release | — |

**Règle de tri** : ce qui devrait être identique entre Debug et Release part dans le `.props`. Ce qui les distingue réellement (`Optimization`, `RuntimeLibrary`, `PreprocessorDefinitions`, `FunctionLevelLinking`) reste dans le bloc conditionnel.

---

## 3. `Build\LV3.Common.props`

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <PropertyGroup>
    <!-- Témoin lu par la cible de garde : si ce fichier n'est pas chargé,
         le build s'arrête au lieu de compiler avec les réglages par défaut. -->
    <LV3CommonPropsLoaded>true</LV3CommonPropsLoaded>
  </PropertyGroup>

  <ItemDefinitionGroup>
    <ClCompile>
      <LanguageStandard>stdcpp23</LanguageStandard>
      <ConformanceMode>true</ConformanceMode>
      <WarningLevel>Level4</WarningLevel>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
      <AdditionalOptions>/utf-8 /Zc:__cplusplus /we4456 /we4457 /we4458 /we4459 /we4553 /we4700 /we4701 /we4715 %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
  </ItemDefinitionGroup>

</Project>
```

### Pourquoi chaque option

| Option | Raison |
|---|---|
| `/utf-8` | Sources et littéraux en UTF-8. Son absence en Release ressuscitait le bug 12 (chaînes ANSI). |
| `/Zc:__cplusplus` | MSVC renvoie `199711L` pour `__cplusplus` par défaut, quel que soit `/std:`. Sans ce flag, `json.hpp`, SDL et toute bibliothèque tierce prennent silencieusement leur branche C++98. **Aucune case dans l'IDE** : cette option se pose ici ou nulle part. |
| `/we4456` `/we4457` `/we4458` `/we4459` | Masquage de variable. `C4459` a immédiatement révélé le **bug 52** : `SimulationClock _clock` déclarée deux fois, les quatre commandes temporelles du clavier pilotant une horloge jamais avancée. |
| `/we4553` | Expression sans effet. C'est l'avertissement qui avait été émis et ignoré lors du bug 23. |
| `/we4700` `/we4701` | Variable non initialisée. |
| `/we4715` | Chemin de sortie sans valeur de retour. |
| `%(AdditionalOptions)` | **Impératif.** Sans lui, la valeur n'hérite pas — elle écrase. Voir §7. |

### Ce qui n'est délibérément PAS dans ce fichier

**Aucun `/arch:`.** L'EXE portait un `/arch:AVX` en `Debug|x64` uniquement. Un flag d'architecture global est incompatible avec le dispatch runtime prévu pour la leçon 13 : le SIMD passera par des `.cpp` dédiés, compilés chacun avec son `/arch:` et sélectionnés par `__cpuid` au démarrage. Un flag de projet ne peut pas faire de dispatch runtime, par construction.

---

## 4. `Build\LV3.User.props`

**Le seul fichier du projet autorisé à contenir un chemin absolu.**

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="UserMacros">
    <SDL2_DIR>G:\code\lib\SDL2\SDL2-2.26.2</SDL2_DIR>
    <SDL2_TTF_DIR>G:\code\lib\SDL2\SDL2_ttf-2.0.14</SDL2_TTF_DIR>
    <LIBRARYV3_DIR>G:\Projects Visual Studio\LibraryV3</LIBRARYV3_DIR>
  </PropertyGroup>
</Project>
```

Un exemplaire par poste. `LV3.User.props.example` est versionné comme modèle.

### `.gitignore`

À la **fin** du fichier (le `.gitignore` standard de GitHub reste intact en tête, remplaçable plus tard sans écraser les ajouts) :

```gitignore
# --- LibraryV3 / RefactoNouvelleLibrairieV3 ---
# Chemins locaux à la machine. Un exemplaire par poste, jamais versionné.
# Modèle suivi : Build/LV3.User.props.example
Build/LV3.User.props
```

> **Piège** : `.gitignore` ne s'applique qu'aux fichiers **non suivis**. Si le fichier a déjà été ajouté à l'index : `git rm --cached Build/LV3.User.props`. Le `--cached` est impératif — sans lui, `git rm` efface le fichier du disque.

---

## 5. Câblage dans les `.vcxproj`

### 5.1 Définir `LV3BuildDir` — tout en haut, après `<Project>`

**EXE** :
```xml
<PropertyGroup>
  <LV3BuildDir>$(MSBuildThisFileDirectory)Build\</LV3BuildDir>
</PropertyGroup>
```

**LIB** :
```xml
<PropertyGroup>
  <LV3BuildDir>$(MSBuildThisFileDirectory)..\RefactoNouvelleLibrairieV3\Build\</LV3BuildDir>
</PropertyGroup>
```

> **Pourquoi pas `$(SolutionDir)`** : cette macro n'est définie que lorsque le build passe par la solution. En ligne de commande, depuis un script ou un pipeline, elle est vide — et le `Condition="exists(...)"` devient faux **en silence**, produisant un binaire compilé sans aucun des réglages. `$(MSBuildThisFileDirectory)` désigne le dossier du fichier qui contient l'expression, et vaut toujours quelque chose.

### 5.2 Importer — en DERNIÈRE position de chaque `<ImportGroup Label="PropertySheets">`

Les **quatre** groupes, dans les **deux** projets :

```xml
<ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
  <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
  <Import Project="$(LV3BuildDir)LV3.Common.props" Condition="exists('$(LV3BuildDir)LV3.Common.props')" />
  <Import Project="$(LV3BuildDir)LV3.User.props" Condition="exists('$(LV3BuildDir)LV3.User.props')" />
</ImportGroup>
```

**La position est le point le plus délicat de tout ce montage.** Voir §7 — c'est là que deux tentatives ont échoué.

### 5.3 La cible de garde — avant `</Project>`

```xml
<Target Name="LV3CheckProps" BeforeTargets="ClCompile" Condition="'$(LV3CommonPropsLoaded)' != 'true'">
  <Error Text="LV3.Common.props introuvable via LV3BuildDir=$(LV3BuildDir) — les réglages communs ne sont PAS appliqués. Corrige LV3BuildDir dans ce .vcxproj." />
</Target>
```

Un fichier de configuration absent produit une **erreur de build nommée, avec le chemin testé**, au lieu d'un binaire compilé avec les mauvais réglages. Même discipline que le `#error` sur `__cpp_lib_expected` : l'absence d'une condition requise doit être bruyante.

### 5.4 Le bloc sans `Condition` — EXE

```xml
<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalIncludeDirectories>$(LIBRARYV3_DIR);$(SDL2_DIR)\include;$(SDL2_TTF_DIR)\include;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
  </ClCompile>
  <Link>
    <AdditionalLibraryDirectories>$(SDL2_DIR)\lib\x64;$(SDL2_TTF_DIR)\lib\x64;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    <AdditionalDependencies>SDL2.lib;SDL2_ttf.lib;%(AdditionalDependencies)</AdditionalDependencies>
  </Link>
</ItemDefinitionGroup>
```

### 5.5 Le bloc sans `Condition` — LIB

```xml
<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalIncludeDirectories>$(ProjectDir);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    <PrecompiledHeader>Use</PrecompiledHeader>
    <PrecompiledHeaderFile>pch.h</PrecompiledHeaderFile>
  </ClCompile>
</ItemDefinitionGroup>
```

Et sur `pch.cpp`, **sans condition** :

```xml
<ClCompile Include="pch.cpp">
  <PrecompiledHeader>Create</PrecompiledHeader>
</ClCompile>
```

C'était le défaut n°5 : limité à `Debug|x64`, aucune autre configuration ne produisait le `.pch` que toutes cherchaient.

### 5.6 Ce qui reste dans les blocs conditionnels

| Reste par configuration | Part dans le `.props` ou le bloc sans `Condition` |
|---|---|
| `Optimization` | `LanguageStandard` |
| `RuntimeLibrary`, `BasicRuntimeChecks` | `ConformanceMode` |
| `PreprocessorDefinitions` (`_DEBUG` / `NDEBUG`) | `WarningLevel` |
| `FunctionLevelLinking`, `IntrinsicFunctions` | `AdditionalOptions` |
| `GenerateDebugInformation` | `AdditionalIncludeDirectories` |
| `EnableCOMDATFolding`, `OptimizeReferences` | `AdditionalLibraryDirectories`, `AdditionalDependencies` |
| `SDLCheck`, `WholeProgramOptimization` | `PrecompiledHeader*` |

**Supprimé purement et simplement** : `EnableEnhancedInstructionSet` (`/arch:AVX`), `IntrinsicFunctions` et `FavorSizeOrSpeed` dans une configuration *Debug*, `AssemblerOutput=All` (pour étudier le codegen, c'est en Release qu'il faut le remettre, ponctuellement — le listing d'un `/Od` n'apprend rien sur l'optimisation).

---

## 6. La racine de contenu — `ResolveContentRoot`

### 6.1 Le problème

```cpp
std::string cheminProjet = PROJECT_DIR;   // chemin ABSOLU de la machine qui compile
```

> **Contre-exemple à refuser** : ajouter `PROJECT_DIR` aux définitions de `Release|x64`. Deux lignes, ça compile, ça tourne — et on produit un exécutable Release qui ne fonctionne que sur la machine qui l'a compilé, cherche ses assets dans un dossier de développement, et casse dès qu'on le copie ailleurs.

> **Règle R36.** Un binaire ne connaît jamais l'arborescence de la machine qui l'a produit. La racine du contenu est une donnée d'exécution : elle se résout au démarrage, par ordre de priorité explicite, et le chemin du projet n'en est que le dernier repli — réservé au confort du développeur.

### 6.2 `Core/ContentRoot.h` — n'inclut QUE `<filesystem>`

```cpp
#pragma once
#include <filesystem>
#include <span>

namespace LV3
{
    inline constexpr const char* kContentMarker = "engine.json";

    [[nodiscard]] std::filesystem::path ExecutableDir();

    // Ordre : 1. LV3_CONTENT_ROOT (env)  2. dossier de l'exe  3. extraCandidates
    // Le moteur ignore ce que sont ces candidats : il ne connaît que le marqueur.
    [[nodiscard]] std::filesystem::path ResolveContentRoot(
        std::span<const std::filesystem::path> extraCandidates = {});
}
```

> **Règle R37.** Un en-tête n'impose jamais à ses consommateurs les dépendances de son implémentation. `ContentRoot.cpp` est le seul fichier qui inclut `Platform.h` (et donc `windows.h`) pour ce service.

### 6.3 `ExecutableDir` — les pièges Win32

`MAX_PATH` (`minwindef.h`) vaut 260, terminaison nulle comprise. **Les fonctions `…W` ne sont pas limitées à 260** : jusqu'à 32 767 caractères. `GetModuleFileNameW` (`libloaderapi.h`, `kernel32.dll`, lié par défaut) :

- **succès** → nombre de `wchar_t` copiés, **zéro final exclu** (donc `n < taille du buffer`)
- **buffer trop petit** → renvoie exactement `nSize`, chaîne tronquée, `ERROR_INSUFFICIENT_BUFFER`
- **échec** → 0

```cpp
std::filesystem::path ExecutableDir()
{
    // hModule = nullptr -> le module de l'EXE du processus.
    // /!\ Si LibraryV3 devient une DLL, ceci renverra toujours l'EXE hôte :
    //     il faudrait GetModuleHandleExW + GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS.
    std::wstring buf(MAX_PATH, L'\0');

    for (;;)
    {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) { Logger::error("ExecutableDir : code=" + std::to_string(GetLastError())); return {}; }
        if (n < buf.size()) { buf.resize(n); break; }
        if (buf.size() >= 32768) { Logger::error("ExecutableDir : chemin > 32767"); return {}; }
        buf.resize(buf.size() * 2);
    }

    std::error_code ec;
    const std::filesystem::path full{ buf };
    const std::filesystem::path canon = std::filesystem::weakly_canonical(full, ec);
    return ec ? full.parent_path() : canon.parent_path();
}
```

> **Contre-exemples** : `argv[0]` décrit *comment on t'a appelé*, pas *où tu es* (nom nu résolu via `PATH`, chemin relatif, lien non résolu). `current_path()` vaut `$(ProjectDir)` depuis l'IDE, le dossier de l'EXE depuis l'explorateur, `C:\Windows\System32` depuis certains raccourcis.

### 6.4 `ResolveContentRoot` — trois candidats, ordre non négociable

```cpp
static bool IsContentRoot(const std::filesystem::path& dir)
{
    if (dir.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(dir / kContentMarker, ec) && !ec;
}
```

**On teste la présence du marqueur, jamais l'existence du dossier seul** : un `x64\Release\` vide capterait la résolution et court-circuiterait les replis suivants.

1. **`LV3_CONTENT_ROOT` (variable d'environnement)** — l'opérateur a toujours le dernier mot. Lu avec `GetEnvironmentVariableW`, pas `std::getenv` : ce dernier est narrow (chemin accentué illisible selon la page de code), déclenche `C4996` sous `/W4`, et renvoie un pointeur vers un tampon partagé.
2. **Dossier de l'exécutable** — le seul cas qui fonctionne sur une machine qui n'a jamais vu le code source.
3. **`extraCandidates`** — fournis par l'application.

En cas d'échec, **la liste des chemins essayés est journalisée**. Un « fichier introuvable » sans cette liste coûte une heure ; avec, trente secondes.

**Pas de cache interne** (`static const auto root = ...`) : ce serait un état global caché, initialisé à un moment que personne ne contrôle, impossible à réinitialiser dans un test, et une seconde autorité à côté d'`EngineConfig`. `main()` résout une fois, stocke, distribue.

### 6.5 La macro, côté application seulement

Une macro est **locale à son unité de compilation**. Une LIB est compilée avec ses propres définitions, avant que l'EXE n'existe ; `#ifdef LV3_PROJECT_DIR` dans un `.cpp` de la LIB est donc toujours faux.

> **Règle R38.** Deux projets qui compilent un même nom de macro avec deux valeurs différentes produisent deux vérités dans un seul binaire. Si ce nom influence une fonction `inline` d'un en-tête partagé, c'est une violation de l'ODR — silencieuse.

`main.cpp` :

```cpp
int main(int argc, char* argv[])
{
    std::vector<std::filesystem::path> devCandidates;

    if (argc > 1) devCandidates.emplace_back(argv[1]);   // racine en argument

#ifdef LV3_PROJECT_DIR
    devCandidates.emplace_back(LV3_PROJECT_DIR);         // confort, Debug uniquement
#endif

    const std::filesystem::path contentRoot = LV3::ResolveContentRoot(devCandidates);
    if (contentRoot.empty()) { Logger::error("Arrêt : aucune racine de contenu."); return -1; }
```

`RefactoNouvelleLibrairieV3.vcxproj`, `Debug|x64` uniquement :

```xml
<PreprocessorDefinitions>LV3_PROJECT_DIR=R"($(ProjectDir))";_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
```

> **Le `R"(...)"` n'est pas décoratif.** `$(ProjectDir)` se termine **toujours** par un antislash. En littéral classique, cet antislash échappe le guillemet fermant, la chaîne dévore la suite de la ligne de commande, et on récolte une erreur incompréhensible située très loin de la cause.

`extraCandidates` étant un `span` et non une macro, la racine devient une donnée d'exécution : la campagne de mesure pourra lancer **la même binaire Release** sur les trois scènes en passant la racine en argument.

### 6.6 Copie du contenu — `LV3CopyContent`

```xml
<Target Name="LV3CopyContent" AfterTargets="Build">
  <!-- ItemGroup DANS la cible : évalué au build, donc un asset ajouté depuis
       le dernier chargement de la solution est pris en compte. -->
  <ItemGroup>
    <LV3ContentRoot Include="$(ProjectDir)engine.json;$(ProjectDir)config.json" />
    <LV3Assets Include="$(ProjectDir)assets\**\*" />
    <LV3Runtime Include="$(ProjectDir)*.dll;$(ProjectDir)tahoma.ttf" />
  </ItemGroup>

  <Copy SourceFiles="@(LV3ContentRoot);@(LV3Runtime)" DestinationFolder="$(OutDir)" SkipUnchangedFiles="true" />

  <!-- %(RecursiveDir) ne contient que la partie captée par le '**' : il faut
       reposer 'assets\' devant, sinon assets\Meshes\rock_a.obj atterrit dans
       $(OutDir)Meshes\ et tous les chemins relatifs de engine.json cassent. -->
  <Copy SourceFiles="@(LV3Assets)" DestinationFiles="@(LV3Assets->'$(OutDir)assets\%(RecursiveDir)%(Filename)%(Extension)')" SkipUnchangedFiles="true" />

  <Message Importance="high" Text="[LV3] Contenu copié vers $(OutDir)" />
</Target>
```

- `SkipUnchangedFiles="true"` : un build qui ne touche pas aux assets ne recopie rien.
- `AfterTargets="Build"` plutôt qu'un `PostBuildEvent` : incrémental, messages dans la fenêtre de sortie, et un échec de copie devient une erreur de build.
- **Pas conditionnée à Release** : Debug est copié aussi, pour que le chemin de livraison soit testé à chaque build. Corollaire : les DLL SDL2 cessent de dépendre du répertoire de travail du débogueur.

> **Contre-exemple à refuser absolument** : `robocopy ... /MIR`. L'option miroir **supprime** de la destination tout ce qui n'est pas dans la source — soit l'`.exe`, les `.pdb` et les DLL. On effacerait le produit du build avec l'étape censée le compléter.

### 6.7 Dette ouverte — `config.json`

`config.json` porte encore `REP_OBJ_DEFAULT` et `REP_GFX_DEFAULT` en **absolu**. Copier le fichier à côté de l'exécutable ne suffira pas : il renverra le moteur vers `G:\Projects Visual Studio\`.

> **Règle R39.** Aucun chemin absolu dans un fichier de contenu. Tout chemin y est relatif à la racine ; c'est le programme qui le compose avec la racine résolue.

**Piège associé** : `operator/` de `std::filesystem::path` **écrase entièrement le membre de gauche si le membre de droite est absolu**.

```cpp
contentRoot / path("assets/mesh/terre.obj")   // -> contentRoot\assets\mesh\terre.obj
contentRoot / path("G:/OBJ/terre.obj")        // -> G:/OBJ/terre.obj   (contentRoot JETÉ)
```

Sans garde, la composition compile, s'exécute, et ignore purement et simplement la racine résolue. D'où, à la lecture de chaque chemin du JSON :

```cpp
LV3_ASSERT(!p.is_absolute() && "Chemin absolu dans un fichier de contenu");
```

`is_absolute()` est un examen purement syntaxique (aucun accès disque). Sous Windows, il exige racine de **nom** *et* racine de **répertoire** : `\Assets\x.obj` et `C:Assets\x.obj` sont **relatifs**, malgré les apparences. `has_root_path()` ne remplace pas `is_absolute()`.

**Reste à faire** : chemins relatifs dans `config.json`, ou suppression des doublons (`engine.json` porte déjà `resources.assets/mesh/scene` — deux fichiers décrivant les mêmes chemins, c'est une autorité de trop, R20).

---

## 7. Les deux échecs du montage, et la règle qui en sort

Le câblage des `.props` a échoué **deux fois** avant de fonctionner. Les deux causes méritent d'être documentées : elles sont invisibles et se reproduiront.

### 7.1 Échec 1 — `ImportGroup` en double

Les nouveaux `ImportGroup` avaient été ajoutés **sans supprimer les anciens**. Pour chaque configuration, le groupe d'origine était évalué *après* le nouveau et réimportait `Microsoft.Cpp.$(Platform).user.props` par-dessus `LV3.Common.props`.

**Symptôme** : `stdcpp23`, `Level4` et `/permissive-` passaient, mais **pas** `AdditionalOptions`. Le fichier utilisateur global de Visual Studio (`%LOCALAPPDATA%\Microsoft\MSBuild\v4.0\Microsoft.Cpp.x64.user.props`) contenait un `AdditionalOptions` **sans** `%(AdditionalOptions)` — donc il écrasait au lieu d'hériter, et ne touchait qu'à cette seule métadonnée.

**Vérification** : exactement **quatre** `ImportGroup Label="PropertySheets"` par projet.

### 7.2 Échec 2 — `<Import>` nus placés trop haut

Dans la LIB, les imports étaient restés en `<Import>` nus juste après `Microsoft.Cpp.props`, tandis que les quatre `ImportGroup` (qui réimportent le fichier utilisateur global) venaient **après**. Même effet.

### 7.3 La règle

> **Règle R40.** Toute métadonnée d'item définie dans un `.vcxproj` doit se terminer par `%(NomDeLaMétadonnée)`. Sans lui, elle ne complète pas l'héritage : elle l'efface. C'est le même mécanisme que l'oubli de `$(IncludePath)` à la fin d'un `IncludePath`, et il est silencieux dans les deux cas.

> **Position correcte** : nos feuilles doivent être en **dernière position de la chaîne d'imports**, c'est-à-dire à la fin de chaque `<ImportGroup Label="PropertySheets">` — pas simplement « après `Microsoft.Cpp.props` ».

### 7.4 Méthode de diagnostic

- Le **Gestionnaire de propriétés** (Affichage → Autres fenêtres) n'affiche que les feuilles importées dans un `<ImportGroup Label="PropertySheets">`. Un `<Import>` nu est appliqué mais invisible.
- Le champ **gris** de Propriétés → C/C++ → Ligne de commande affiche la commande réellement passée à `cl.exe`. **C'est lui qui tranche.**
- Ce n'est pas le raisonnement qui a trouvé la cause : deux hypothèses plausibles et fausses ont précédé. C'est un `grep` sur les fichiers de build. Même discipline que pour le moteur : **ne crois pas la documentation, lis la source**.

---

## 8. Séparation des commutateurs de compilation

### 8.1 Le point de départ

```cpp
#ifdef _DEBUG
    #define LV3_DEBUG           1
    #define LV3_ASSERT(x)       assert(x)
    #define LV3_DEBUG_LOG       1
    #define LV3_VERBOSE_LOG     1
#else
    #define LV3_DEBUG           0
    #define LV3_ASSERT(x)       ((void)0)
    ...
#endif
```

**Quatre commutateurs, une seule condition.** Impossible d'activer les invariants sans réactiver les journaux verbeux et le `RenderSystem` console — donc en pratique, on ne les active jamais.

> **Règle R45.** Tests et invariants ne se gouvernent pas par le même commutateur. Un test vérifie un comportement à la demande, hors production ; un invariant garde une propriété pendant l'exécution, y compris là où ça compte. Les lier à `_DEBUG` revient à retirer le filet exactement là où on tombe.

### 8.2 Quatre commutateurs indépendants

```cpp
#ifndef LV3_ASSERTS_ENABLED
    #ifdef _DEBUG
        #define LV3_ASSERTS_ENABLED 1
    #else
        #define LV3_ASSERTS_ENABLED 0
    #endif
#endif
// idem pour LV3_DEBUG, LV3_DEBUG_LOG, LV3_VERBOSE_LOG
```

Le `#ifndef` en tête est le point clé : une configuration de projet peut forcer la valeur sans toucher au fichier. Les valeurs par défaut dérivent de `_DEBUG`, donc Debug et Release se comportent comme avant.

| Ce que fait le code | Commutateur |
|---|---|
| Vérifie une propriété qui doit toujours être vraie | `LV3_ASSERT` |
| Fonction de validation d'état (`CheckSceneInvariants`, `ValidateHierarchy`) | `#if LV3_ASSERTS_ENABLED` |
| Outil de développement, dump, rendu console | `#if LV3_DEBUG` |
| Trace de diagnostic | `LV3_LOG_DEBUG` |
| Trace détaillée, par entité ou par frame | `LV3_LOG_VERBOSE` |
| Erreur réelle, chemin d'échec | `Logger::error`, **sans garde** |

### 8.3 `LV3_ASSERT` indépendant de `assert`

`assert` de la bibliothèque standard se neutralise dès que **`NDEBUG`** est défini — ce que fait Release. Un `LV3_ASSERT(x) → assert(x)` aurait donc produit un commutateur qui ne commute rien : `LV3_ASSERTS_ENABLED=1` en Release n'aurait rien activé.

```cpp
namespace LV3
{
    // Inline : vit dans config.h. Utilise fprintf et non Logger, car
    // Logger.h inclut config.h -> cycle d'inclusion assuré.
    [[noreturn]] inline void AssertFailed(const char* expr, const char* file, int line)
    {
        std::fprintf(stderr, "\n[LV3_ASSERT] %s\n  %s:%d\n", expr, file, line);
        std::fflush(stderr);
    #ifdef _MSC_VER
        __debugbreak();     // s'arrête DANS le débogueur, au bon endroit
    #endif
        std::abort();
    }
}

#if LV3_ASSERTS_ENABLED
    // __VA_ARGS__ et non (x) : LV3_ASSERT(std::min(a,b) > 0) serait
    // vue comme DEUX arguments avec la forme (x).
    #define LV3_ASSERT(...) \
        do { if (!(__VA_ARGS__)) LV3::AssertFailed(#__VA_ARGS__, __FILE__, __LINE__); } while (0)
#else
    // sizeof : l'expression est COMPILÉE (donc type-vérifiée et comptée
    // comme un usage des variables) mais jamais ÉVALUÉE.
    #define LV3_ASSERT(...) ((void)sizeof(!(__VA_ARGS__)))
#endif
```

**Le `sizeof` est le gain principal.** L'ancien `((void)0)` jetait l'expression sans la compiler :

- Les variables utilisées uniquement dans un assert devenaient mortes en Release → `C4189` en cascade. Avec `sizeof`, elles restent référencées : l'avertissement s'éteint **parce que le code est correct**, pas parce qu'on l'a masqué.
- Un assert non compilé pouvait référencer un membre supprimé ou une signature changée sans que personne ne le sache. Chaque build Release type-vérifie désormais tous les asserts.

**Gain immédiat constaté** : la première compilation a échoué sur `FragmentContext::magic`, champ déclaré sous `#if LV3_DEBUG` et lu par une assertion désormais compilée dans toutes les configurations. Incohérence présente depuis longtemps, invisible jusque-là.

> **Règle R47.** Une donnée qui n'existe que pour être vérifiée par une assertion partage impérativement le commutateur de cette assertion. Deux `#if` distincts sur un témoin et son contrôle finissent toujours par diverger — et la divergence ne se voit que dans la configuration qu'on compile le moins.

> **Règle R46.** Une assertion n'a jamais d'effet de bord. `LV3_ASSERT(++i < n)` ou `LV3_ASSERT(reg.CreateEntity() != NULL_ENTITY)` : l'expression est compilée mais **jamais exécutée** quand les asserts sont éteints. Debug et Release divergent, et le bug ne se manifeste que là où on ne le cherche pas.

### 8.4 `config.h` est une feuille de l'arbre d'inclusion

> **Règle R48.** Un en-tête de configuration est une feuille : tout le monde l'inclut, il n'inclut personne du projet. Une macro qui nomme un service du moteur ne crée aucune dépendance d'en-tête — seule l'unité qui l'utilise doit inclure ce service.

```cpp
// config.h — DÉFINITION : aucun include de Logger nécessaire
#if LV3_DEBUG_LOG
    #define LV3_LOG_DEBUG(msg)   LV3::Logger::info(msg)
#else
    #define LV3_LOG_DEBUG(msg)   ((void)0)
#endif
```

```cpp
// ContentRoot.cpp — APPEL : ce fichier inclut déjà Logger.h
LV3_LOG_DEBUG("[Content] racine = " + exeDir.string());
```

`config.h` ne doit contenir que `<cstdio>`, `<cstdlib>`, des `constexpr`, des `#define` et `AssertFailed`. **Aucune instruction exécutable hors d'un corps de fonction** — un appel collé au niveau du namespace produit un `C2653` qui masque le vrai défaut de structure.

Placer les macros **hors** du `namespace LV3` : le préprocesseur ignore les namespaces, mais les y laisser laisse croire que `LV3_ASSERT` est qualifié alors qu'il est global.

---

## 9. La suite de tests

`TestCameraZoom.cpp` enfermait tout son contenu dans `#ifdef _DEBUG`, tandis que `RunAllTests.cpp` déclarait et appelait sans garde. Résultat : `LNK2001`, **uniquement en Release**.

> **Règle R43.** Une garde de compilation autour d'une **définition** doit avoir sa jumelle autour de chaque **déclaration** et de chaque **appel**. Une définition conditionnelle appelée inconditionnellement ne produit jamais d'erreur de compilation : elle produit une erreur d'édition de liens, à des kilomètres de la cause, et seulement dans la configuration où la condition est fausse.

**Sens de la correction** : garder la garde sur la définition et l'ajouter à l'appel — **pas** l'inverse. Le corps de ces tests repose sur des assertions ; sans garde, ils parcourraient leurs sections sans rien vérifier et annonceraient `tous les cas passent`. **Un test qui ne peut plus échouer ment.**

```cpp
bool RunAllTests(Registry& registry)
{
#ifdef _DEBUG
    // ... tout le corps
    return s_failures == 0;
#else
    (void)registry;
    Logger::info("[TNR] Suite de tests désactivée en Release : aucun test exécuté.");
    return true;
#endif
}
```

Les `static` du fichier (`s_failures`, `Run`) doivent entrer dans la même garde, sinon `C4505`.

> **Règle R44.** Un avertissement se corrige en supprimant sa cause, jamais en supprimant sa visibilité. `(void)x` et `[[maybe_unused]]` sont réservés au cas où l'inutilisation est réellement voulue et définitive.

> **Règle R49.** Quand `[[nodiscard]]` et « variable non référencée » se contredisent, ce n'est jamais un conflit de règles : c'est que la valeur a un sens que le code n'a pas encore exploité. Le `[[nodiscard]]` dit « regarde-moi » ; le `C4189` dit « tu m'as rangée sans me regarder ».

Cas concret — `TestF1_EntityVersioning` §4 : l'entité recyclée n'était pas un déchet, c'était **le cœur du test**. Sans vérifier qu'elle réoccupe le slot, un `Registry` qui ne recyclerait jamais ses index passerait le test.

```cpp
LV3::Entity held = reg.CreateEntity();
const std::uint32_t heldIndex = LV3::EntityIndex(held);
reg.DestroyEntity(held);

LV3::Entity recycled = reg.CreateEntity();
LV3_ASSERT(LV3::EntityIndex(recycled) == heldIndex && "le slot n'a pas été recyclé");
LV3_ASSERT(recycled != held);
LV3_ASSERT(!reg.IsAlive(held));
LV3_ASSERT(reg.IsAlive(recycled));
```

---

## 10. Résultat et validation

**Zéro erreur, zéro avertissement** en `Debug|x64` et `Release|x64`, sur les deux projets, avec `Level4` et huit catégories promues en erreurs.

La preuve de portabilité est dans la sortie de build elle-même : le projet, configuré sur un poste (`G:\Projects Visual Studio\`), a été compilé sur un autre (`C:\Pascal_Perso\source\repos\`) sans modifier un seul fichier versionné.

### Bugs découverts par cette phase

| # | Bug | Révélé par |
|---|---|---|
| 52 | `SimulationClock _clock` déclarée deux fois — les 4 commandes temporelles clavier pilotent une horloge jamais avancée | `C4459` |
| 61 | `capacity` et `slotCount` reçus et jamais lus dans `CameraBinding` — contrat de borne non tenu | `C4100` |
| — | Suite de tests inerte en Release (valeurs attendues calculées, jamais comparées) | `C4189` en cascade |
| — | `FragmentContext::magic` sous `LV3_DEBUG` alors que son assertion est sous `LV3_ASSERTS_ENABLED` | `sizeof` du nouveau `LV3_ASSERT` |
| — | `Release\|x64` définissait `_DEBUG` — `_ITERATOR_DEBUG_LEVEL` incohérent avec la LIB | lecture du `.vcxproj` |

### Test de livraison

Copier le dossier `x64\Release\` complet ailleurs (autre disque, `C:\Temp\`) et lancer l'exécutable par double-clic, **sans Visual Studio**. S'il démarre et charge la scène, la livraison est correcte. Sinon, le journal `[Content] essayé : …` indique quel candidat a été testé et rejeté.

---

## 11. Reste ouvert

| Objet | Détail |
|---|---|
| `config.json` | Chemins absolus `REP_OBJ_DEFAULT` / `REP_GFX_DEFAULT` ; doublon avec `engine.json` (R20) |
| `LV3_ASSERT` sur `is_absolute()` | Garde à poser à la lecture de chaque chemin du JSON |
| `assert(` restants | Passe `grep` à terminer ; `#include <cassert>` à retirer là où il ne servait qu'à ça |
| Configurations Win32 | Toujours présentes et jamais construites ; suppression via le Gestionnaire de configurations |
| `Core\Types.h`, `LibraryV3.cpp` | Présents sur le disque, absents du build (R29) |
| `Scene\CameraBinding.CPP` | Extension en majuscules |
| `.vcxproj` de l'EXE | Référence `Documentations\Lecon_04_Rasterizer_Partie2.md` ; annexes A6 à A9 absentes |
| Journal de bugs | Deux autorités de numérotation (audit jusqu'à 51, A9 a émis 32-35). Un seul compteur, reprise à 52. |
| `Gfx.cpp(32)` | Fonction-souche à cinq paramètres inutilisés : implémenter ou supprimer |

### Suite immédiate de la phase 0

1. **`TriggerSystem`** — un `std::set` alloué *et copié* par entité et par frame, plus `std::cout` par événement. **Actif en Release, sous aucune garde.** Sur la scène ceinture, l'allocateur et la console domineront le profil et masqueront le O(N²) qu'on cherche à mesurer.
2. **`RenderSystem` console** — appelé chaque frame sous `#if LV3_DEBUG`, une ligne par entité.
3. **`RelWithAsserts`** — dupliquer **`Release`** (surtout pas Debug, sinon `LV3_DEBUG=1` ramène le `RenderSystem` console) et ajouter `LV3_ASSERTS_ENABLED=1`. Protocole de mesure : une passe en `RelWithAsserts` pour valider que la scène est saine, une passe en `Release` pur pour les chiffres.
