// 1.4: Mibase VINTAGE (sinte analògic virtual de caràcter clàssic).
#include "mibase/Vintage.h"
#include <set>
namespace
{
    std::unique_ptr<mibase::analogic::Vintage> vintageDe (const std::string& canvis, bool net = true)
    {
        auto s = std::make_unique<mibase::analogic::Vintage>();
        s->posaEstat (mibase::analogic::construeix (canvis));
        if (net) for (int p : { (int) mibase::analogic::RvMix, (int) mibase::analogic::DlMix, (int) mibase::analogic::Chorus, (int) mibase::analogic::PhMix }) s->posa (p, 0.0f);
        s->prepara (48000, 256);
        return s;
    }
    SonSB sonaV (mibase::analogic::Vintage& s, std::vector<std::array<int, 4>> ev, int total)
    {
        SonSB r;
        r.L.assign ((size_t) total, 0.0f);
        r.R.assign ((size_t) total, 0.0f);
        for (int a = 0; a < total; a += 256)
        {
            const int n = std::min (256, total - a);
            std::vector<mibase::EventInstrument> e;
            for (auto& x : ev) if (x[0] >= a && x[0] < a + n) e.push_back ({ x[0] - a, (std::uint8_t) x[1], (std::uint8_t) x[2], (std::uint8_t) x[3] });
            s.processa (e.data(), (int) e.size(), r.L.data() + a, r.R.data() + a, n);
        }
        return r;
    }
    double rmsV (const std::vector<float>& v, int a, int b)
    {
        double e = 0.0;
        for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i];
        return std::sqrt (e / std::max (1, b - a));
    }

    void proves104()
    {
        using namespace mibase;
        using namespace mibase::analogic;
        sinte::tempo().store (120.0);

        prova ("1.4 VINTAGE: 150 presets o més, únics, a les 10 categories i de les 21 famílies, tots sonen bé", []
        {
            const auto& ps = presetsFabrica();
            comprova (ps.size() >= 150, "presets: " + std::to_string (ps.size()));
            comprova (categories().size() == 10 && families().size() == 21, "10 categories i 21 famílies");
            std::set<std::string> noms, estats;
            std::map<std::string, int> perCat, perFam;
            for (const auto& p : ps)
            {
                noms.insert (p.nom); estats.insert (p.estat); ++perCat[p.categoria];
                for (const auto& f : families()) if (p.descripcio.rfind (f + " · ", 0) == 0) ++perFam[f];
            }
            comprova (noms.size() == ps.size(), "noms únics");
            comprova (estats.size() == ps.size(), "cap preset repetit");
            for (const auto& c : categories()) comprova (perCat[c] >= 7, "categoria " + c + ": " + std::to_string (perCat[c]));
            comprova (perCat.size() == categories().size(), "cap preset fora de les categories");
            for (const auto& f : families()) comprova (perFam[f] >= 7, "família " + f + ": " + std::to_string (perFam[f]));
            // tots sonen, sense valors no vàlids ni per sobre de 0 dBFS
            int muts = 0, dolents = 0;
            for (const auto& p : ps)
            {
                Vintage s;
                s.posaEstat (p.estat);
                s.prepara (48000, 256);
                auto r = sonaV (s, { { 0, 0x90, 48, 100 }, { 0, 0x90, 55, 100 }, { 48000, 0x80, 48, 0 }, { 48000, 0x80, 55, 0 } }, 72000);
                float pic = 0.0f; bool nan = false;
                for (size_t i = 0; i < r.L.size(); ++i) { if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i])) nan = true; pic = std::max ({ pic, std::abs (r.L[i]), std::abs (r.R[i]) }); }
                if (nan || pic >= 1.0f) ++dolents;
                if (pic < 0.01f) ++muts;
            }
            comprova (dolents == 0, "cap preset amb valors no vàlids o que passi de 0 dBFS (" + std::to_string (dolents) + ")");
            comprova (muts == 0, "cap preset mut (" + std::to_string (muts) + ")");
        });

        prova ("1.4 VINTAGE: afinació exacta sense caràcter, i deriva amb el caràcter analògic", []
        {
            auto s = vintageDe ("an=0 o1w=3 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto r = sonaV (*s, { { 0, 0x90, 69, 100 } }, 48000);
            const double hz = hzSB (r.L, 12000, 48000);
            comprova (std::abs (hz - 440.0) < 0.5, "La a 440 Hz (" + std::to_string (hz) + ")");
            auto d = vintageDe ("an=1 ader=1 aedat=0 ains=0 afil=0 o1w=3 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto r2 = sonaV (*d, { { 0, 0x90, 69, 100 } }, 48000 * 6);
            double mn = 1e9, mx = 0;
            for (int k = 0; k < 6; ++k) { const double h = hzSB (r2.L, k * 48000 + 4800, k * 48000 + 43200); mn = std::min (mn, h); mx = std::max (mx, h); }
            const double cents = 1200.0 * std::log2 (mx / mn);
            comprova (cents > 0.5 && cents < 40.0, "la deriva mou l'afinació a poc a poc (" + std::to_string (cents) + " ct)");
            auto o = vintageDe ("o1o=-1 an=0 o1w=3 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto r3 = sonaV (*o, { { 0, 0x90, 69, 100 } }, 48000);
            comprova (std::abs (hzSB (r3.L, 12000, 48000) - 220.0) < 0.5, "el peu de 16' sona una octava avall");
        });

        prova ("1.4 VINTAGE: els quatre filtres filtren (LP fosc, HP brillant) i el Ladder oscil·la sol", []
        {
            for (int ft = 0; ft < 4; ++ft)
            {
                auto lp = vintageDe ("an=0 fenv=0 fkey=0 res=0.2 cut=500 ft=" + std::to_string (ft));
                auto hp = vintageDe ("an=0 fenv=0 fkey=0 res=0.2 cut=500 fmo=2 ft=" + std::to_string (ft));
                auto a = sonaV (*lp, { { 0, 0x90, 48, 100 } }, 24000), b = sonaV (*hp, { { 0, 0x90, 48, 100 } }, 24000);
                comprova (brillSB (a.L, 4800, 24000) * 4.0 < brillSB (b.L, 4800, 24000), "filtre " + std::to_string (ft) + ": el passa-baixos és molt més fosc que el passa-alts");
            }
            auto s = vintageDe ("an=0 o1n=0 nzn=0.02 res=1 cut=1000 fenv=0 fkey=0 aa=1 as=1");
            auto r = sonaV (*s, { { 0, 0x90, 60, 100 } }, 72000);
            const double hz = hzSB (r.L, 48000, 72000);
            comprova (rmsV (r.L, 48000, 72000) > 0.005 && std::abs (hz - 1000.0) < 150.0, "el Ladder al màxim xiula sol a la freqüència de tall (" + std::to_string (hz) + " Hz)");
            auto m = vintageDe ("an=0 ft=3 o1n=0 nzn=0.02 res=1 cut=800 fenv=0 fkey=0 aa=1 as=1");
            auto rm = sonaV (*m, { { 0, 0x90, 60, 100 } }, 72000);
            comprova (rmsV (rm.L, 48000, 72000) > 0.01 && picSB (rm.L, 0, 72000) < 1.0f, "el MS-20 també xiula, però no s'escapa");
        });

        prova ("1.4 VINTAGE: sync, FM, sub i ring canvien el so; l'osc 3 lent no sona", []
        {
            auto net = vintageDe ("an=0 o1n=0 o1w=3 o2w=3 o2n=0.8 o2s=5 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto sy = vintageDe ("an=0 o1n=0 o1w=3 o2w=3 o2n=0.8 o2s=5 sync=1 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto a = sonaV (*net, { { 0, 0x90, 48, 100 } }, 24000), b = sonaV (*sy, { { 0, 0x90, 48, 100 } }, 24000);
            comprova (brillSB (b.L, 4800, 24000) > brillSB (a.L, 4800, 24000) * 1.3, "el sync fa el so més brillant");
            auto fm = vintageDe ("an=0 o1w=3 o3w=3 o3o=1 fm=0.6 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto c = sonaV (*fm, { { 0, 0x90, 48, 100 } }, 24000);
            auto si = vintageDe ("an=0 o1w=3 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto d = sonaV (*si, { { 0, 0x90, 48, 100 } }, 24000);
            comprova (brillSB (c.L, 4800, 24000) > brillSB (d.L, 4800, 24000) * 2.0, "l'FM afegeix harmònics a un sinus");
            auto sub = vintageDe ("an=0 o1n=0 subn=1 subw=1 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto e = sonaV (*sub, { { 0, 0x90, 69, 100 } }, 48000);
            comprova (std::abs (hzSB (e.L, 12000, 48000) - 220.0) < 1.0, "el sub és una octava avall");
            auto lent = vintageDe ("an=0 o1n=0 o3n=1 o3lo=1 cut=18000 fenv=0 res=0");
            auto f = sonaV (*lent, { { 0, 0x90, 69, 100 } }, 24000);
            comprova (picSB (f.L, 0, 24000) < 1e-3f, "l'osc 3 lent és només per modular");
            auto rg = vintageDe ("an=0 o1w=3 o1n=0 o2w=3 o2s=7 ring=1 cut=18000 fenv=0 fkey=0 res=0 drv=0");
            auto g = sonaV (*rg, { { 0, 0x90, 60, 100 } }, 24000);
            comprova (rmsV (g.L, 4800, 24000) > 0.01, "el ring sona tot sol");
        });

        prova ("1.4 VINTAGE: mono, legato, poli i unison", []
        {
            auto p = vintageDe ("mode=2 veus=4");
            sonaV (*p, { { 0, 0x90, 48, 100 }, { 0, 0x90, 52, 100 }, { 0, 0x90, 55, 100 }, { 0, 0x90, 59, 100 }, { 0, 0x90, 62, 100 } }, 4800);
            comprova (p->veusActives() == 4, "poli amb 4 veus: 5 notes en fan sonar 4 (" + std::to_string (p->veusActives()) + ")");
            auto u = vintageDe ("mode=2 veus=8 uni=4");
            sonaV (*u, { { 0, 0x90, 48, 100 }, { 0, 0x90, 52, 100 }, { 0, 0x90, 55, 100 } }, 4800);
            comprova (u->veusActives() == 12, "3 notes amb unison de 4: 12 veus (" + std::to_string (u->veusActives()) + ")");
            auto m = vintageDe ("mode=0 uni=3");
            sonaV (*m, { { 0, 0x90, 48, 100 }, { 2400, 0x90, 52, 100 } }, 4800);
            comprova (m->veusActives() == 3, "mono amb unison de 3: 3 veus");
            auto l = vintageDe ("mode=1 glide=200 an=0 o1w=3 cut=18000 fenv=0 fkey=0 res=0");
            auto r = sonaV (*l, { { 0, 0x90, 57, 100 }, { 24000, 0x90, 69, 100 } }, 48000);
            const double mig = hzSB (r.L, 24500, 26500);
            comprova (mig > 225.0 && mig < 430.0, "legato amb glide: llisca de 220 a 440 Hz (" + std::to_string (mig) + ")");
        });

        prova ("1.4 VINTAGE: el caràcter es pot apagar del tot i els controls responen", []
        {
            auto a = vintageDe ("an=0 aest=1 ader=1 asor=1");
            auto r = sonaV (*a, { { 0, 0x90, 48, 100 } }, 24000);
            comprova (std::abs (rmsV (r.L, 4800, 24000) - rmsV (r.R, 4800, 24000)) < 1e-6, "Analògic Off: res de variació estèreo ni soroll");
            auto b = vintageDe ("an=1 aest=1");
            auto r2 = sonaV (*b, { { 0, 0x90, 48, 100 }, { 0, 0x90, 55, 100 }, { 0, 0x90, 60, 100 } }, 24000);
            comprova (std::abs (rmsV (r2.L, 4800, 24000) - rmsV (r2.R, 4800, 24000)) > 1e-4, "amb variació estèreo, cada veu va a un costat");
            auto c = vintageDe ("an=1 acal=0 asor=1 aedat=1 o1n=0");
            auto r3 = sonaV (*c, { { 0, 0x90, 48, 100 } }, 24000);
            comprova (rmsV (r3.L, 4800, 24000) > 1e-6 && rmsV (r3.L, 4800, 24000) < 1e-3, "soroll de fons: hi és, però molt fluix");
            auto s = vintageDe ("");
            comprova (s->paramDeCC (74) == FTall && s->paramDeCC (71) == FRes, "CC74 tall i CC71 ressonància");
            comprova (s->nomComplet (O2Fi) == "Osc 2 · Fi" && s->nomComplet (FA) == "Env filtre · Atac", "noms per a l'automatització");
            Vintage x; x.posaEstat (presetsFabrica()[40].estat);
            Vintage y; y.posaEstat (x.estat());
            comprova (x.estatSo() == y.estatSo(), "l'estat es desa i es llegeix igual");
        });

        prova ("1.4 VINTAGE: arpegiador, sons a l'atzar i variacions", []
        {
            auto s = vintageDe ("arp=1 arpo=2 arpr=3 mode=2");
            auto r = sonaV (*s, { { 0, 0x90, 48, 100 }, { 0, 0x90, 52, 100 } }, 48000);
            comprova (s->pasArp() >= 0 && rmsV (r.L, 24000, 48000) > 1e-3, "l'arpegiador toca sol");
            for (int t = 0; t < (int) tipusAtzar().size(); ++t)
            {
                Vintage v;
                v.posaEstat (aleatori (100u + (unsigned) t, t));
                v.prepara (48000, 256);
                auto q = sonaV (v, { { 0, 0x90, 48, 100 }, { 0, 0x90, 55, 100 } }, 48000);
                bool ok = true; for (auto x : q.L) ok = ok && std::isfinite (x) && std::abs (x) < 1.0f;
                comprova (ok && picSB (q.L, 0, 48000) > 0.005f, "so a l'atzar «" + tipusAtzar()[(size_t) t] + "» vàlid");
            }
            const auto& e = presetsFabrica()[10].estat;
            comprova (varia (e, 7u, 0.5f) != e, "la variació canvia el so");
        });

        prova ("1.4 VINTAGE: CPU d'un acord de 8 notes amb unison", []
        {
            auto s = vintageDe ("mode=2 veus=8 uni=2 o2n=0.7 o3n=0.5 subn=0.4 nzn=0.1");
            std::vector<std::array<int, 4>> ev;
            for (int k = 0; k < 8; ++k) ev.push_back ({ 0, 0x90, 48 + k * 3, 100 });
            const auto t0 = std::chrono::steady_clock::now();
            sonaV (*s, ev, 48000);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            comprova (s->veusActives() == 16, "16 veus");
            comprova (ms < 400.0, "1 s de so amb 16 veus en " + std::to_string ((int) ms) + " ms");
        });
    }
}
