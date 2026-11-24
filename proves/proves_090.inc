// 0.90: Mibase ATMOS (pads, textures, drones).
#include "mibase/Atmos.h"
#include <set>
namespace
{
    SonSB sonaAtmos (mibase::atmos::Atmos& s, std::vector<std::array<int, 4>> ev, int total)
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
    std::vector<std::array<int, 4>> acordAtmos (std::initializer_list<int> notes, int fins = -1)
    {
        std::vector<std::array<int, 4>> ev;
        for (int n : notes) ev.push_back ({ 0, 0x90, n, 100 });
        if (fins > 0) for (int n : notes) ev.push_back ({ fins, 0x80, n, 0 });
        return ev;
    }
    double rmsDb (const SonSB& s, size_t a, size_t b)
    {
        double q = 0; b = std::min (b, s.L.size());
        for (size_t i = a; i < b; ++i) q += 0.5 * (s.L[i] * s.L[i] + s.R[i] * s.R[i]);
        return 10.0 * std::log10 (q / std::max<size_t> (1, b - a) + 1e-20);
    }
    /** Brillantor: proporció d'energia de la derivada (com més aguts, més alta). */
    double brillantor (const SonSB& s, size_t a, size_t b)
    {
        double e = 1e-20, d = 0; b = std::min (b, s.L.size());
        for (size_t i = a + 1; i < b; ++i) { e += s.L[i] * s.L[i]; const double x = s.L[i] - s.L[i - 1]; d += x * x; }
        return d / e;
    }

