////=========================================================
//// bench.cpp - comparaison des 5 strategies d'effacement
////
//// Compilation (x64, Release) :
////   ml64 /c /Fo clearscreen.obj clearscreen.asm
////   cl /O2 /EHsc bench.cpp clearscreen.obj /Fe:bench.exe
////
//// Sous Visual Studio : projet x64 Release, puis
////   Clic droit projet > Build Dependencies > Build Customizations
////   > cocher "masm", et mettre clearscreen.asm en type
////   "Microsoft Macro Assembler".
////=========================================================
//
//#include <windows.h>
//#include <intrin.h>
//#include <stdio.h>
//#include <string.h>
//#include <vector>
//#include <algorithm>
//
//extern "C" void CleanScreenV1(void* p, unsigned long long bytes);
//extern "C" void CleanScreenV2(void* p, unsigned long long bytes);
//extern "C" void CleanScreenV3(void* p, unsigned long long bytes);
//extern "C" void CleanScreenV4(void* p, unsigned long long bytes);
//
//typedef void (*ClearFn)(void*, unsigned long long);
//
//// --- V5 : memset, enveloppe pour partager la meme signature -------------
//static void CleanScreenV5(void* p, unsigned long long bytes)
//{
//    memset(p, 0, (size_t)bytes);
//}
//
//// --- empeche le compilateur d'eliminer les appels -----------------------
//static volatile unsigned char g_sink = 0;
//
////=========================================================
//// Detection CPU
////=========================================================
//static bool hasAVX2()
//{
//    int r[4];
//    __cpuid(r, 0);
//    if (r[0] < 7) return false;
//    __cpuidex(r, 7, 0);
//    return ((r[1] >> 5) & 1) != 0;                 // EBX bit 5 = AVX2
//}
//
//static bool hasAVX512F()
//{
//    int r[4];
//    __cpuid(r, 0);
//    if (r[0] < 7) return false;
//
//    __cpuid(r, 1);
//    if (((r[2] >> 27) & 1) == 0) return false;     // OSXSAVE
//
//    unsigned long long xcr0 = _xgetbv(0);
//    if ((xcr0 & 0xE6) != 0xE6) return false;       // OS sauve bien l'etat ZMM
//
//    __cpuidex(r, 7, 0);
//    return ((r[1] >> 16) & 1) != 0;                // EBX bit 16 = AVX512F
//}
//
////=========================================================
//// Mesure
////=========================================================
//static double gbPerSec(ClearFn fn, void* buf, size_t bytes, int iters)
//{
//    fn(buf, bytes);                                 // warmup
//
//    LARGE_INTEGER freq, t0, t1;
//    QueryPerformanceFrequency(&freq);
//    QueryPerformanceCounter(&t0);
//
//    for (int i = 0; i < iters; ++i)
//        fn(buf, bytes);
//
//    QueryPerformanceCounter(&t1);
//
//    g_sink = ((unsigned char*)buf)[bytes - 1];      // barriere anti-optimisation
//
//    double sec = double(t1.QuadPart - t0.QuadPart) / double(freq.QuadPart);
//    if (sec <= 0.0) return 0.0;
//    return (double(bytes) * double(iters)) / sec / 1e9;
//}
//
//// mediane de plusieurs series : plus robuste que la moyenne
//static double medianGBs(ClearFn fn, void* buf, size_t bytes, int iters, int runs)
//{
//    std::vector<double> v;
//    v.reserve(runs);
//    for (int i = 0; i < runs; ++i)
//        v.push_back(gbPerSec(fn, buf, bytes, iters));
//    std::sort(v.begin(), v.end());
//    return v[v.size() / 2];
//}
//
////=========================================================
//// Verification de correction
////=========================================================
//static bool verify(ClearFn fn, void* buf, size_t bytes)
//{
//    memset(buf, 0xFF, bytes);
//    fn(buf, bytes);
//
//    const unsigned char* p = (const unsigned char*)buf;
//    for (size_t i = 0; i < bytes; ++i)
//        if (p[i] != 0) return false;
//    return true;
//}
//
////=========================================================
//
//int TestCleanBuffer()
//{
//    const bool avx2 = hasAVX2();
//    const bool avx512 = hasAVX512F();
//
//    printf("CPU : AVX2=%s  AVX-512F=%s\n\n",
//        avx2 ? "oui" : "non", avx512 ? "oui" : "non");
//
//    if (!avx2) {
//        printf("AVX2 absent : test impossible.\n");
//        return 1;
//    }
//
//    // Fige la frequence et le coeur autant que possible
//    SetProcessAffinityMask(GetCurrentProcess(), 1);
//    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
//    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
//
//    struct Variant { const char* name; ClearFn fn; bool needs512; };
//    Variant variants[] = {
//        { "V1  origine    (vmovdqu)",     CleanScreenV1, false },
//        { "V2  AVX2 x4    (vmovdqa)",     CleanScreenV2, false },
//        { "V3  AVX2 x4    (vmovntdq)",    CleanScreenV3, false },
//        { "V4  AVX-512 x4 (vmovntdq)",    CleanScreenV4, true  },
//        { "V5  memset()",                 CleanScreenV5, false },
//    };
//    const int NV = (int)(sizeof(variants) / sizeof(variants[0]));
//
//    struct Size { const char* label; size_t bytes; };
//    Size sizes[] = {
//        { "256 Ko  (tient en L2)",         256ull * 1024 },
//        { "8 Mo    (1080p x4, ~L3)",       1920ull * 1080 * 4 },
//        { "32 Mo   (4K x4, hors cache)",   3840ull * 2160 * 4 },
//    };
//    const int NS = (int)(sizeof(sizes) / sizeof(sizes[0]));
//
//    // Buffer unique, dimensionne sur le plus grand cas, aligne 64
//    size_t maxBytes = 0;
//    for (int s = 0; s < NS; ++s) maxBytes = max(maxBytes, sizes[s].bytes);
//
//    void* buf = _aligned_malloc(maxBytes, 64);
//    if (!buf) { printf("Allocation impossible.\n"); return 1; }
//
//    // --- Correction avant performance ----------------------------------
//    printf("Verification de correction (32 Ko) :\n");
//    for (int v = 0; v < NV; ++v) {
//        if (variants[v].needs512 && !avx512) {
//            printf("  %-28s ignoree (pas d'AVX-512)\n", variants[v].name);
//            continue;
//        }
//        bool ok = verify(variants[v].fn, buf, 32 * 1024);
//        printf("  %-28s %s\n", variants[v].name, ok ? "OK" : "ECHEC");
//        if (!ok) { printf("\nArret : resultat incorrect.\n"); _aligned_free(buf); return 1; }
//    }
//    printf("\n");
//
//    // --- Debit ----------------------------------------------------------
//    const double TARGET_BYTES = 4e9;   // volume ecrit par serie
//    const int RUNS = 5;
//
//    for (int s = 0; s < NS; ++s) {
//        const size_t bytes = sizes[s].bytes;
//        int iters = (int)(TARGET_BYTES / double(bytes));
//        if (iters < 3) iters = 3;
//
//        printf("=== %s  -  %d iterations x %d series ===\n",
//            sizes[s].label, iters, RUNS);
//
//        double ref = 0.0;
//        for (int v = 0; v < NV; ++v) {
//            if (variants[v].needs512 && !avx512) continue;
//
//            double gbs = medianGBs(variants[v].fn, buf, bytes, iters, RUNS);
//            if (v == 0) ref = gbs;
//
//            double ms = (double(bytes) / 1e9) / gbs * 1000.0;
//            printf("  %-28s %7.2f Go/s   %7.3f ms/appel   x%.2f\n",
//                variants[v].name, gbs, ms, ref > 0.0 ? gbs / ref : 0.0);
//        }
//        printf("\n");
//    }
//
//    _aligned_free(buf);
//    printf("(g_sink = %u, ignorer)\n", (unsigned)g_sink);
//    return 0;
//}

