# Leçon 04 — Le Rasterizer (Partie 1 : Triangle Setup)

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Namespace canonique :** `LV3` | **Affichage :** SDL2 (texture STREAMING, écriture pixel directe)

---

## 0. Position dans le pipeline

```
Model Space → World Space → View Space → Clip Space → NDC → Viewport (Screen Space)
                                                                    ↓
                                                    ▶▶▶ TRIANGLE SETUP ◀◀◀   ← Partie 1
                                                                    ↓
                                                    Edge Functions (test inside/outside)
                                                                    ↓
                                                    Coordonnées barycentriques
                                                                    ↓
                                                    Z-Buffer / Shading            ← Partie 2
```

**Périmètre de la Partie 1 :** tout ce qui se passe *après* la transformation viewport et *avant* le shading. Volontairement exclu : clipping near, correction perspective, Z-buffer.

---

## 1. Décision fondatrice — edge function, pas scanline

**Règle dictatoriale n°1 : le rasterizer LV3 est intégralement basé edge function (Pineda, 1988). Aucune implémentation scanline ne sera acceptée.**

| Critère | Scanline | Edge function |
|---|---|---|
| Cas particuliers (triangles plats, aigus, inversés) | Nombreux | **Aucun** |
| Parallélisation | Difficile (dépendances inter-lignes) | **Triviale** (chaque pixel indépendant) |
| Coordonnées barycentriques | À recalculer | **Gratuites** |
| Correspondance matérielle | Obsolète | **Méthode des GPU modernes** |

---

## 2. La fonction de bord

Pour une arête A→B et un point P :

```
E(A, B, P) = (B.x - A.x) * (P.y - A.y) - (B.y - A.y) * (P.x - A.x)
```

C'est le **double de l'aire signée** du triangle (A, B, P). Son signe indique de quel côté de la droite (AB) se trouve P.

Pour un triangle (V0, V1, V2) :

```
w0 = E(V1, V2, P)   ← poids de V0
w1 = E(V2, V0, P)   ← poids de V1
w2 = E(V0, V1, P)   ← poids de V2
```

**P est à l'intérieur si et seulement si w0, w1, w2 sont de même signe que l'aire totale.**

> ⚠️ **Piège d'indexation :** `w0` est le poids **du sommet V0**, pas de l'arête `V0V1`. Inversion classique lors de l'interpolation d'UV.

---

## 3. Règle dictatoriale n°2 — le winding order en espace écran

`EWindingOrder::CCW` est la convention front-face en coordonnées Y-up. **Mais le framebuffer a Y qui pointe vers le bas** (pixel (0,0) en haut-gauche).

**Conséquence : après transformation viewport, un triangle CCW en NDC devient visuellement CW en espace écran.**

Si le test de signe est câblé en dur sur `>= 0`, `ECullMode::Back` élimine les mauvaises faces — bug silencieux (rien ne crash, des faces disparaissent au mauvais endroit).

**Règle : test de signe et cull mode calibrés ensemble, une fois, avec test unitaire explicite.** La solution retenue teste dynamiquement le signe de l'aire, ce qui rend le rasterizer indifférent au winding :

```cpp
bool inside = (area > 0.0f) ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                             : (w0 <= 0 && w1 <= 0 && w2 <= 0);
```

---

## 4. Règle dictatoriale n°3 — le Top-Left Rule

**Problème :** quand `w == 0` (point exactement sur une arête), qui rasterise ce pixel — le triangle A ou le triangle B qui partage cette arête ?

| Sans règle | Conséquence |
|---|---|
| Aucun des deux | **Trou** visible entre triangles adjacents |
| Les deux | **Double blending** sur les bords (catastrophique en `EBlendMode::AlphaBlend`) |

**Convention professionnelle (DirectX, OpenGL, LV3) :** une arête *top* ou *left* inclut le pixel sur `== 0` ; une arête *bottom* ou *right* l'exclut.

- Arête **top** : horizontale, allant vers la gauche (`dy == 0 && dx < 0`)
- Arête **left** : allant vers le bas (`dy > 0`)

Chaque edge function reçoit un biais : `0` si top/left, `-1` sinon.

---

## 5. Code canonique

### `Rasterizer.h`

