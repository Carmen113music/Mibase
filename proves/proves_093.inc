// 0.93: Mibase NEO (sinte híbrid de disseny de so).
#include "mibase/Neo.h"
#include <set>
namespace
{
    using NeoPtr = std::unique_ptr<mibase::neo::Neo>;
    NeoPtr neoDe (const std::string& canvis, bool net = true)
    {
        auto s = std::make_unique<mibase::neo::Neo>();
        s->posaEstat (mibase::neo::construeix (canvis));
        if (net) for (int p : { (int) mibase::neo::RvMix, (int) mibase::neo::DlMix, (int) mibase::neo::ChMix }) s->posa (p, 0.0f);
        s->prepara (48000, 256);
        return s;
    }
    SonSB sonaNeo (mibase::neo::Neo& s, std::vector<std::array<int, 4>> ev, int total)
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
    int creuaments (const SonSB& r, int a, int b)
    {
        int c = 0;
        for (int i = a + 1; i < b; ++i) if (r.L[(size_t) i - 1] < 0 && r.L[(size_t) i] >= 0) ++c;
        return c;
    }
    double variacio (const SonSB& r, int a, int b, int pas)
    {
        // quant canvia la brillantor d'un tros a l'altre (moviment)
        double mn = 1e9, mx = 0;
        for (int x = a; x + pas <= b; x += pas) { const double v = brillantor (r, (size_t) x, (size_t) (x + pas)); mn = std::min (mn, v); mx = std::max (mx, v); }
        return mx / std::max (1e-9, mn);
    }

