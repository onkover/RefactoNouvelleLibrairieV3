#define GFX
#include "pch.h"          // ← première ligne, toujours
#include "gfx.h"





//************************************
void Init_Render()
{

	//displayNormal(false);
	//displayEdge(false);


	
}

void Clean_Render()
{
	//myZBuf->CleandepthBuffer(float(-pFrustum->farClippingPlane));
//	CleanScreenASM((__m256i*)(myScreen.ptrScreen), (myScreen.Width >> 3) * (myScreen.Height));

}

/*
	Convention des matrices : Column major (vecteurs colonnes) => OpenGL et Vulkan.
		La transformation s'écrit : v_final = M_finale * v_local
		L'ordre des matrices est lu de droite à gauche (la transformation la plus proche du vecteur est appliquée en premier
		La chaîne de transformation est : Matrice_finale = Projection * Vue * Monde * Locale
		L'ordre interne (TRS) est : matrice = Translation * Rotation * Scale

		Ordre de multiplication des matrices monde et locale : Matrice_Monde * Matrice_Locale
		Soit : Matrice_final = translation_monde * rotation_monde * dimensionnement_monde * translation_locale * rotation_locale * dimensionnement_locale

	Convention des matrices : row major (vecteurs lignes) => DirectX
		La transformation s'écrit : v_final = v_local * M_finale
		L'ordre des matrices est lu de gauche à droite (la transformation la plus proche du vecteur est appliquée en premier).
		L'ordre interne (SRT) est inversé : matrice = Scale * Rotation * Translation
		La chaîne de transformation est : Matrice_finale = Locale * Monde * Vue * Projection

		Ordre de multiplication des matrices monde et locale : Matrice_Locale * Matrice_Monde
		Soit : Matrice_final = dimensionnement_locale * rotation_locale * translation_locale * dimensionnement_monde * rotation_monde *  translation_monde

	Camera
		La matrice de Vue (View Matrix) a pour but de transformer le monde entier de telle sorte que la caméra se retrouve à l'origine (0,0,0), regardant dans une direction standard (souvent le long de l'axe Z négatif).
		Pour ce faire, on applique au monde la transformation inverse de celle de la caméra.
		Donc, matrice_vue = matrice_camera.inverse() est l'approche standard et correcte.
		La formule finale pour un vertex shader serait donc (en convention vecteurs lignes) : position_finale = position_locale * matMesh * matWorld * matrice_vue * matrice_projection;

*/

void RenderObject()
{
//	// 1. Calculer la matrice View (caméra)
//	Matrix44f viewMatrix = myWorld->myCamera->getViewMatrix();
//
//	// 2. Calculer la matrice de projection
//	Matrix44f projectionMatrix = myFrustum->getProjectionMatrix();
//
//	// 3. Calculer la matrice combinée finale (row-major)
//	Matrix44f viewProjectionMatrix = viewMatrix * projectionMatrix;
//
//	// 4. Extraire les plans du frustum à partir de la matrice combinée
//	// Les plans sont bien définis dans le worldspace
//	myFrustum->Frustum_ExtractPlan(viewProjectionMatrix);
//	//drawDebugFrustum(viewMatrix, viewProjectionMatrix, 255 << 16, &myScreen);
//
//
//	myWorld->matWorldInversed = myWorld->matWorld.inverse();
//	// 2. Itération linéaire sur tous les maillages
//	//SparseSet<TransformComponent>* TransformComponentsPool = registry.getStorage<TransformComponent>();
//	//auto& Transforms = TransformComponentsPool->GetDenseData();
//	//auto& Entities = TransformComponentsPool->GetDenseEntities();
//
//	for (auto& [entity, mesh, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
//	{
//		std::shared_ptr<MeshClass> myMesh = mesh.m_mesh;
//
//		// 5. Calculer la matrice de modèle (Model Matrix)
//		Matrix44f modelMatrix = transform.m_worldTransform;
//
//		// 6.Calculer l'AABB local du mesh et son pendant dans le worldspace
////		myMesh->AABB.resetAABB();
////		myMesh->buildAABB(VERTEXSTATE::OBJECT);
//		AABB3d worldAABB;
//		worldAABB = calculateWorldAABB(myMesh->AABB, modelMatrix);
//
//		// 7. Tester l'AABB du mesh contre le frustum
//		// L'AABB est défini dans le worldspace ainsi que les plans du frustum. Le test de visibilité est donc possible
//		bool isVisible = isAABBVisibleIntoWorldSpace(worldAABB, myFrustum);
//		if (isVisible == true)
//		{
//
//			//	DrawAABB(worldAABB, myFrustum, viewMatrix, &myScreen, 255 << 8);
//
//				// Transformation de la caméra dans le modelspace de l'objet => nécessaire pour le backface culling
//			Matrix44f invModelMatrix = modelMatrix.inverse();
//			invModelMatrix.multVecMatrix(myWorld->myCamera->CameraPos, myWorld->myCamera->ObjectSpaceCameraPos);
//
//			for (int i = 0; i < myMesh->nb_faces; i++)
//			{
//				Poly* pPoly = myMesh->Face(i);
//
//				// 8. Backface culling dans l'espace objet
//				pPoly->Objectspace_ComputeBackfaceCulling(myWorld->myCamera->ObjectSpaceCameraPos, TypeObject::Object);
//				if (pPoly->isCameraVisible == true)
//				{
//					// 9. L'objet est visible, on peut donc le transformer dans le worldspace via sa matrice "Model"
//					TransformLocalMesh(pPoly, modelMatrix);
//
//					// 10. Passage en viewspace et projection
//					Clip3DAndProject(pPoly, myFrustum, myWorld, TypeObject::Object, &myScreen, viewProjectionMatrix);
//				}
//			}
//		}
//	}
//
//	DrawCameraOrientation(myFrustum, myWorld, &myScreen);
}


void End_Render()
{

}