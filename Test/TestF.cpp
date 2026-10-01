#include "pch.h"

// Gestion du scenegraph
#include "Scene/Registry.hpp"
#include "Core/EventBus.hpp"
//#include "Scene/SceneGraph.hpp"
#include "Scene/system.hpp"
#include "Scene/Serializer.hpp"

namespace LV3::Tests
{
#if LV3_DEBUG
	void TestF1_EntityVersioning()
	{
		LV3::Registry reg;

		// --- 1. Le recyclage réutilise l'index mais change le handle ---
		LV3::Entity a = reg.CreateEntity();							// index 0, gen 0
		reg.addComponent(a, LV3::HealthComponent{ 100, 100 });
		reg.DestroyEntity(a);
		LV3::Entity b = reg.CreateEntity();							// index 0, gen 1
		LV3_ASSERT(LV3::EntityIndex(a) == LV3::EntityIndex(b));			// même slot...
		LV3_ASSERT(a != b);												// ...ticket différent

		// --- 2. Le périmé est mort, le neuf est vivant ---
		LV3_ASSERT(!reg.IsAlive(a));
		LV3_ASSERT(reg.IsAlive(b));

		// --- 3. Aucun composant fantôme ne traverse les générations ---
		LV3_ASSERT(!reg.hasComponent<LV3::HealthComponent>(a));			// handle périmé → refusé par Has()
		LV3_ASSERT(!reg.hasComponent<LV3::HealthComponent>(b));			// nouvelle entité → vierge

		// --- 4. Le scénario TriggerComponent : un handle stocké survit à son entité ---
		//LV3::Entity held = reg.CreateEntity();
		//reg.DestroyEntity(held);
		////reg.DestroyEntity(held);						// Un assert doit se déclencher ici si on tente de détruire une entité déjà morte
		//LV3::Entity Result = reg.CreateEntity();		// recycle le slot de 'held' - Resuklt est prévsent en raison du [[nodiscard]] sinon il y aura un warnin. A voir s'il faut à l'avenir tester ce retour
		//assert(!reg.IsAlive(held));									// le voisin mémorisé est bien déclaré mort

		// --- 4. Le scénario TriggerComponent : un handle stocké survit à son entité ---
		LV3::Entity held = reg.CreateEntity();
		const std::uint32_t heldIndex = LV3::EntityIndex(held);
		reg.DestroyEntity(held);

		// Le slot de 'held' est REOCCUPE : c'est ce qui rendrait le handle
		// perime dangereux si le versionnage ne faisait pas son travail.
		LV3::Entity recycled = reg.CreateEntity();
		LV3_ASSERT(LV3::EntityIndex(recycled) == heldIndex);   // meme slot, prouve le recyclage
		LV3_ASSERT(recycled != held);                          // ticket different
		LV3_ASSERT(!reg.IsAlive(held));                        // l'ancien est bien mort
		LV3_ASSERT(reg.IsAlive(recycled));                     // le neuf est bien vivant

		Logger::success("[F1]Versionnage des entités : tous les invariants tiennent");
	}

	void TestF5_ResourceManager_UnloadMesh()
	{
		ResourceManager rm;
		OBJLoadOptions opts;

		// Charge plusieurs meshes distincts — adapte ces chemins à des .obj réels de ton projet
		const std::vector<std::string> paths = {
			"Assets/Meshes/cube.obj",
			"Assets/Meshes/sphere 10 faces.obj"
		};

		std::vector<MeshHandle> handles;
		for (const auto& p : paths)
		{
			auto result = rm.LoadMeshChecked(p, opts);
			LV3_ASSERT(result.has_value() && "\033[31mEchec de chargement — verifie que les chemins de test existent\033[0m");
			handles.push_back(*result);
		}

		const size_t countBefore = rm.GetMeshCount();
		LV3_ASSERT(countBefore == paths.size());

		// --- CIBLE : le PREMIER mesh de la liste ---
		const MeshHandle target = handles[0];
		const std::string targetPath = paths[0];

		// 1. Invariants AVANT suppression — la cible est bien vivante et retrouvable dans les deux sens
		LV3_ASSERT(rm.GetMesh(target) != nullptr);
		LV3_ASSERT(rm.IsMeshLoaded(targetPath));
		LV3_ASSERT(rm.FindMesh(targetPath) == target);

		// --- ACTION ---
		rm.UnloadMesh(target);

		// 2. Invariants APRÈS suppression
		LV3_ASSERT(rm.GetMeshCount() == countBefore - 1);        // exactement un mesh de moins, pas plus
		LV3_ASSERT(rm.GetMesh(target) == nullptr);               // le handle périmé résout désormais à null
		LV3_ASSERT(!rm.IsMeshLoaded(targetPath));                // preuve que m_meshIdToPath a bien retrouvé
		LV3_ASSERT(rm.FindMesh(targetPath) == MeshHandle::Invalid());  // et nettoyé m_pathToMesh — le cœur de F5

		// 3. Les AUTRES meshes ne doivent SUBIR AUCUN effet de bord
		//    (garde contre une éventuelle confusion d'index dans la map inverse)
		for (size_t i = 1; i < handles.size(); ++i)
		{
			LV3_ASSERT(rm.GetMesh(handles[i]) != nullptr);
			LV3_ASSERT(rm.IsMeshLoaded(paths[i]));
			LV3_ASSERT(rm.FindMesh(paths[i]) == handles[i]);
		}

		// 4. Double-unload : doit être un no-op silencieux, jamais un crash
		//    (m_meshIdToPath ne retrouve plus rien pour ce handle -> if() ne s'exécute pas -> erase(id) sur un id déjà absent, sans effet)
		rm.UnloadMesh(target);
		LV3_ASSERT(rm.GetMeshCount() == countBefore - 1);        // aucun décrément supplémentaire

		// 5. Recharger le même chemin doit fonctionner normalement, sans résidu de l'ancien handle
		auto reload = rm.LoadMeshChecked(targetPath, opts);
		LV3_ASSERT(reload.has_value());
		LV3_ASSERT(reload->id != target.id);                     // AllocateMeshHandle ne recycle jamais les ids : nouveau mesh, nouvel id
		LV3_ASSERT(rm.IsMeshLoaded(targetPath));
		LV3_ASSERT(rm.GetMeshCount() == countBefore);             // on est revenu au compte initial

		Logger::success("[F5] UnloadMesh : tous les invariants tiennent.");		
	}