//=========================================================
// bench_cold.cpp - effacement memoire : cache CHAUD vs cache FROID
//
// Utilise le meme clearscreen.asm que bench.cpp.
//
//   ml64 /c /Fo clearscreen.obj clearscreen.asm
//   cl /O2 /EHsc bench_cold.cpp clearscreen.obj /Fe:bench_cold.exe
//
// PRINCIPE
//   Mode CHAUD : on boucle sur le buffer, il reste resident en cache.
//                C'est ce que mesurait bench.cpp.
//   Mode FROID : entre deux effacements, le buffer est evince du cache
//                par clflushopt. L'eviction est faite HORS CHRONO :
//                chaque effacement est chronometre individuellement,
//                on ne conserve que la mediane.
//                C'est ce qui ressemble a une vraie boucle de rendu.
//=========================================================

#include <windows.h>
#include <intrin.h>
#include <immintrin.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>

extern "C" void CleanScreenV1(void* p, unsigned long long bytes);
extern "C" void CleanScreenV2(void* p, unsigned long long bytes);
extern "C" void CleanScreenV3(void* p, unsigned long long bytes);
extern "C" void CleanScreenV4(void* p, unsigned long long bytes);

typedef void (*ClearFn)(void*, unsigned long long);

namespace LV3::Tests
{

    static void CleanScreenV5(void* p, unsigned long long bytes)
    {
        memset(p, 0, (size_t)bytes);
    }

    static volatile unsigned char g_sink = 0;
    static double g_qpcFreq = 0.0;
    static bool   g_useClflushopt = false;

