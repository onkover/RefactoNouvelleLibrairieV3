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
#endif
}