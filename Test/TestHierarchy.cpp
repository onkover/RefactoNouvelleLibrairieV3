#include "pch.h"          // ← première ligne, toujours
#include "scene/hierarchy.hpp"
#include "Core/Logger.h"

namespace LV3::Tests
{
	bool test_hierarchy()
	{
	// ============================================================
	// TEST 0 — sanite de HasOutgoingLinks — a retirer apres validation
	// ============================================================
	//	using namespace LV3;
		bool test1, test2;

		Registry testRegistry;
		Entity a = testRegistry.CreateEntity();
		Entity b = testRegistry.CreateEntity();

		linkChildToParent(testRegistry, b, a);   // b enfant de a

		test1 = HasOutgoingLinks(testRegistry, a);
		test2 = HasOutgoingLinks(testRegistry, b);
		Logger::info("[Test0] avant Detach — HasOutgoingLinks(a)="
			+ std::to_string(test1)
			+ "  HasOutgoingLinks(b)="
			+ std::to_string(test2));
		// Attendu : 1 et 1 (a a un enfant, b a un parent)

		if(test1 && test2)
			Logger::success("Attendu : 1 et 1 (a a un enfant, b a un parent) ==> OK");
		else
		{
			Logger::error("Attendu : 1 et 1 (a a un enfant, b a un parent) ==> KO");
			return false;
		}


		Detach(testRegistry, b);

		test1 = HasOutgoingLinks(testRegistry, a);
		test2 = HasOutgoingLinks(testRegistry, b);

		Logger::info("[Test0] apres Detach — HasOutgoingLinks(a)="
			+ std::to_string(test1)
			+ "  HasOutgoingLinks(b)="
			+ std::to_string(test2));

		if (!test1 && !test2)
			Logger::success("Attendu : 0 et 0 ==> OK");
		else
		{
			Logger::error("Attendu : 0 et 0 ==> KO");
			return false;
		}
		return true;
	}
}