    //=========================================================
    // Detection CPU
    //=========================================================
    static bool hasAVX2()
    {
        int r[4];
        __cpuid(r, 0);
        if (r[0] < 7) return false;
        __cpuidex(r, 7, 0);
        return ((r[1] >> 5) & 1) != 0;
    }

    static bool hasAVX512F()
    {
        int r[4];
        __cpuid(r, 0);
        if (r[0] < 7) return false;
        __cpuid(r, 1);
        if (((r[2] >> 27) & 1) == 0) return false;      // OSXSAVE
        unsigned long long xcr0 = _xgetbv(0);
        if ((xcr0 & 0xE6) != 0xE6) return false;
        __cpuidex(r, 7, 0);
        return ((r[1] >> 16) & 1) != 0;
    }

    static bool hasCLFLUSHOPT()
    {
        int r[4];
        __cpuid(r, 0);
        if (r[0] < 7) return false;
        __cpuidex(r, 7, 0);
        return ((r[1] >> 23) & 1) != 0;                 // EBX bit 23
    }

    static void printCpuName()
    {
        int r[4];
        char name[49] = { 0 };
        __cpuid(r, 0x80000000);
        if ((unsigned)r[0] < 0x80000004u) { printf("CPU : inconnu\n"); return; }
        __cpuid((int*)(name + 0), 0x80000002);
        __cpuid((int*)(name + 16), 0x80000003);
        __cpuid((int*)(name + 32), 0x80000004);
        printf("CPU : %s\n", name);
    }

    //=========================================================
    // Eviction du buffer hors des caches
    //=========================================================
    static void evictBuffer(void* p, size_t bytes)
    {
        char* q = (char*)p;
        if (g_useClflushopt) {
            for (size_t i = 0; i < bytes; i += 64)
                _mm_clflushopt(q + i);
        }
        else {
            for (size_t i = 0; i < bytes; i += 64)
                _mm_clflush(q + i);
        }
        _mm_sfence();       // clflushopt est faiblement ordonne
    }

    //=========================================================
    // Mesures
    //=========================================================
    static double warmGBs(ClearFn fn, void* buf, size_t bytes, int iters)
    {
        fn(buf, bytes);                                  // warmup

        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        for (int i = 0; i < iters; ++i)
            fn(buf, bytes);
        QueryPerformanceCounter(&t1);

        g_sink = ((unsigned char*)buf)[bytes - 1];

        double sec = double(t1.QuadPart - t0.QuadPart) / g_qpcFreq;
        if (sec <= 0.0) return 0.0;
        return (double(bytes) * double(iters)) / sec / 1e9;
    }

    // Chronometrage individuel, eviction entre chaque mesure.
    static double coldGBs(ClearFn fn, void* buf, size_t bytes, int iters, double* msOut)
    {
        fn(buf, bytes);                                  // amorce code + TLB

        std::vector<double> t;
        t.reserve(iters);

        for (int i = 0; i < iters; ++i) {
            evictBuffer(buf, bytes);                     // <-- hors chrono

            LARGE_INTEGER a, b;
            QueryPerformanceCounter(&a);
            fn(buf, bytes);                              // <-- seul segment mesure
            QueryPerformanceCounter(&b);

            t.push_back(double(b.QuadPart - a.QuadPart) / g_qpcFreq);
        }
        g_sink = ((unsigned char*)buf)[bytes - 1];

        std::sort(t.begin(), t.end());
        double med = t[t.size() / 2];
        if (msOut) *msOut = med * 1000.0;
        if (med <= 0.0) return 0.0;
        return double(bytes) / med / 1e9;
    }

    //=========================================================
    // Verifications
    //=========================================================
    static bool verify(ClearFn fn, void* buf, size_t bytes)
    {
        memset(buf, 0xFF, bytes);
        fn(buf, bytes);
        const unsigned char* p = (const unsigned char*)buf;
        for (size_t i = 0; i < bytes; ++i)
            if (p[i] != 0) return false;
        return true;
    }

    // Cout d'une paire QPC : doit etre negligeable devant un effacement.
    static double qpcOverheadNs()
    {
        const int N = 20000;
        LARGE_INTEGER a, b, s, e;
        QueryPerformanceCounter(&s);
        for (int i = 0; i < N; ++i) {
            QueryPerformanceCounter(&a);
            QueryPerformanceCounter(&b);
        }
        QueryPerformanceCounter(&e);
        return (double(e.QuadPart - s.QuadPart) / g_qpcFreq) / N * 1e9;
    }

    // Controle que l'eviction fait bien son travail : le meme effacement
    // doit etre nettement plus lent apres flush qu'apres un acces chaud.
    static void sanityCheckEviction(ClearFn fn, void* buf, size_t bytes)
    {
        double dummy;
        double warm = warmGBs(fn, buf, bytes, 200);
        double cold = coldGBs(fn, buf, bytes, 50, &dummy);
        printf("  Controle eviction (V2, %zu Ko) : chaud %.1f Go/s -> froid %.1f Go/s",
            bytes / 1024, warm, cold);
        if (cold < warm * 0.85) printf("  [eviction effective]\n");
        else                    printf("  [SUSPECT : flush sans effet ?]\n");
    }