    void proves090()
    {
        using namespace mibase;
        using namespace mibase::atmos;
        constexpr int fs = 48000;

        prova ("0.90 ATMOS: més de 150 presets, únics, amb categoria i descripció", []
        {
            const auto& ps = presetsFabrica();
            comprova (ps.size() >= 150, "presets: " + std::to_string (ps.size()));
            std::set<std::string> noms, estats, cats;
            for (const auto& p : ps) { noms.insert (p.nom); estats.insert (p.estat); cats.insert (p.categoria); comprova (p.descripcio.size() > 20, "descripció a «" + p.nom + "»"); }
            comprova (noms.size() == ps.size(), "noms únics");
            comprova (estats.size() == ps.size(), "cap preset repetit (paràmetres)");
            comprova (cats.size() == categories().size(), "totes les categories tenen presets");
            for (const auto& c : categories())
            {
                int n = 0; for (const auto& p : ps) if (p.categoria == c) ++n;
                comprova (n >= 4, c + ": " + std::to_string (n));
            }
            // de veritat diferents: combinacions d'ones i taules dels oscil·ladors que sonen
            std::set<std::string> combinacions;
            for (const auto& p : ps)
            {
                Atmos s; s.posaEstat (p.estat);
                std::string k;
                for (int o = 0; o < 4; ++o)
                    if (s.valor (O1Niv + o * 8) > 0.0f) k += std::to_string ((int) s.valor (O1Ona + o * 8)) + "/" + std::to_string ((int) s.valor (O1Taula + o * 8)) + " ";
                k += std::to_string ((int) s.valor (FTipus)) + (s.valor (NzNiv) > 0 ? "n" : "") + (s.valor (ArpMode) > 0 ? "a" : "");
                combinacions.insert (k);
            }
            comprova (combinacions.size() >= 70, "configuracions d'oscil·ladors diferents: " + std::to_string (combinacions.size()));
        });

        prova ("0.90 ATMOS: tots els presets sonen, sense valors no vàlids ni passar de 0 dBFS, i anivellats", []
        {
            double mn = 0, mx = -200;
            for (const auto& p : presetsFabrica())
            {
                Atmos s; s.posaEstat (p.estat); s.prepara (fs, 256);
                const auto r = sonaAtmos (s, acordAtmos ({ 48, 52, 55, 59 }, fs * 2), fs * 3);
                bool ok = true; float pic = 0.0f;
                for (size_t i = 0; i < r.L.size(); ++i)
                {
                    if (! std::isfinite (r.L[i]) || ! std::isfinite (r.R[i])) ok = false;
                    pic = std::max (pic, std::max (std::abs (r.L[i]), std::abs (r.R[i])));
                }
                double fort = -200;
                for (size_t a = 0; a + fs / 2 <= r.L.size(); a += fs / 8) fort = std::max (fort, rmsDb (r, a, a + fs / 2));
                comprova (ok, "sense NaN: " + p.nom);
                comprova (pic < 1.0f, "no passa de 0 dBFS: " + p.nom);
                comprova (fort > -30.0, "sona: " + p.nom + " (" + std::to_string (fort) + " dB)");
                mn = std::min (mn, fort); mx = std::max (mx, fort);
            }
            std::printf ("        sonoritat dels presets (3 s, acord de 4 notes): de %.1f a %.1f dB\n", mn, mx);
            comprova (mx - mn < 9.0, "anivellats (diferència < 9 dB)");
        });

        prova ("0.90 ATMOS: afinació, poli de 12 veus, mono i alliberament", []
        {
            Atmos s;
            for (int o = 0; o < 4; ++o) s.posa (O1Niv + o * 8, 0.0f);
            s.posa (O1Niv, 1.0f); s.posa (O1Ona, (float) Sinus); s.posa (Unison, 1.0f); s.posa (Deriva, 0.0f);
            for (int k = 0; k < 8; ++k) s.posa (M1Quant + k * 3, 0.0f);
            s.posa (ChMix, 0.0f); s.posa (DlMix, 0.0f); s.posa (RvMix, 0.0f); s.posa (AA, 1.0f); s.posa (FTall, 18000.0f);
            s.prepara (fs, 256);
            const auto r = sonaAtmos (s, acordAtmos ({ 69 }), fs);
            int creus = 0; for (int i = fs / 2 + 1; i < fs; ++i) if (r.L[(size_t) i - 1] < 0 && r.L[(size_t) i] >= 0) ++creus;
            comprova (std::abs (creus * 2 - 440) <= 2, "La4 = 440 Hz (" + std::to_string (creus * 2) + ")");
            Atmos p; p.prepara (fs, 256);
            std::vector<std::array<int, 4>> ev;
            for (int n = 0; n < 12; ++n) ev.push_back ({ 0, 0x90, 48 + n * 2, 100 });
            sonaAtmos (p, ev, 4096);
            comprova (p.veusActives() == 12, "12 veus alhora (" + std::to_string (p.veusActives()) + ")");
            ev.push_back ({ 0, 0x90, 100, 100 });
            Atmos q; q.prepara (fs, 256); sonaAtmos (q, ev, 4096);
            comprova (q.veusActives() == 12, "la 13a roba una veu, no en fa més");
            Atmos m; m.posa (Veus, 1.0f); m.prepara (fs, 256);
            sonaAtmos (m, acordAtmos ({ 48, 52, 55 }), 4096);
            comprova (m.veusActives() == 1, "mono: una veu");
            Atmos a; a.posa (AR, 100.0f); a.posa (RvMix, 0.0f); a.posa (DlMix, 0.0f); a.posa (ChMix, 0.0f); a.prepara (fs, 256);
            sonaAtmos (a, acordAtmos ({ 60 }, 4800), fs * 2);
            comprova (a.veusActives() == 0, "la nota s'apaga després de l'alliberament");
        });

        prova ("0.90 ATMOS: granular, textures i estèreo", []
        {
            for (int t = 0; t < 4; ++t)
            {
                Atmos s;
                for (int o = 0; o < 4; ++o) s.posa (O1Niv + o * 8, 0.0f);
                s.posa (NzNiv, 1.0f); s.posa (NzTipus, (float) t); s.posa (NzMov, 0.8f); s.posa (AA, 1.0f);
                s.prepara (fs, 256);
                const auto r = sonaAtmos (s, acordAtmos ({ 60 }), fs);
                comprova (rmsDb (r, fs / 2, fs) > -55.0, "textura " + std::to_string (t) + " sona (" + std::to_string (rmsDb (r, fs / 2, fs)) + ")");
            }
            Atmos g;
            for (int o = 0; o < 4; ++o) g.posa (O1Niv + o * 8, 0.0f);
            g.posa (O1Ona, (float) Granular); g.posa (O1Niv, 1.0f); g.posa (O1Taula, 8.0f); g.posa (GrDens, 30.0f); g.posa (AA, 1.0f);
            g.prepara (fs, 256);
            const auto r = sonaAtmos (g, acordAtmos ({ 60 }), fs * 2);
            double c = 0, l = 0, rr = 0;
            for (size_t i = fs; i < r.L.size(); ++i) { c += r.L[i] * r.R[i]; l += r.L[i] * r.L[i]; rr += r.R[i] * r.R[i]; }
            comprova (rmsDb (r, fs, fs * 2) > -40.0, "els grànuls sonen");
            comprova (c / std::sqrt (l * rr + 1e-20) < 0.97, "i s'escampen en estèreo");
        });

        prova ("0.90 ATMOS: l'evolució i els macros canvien el so", []
        {
            const Preset* alba = nullptr;
            for (const auto& p : presetsFabrica()) if (p.nom == "Alba") alba = &p;
            comprova (alba != nullptr, "hi ha «Alba»");
            if (alba == nullptr) return;
            Atmos s; s.posaEstat (alba->estat); s.posa (RvMix, 0.0f); s.posa (DlMix, 0.0f); s.prepara (fs, 256);
            const auto r = sonaAtmos (s, acordAtmos ({ 48, 55, 60 }), fs * 16);
            const double b0 = brillantor (r, fs * 3, fs * 4), b1 = brillantor (r, fs * 15, fs * 16);
            std::printf ("        evolució «Alba»: brillantor %.4f → %.4f\n", b0, b1);
            comprova (b1 > b0 * 1.5, "es fa més brillant amb el temps");
            auto amb = [&] (int p, float v)
            {
                Atmos x; x.posa (p, v); x.posa (RvMix, 0.0f); x.posa (DlMix, 0.0f); x.posa (ChMix, 0.0f); x.prepara (fs, 256);
                const auto rr = sonaAtmos (x, acordAtmos ({ 48, 55, 60 }), fs * 3);
                return std::make_pair (brillantor (rr, fs * 2, fs * 3), rmsDb (rr, fs / 10, fs / 5));
            };
            comprova (amb (MFiltre, 1.0f).first > amb (MFiltre, -1.0f).first * 2.0, "macro Filtre: obre i tanca");
            comprova (amb (MBrillantor, 1.0f).first > amb (MBrillantor, -1.0f).first * 1.5, "macro Brillantor");
            comprova (amb (MAtac, -1.0f).second > amb (MAtac, 1.0f).second + 6.0, "macro Atac: més ràpid = més so al principi");
        });

        prova ("0.90 ATMOS: freeze de la reverb, estat, CC i automatització", []
        {
            Atmos s; s.posa (RvMix, 0.6f); s.posa (RvMida, 1.0f); s.posa (AR, 50.0f); s.prepara (fs, 256);
            sonaAtmos (s, acordAtmos ({ 60, 64 }, fs), fs * 2);
            s.posa (RvFreeze, 1.0f);
            const auto r = sonaAtmos (s, {}, fs * 6);
            comprova (rmsDb (r, fs * 5, fs * 6) > -60.0 && s.fentSo(), "amb el freeze, la cua no s'apaga (" + std::to_string (rmsDb (r, fs * 5, fs * 6)) + ")");
            Atmos a; a.posa (FTall, 777.0f); a.posa (MMoviment, 0.4f); a.assignaCC (20, MBrillantor); a.nomPreset = "Prova";
            Atmos b; b.posaEstat (a.estat());
            comprova (std::abs (b.valor (FTall) - 777.0f) < 0.5f && std::abs (b.valor (MMoviment) - 0.4f) < 1e-4f, "l'estat torna el so exacte");
            comprova (b.paramDeCC (20) == MBrillantor && b.nomPreset == "Prova", "i les assignacions de CC i el nom");
            comprova (b.numParametres() == numParams && b.nomComplet (M3Quant) == "Matriu 3 · Quantitat", "automatització: noms amb la secció");
        });

        prova ("0.90 ATMOS: atzar musical de cada tipus i variacions", []
        {
            for (int t = 0; t < (int) tipusPad().size(); ++t)
                for (unsigned l = 1; l <= 3; ++l)
                {
                    Atmos s; s.posaEstat (aleatori (l * 977u + (unsigned) t, t)); s.prepara (fs, 256);
                    const auto r = sonaAtmos (s, acordAtmos ({ 48, 52, 55, 59 }, fs * 2), fs * 3);
                    double fort = -200; float pic = 0; bool ok = true;
                    for (size_t a = 0; a + fs / 2 <= r.L.size(); a += fs / 8) fort = std::max (fort, rmsDb (r, a, a + fs / 2));
                    for (size_t i = 0; i < r.L.size(); ++i) { ok = ok && std::isfinite (r.L[i]); pic = std::max (pic, std::abs (r.L[i])); }
                    comprova (ok && pic < 1.0f && fort > -32.0 && fort < -8.0, tipusPad()[(size_t) t] + " " + std::to_string (l) + ": " + std::to_string (fort) + " dB");
                }
            const auto e = presetsFabrica()[10].estat;
            comprova (varia (e, 5, 0.35f) != e && varia (e, 5, 0.35f) == varia (e, 5, 0.35f), "variació: canvia i és repetible");
            std::vector<float> o (256);
            comprova (previsualitza (e, o.data(), 256) > 0.01f, "previsualització de la forma d'ona");
        });

        prova ("0.90 ATMOS: consum amb 12 veus del preset més pesat", []
        {
            const Preset* p = nullptr;
            for (const auto& x : presetsFabrica()) if (x.nom == "Muntanya") p = &x;
            if (p == nullptr) return;
            Atmos s; s.posaEstat (p->estat); s.prepara (fs, 256);
            std::vector<std::array<int, 4>> ev;
            for (int n = 0; n < 12; ++n) ev.push_back ({ 0, 0x90, 40 + n * 3, 100 });
            const auto t0 = std::chrono::steady_clock::now();
            sonaAtmos (s, ev, fs);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("        1 s de 12 notes (4 osc, unison de 5): %.0f ms\n", ms);
            comprova (ms < 600.0, "cap en temps real amb marge");
        });
    }
}
