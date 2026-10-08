#include "pch.h"
#include "Core/JsonReader.h"
#include "Scene/SerializerHelpers.hpp"

namespace LV3::Tests
{
#if LV3_DEBUG
    void TestD3_1_JsonReader_Contrats()
    {
        const nlohmann::json j = nlohmann::json::parse(R"({
            "entier": 7, "texte": "abc", "nul": null, "delegue": null,
            "vec": [1, 2, 3], "vecNul": null, "vecCourt": [0, 90], "vecLong": [1, 2, 3, 4],
            "inconnue": true })");
        JsonReader r(j, "TestD3", "test");
        const std::uint32_t w0 = Logger::warnCount();

        // ── Read ──────────────────────────────────────────────────────
        LV3_ASSERT(r.Read("entier", 0) == 7);           LV3_ASSERT(Logger::warnCount() == w0);      // exacte : silence
        LV3_ASSERT(r.Read("nul", 5) == 5);              LV3_ASSERT(Logger::warnCount() == w0);      // null : info, PAS un souci
        LV3_ASSERT(r.Read("absente", 3) == 3);          LV3_ASSERT(Logger::warnCount() == w0 + 1);  // absente : souci
        LV3_ASSERT(r.Read("texte", 1.0f) == 1.0f);      LV3_ASSERT(Logger::warnCount() == w0 + 2);  // type invalide : souci

        // ── TryRead ───────────────────────────────────────────────────
        LV3_ASSERT(!r.TryRead("delegue", 0.5f).has_value());   LV3_ASSERT(Logger::warnCount() == w0 + 2);  // null : info
        LV3_ASSERT(!r.TryRead("absente2", 0.5f).has_value());  LV3_ASSERT(Logger::warnCount() == w0 + 3);  // absente : souci
        const auto e = r.TryRead("entier", 0);
        LV3_ASSERT(e.has_value() && *e == 7);                  LV3_ASSERT(Logger::warnCount() == w0 + 3);  // exacte

        // ── ReadVector ────────────────────────────────────────────────
        const Vec3f v = r.ReadVector("vec", Vec3f::Zero());
        LV3_ASSERT(v.x == 1.0f && v.y == 2.0f && v.z == 3.0f); LV3_ASSERT(Logger::warnCount() == w0 + 3);  // exact
        (void)r.ReadVector("vecNul", Vec3f::One());            LV3_ASSERT(Logger::warnCount() == w0 + 3);  // null : info
        (void)r.ReadVector("vecAbsent", Vec3f::Zero());
        (void)r.ReadVector("vecCourt", Vec3f::Zero());
        (void)r.ReadVector("vecLong", Vec3f::Zero());        LV3_ASSERT(Logger::warnCount() == w0 + 6);  // 3 soucis

        // ── CONTROLE POSITIF : 'inconnue' jamais lue -> WarnUnread DOIT parler ──
        r.WarnUnread();                                        LV3_ASSERT(Logger::warnCount() == w0 + 7);

        Logger::success("[D3.1] JsonReader : exacte / absente / null — aucune absence muette");
    }

    void TestD3_2_SurchargesCamera()
    {
        const nlohmann::json j = nlohmann::json::parse(R"({
            "ok": 40.0, "nul": null, "zero": 0, "negatif": -3, "enorme": 1e39, "texte": "x" })");
        JsonReader r(j, "Camera", "test");
        const float moteur = 80.0f;
        const std::uint32_t w0 = Logger::warnCount();

        // 1. Valeur exacte valide -> surcharge retenue, silence
        const auto ok = ReadPositiveOverride(r, "ok", moteur, "Camera", "test");
        LV3_ASSERT(ok.has_value() && *ok == 40.0f);                                     LV3_ASSERT(Logger::warnCount() == w0);

        // 2. null -> suit le moteur, info (pas un souci)
        LV3_ASSERT(!ReadPositiveOverride(r, "nul", moteur, "Camera", "test"));         LV3_ASSERT(Logger::warnCount() == w0);

        // 3. Absente -> suit le moteur, souci (TryRead)
        LV3_ASSERT(!ReadPositiveOverride(r, "absente", moteur, "Camera", "test"));     LV3_ASSERT(Logger::warnCount() == w0 + 1);

        // 4. Hors domaine : 0, negatif, inf (1e39 -> float) -> suit le moteur, souci (domaine)
        LV3_ASSERT(!ReadPositiveOverride(r, "zero", moteur, "Camera", "test"));     LV3_ASSERT(Logger::warnCount() == w0 + 2);
        LV3_ASSERT(!ReadPositiveOverride(r, "negatif", moteur, "Camera", "test"));     LV3_ASSERT(Logger::warnCount() == w0 + 3);
        LV3_ASSERT(!ReadPositiveOverride(r, "enorme", moteur, "Camera", "test"));     LV3_ASSERT(Logger::warnCount() == w0 + 4);

        // 5. Type invalide -> suit le moteur, souci (TryRead), UN SEUL message (pas de double compte)
        LV3_ASSERT(!ReadPositiveOverride(r, "texte", moteur, "Camera", "test"));       LV3_ASSERT(Logger::warnCount() == w0 + 5);

        // 6. Toutes les cles ont ete lues -> WarnUnread muet
        r.WarnUnread();                                                                 LV3_ASSERT(Logger::warnCount() == w0 + 5);

        Logger::success("[D3.2] Surcharges camera : valeur / null / absente / hors domaine");
    }


#endif
}