	// Test 4c : chargement des chaines de LOD des rochers.
// dir : dossier contenant rock_x.obj, rock_x_Lk.obj et rock_x.lod.json
//       (le meme prefixe que celui utilise par la scene pour rock_a.obj).
	bool TestLodChain_Load(const std::string& dir)
	{
		using namespace LV3;
		int failures = 0;
		auto check = [&failures](bool ok, const std::string& what) {
			if (!ok) { ++failures; Logger::error("[TestLodChain] ECHEC : " + what); }
			};

		ResourceManager rm;

		// 1. Les trois chaines se chargent
		LodChainHandle h[3];
		const char* names[3] = { "rock_a", "rock_b", "rock_c" };
		for (int i = 0; i < 3; ++i)
		{
			const auto r = rm.LoadLodChainChecked(dir + "/" + names[i] + ".lod.json");
			check(r.has_value(), std::string(names[i]) + " : chargement");
			h[i] = r.value_or(LodChainHandle::Invalid());
		}
		check(rm.GetLodChainCount() == 3, "3 chaines enregistrees");

		// 2. Contenu de rock_a
		const LodChain* a = rm.GetLodChain(h[0]);
		check(a != nullptr, "rock_a accessible");
		if (a)
		{
			check(a->levelCount == 4, "rock_a : 4 niveaux");
			check(a->bounds.IsValid(), "rock_a : bounds non vide");
			// L'union contient la boite de L0 (et peut la depasser)
			const AABB3d b0 = rm.GetMesh(a->levels[0])->GetMeshAABB();
			check(a->bounds.Contains(b0.min) && a->bounds.Contains(b0.max), "rock_a : bounds contient L0");

			// 3. Selection : invEps(rock_a) ~ 2,645 / 1,781 / 1,580
			check(SelectLodLevel(*a, 3.0f) == 0, "q'=3,0 -> L0");
			check(SelectLodLevel(*a, 2.0f) == 1, "q'=2,0 -> L1");
			check(SelectLodLevel(*a, 1.7f) == 2, "q'=1,7 -> L2");
			check(SelectLodLevel(*a, 1.0f) == 3, "q'=1,0 -> L3");
			check(SelectLodLevel(*a, std::numeric_limits<float>::infinity()) == 0, "q'=+inf -> L0");
			check(SelectLodLevel(*a, std::numeric_limits<float>::quiet_NaN()) == 0, "q'=NaN -> L0");
		}

		// 4. Cache : meme fichier, ecriture differente -> meme handle, aucune chaine de plus
		const auto again = rm.LoadLodChainChecked(dir + "\\.\\rock_a.lod.json");
		check(again.has_value() && *again == h[0], "cache : meme handle pour un chemin equivalent");
		check(rm.GetLodChainCount() == 3, "cache : toujours 3 chaines");

		// 5. Partage : L0 de la chaine EST le mesh rock_a.obj du cache (aucun double chargement)
		if (a)
			check(rm.FindMesh(dir + "/rock_a.obj") == a->levels[0], "L0 partage avec rock_a.obj");

		// 6. Erreur typee : fichier absent (un Logger::error est ATTENDU ici)
		// Le fichier absent.lod.json n'existe pas, et ne doit pas exister. 
		// On demande volontairement au ResourceManager de charger un fichier introuvable, pour vérifier deux choses :
		//		1. le chargement échoue proprement, sans exception ni plantage;
		//		2.l'erreur rendue est la bonne : FileNotFound, et pas ParseFailed ou une autre catégorie.
		Logger::info("[TestLodChain] Test 6 : l'erreur FileNotFound qui suit est VOLONTAIRE");
		const auto missing = rm.LoadLodChainChecked(dir + "/absent.lod.json");
		check(!missing && missing.error() == ELodChainLoadError::FileNotFound, "absent -> FileNotFound");

		Logger::info("[TestLodChain] " + std::string(failures == 0 ? "OK" : "ECHEC") +
			" (" + std::to_string(failures) + " echec(s))");
		return failures == 0;
	}

#endif
}