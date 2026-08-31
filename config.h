#pragma once
#include <string>
#include <unordered_map>

// Définition des structures de données pour "unordered_map"
struct AssetStruct {
	std::string type;
	std::string object;
};

struct ViewportStruct {
	std::string nom;
	int largeur{ 0 };
	int hauteur{ 0 };
};

struct config
{
	std::string repObjDefault="";
	std::string repGfxDefault="";

	int screenWidth=0, screenHeight=0;

	std::unordered_map<std::string, AssetStruct> mapAssets;
	std::unordered_map<std::string, ViewportStruct> mapViewports;

};