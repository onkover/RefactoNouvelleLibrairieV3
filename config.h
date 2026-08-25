#pragma once
#include <string>

struct config
{
	std::string repObjDefault="";
	std::string repGfxDefault="";
	int screenWidth=0, screenHeight=0;
	std::string GizmoMeshPersective = "";
	std::string GizmoMeshOrthographiq = "";
	std::string GraphSceneName = "";
};