    void proves093()
    {
        using namespace mibase;
        using namespace mibase::neo;
        constexpr int fs = 48000;
        sinte::tempo().store (120.0);

        prova ("0.93 NEO: més de 150 presets, únics, a les 21 categories, amb les 8 macros configurades", []
        {
            const auto& ps = presetsFabrica();
            comprova (ps.size() >= 150, "presets: " + std::to_string (ps.size()));
            comprova (categories().size() == 21, "21 categories");
            std::set<std::string> noms, estats;
            for (const auto& p : ps) { noms.insert (p.nom); estats.insert (p.estat); comprova (p.descripcio.size() > 20, "descripció a «" + p.nom + "»"); }
            comprova (noms.size() == ps.size(), "noms únics");
            comprova (estats.size() == ps.size(), "cap preset repetit");
            for (const auto& c : categories())
            {
                int n = 0; for (const auto& p : ps) if (p.categoria == c) ++n;
                comprova (n >= 6, c + ": " + std::to_string (n));
            }
            int malament = 0;
            std::set<std::string> motors;
            for (const auto& p : ps)
            {
                Neo s; s.posaEstat (p.estat);
                for (int k = 0; k < numMacros; ++k)
                {
                    bool te = s.valor (Mc1D1 + k * 4) > 0.5f || s.valor (Mc1D2 + k * 4) > 0.5f;
                    for (int j = 0; j < numSlots; ++j) if ((int) s.valor (M1Via + j * 4) == VMac1 + k && s.valor (M1Font + j * 4) > 0.5f) te = true;
                    if (! te) ++malament;
                }
                if (std::abs (s.valor (Mac2) - 0.5f) > 0.01f) ++malament;
                std::string m;
                for (int o = 0; o < 3; ++o) if (s.valor (O1Niv + o * 11) > 0.0f || o == 0) m += std::to_string ((int) s.valor (O1Motor + o * 11));
                motors.insert (m + "/" + std::to_string ((int) s.valor (FTipus)));
            }
            comprova (malament == 0, "totes les macros fan alguna cosa (" + std::to_string (malament) + " sense)");
            comprova (motors.size() >= 40, "combinacions de motors i filtres: " + std::to_string (motors.size()));
        });

        prova ("0.93 NEO: tots els presets sonen, sense NaN ni passar de 0 dBFS, i anivellats", []
        {
            double mn = 0, mx = -200;
            for (const auto& p : presetsFabrica())
            {
                Neo s; s.posaEstat (p.estat); s.prepara (fs, 256);
                const bool mono = s.valor (Mode) < 1.5f;
                std::vector<std::array<int, 4>> ev;
                if (mono) ev.push_back ({ 0, 0x90, s.valor (O3Oct) < -0.5f && s.valor (Veus) < 1.5f ? 45 : 60, 100 });
                else for (int n : { 57, 60, 64 }) ev.push_back ({ 0, 0x90, n, 100 });
                const auto r = sonaNeo (s, ev, fs * 3);
                bool ok = true; float pic = 0.0f;
                for (size_t i = 0; i < r.L.size(); ++i)
                {
                    if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i])) ok = false;
                    pic = std::max (pic, std::max (std::abs (r.L[i]), std::abs (r.R[i])));
                }
                double fort = -200;
                for (size_t a = 0; a + fs / 20 <= r.L.size(); a += fs / 40) fort = std::max (fort, rmsDb (r, a, a + fs / 20));
                comprova (ok, "sense NaN: " + p.nom);
                comprova (pic < 1.0f, "no passa de 0 dBFS: " + p.nom);
                comprova (fort > -30.0, "sona: " + p.nom + " (" + std::to_string (fort) + " dB)");
                mn = std::min (mn, fort); mx = std::max (mx, fort);
            }
            std::printf ("        sonoritat dels presets (finestra més forta de 50 ms): de %.1f a %.1f dB\n", mn, mx);
        });

        prova ("0.93 NEO: afinació dels sis motors i del sub", []
        {
            const char* motors[] = { "o1m=0 o1t=0 o1p=0", "o1m=1 o1f=0.33", "o1m=2 o1p=0.5 o1f=0.5", "o1m=3 o1p=0 o1f=0.09", "o1m=4 o1t=0 gd=60 gm=60 gs=0", "o1m=1 o1f=0 subn=1" };
            for (int m = 0; m < 6; ++m)
            {
                auto s = neoDe (std::string (motors[m]) + " cut=18000 fenv=0 aa=1 as=1 uni=1");
                const auto r = sonaNeo (*s, { { 0, 0x90, 69, 100 } }, fs);
                const int c = creuaments (r, fs / 2, fs) * 2;
                comprova (m == 4 ? std::abs (c - 440) <= 40 : std::abs (c - 440) <= 3, std::string ("motor ") + std::to_string (m) + ": " + std::to_string (c) + " Hz");
            }
            auto z = neoDe ("o1m=5 aa=1 as=1");
            const auto r = sonaNeo (*z, { { 0, 0x90, 60, 100 } }, fs / 2);
            comprova (rmsDb (r, fs / 4, fs / 2) > -40.0, "el soroll sona");
        });

        prova ("0.93 NEO: polifonia configurable, mono i legato", []
        {
            auto s = neoDe ("veus=4 ar=3000");
            std::vector<std::array<int, 4>> ev;
            for (int n = 0; n < 8; ++n) ev.push_back ({ n * 512, 0x90, 48 + n * 2, 100 });
            sonaNeo (*s, ev, 8192);
            comprova (s->veusActives() == 4, "4 veus com a màxim (" + std::to_string (s->veusActives()) + ")");
            auto p = neoDe ("veus=16");
            ev.clear();
            for (int n = 0; n < 16; ++n) ev.push_back ({ 0, 0x90, 40 + n * 2, 100 });
            sonaNeo (*p, ev, 4096);
            comprova (p->veusActives() == 16, "16 veus alhora");
            auto m = neoDe ("mode=0");
            sonaNeo (*m, { { 0, 0x90, 48, 100 }, { 0, 0x90, 52, 100 }, { 0, 0x90, 55, 100 } }, 4096);
            comprova (m->veusActives() == 1, "mono: una veu");
            auto l = neoDe ("mode=1 glide=200 o1m=1 o1f=0.33 cut=18000 fenv=0 aa=1 as=1");
            const auto r = sonaNeo (*l, { { 0, 0x90, 57, 100 }, { fs / 2, 0x90, 69, 100 } }, fs + fs / 2);
            const int c = creuaments (r, fs + fs / 4, fs + fs / 2) * 4;
            comprova (std::abs (c - 440) <= 6, "legato: llisca fins a la nota nova (" + std::to_string (c) + ")");
            auto a = neoDe ("ar=100");
            sonaNeo (*a, { { 0, 0x90, 60, 100 }, { 4800, 0x80, 60, 0 } }, fs);
            comprova (a->veusActives() == 0, "la nota s'apaga després de l'alliberament");
        });

        prova ("0.93 NEO: matriu (LFO → tall), via amb la macro MOVIMENT i ritme d'àudio", []
        {
            const std::string base = "o1m=1 o1f=0.33 cut=800 res=0.1 fenv=0 aa=1 as=1 l1r=2 l1>cut=0.6";
            auto amb = neoDe (base);
            const auto r1 = sonaNeo (*amb, { { 0, 0x90, 48, 100 } }, fs);
            auto quiet = neoDe (base + " mac2=0");
            const auto r2 = sonaNeo (*quiet, { { 0, 0x90, 48, 100 } }, fs);
            const double v1 = variacio (r1, fs / 10, fs, fs / 20), v2 = variacio (r2, fs / 10, fs, fs / 20);
            comprova (v1 > 2.0, "l'LFO mou el tall (" + std::to_string (v1) + ")");
            comprova (v2 < 1.3, "MOVIMENT a zero l'atura (" + std::to_string (v2) + ")");
            auto au0 = neoDe ("o1m=2 o1p=0 o1f=0 o3m=2 o3p=0 o3f=0 o3o=0 o3s=7 o3n=0 cut=18000 aa=1 as=1");
            auto au1 = neoDe ("o1m=2 o1p=0 o1f=0 o3m=2 o3p=0 o3f=0 o3o=0 o3s=7 o3n=0 cut=18000 aa=1 as=1 o3>to1=0.6");
            const auto a0 = sonaNeo (*au0, { { 0, 0x90, 60, 100 } }, fs / 2), a1 = sonaNeo (*au1, { { 0, 0x90, 60, 100 } }, fs / 2);
            comprova (brillantor (a1, fs / 4, fs / 2) > 2.0 * brillantor (a0, fs / 4, fs / 2), "l'osc 3 modula el to de l'1 a ritme d'àudio (FM)");
            auto pas = neoDe ("o1m=1 o1f=0.33 aa=1 as=1 pr=1 p1=1 p2=-1 pn=2 pas>vol=0.9");
            const auto rp = sonaNeo (*pas, { { 0, 0x90, 60, 100 } }, fs);
            comprova (rmsDb (rp, 3000, 9000) - rmsDb (rp, 15000, 21000) > 10.0, "els passos mouen el volum a corxeres");
        });

        prova ("0.93 NEO: macros (Brillantor, Agressivitat, Morph, Amplada, Espai)", []
        {
            auto so = [] (const std::string& c, double& br, double& amp, double& cua)
            {
                auto s = neoDe ("o1m=0 o1t=3 o1p=0 cut=1500 fenv=0 aa=1 as=1 ar=50 " + c, false);
                const auto r = sonaNeo (*s, { { 0, 0x90, 60, 100 }, { fs / 2, 0x80, 60, 0 } }, fs + fs / 2);
                br = brillantor (r, fs / 4, fs / 2);
                double d = 0, t = 1e-20;
                for (int i = fs / 4; i < fs / 2; ++i) { d += (r.L[(size_t) i] - r.R[(size_t) i]) * (r.L[(size_t) i] - r.R[(size_t) i]); t += r.L[(size_t) i] * r.L[(size_t) i]; }
                amp = d / t;
                cua = rmsDb (r, fs, fs + fs / 4);
            };
            double b0, a0, c0, b1, a1, c1;
            so ("", b0, a0, c0);
            so ("o1m=1 o1f=0.33", b0, a0, c0);
            so ("o1m=1 o1f=0.33 mac3=1", b1, a1, c1);
            comprova (b1 > 1.5 * b0, "BRILLANTOR obre el so");
            so ("", b0, a0, c0);
            so ("mac4=1", b1, a1, c1);
            comprova (b1 > 1.2 * b0, "AGRESSIVITAT afegeix harmònics");
            so ("uni=4 mac5=1", b1, a1, c1);
            comprova (a1 > a0 + 0.05, "AMPLADA obre l'estèreo");
            so ("mac6=1", b1, a1, c1);
            comprova (c1 > c0 + 20.0, "ESPAI afegeix cua (reverb i delay)");
            double m0, m1, x;
            so ("o1t=6 mac8=0", m0, x, x);
            so ("o1t=6 mac8=1", m1, x, x);
            comprova (std::abs (m1 - m0) / m0 > 0.15, "MORPH canvia el timbre");
        });

        prova ("0.93 NEO: filtres, ressonadors, plec i efectes no fan valors no vàlids", []
        {
            std::vector<std::string> llista;
            for (int f = 0; f < 8; ++f) llista.push_back ("ft=" + std::to_string (f) + " res=0.95");
            for (int m = 0; m < 4; ++m) llista.push_back ("rsm=1 rsd=1 rst=" + std::to_string (m));
            for (int w = 1; w < 7; ++w) llista.push_back ("o2m=1 o2n=0.5 o1w=" + std::to_string (w) + " o1wq=1");
            llista.push_back ("fold=1 dq=1 dt=2");
            llista.push_back ("crm=1 crb=2 crr=40 shm=1 shh=-1000 chm=1 dlm=1 dlf=0.92 rvm=1 rvt=3 tilt=1 exc=1");
            llista.push_back ("o1m=4 gd=200 gm=8 gs=1 uni=8");
            for (const auto& c : llista)
            {
                auto s = neoDe ("o1m=1 o1f=0.33 aa=1 as=1 " + c, false);
                std::vector<std::array<int, 4>> ev;
                for (int n : { 24, 60, 96, 108 }) ev.push_back ({ 0, 0x90, n, 127 });
                const auto r = sonaNeo (*s, ev, fs);
                bool ok = true; float pic = 0.0f;
                for (size_t i = 0; i < r.L.size(); ++i) { if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i])) ok = false; pic = std::max (pic, std::abs (r.L[i])); }
                comprova (ok && pic < 1.0f && pic > 1e-4f, c);
            }
            auto f0 = neoDe ("o1m=1 o1f=0.33 cut=18000 aa=1 as=1"), f1 = neoDe ("o1m=1 o1f=0.33 cut=18000 aa=1 as=1 shm=1 shh=300");
            const auto r0 = sonaNeo (*f0, { { 0, 0x90, 45, 100 } }, fs / 2), r1 = sonaNeo (*f1, { { 0, 0x90, 45, 100 } }, fs / 2);
            comprova (creuaments (r1, fs / 4, fs / 2) > creuaments (r0, fs / 4, fs / 2) + 30, "el desplaçador mou les freqüències amunt");
        });

        prova ("0.93 NEO: desar i carregar, atzar per tipus, variacions i arpegiador", []
        {
            const auto& p = presetsFabrica()[17];
            Neo a; a.posaEstat (p.estat);
            Neo b; b.posaEstat (a.estatSo());
            comprova (a.estatSo() == b.estatSo(), "l'estat es desa i es torna a llegir igual");
            std::set<std::string> fets;
            for (int t = 0; t < (int) tipusAtzar().size(); ++t)
                for (unsigned l = 1; l <= 3; ++l)
                {
                    const auto e = aleatori (l * 31 + (unsigned) t, t);
                    fets.insert (e);
                    Neo s; s.posaEstat (e); s.prepara (fs, 256);
                    const auto r = sonaNeo (s, { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 } }, fs * 2);
                    bool ok = true; double fort = -200;
                    for (size_t i = 0; i < r.L.size(); ++i) if (! std::isfinite (r.L[i])) ok = false;
                    for (size_t x = 0; x + fs / 20 <= r.L.size(); x += fs / 40) fort = std::max (fort, rmsDb (r, x, x + fs / 20));
                    comprova (ok && fort > -40.0, "atzar «" + tipusAtzar()[(size_t) t] + "» sona (" + std::to_string (fort) + ")");
                }
            comprova (fets.size() == tipusAtzar().size() * 3, "l'atzar fa sons diferents");
            const auto v = varia (p.estat, 5, 1.0f);
            Neo c; c.posaEstat (v);
            comprova (v != p.estat && c.valor (Mc3D1) == a.valor (Mc3D1) && c.valor (Mac2) == a.valor (Mac2), "variar canvia el so i respecta les macros");
            // arpegiador: una tecla fa notes
            Neo s; s.posaEstat (construeix ("arp=1 arpr=3 arpo=2 aa=1 ad=80 as=0 ar=40"));
            s.prepara (fs, 256);
            const auto r = sonaNeo (s, { { 0, 0x90, 60, 100 } }, fs);
            int cops = 0; bool alt = false;
            for (int x = 0; x + 480 <= fs; x += 480) { const bool so = rmsDb (r, (size_t) x, (size_t) x + 480) > -35.0; if (so && ! alt) ++cops; alt = so; }
            comprova (cops >= 6, "una tecla amb l'arpegiador fa moltes notes (" + std::to_string (cops) + ")");
        });
    }
}