```cpp
#pragma once
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace LV3
{

struct Vec2f { float x, y; };

struct BarycentricWeights { float w0, w1, w2; }; // normalisés — somme == 1.0

using FragmentCallback = void(*)(int32_t x, int32_t y,
                                  const BarycentricWeights& bary,
                                  void* userData);

float EdgeFunction(const Vec2f& a, const Vec2f& b, const Vec2f& p);
bool  IsTopLeft(const Vec2f& a, const Vec2f& b);

void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
                        int32_t screenWidth, int32_t screenHeight,
                        FragmentCallback onFragment, void* userData);

} // namespace LV3
```

### `Rasterizer.cpp`

```cpp
#include "Rasterizer.h"

namespace LV3
{

float EdgeFunction(const Vec2f& a, const Vec2f& b, const Vec2f& p)
{
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool IsTopLeft(const Vec2f& a, const Vec2f& b)
{
    Vec2f edge{ b.x - a.x, b.y - a.y };
    bool isTop  = (edge.y == 0.0f) && (edge.x < 0.0f);
    bool isLeft = (edge.y > 0.0f);
    return isTop || isLeft;
}

void RasterizeTriangle(const Vec2f& v0, const Vec2f& v1, const Vec2f& v2,
                        int32_t screenWidth, int32_t screenHeight,
                        FragmentCallback onFragment, void* userData)
{
    // 1. Bounding box, clampée au viewport
    int32_t minX = static_cast<int32_t>(std::floor(std::min({ v0.x, v1.x, v2.x })));
    int32_t minY = static_cast<int32_t>(std::floor(std::min({ v0.y, v1.y, v2.y })));
    int32_t maxX = static_cast<int32_t>(std::ceil (std::max({ v0.x, v1.x, v2.x })));
    int32_t maxY = static_cast<int32_t>(std::ceil (std::max({ v0.y, v1.y, v2.y })));

    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    maxX = std::min(maxX, screenWidth  - 1);
    maxY = std::min(maxY, screenHeight - 1);

    // 2. Aire totale (2× aire signée) — normalise les barycentriques
    const float area = EdgeFunction(v0, v1, v2);
    if (area == 0.0f) return; // triangle dégénéré

    // 3. Biais top-left par arête
    const float bias0 = IsTopLeft(v1, v2) ? 0.0f : -1.0f;
    const float bias1 = IsTopLeft(v2, v0) ? 0.0f : -1.0f;
    const float bias2 = IsTopLeft(v0, v1) ? 0.0f : -1.0f;

    // 4. Boucle pixel
    for (int32_t y = minY; y <= maxY; ++y)
    {
        for (int32_t x = minX; x <= maxX; ++x)
        {
            Vec2f p{ x + 0.5f, y + 0.5f }; // CENTRE du pixel, jamais son coin

            float w0 = EdgeFunction(v1, v2, p) + bias0;
            float w1 = EdgeFunction(v2, v0, p) + bias1;
            float w2 = EdgeFunction(v0, v1, p) + bias2;

            bool inside = (area > 0.0f) ? (w0 >= 0 && w1 >= 0 && w2 >= 0)
                                         : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue;

            BarycentricWeights bary{ w0 / area, w1 / area, w2 / area };
            onFragment(x, y, bary, userData);
        }
    }
}

} // namespace LV3
```

> ⚠️ **`p.x + 0.5f, p.y + 0.5f`** — on teste le **centre** du pixel. Une erreur ici décale silencieusement toute la géométrie d'un demi-pixel.

---

## 6. Aparté — Le pointeur de fonction (`FragmentCallback`)

### Ce que c'est

```cpp
using FragmentCallback = void(*)(int32_t, int32_t, const BarycentricWeights&, void*);
```

**Ce n'est pas une fonction — c'est un alias de type**, au même titre que `using MeshHandle = uint32_t;`. Le type nommé est un *pointeur vers une fonction* de cette signature.

### Le mécanisme

Un pointeur de donnée contient l'adresse d'une donnée. Un pointeur de fonction contient l'**adresse du code compilé** d'une fonction.

```cpp
int Add(int a, int b) { return a + b; }
int Sub(int a, int b) { return a - b; }

int (*ptr)(int, int);
ptr = Add;            // ptr contient l'adresse de Add
int r1 = ptr(3, 4);   // saut à cette adresse → 7
ptr = Sub;            // ptr contient l'adresse de Sub
int r2 = ptr(3, 4);   // MÊME ligne d'appel → -1
```

**Un nom de fonction sans parenthèses se décompose automatiquement en son adresse.**

### Application au rasterizer

