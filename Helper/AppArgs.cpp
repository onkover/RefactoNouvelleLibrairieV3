#include "pch.h"          // premiere ligne, toujours
#include "AppArgs.h"
#include "Core/Logger.h"

#include <charconv>
#include <string_view>

using namespace LV3;

namespace
{
	// "--bench=3000" avec la cle "--bench=" -> value = "3000"
	bool Match(std::string_view arg, std::string_view key, std::string_view& outValue)
	{
		if (!arg.starts_with(key)) return false;
		outValue = arg.substr(key.size());
		return true;
	}

	// from_chars et PAS std::stoul : pas d'exception, pas de locale, et
	// surtout il dit ou il s'est arrete -- "300abc" est rejete, la ou
	// stoul rendrait 300 sans broncher.
	bool ParseU32(std::string_view s, uint32_t& out)
	{
		if (s.empty()) return false;
		uint32_t v = 0;
		const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
		if (r.ec != std::errc{} || r.ptr != s.data() + s.size()) return false;
		out = v;
		return true;
	}

	// Un nom de fichier de contenu : relatif, sans remontee. Meme invariant
	// que le bug 67 -- tout se resout SOUS contentRoot. operator/ avec un
	// operande absolu ECRASE la racine, silencieusement.
	bool IsSafeRelative(const std::string& s)
	{
		if (s.empty()) return false;
		const std::filesystem::path p(s);
		if (p.is_absolute()) return false;
		for (const auto& part : p)
			if (part == "..") return false;
		return true;
	}
}

void PrintUsage()
{
	Logger::info(
		"Usage : RefactoNouvelleLibrairieV3.exe [<racine_de_contenu>] [options]\n"
		"  --scene=<fichier.json>  scene a charger (relative a resources.scene) — prime sur config.json\n"
		"  --bench=<N>             mode banc : N frames simulees puis sortie automatique\n"
		"  --warmup=<N>            frames de chauffe exclues du resume (defaut 60)\n"
		"  --csv=<chemin>          fichier de mesures, relatif a la racine de contenu\n"
		"  --help                  cette aide\n"
		"Exemple :\n"
		"  RefactoNouvelleLibrairieV3.exe \"D:\\...\\RefactoNouvelleLibrairieV3\""
		" --scene=solar_system_v1compat_belt.json --bench=3000 --csv=Mesures/belt_release.csv");
}

AppArgs ParseArgs(int argc, char* argv[])
{
	AppArgs a;
	bool rootSeen = false;

	for (int i = 1; i < argc; ++i)
	{
		const std::string_view arg = argv[i];
		std::string_view v;

		if (arg == "--help" || arg == "-h") { a.wantsHelp = true; continue; }

		// --- Positionnel : la racine, et elle seule ---
		if (!arg.starts_with("--"))
		{
			if (rootSeen)
			{
				Logger::error("[Args] argument positionnel en trop : '" + std::string(arg)
					+ "' — la scene se choisit avec --scene=, jamais par position.");
				a.valid = false;
				continue;
			}
			a.contentRoot = std::filesystem::path(arg);
			rootSeen = true;
			continue;
		}

		// --- Options a valeur collee ---
		if (Match(arg, "--scene=", v))
		{
			a.scene.assign(v);
			if (!IsSafeRelative(a.scene))
			{
				Logger::error("[Args] --scene doit etre un chemin RELATIF sans '..' : '" + a.scene + "'");
				a.valid = false;
			}
			continue;
		}

		if (Match(arg, "--csv=", v))
		{
			a.csv.assign(v);
			if (!IsSafeRelative(a.csv))
			{
				Logger::error("[Args] --csv doit etre un chemin RELATIF sans '..' : '" + a.csv + "'");
				a.valid = false;
			}
			continue;
		}

		if (Match(arg, "--bench=", v))
		{
			if (!ParseU32(v, a.benchFrames) || a.benchFrames == 0)
			{
				Logger::error("[Args] --bench attend un entier strictement positif : '"
					+ std::string(v) + "'");
				a.valid = false;
			}
			continue;
		}

		if (Match(arg, "--warmup=", v))
		{
			if (!ParseU32(v, a.warmupFrames))
			{
				Logger::error("[Args] --warmup attend un entier : '" + std::string(v) + "'");
				a.valid = false;
			}
			continue;
		}

		// --- Rien n'a matche : on ARRETE. Meme discipline que WarnUnread. ---
		Logger::error("[Args] option inconnue : '" + std::string(arg) + "'");
		a.valid = false;
	}

	// Degradation propre plutot qu'assertion en pleine campagne : Profiler::Begin
	// exige warmup < frames, et une chauffe par defaut a 60 sur un --bench=10
	// ferait sauter l'assert en RelWithAsserts. On corrige et on le DIT.
	if (a.benchFrames > 0 && a.warmupFrames >= a.benchFrames)
	{
		const uint32_t clamped = a.benchFrames / 4;   // 0 accepte : --bench=1 reste licite
		Logger::warn("[Args] --warmup (" + std::to_string(a.warmupFrames)
			+ ") >= --bench (" + std::to_string(a.benchFrames)
			+ ") — ramene a " + std::to_string(clamped));
		a.warmupFrames = clamped;
	}

	return a;
}