    //=========================================================
    int TestCleanBuffer()
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_qpcFreq = double(f.QuadPart);

        printCpuName();

        const bool avx2 = hasAVX2();
        const bool avx512 = hasAVX512F();
        g_useClflushopt = hasCLFLUSHOPT();

        printf("AVX2=%s  AVX-512F=%s  CLFLUSHOPT=%s\n",
            avx2 ? "oui" : "non",
            avx512 ? "oui" : "non",
            g_useClflushopt ? "oui" : "non (repli sur clflush)");

        if (!avx2) { printf("AVX2 absent : test impossible.\n"); return 1; }

        SetProcessAffinityMask(GetCurrentProcess(), 1);
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        printf("Resolution QPC : %.1f MHz   cout d'une paire : %.0f ns\n",
            g_qpcFreq / 1e6, qpcOverheadNs());
        printf("(a comparer aux centaines de microsecondes d'un effacement)\n\n");

        struct Variant { const char* name; ClearFn fn; bool needs512; };
        Variant variants[] = {
            { "V1  origine    (vmovdqu)",  CleanScreenV1, false },
            { "V2  AVX2 x4    (vmovdqa)",  CleanScreenV2, false },
            { "V3  AVX2 x4    (vmovntdq)", CleanScreenV3, false },
            { "V4  AVX-512 x4 (vmovntdq)", CleanScreenV4, true  },
            { "V5  memset()",              CleanScreenV5, false },
        };
        const int NV = (int)(sizeof(variants) / sizeof(variants[0]));

        struct Size { const char* label; size_t bytes; };
        Size sizes[] = {
            { "256 Ko",             256ull * 1024 },
            { "8 Mo   (1080p x4)",  8ull * 1024 * 1024 },
            { "12 Mo",              12ull * 1024 * 1024 },
            { "16 Mo",              16ull * 1024 * 1024 },
            { "24 Mo",              24ull * 1024 * 1024 },
            { "32 Mo  (4K x4)",     32ull * 1024 * 1024 },
        };
        const int NS = (int)(sizeof(sizes) / sizeof(sizes[0]));

        size_t maxBytes = 0;
        for (int s = 0; s < NS; ++s)
            if (sizes[s].bytes > maxBytes) maxBytes = sizes[s].bytes;

        void* buf = _aligned_malloc(maxBytes, 64);
        if (!buf) { printf("Allocation impossible.\n"); return 1; }
        memset(buf, 0xFF, maxBytes);                     // force le mapping des pages

        // --- Correction -----------------------------------------------------
        printf("Verification de correction (32 Ko) :\n");
        for (int v = 0; v < NV; ++v) {
            if (variants[v].needs512 && !avx512) {
                printf("  %-28s ignoree (pas d'AVX-512)\n", variants[v].name);
                continue;
            }
            bool ok = verify(variants[v].fn, buf, 32 * 1024);
            printf("  %-28s %s\n", variants[v].name, ok ? "OK" : "ECHEC");
            if (!ok) { _aligned_free(buf); return 1; }
        }
        sanityCheckEviction(CleanScreenV2, buf, 1024 * 1024);
        printf("\n");

        // --- Mesures --------------------------------------------------------
        printf("%-28s %12s %12s %12s %8s\n",
            "", "CHAUD Go/s", "FROID Go/s", "FROID ms", "ecart");
        printf("--------------------------------------------------------------------------------\n");

        for (int s = 0; s < NS; ++s) {
            const size_t bytes = sizes[s].bytes;

            int warmIters = (int)(2e9 / double(bytes));
            if (warmIters < 5) warmIters = 5;

            int coldIters = (int)(1e9 / double(bytes));
            if (coldIters < 15)  coldIters = 15;
            if (coldIters > 200) coldIters = 200;

            printf("=== %s ===\n", sizes[s].label);

            for (int v = 0; v < NV; ++v) {
                if (variants[v].needs512 && !avx512) continue;

                double warm = warmGBs(variants[v].fn, buf, bytes, warmIters);
                double ms = 0.0;
                double cold = coldGBs(variants[v].fn, buf, bytes, coldIters, &ms);

                printf("  %-26s %12.2f %12.2f %12.3f %7.0f%%\n",
                    variants[v].name, warm, cold, ms,
                    warm > 0.0 ? (cold / warm - 1.0) * 100.0 : 0.0);
            }
            printf("\n");
        }

        _aligned_free(buf);
        printf("(g_sink = %u, ignorer)\n", (unsigned)g_sink);
        return 0;
    }
}