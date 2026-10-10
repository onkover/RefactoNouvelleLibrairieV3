#include "pch.h"
#include "Core/JsonReader.h"
#include "Scene/SerializerHelpers.hpp"
#include "Scene/Serializer.hpp"
#include "Ressources/ResourceManager.h"
#include <filesystem>
#include <fstream>

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

    struct SceneLoadResult { bool ok; std::uint32_t warns; std::size_t linked; };

    namespace
    {
        // Ecrit le texte dans un fichier temporaire et le charge dans un monde VIERGE.
        SceneLoadResult LoadSceneText(const std::string& text)
        {
            const std::filesystem::path dir = std::filesystem::temp_directory_path();
            const std::string file = "lv3_test_d3_3a.json";
            { std::ofstream(dir / file) << text; }

            Registry        reg;
            ResourceManager rm;
            const std::uint32_t w0 = Logger::warnCount();
            const bool ok = SceneSerializer::LoadSceneGraph(dir.string() + "/", file, reg, rm);

            std::size_t linked = 0;
            for (const HierarchyComponent& h : reg.View<HierarchyComponent>())
                if (h.m_parent != NULL_ENTITY) ++linked;

            return { ok, Logger::warnCount() - w0, linked };
        }

        const std::string kComps = R"("components": { "Transform": { "translation": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1] } })";
    }

    void TestD3_3a_NiveauNoeud()
    {
        // 1. Nominal, enfant declare AVANT son parent (prouve la resolution differee)
        auto r = LoadSceneText(R"({ "sceneName": "scene TNR1", "nodes": [
            { "id": "Child", "parent": "Root", "_type": "Lune", )" + kComps + R"( },
            { "id": "Root",  "parent": null, )" + kComps + R"( } ] })");
        LV3_ASSERT(r.ok && r.warns == 0 && r.linked == 1);

        // 2. sceneName absent : AVANT -> std::terminate ; APRES -> 1 souci, chargement OK
        r = LoadSceneText(R"({ "nodes": [ { "id": "Root", "parent": null, )" + kComps + R"( } ] })");
        LV3_ASSERT(r.ok && r.warns == 1);

        // 3. Faute de frappe 'parnet' : 2 soucis ('parent' absente + 'parnet' ignoree), aucun lien
        r = LoadSceneText(R"({ "sceneName": "scene TNR2", "nodes": [
            { "id": "Root",  "parent": null, )" + kComps + R"( },
            { "id": "Child", "parnet": "Root", )" + kComps + R"( } ] })");
        LV3_ASSERT(r.ok && r.warns == 2 && r.linked == 0);

        // 4. Refus francs (5 lignes ROUGES attendues) :
        //    id absent / parent inconnu / parent non texte (2 lignes) / nodes absent
        Logger::info("[D3.3a] Test 4 : \033[31mles 5 erreurs qui suivent sont VOLONTAIRES (refus francs)\033[0m");
        LV3_ASSERT(!LoadSceneText(R"({ "sceneName": "scene TNR", "nodes": [ { "parent": null, )" + kComps + R"( } ] })").ok);
        LV3_ASSERT(!LoadSceneText(R"({ "sceneName": "scene TNR", "nodes": [ { "id": "A", "parent": "Ghost", )" + kComps + R"( } ] })").ok);
        LV3_ASSERT(!LoadSceneText(R"({ "sceneName": "scene TNR", "nodes": [ { "id": "A", "parent": 42, )" + kComps + R"( } ] })").ok);
        LV3_ASSERT(!LoadSceneText(R"({ "sceneName": "scene TNR" })").ok);

        Logger::success("[D3.3a] Niveau noeud : une lecture, un lecteur, aucune absence muette");
    }

    void TestD3_3b_Blocs()
    {
        const nlohmann::json j = nlohmann::json::parse(R"({ "plein": { "a": 1 }, "nul": null, "faux": 3 })");
        JsonReader r(j, "TestD3b", "test");
        const std::uint32_t w0 = Logger::warnCount();

        // 1. Bloc present : lecture normale, aucun souci
        { JsonReader b = r.Child("plein"); LV3_ASSERT(b.Read("a", 0) == 1); b.WarnUnread(); }
        LV3_ASSERT(Logger::warnCount() == w0);

        // 2. Bloc null : info pour le bloc, ses cles sont ANNONCEES -> aucun souci
        { JsonReader b = r.Child("nul"); LV3_ASSERT(b.Read("x", 7) == 7); LV3_ASSERT(b.ReadVector("v", Vec3f::One()).x == 1.0f); }
        LV3_ASSERT(Logger::warnCount() == w0);

        // 3. Bloc absent : 1 souci pour le bloc + 1 par cle lue
        { JsonReader b = r.Child("absent"); LV3_ASSERT(b.Read("x", 7) == 7); }
        LV3_ASSERT(Logger::warnCount() == w0 + 2);

        // 4. Bloc de mauvais type : comme absent
        { JsonReader b = r.Child("faux"); LV3_ASSERT(b.Read("x", 7) == 7); }
        LV3_ASSERT(Logger::warnCount() == w0 + 4);

        // 5. La delegation se transmet : un sous-bloc absent DANS un bloc null reste une annonce
        { JsonReader b = r.Child("nul"); JsonReader c = b.Child("sous"); LV3_ASSERT(c.Read("y", 2) == 2); }
        LV3_ASSERT(Logger::warnCount() == w0 + 4);

        r.WarnUnread();
        LV3_ASSERT(Logger::warnCount() == w0 + 4);

        Logger::success("[D3.3b] Blocs : present / null / absent / mauvais type");
    }

    void TestD3_3c_Parseurs()
    {
        // Une camera ECRITE EN ENTIER : seules les cles testees varient
        const std::string head = R"({ "sceneName": "TNR3", "nodes": [ { "id": "Cam", "parent": null, "components": {
            "Transform": { "translation": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1] },
            "Health": { "maxHealth": 80, "currentHealth": 50 },
            "Camera": { "projection": "perspective", "lens": "fov", "fov": 60.0, "near": 0.1,
                        "depthDisplayRange": null, "lodTolerancePx": null,
                        "category": "gameplay", "active": true, "priority": 0, )";
        const std::string tail = R"( } } } ] })";

        // 1. infiniteFar sans 'far' + gizmo complet : AUCUN souci (impossible avant 3.3c)
        auto r = LoadSceneText(head + R"("infiniteFar": true, "gizmo": { "length": 3.0 })" + tail);
        LV3_ASSERT(r.ok && r.warns == 0);

        // 2. Plan lointain fini : 'far' est lu, aucun souci
        r = LoadSceneText(head + R"("infiniteFar": false, "far": 500.0, "gizmo": { "length": 3.0 })" + tail);
        LV3_ASSERT(r.ok && r.warns == 0);

        // 3. infiniteFar ET 'far' : 'far' n'est pas lu -> 1 souci ("cle ignoree 'far'")
        r = LoadSceneText(head + R"("infiniteFar": true, "far": 500.0, "gizmo": { "length": 3.0 })" + tail);
        LV3_ASSERT(r.ok && r.warns == 1);

        // 4. gizmo null : "pas de gizmo" annonce, aucun souci
        r = LoadSceneText(head + R"("infiniteFar": true, "gizmo": null)" + tail);
        LV3_ASSERT(r.ok && r.warns == 0);

        // 5. gizmo absent : 2 soucis (le bloc + sa cle 'length')
        r = LoadSceneText(head + R"("infiniteFar": true)" + tail);
        LV3_ASSERT(r.ok && r.warns == 2);

        Logger::success("[D3.3c] Parseurs : far conditionnel, gizmo, currentHealth");
    }

    namespace
    {
        // Un triangle minimal : 3 sommets, 1 face
        constexpr const char* kTri = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";

        std::uint32_t LoadObjText(const std::string& name, const std::string& obj)
        {
            const std::filesystem::path dir = std::filesystem::temp_directory_path();
            { std::ofstream(dir / name) << obj; }
            ResourceManager rm;
            const std::uint32_t w0 = Logger::warnCount();
            const auto h = rm.LoadMeshChecked((dir / name).string(), {});
            LV3_ASSERT(h.has_value());                       // un souci n'empeche pas le chargement
            return Logger::warnCount() - w0;
        }
    }

    void TestD3_3e_Materiaux()
    {
        const std::filesystem::path dir = std::filesystem::temp_directory_path();
        { std::ofstream(dir / "lv3_d3e_ok.mtl") << "newmtl M_Ok\nKd 0.5 0.5 0.5\n"; }
        { std::ofstream(dir / "lv3_d3e_bad.mtl") << "Kd 1 1 1\nnewmtl M_Bad\nNi 1.5\nmap_Kd absente.png\n"; }

        // 1. Nominal : mtllib present, usemtl connu -> aucun souci
        LV3_ASSERT(LoadObjText("lv3_d3e_1.obj", std::string("mtllib lv3_d3e_ok.mtl\nusemtl M_Ok\n") + kTri) == 0);

        // 2. mtllib absent, donc usemtl inconnu -> 2 soucis
        LV3_ASSERT(LoadObjText("lv3_d3e_2.obj", std::string("mtllib lv3_d3e_absent.mtl\nusemtl M_Ok\n") + kTri) == 2);

        // 3. usemtl inconnu dans un mtllib present -> 1 souci
        LV3_ASSERT(LoadObjText("lv3_d3e_3.obj", std::string("mtllib lv3_d3e_ok.mtl\nusemtl M_Faux\n") + kTri) == 1);

        // 4. MTL defectueux : directive avant newmtl, directive inconnue, texture absente -> 3 soucis
        LV3_ASSERT(LoadObjText("lv3_d3e_4.obj", std::string("mtllib lv3_d3e_bad.mtl\nusemtl M_Bad\n") + kTri) == 3);

        // 5. Ni mtllib ni usemtl (cas des gizmos) -> aucun souci : rien n'a ete promis
        LV3_ASSERT(LoadObjText("lv3_d3e_5.obj", kTri) == 0);

        Logger::success("[D3.3e] Materiaux : mtllib / usemtl / directives / textures");
    }

    void TestD3_4a_CacheMeshOptions()
    {
        Logger::info("[D3.4a] test cache mesh option");
        const std::filesystem::path obj = std::filesystem::temp_directory_path() / "lv3_test_d3_4a.obj";
        { std::ofstream(obj) << "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n"; }
        const std::string path = obj.string();

        OBJLoadOptions A;                 // scale 1
        OBJLoadOptions B; B.scale = 2.0f; // meme fichier, autre option

        ResourceManager rm;
        const auto hA = rm.LoadMeshChecked(path, A);
        const auto hB = rm.LoadMeshChecked(path, B);
        LV3_ASSERT(hA && hB);

        // 1. Deux options -> deux meshes distincts (avant : hB == hA, en silence)
        LV3_ASSERT(*hA != *hB);
        LV3_ASSERT(rm.GetMeshCount() == 2);

        // 2. C'est bien l'option qui a ete appliquee : la boite double
        LV3_ASSERT(rm.GetMesh(*hB)->GetMeshAABB().max.x == 2.0f * rm.GetMesh(*hA)->GetMeshAABB().max.x);

        // 3. Memes options -> le cache sert, aucun mesh de plus
        const auto again = rm.LoadMeshChecked(path, A);
        LV3_ASSERT(again && *again == *hA && rm.GetMeshCount() == 2);

        // 4. Recherche : le couple (chemin, options) est la cle
        LV3_ASSERT(rm.FindMesh(path, A) == *hA && rm.FindMesh(path, B) == *hB);
        OBJLoadOptions C; C.flipUVsVertically = !A.flipUVsVertically;
        LV3_ASSERT(!rm.IsMeshLoaded(path, C));

        // 5. Decharger une variante ne touche pas l'autre
        rm.UnloadMesh(*hB);
        LV3_ASSERT(!rm.IsMeshLoaded(path, B) && rm.IsMeshLoaded(path, A));

        Logger::success("[D3.4a] Cache meshes : cle = chemin + options");
    }
#endif
}