---
name: architecture-rules
description: Binding architectural rules and hard-won engine learnings — dirty-flag contract, scene/ECS invariants, rendering pipeline, testing
sources: [backfill, cowork]
aliases: [règles dictatoriales, R11bis]
---
## Architecture rules

- [stated] R11bis (dirty flag contract): only the writer of `m_local` may raise the dirty flag; only `LocalTransformSystem`, after actual recomposition, may lower it — never inside read-only or display systems.
- [stated] S3 (Anchor/Body pattern): orbital position and body orientation never share a node.
- [stated] S4: a component contains only what its system reads/writes.
- [stated] S7, S9: trigger radius is a gameplay value in world units, never inherits scale.
- [stated] S10 (doctrine du temps): time scale is multiplicative, like camera zoom. Two deltas coexist and are never interchangeable: `realDt` (wall-clock, clamped, feeds `SimulationClock::Advance`) drives player/device-facing systems — `PlayerInputSystem`, `CameraFPSControllerSystem`, `CameraFollowSystem` — which must keep responding while the simulation is paused or time-scaled; `simDt` (returned by `Advance`, gated by pause/scale) drives world-simulated systems — `AnimationSystem` (orbites, spins). A system takes whichever dt matches what it represents, jamais les deux, jamais par supposition.
- [stated] S13 (ordre canonique de la boucle principale, discussion D — numéro provisoire à confirmer avec Onky): DEUX passes de cuisson des matrices par frame, pas une. Par frame, dans cet ordre strict — (1) mesurer `realDt`, clamper ; (2) avancer l'horloge (`simDt = _clock.Advance(realDt)`) ; (3) construire l'`InputState` de la frame, toujours avant tout système qui le lit (bug 45 : `PlayerInputSystem` s'exécutait avant `BuildInputState()`) ; (4) systèmes qui écrivent `m_local` d'entités du MONDE — `PlayerInputSystem` sur `realDt`, `AnimationSystem` sur `simDt` ; (5) CUISSON N°1, complète, sur toute la scène — `LocalTransformSystem(registry)` puis `WorldTransformSystem(registry)` : sans cette passe intermédiaire, les systèmes caméra liraient un `m_worldMatrix` vieux d'une frame (`CameraFollowSystem` lisant la position de sa cible juste déplacée par `PlayerInputSystem`) ; (6) systèmes caméra qui LISENT ces matrices fraîches et ÉCRIVENT `m_local` des caméras — `CameraFPSControllerSystem`/`CameraFollowSystem` sur `realDt`, `CameraZoomSystem`, puis sélection de caméra, `BuildCameraBindings`, `CameraGizmoSystem` ; (7) CUISSON N°2, CIBLÉE — `LocalTransformSystem(registry)` (dirty-only, quasi gratuit) puis `WorldTransformSystem(registry, roots)` avec `roots` = les seules caméras rendues cette frame (jamais la forme complète ici : coûterait O(N) pour ~2 entités changées, cf. le pattern de re-cuisson ciblée plus bas) ; (8) `TriggerSystem` et tout ce qui lit `m_worldMatrix` ; (9) construction des vues puis rendu.
- [stated] S11: orbital angles are calculated from the simulated date, never accumulated.
- [stated] S12: an event names a transition, not its context.
- [stated] S1: a scene stores only real physical units.
- [stated] `TriggerSystem` rule (L06): never execute user code inside collision detection; use a flat event queue with Enter/Stay/Exit and no callbacks during iteration.
- [stated] `CameraBinding` separates static associations from derived matrices, avoiding one-frame-lag bugs.
- [stated] A `hasFarPlane` boolean plus an unconditionally assigned `farPlane` is preferred over sentinel values.

## Rendering pipeline learnings

- [stated] Reverse-Z (`near → 1`, `far → 0`): the two non-linearities (perspective `1/z` storage + float ULP distribution) cancel rather than compound; depth clear = `0.0f`, early-Z test uses `>`; enables an infinite far plane via `PerspectiveInfinite()`.
- [stated] Orthographic camera immobility on forward/backward motion is a mathematical property (no Z division), not a bug.
- [stated] Front-face area is negative in raster space (Y-flip); the backface culling sign must account for this.
- [stated] Clipping seam cracks require both an antisymmetric `EdgeFunction` formulation and a canonical-order `ClipLess` comparator.
- [stated] `invW` is the critical passenger across the clip→NDC boundary; `nearPlane`/`farPlane` in `FragmentContext` were retired in favor of `invW`-based distance reconstruction.

## ECS / systems learnings

- [stated] Entity versioning: 16-bit index / 16-bit generation packing in `Entity` (repack from the original 24/8 split — décidé pour le bug 54 : 65 536 entités max, 65 536 recyclages d'un slot avant bouclage de l'ABA, marge ×16 au-dessus de `LV3_MAX_ENTITIES`).
- [stated] The `ComponentView` iterator must use its captured storage pointer tuple directly, not re-query the Registry per entity.
- [stated] A paged sparse array (`PagedSparseArray`, 4096 entries/page) prevents memory waste with high entity indices.
- [stated] `active`/`priority` (camera lens, render policy) and `enabled` (controller, system writes) are distinct, orthogonal concepts.
- [stated] Targeted re-bake pattern: `WorldTransformSystem(registry, roots)` re-propagates only the given root entities (and their descendants) instead of retraversing the whole scene — reuses `PropagateWorld` directly. Used for the second per-frame bake pass (cameras + their gizmo children), so its cost stays O(rendered cameras) rather than O(N) as the scene grows. Precondition, asserted (`LV3_ASSERT(IsRoot(registry, e))`), never assumed: every entity in `roots` must actually be a root (no parent) — a camera qualifies today because no scene parents a camera.

## Testing

- [stated] TNR (non-regression test) system is based on `JsonReader` with `WarnUnread()`, with numbered bugs tracked in a journal.
- [stated] Coverage tests (pixel-counting) are effective for diagnosing clipping correctness.
- [stated] All new systems integrate with the `WarnUnread`/`LV3_ASSERT`/`Logger` pattern.
