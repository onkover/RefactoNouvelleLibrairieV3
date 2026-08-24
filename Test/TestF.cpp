#include "pch.h"

// Gestion du scenegraph
#include "Scene/Registry.hpp"
#include "Core/EventBus.hpp"
#include "Scene/SceneGraph.hpp"
#include "Scene/system.hpp"
#include "Scene/Serializer.hpp"

namespace LV3::Tests
{

	void TestF1_EntityVersioning()
	{
		LV3::Registry reg;

		// --- 1. Le recyclage réutilise l'index mais change le handle ---
		LV3::Entity a = reg.CreateEntity();							// index 0, gen 0
		reg.addComponent(a, LV3::HealthComponent{ 100, 100 });
		reg.DestroyEntity(a);
		LV3::Entity b = reg.CreateEntity();							// index 0, gen 1
		assert(LV3::EntityIndex(a) == LV3::EntityIndex(b));			// même slot...
		assert(a != b);												// ...ticket différent

		// --- 2. Le périmé est mort, le neuf est vivant ---
		assert(!reg.IsAlive(a));
		assert(reg.IsAlive(b));

		// --- 3. Aucun composant fantôme ne traverse les générations ---
		assert(!reg.hasComponent<LV3::HealthComponent>(a));			// handle périmé → refusé par Has()
		assert(!reg.hasComponent<LV3::HealthComponent>(b));			// nouvelle entité → vierge

		// --- 4. Le scénario TriggerComponent : un handle stocké survit à son entité ---
		LV3::Entity held = reg.CreateEntity();
		reg.DestroyEntity(held);
		//reg.DestroyEntity(held);				// Un assert doit se déclencher ici si on tente de détruire une entité déjà morte
		reg.CreateEntity();											// recycle le slot de 'held'
		assert(!reg.IsAlive(held));									// le voisin mémorisé est bien déclaré mort

		std::cout << "\033[32m[F1] Versionnage des entités : tous les invariants tiennent.\n";
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
			assert(result.has_value() && "\033[31mEchec de chargement — verifie que les chemins de test existent\033[0m");
			handles.push_back(*result);
		}

		const size_t countBefore = rm.GetMeshCount();
		assert(countBefore == paths.size());

		// --- CIBLE : le PREMIER mesh de la liste ---
		const MeshHandle target = handles[0];
		const std::string targetPath = paths[0];

		// 1. Invariants AVANT suppression — la cible est bien vivante et retrouvable dans les deux sens
		assert(rm.GetMesh(target) != nullptr);
		assert(rm.IsMeshLoaded(targetPath));
		assert(rm.FindMesh(targetPath) == target);

		// --- ACTION ---
		rm.UnloadMesh(target);

		// 2. Invariants APRÈS suppression
		assert(rm.GetMeshCount() == countBefore - 1);        // exactement un mesh de moins, pas plus
		assert(rm.GetMesh(target) == nullptr);               // le handle périmé résout désormais à null
		assert(!rm.IsMeshLoaded(targetPath));                // preuve que m_meshIdToPath a bien retrouvé
		assert(rm.FindMesh(targetPath) == MeshHandle::Invalid());  // et nettoyé m_pathToMesh — le cœur de F5

		// 3. Les AUTRES meshes ne doivent SUBIR AUCUN effet de bord
		//    (garde contre une éventuelle confusion d'index dans la map inverse)
		for (size_t i = 1; i < handles.size(); ++i)
		{
			assert(rm.GetMesh(handles[i]) != nullptr);
			assert(rm.IsMeshLoaded(paths[i]));
			assert(rm.FindMesh(paths[i]) == handles[i]);
		}

		// 4. Double-unload : doit être un no-op silencieux, jamais un crash
		//    (m_meshIdToPath ne retrouve plus rien pour ce handle -> if() ne s'exécute pas -> erase(id) sur un id déjà absent, sans effet)
		rm.UnloadMesh(target);
		assert(rm.GetMeshCount() == countBefore - 1);        // aucun décrément supplémentaire

		// 5. Recharger le même chemin doit fonctionner normalement, sans résidu de l'ancien handle
		auto reload = rm.LoadMeshChecked(targetPath, opts);
		assert(reload.has_value());
		assert(reload->id != target.id);                     // AllocateMeshHandle ne recycle jamais les ids : nouveau mesh, nouvel id
		assert(rm.IsMeshLoaded(targetPath));
		assert(rm.GetMeshCount() == countBefore);             // on est revenu au compte initial

		std::cout << "\033[32m[F5] UnloadMesh : tous les invariants tiennent.\n\033[0m";
	}
}