| Étape | Ce qui se passe |
|---|---|
| **A** | `ShadeFragment_Unlit` est écrite dans `Fragment.cpp` — vraie fonction, vrai corps, adresse fixe |
| **B** | `RasterizeTriangle(..., ShadeFragment_Unlit, &ctx)` — le nom se réduit à son adresse |
| **C** | La variable locale `onFragment` reçoit cette adresse (équivalent de `ptr = Add`) |
| **D** | `onFragment(x, y, bary, userData)` saute exécuter `ShadeFragment_Unlit` (équivalent de `ptr(3,4)`) |

**`RasterizeTriangle` ne connaît jamais `ShadeFragment_Unlit` par son nom.** Il connaît uniquement la *forme* de signature promise par `FragmentCallback`.

### Pourquoi `void* userData`

Un pointeur de fonction C brut **ne peut pas capturer de contexte** (contrairement à une lambda). Le seul canal pour transmettre framebuffer, Z-buffer, texture… est ce pointeur opaque, que l'implémentation réinterprète via `static_cast` vers **son** type de contexte.

> ⚠️ **Risque :** cast vers le mauvais type = comportement indéfini silencieux.

### Optimisation future (reportée)

Le pointeur de fonction brut **empêche l'inlining** — chaque fragment coûte un saut indirect. La version professionnelle template le callback :

```cpp
template<typename FragmentFn>
void RasterizeTriangle(..., FragmentFn&& onFragment)
```

Le compilateur connaît alors le type exact à la compilation et inline entièrement dans la boucle pixel. **Reporté volontairement** pour ne pas mélanger apprentissage du mécanisme et complexité template.

---

## 7. Architecture des fichiers

```
Rasterizer.h        → Vec2f, BarycentricWeights, FragmentCallback, RasterizeTriangle
   ↑
FrameBuffer.h       → Color, FrameBuffer (Bind, SetPixel)
   ↑
Fragment.h          → UnlitContext, DepthContext, ShadeFragment_*
   ↑
Renderer.h          → Triangle2D, Renderer::DrawTriangle
```

**Renderer.h n'inclut PAS Fragment.h** — le branchement des callbacks est un détail d'implémentation qui vit dans `Renderer.cpp`. Ce qui ne figure pas dans la signature publique ne remonte pas dans le header.

### `FrameBuffer.h`

```cpp
#pragma once
#include <cstdint>
#include <bit>
#include <cassert>

namespace LV3
{

struct Color { uint8_t b, g, r, a; }; // ordre mémoire pour ARGB8888 little-endian

class FrameBuffer
{
public:
    void Bind(void* lockedPixels, int32_t pitchBytes, int32_t w, int32_t h)
    {
        assert(lockedPixels != nullptr && "FrameBuffer::Bind — pixels null");
        assert(pitchBytes > 0 && "FrameBuffer::Bind — pitch invalide");
        m_Pixels = lockedPixels;
        m_Pitch  = pitchBytes;
        m_Width  = w;
        m_Height = h;
    }

    inline void SetPixel(int32_t x, int32_t y, Color color)
    {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return;
        uint8_t* row = reinterpret_cast<uint8_t*>(m_Pixels) + y * m_Pitch;
        reinterpret_cast<uint32_t*>(row)[x] = std::bit_cast<uint32_t>(color);
    }

    int32_t Width()  const { return m_Width; }
    int32_t Height() const { return m_Height; }

private:
    void*   m_Pixels = nullptr;
    int32_t m_Pitch  = 0;
    int32_t m_Width  = 0;
    int32_t m_Height = 0;
};

} // namespace LV3
```

### `Fragment.h` / `Fragment.cpp`

```cpp
// Fragment.h
#pragma once
#include "Rasterizer.h"
#include "FrameBuffer.h"

namespace LV3
{

struct UnlitContext { FrameBuffer* fb; Color color; };
struct DepthContext { FrameBuffer* fb; float z0, z1, z2; };

void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);
void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData);

} // namespace LV3
```

```cpp
// Fragment.cpp
#include "Fragment.h"

namespace LV3
{

void ShadeFragment_Unlit(int32_t x, int32_t y, const BarycentricWeights&, void* userData)
{
    auto* ctx = static_cast<UnlitContext*>(userData);
    ctx->fb->SetPixel(x, y, ctx->color);
}

void ShadeFragment_Depth(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
{
    auto* ctx = static_cast<DepthContext*>(userData);
    float depth = bary.w0 * ctx->z0 + bary.w1 * ctx->z1 + bary.w2 * ctx->z2;
    uint8_t gray = static_cast<uint8_t>(std::clamp(depth, 0.0f, 1.0f) * 255.0f);
    ctx->fb->SetPixel(x, y, Color{ gray, gray, gray, 255 });
}

} // namespace LV3
```

### `Renderer.cpp` — le point de branchement

```cpp
#include "Renderer.h"
#include "Rasterizer.h"
#include "Fragment.h"

namespace LV3
{

void Renderer::DrawTriangle(const Triangle2D& tri, FrameBuffer& fb, ERenderMode mode)
{
    switch (mode)
    {
        case ERenderMode::Solid:
        {
            UnlitContext ctx{ &fb, Color{60, 60, 255, 255} };
            RasterizeTriangle(tri.v0, tri.v1, tri.v2, fb.Width(), fb.Height(),
                               ShadeFragment_Unlit, &ctx);   // ← adresse de fonction n°1
            break;
        }
        case ERenderMode::Depth:
        {
            DepthContext ctx{ &fb, tri.z0, tri.z1, tri.z2 };
            RasterizeTriangle(tri.v0, tri.v1, tri.v2, fb.Width(), fb.Height(),
                               ShadeFragment_Depth, &ctx);   // ← adresse de fonction n°2
            break;
        }
        default: break;
    }
}

} // namespace LV3
```

**Point clé :** `RasterizeTriangle` est identique dans les deux branches. Seule l'adresse contenue dans `onFragment` change.

---

## 8. Écriture pixel SDL2 — les trois règles

### Règle 1 : Lock une fois par frame, jamais par pixel

`SDL_LockTexture` est un appel système. Locker/délocker par pixel est catastrophique et viole le principe même du framebuffer CPU.

```cpp
void* pixels = nullptr;
int   pitch  = 0;

if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0)
{
    SDL_Log("SDL_LockTexture a échoué : %s", SDL_GetError());
    return;
}

frameBuffer.Bind(pixels, pitch, width, height);
// ... toute la boucle de rendu ...
SDL_UnlockTexture(texture);
SDL_RenderCopy(renderer, texture, nullptr, nullptr);
SDL_RenderPresent(renderer);
```

### Règle 2 : le `pitch` n'est PAS `width * 4`

SDL peut aligner chaque ligne sur une frontière mémoire (16/32/64 octets) pour permettre des accès vectorisés côté GPU. Le `pitch` est la **vraie distance en octets** entre deux débuts de ligne — parfois supérieure à `width * 4` à cause d'un padding invisible.

**Pourquoi `y * width + x` a longtemps fonctionné :** beaucoup de largeurs « rondes » (256, 512, 640, 1920…) s'alignent naturellement, donc `pitch == width * 4` et les deux formules coïncident. **C'est de la chance, pas un contrat.**

| Formule | Unité | Validité |
|---|---|---|
| `y * width + x` | pixels | ❌ Casse dès qu'il y a padding |
| `y * pitch` (octets) puis `[x]` (pixels) | mixte | ✅ Toujours correct |

**Le `pitch` REMPLACE `width * 4`, il ne s'y ajoute pas.** `y * pitch + width + x` sauterait une ligne supplémentaire par pixel.

### Le double cast est obligatoire

```cpp
uint8_t* row = reinterpret_cast<uint8_t*>(m_Pixels) + y * m_Pitch;  // arithmétique en OCTETS
reinterpret_cast<uint32_t*>(row)[x] = color;                         // indexation en PIXELS
```

L'arithmétique de pointeur avance par **taille du type pointé**. `reinterpret_cast<uint32_t*>(m_Pixels) + y * m_Pitch` multiplierait le pitch par 4 — saut 4× trop loin par ligne.

### Règle 3 : format de pixel fixé explicitement

```cpp
SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                   SDL_TEXTUREACCESS_STREAMING, width, height);
```

Format non fixé = canaux R et B potentiellement inversés — bug qui ne casse rien structurellement et peut passer inaperçu longtemps.

---

## 9. Débogage SDL — vérification systématique des retours

**Règle dictatoriale : toute fonction SDL retournant un pointeur ou un code d'erreur est vérifiée immédiatement.**

En cas d'échec, `SDL_LockTexture` retourne `-1` et **ne touche ni le pointeur ni le pitch**. Symptôme observé en session : `ptrScreen` semblait contenir une adresse (reliquat de mémoire non initialisée) alors que `pitch` valait 0. **Le pitch à 0 était le seul des deux à dire la vérité.**

| Erreur SDL | Cause |
|---|---|
| `Parameter 'texture' is invalid` | Texture `nullptr` — `SDL_CreateTexture` a échoué ou n'a jamais été appelée |
| `texture not created with SDL_TEXTUREACCESS_STREAMING` | Texture créée en `STATIC` ou `TARGET` |
| Lock échoue en frame N+1 | `SDL_UnlockTexture` manquant sur un chemin de sortie |

**Piège du shadowing :** une variable locale portant le même nom que le membre de classe reçoit la texture, tandis que le membre reste `nullptr`.

```cpp
bool Engine::Init()
{
    SDL_Texture* SDLtexture = SDL_CreateTexture(...); // ← BUG : nouvelle variable locale
    // m_SDLtexture (le membre) n'est jamais assigné
}
```

---

## 10. Suite de validation (Partie 1 — tous validés ✅)

| # | Test | Critère de réussite |
|---|---|---|
| **1** | Winding order — inverser V1/V2 à l'appel | Le triangle s'affiche **dans les deux sens** |
| **2** | Top-left rule — quad en 2 triangles de couleurs différentes | Aucun pixel de fond visible sur la diagonale |
| **3** | Clamping — triangle largement hors écran | Affichage partiel, **aucun crash** |
| **4** | Barycentriques — `ShadeFragment_Barycentric` | Dégradé RGB, sommets en couleur pure |

### Test 2 — vérification déterministe (au-delà de l'inspection visuelle)

Un double-blending ne se voit pas en opaque (le second triangle écrase le premier, résultat identique). **Il faut compter, pas regarder :**

```cpp
struct CountContext { std::vector<uint8_t>* counts; int32_t width; };

void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights&, void* userData)
{
    auto* ctx = static_cast<CountContext*>(userData);
    (*ctx->counts)[y * ctx->width + x]++;  // ici width suffit : buffer propriétaire, pas SDL
}

// après rendu des deux triangles :
for (auto c : counts)
    assert(c <= 1 && "Top-left rule violée : pixel rasterisé deux fois");
```

**À conserver dans la suite de tests** — détectera toute régression lors de l'introduction du clipping (Partie 2), qui génère de nouveaux triangles partageant des arêtes.

### Test 4 — fonction de référence

```cpp
void ShadeFragment_Barycentric(int32_t x, int32_t y, const BarycentricWeights& bary, void* userData)
{
    auto* ctx = static_cast<UnlitContext*>(userData);
    ctx->fb->SetPixel(x, y, Color{
        static_cast<uint8_t>(bary.w2 * 255.0f), // b
        static_cast<uint8_t>(bary.w1 * 255.0f), // g
        static_cast<uint8_t>(bary.w0 * 255.0f), // r
        255
    });
}
```

**Test de non-régression permanent** : tout bug futur du triangle setup se verra comme une déformation du dégradé. Servira aussi de référence visuelle en Partie 2 pour constater à quel point l'interpolation affine est fausse en perspective.

---

## 11. Limite connue de la Partie 1

L'interpolation barycentrique implémentée est **affine** (linéaire en espace écran) :

```cpp
float depth = w0 * v0.z + w1 * v1.z + w2 * v2.z;
```

**Elle est mathématiquement fausse en perspective** dès que le triangle n'est pas parallèle au plan de la caméra. La correction (division par `1/w`) est reportée à la Partie 2 — mélanger les deux empêcherait de distinguer un bug de topologie d'un bug de perspective.

---

## 12. Synthèse des règles dictatoriales

| # | Règle |
|---|---|
| **1** | Rasterizer intégralement basé edge function. Aucun scanline. |
| **2** | Test de signe et cull mode calibrés ensemble, avec test unitaire explicite. |
| **3** | Top-left rule obligatoire (biais `0` / `-1` par arête). |
| **4** | Test au **centre** du pixel (`+0.5f`), jamais au coin. |
| **5** | `SDL_LockTexture` une fois par frame, jamais par pixel. |
| **6** | Toute indexation dans un buffer SDL passe par le `pitch`, jamais par `width` recalculé. |
| **7** | Format de pixel fixé explicitement (`ARGB8888`), jamais deviné. |
| **8** | Toute fonction SDL retournant un pointeur ou un code d'erreur est vérifiée immédiatement. |

---

## 13. Prochaine étape — Leçon 04 Partie 2

- **Clipping near-plane en espace de clip** (avant division perspective)
- **Interpolation perspective-correcte** (division par `1/w`)
- **Z-Buffer en reverse-Z** (précision flottante optimale près de la caméra)
