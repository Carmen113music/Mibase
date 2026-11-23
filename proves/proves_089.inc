// 0.89: motor comú de sintes, arpegiador, Mibase LEAD.
namespace
{
    // Un sinte de prova que apunta quan li arriben les notes (per veure l'arpegiador)
    const mibase::sinte::InfoParam taulaProva[] = {
        { "arp", "Arp", 0, 0, 7, 0, mibase::sinte::Enter, mibase::sinte::modesArp, "" },
        { "arpr", "Ritme", 0, 0, 5, 3, mibase::sinte::Enter, mibase::sinte::ritmesArp, "" },
        { "arpo", "Octaves", 0, 1, 4, 1, mibase::sinte::Enter, nullptr, "" },
        { "arpg", "Gate", 0, 0.05f, 1, 0.5f, mibase::sinte::Lineal, nullptr, "" },
        { "arpsw", "Swing", 0, 0, 0.6f, 0, mibase::sinte::Lineal, nullptr, "" },
        { "arpp", "Patró", 0, 0, 15, 0, mibase::sinte::Enter, mibase::sinte::nomsPatrons, "" },
    };
    struct SinteProva : mibase::sinte::Base
    {
        SinteProva() : Base (taulaProva, 6, "prova", { 0, 1, 2, 3, 4, 5 }) {}
        std::vector<std::pair<long, int>> ons, offs;
        long t = 0;
        int sonen = 0;
        void preparaSo (double) override {}
        void notaOn (int n, int) noexcept override { ons.push_back ({ t, n }); ++sonen; }
        void notaOff (int n) noexcept override { offs.push_back ({ t, n }); --sonen; }
        void apagaVeus() noexcept override { sonen = 0; }
        bool veusSonen() const noexcept override { return sonen > 0; }
        int numVeusActives() const noexcept override { return sonen; }
        void renderitza (float* L, float* R, int n) noexcept override { for (int i = 0; i < n; ++i) L[i] = R[i] = 0.0f; t += n; }
    };
    void sonaProva (SinteProva& s, std::vector<std::array<int, 4>> ev, int total)
    {
        std::vector<float> L (256), R (256);
        for (int a = 0; a < total; a += 256)
        {
            const int n = std::min (256, total - a);
            std::vector<mibase::EventInstrument> e;
            for (auto& x : ev) if (x[0] >= a && x[0] < a + n) e.push_back ({ x[0] - a, (std::uint8_t) x[1], (std::uint8_t) x[2], (std::uint8_t) x[3] });
            s.processa (e.data(), (int) e.size(), L.data(), R.data(), n);
        }
    }
    SonSB sonaLead (mibase::lead::Lead& s, std::vector<std::array<int, 4>> ev, int total)
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

    void proves089()
    {
        using namespace mibase;
        prova ("0.89 arpegiador: una sola tecla fa sonar el patró amb el tempo", []
        {
            sinte::tempo().store (120.0);
            SinteProva s;
            s.prepara (48000.0, 256);
            s.posa (0, 7.0f);   // patró
            s.posa (5, 0.0f);   // «Octaves»: 0 12 0 12…
            sonaProva (s, { { 0, 0x90, 48, 100 }, { 48000, 0x80, 48, 0 } }, 60000);
            comprova (s.ons.size() == 8, "1 s a 120 bpm i semicorxeres = 8 passos (" + std::to_string (s.ons.size()) + ")");
            comprova (s.ons.size() >= 2 && s.ons[0].second == 48 && s.ons[1].second == 60, "fa la nota i l'octava");
            comprova (s.ons.size() >= 2 && std::abs (s.ons[1].first - 6000) <= 1, "cada pas a la mostra exacta (6000)");
            comprova (s.sonen == 0 && s.offs.size() == s.ons.size(), "en deixar la tecla, tot apagat");
            comprova (! s.fentSo() && s.pasArp() < 0, "i l'arpegiador s'atura");
        });
        prova ("0.89 arpegiador: acords, octaves, gate, lligades i fentSo", []
        {
            sinte::tempo().store (120.0);
            SinteProva s;
            s.prepara (48000.0, 256);
            s.posa (0, 1.0f); s.posa (2, 2.0f); s.posa (1, 1.0f);   // amunt, 2 octaves, corxeres
            sonaProva (s, { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 }, { 0, 0x90, 67, 100 } }, 72000);
            std::vector<int> n; for (auto& x : s.ons) n.push_back (x.second);
            comprova (n.size() >= 6 && n[0] == 60 && n[1] == 64 && n[2] == 67 && n[3] == 72 && n[4] == 76 && n[5] == 79, "amunt en dues octaves");
            comprova (s.fentSo(), "mentre es prem, el sinte «fa so» (el mesclador no l'atura entre notes)");
            comprova (! s.offs.empty() && s.offs[0].first == 6000, "gate del 50 %: l'apagada a mig pas");
            SinteProva l;
            l.prepara (48000.0, 256);
            l.posa (0, 1.0f); l.posa (3, 1.0f);   // gate al màxim: lligades
            sonaProva (l, { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 } }, 13000);
            comprova (l.ons.size() == 3 && l.offs.size() == 2 && l.offs[0].first == l.ons[1].first, "lligades: l'apagada arriba just després de la nota nova");
            SinteProva c;
            c.prepara (48000.0, 256);
            c.posa (0, 6.0f);   // acord
            sonaProva (c, { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 } }, 6500);
            comprova (c.ons.size() == 4, "mode acord: totes les notes a cada pas");
        });
        prova ("0.89 SUB & BASS: les seqüències toquen soles amb una tecla", []
        {
            sinte::tempo().store (120.0);
            const subbass::Preset* p = nullptr;
            for (auto& x : subbass::presetsFabrica()) if (x.nom == "Seqüència berlinesa") p = &x;
            comprova (p != nullptr, "hi ha el preset");
            if (p == nullptr) return;
            subbass::SubBass s;
            s.posaEstat (p->estat);
            s.prepara (48000.0, 256);
            auto r = sonaSB (s, { { 0, 0x90, 36, 100 } }, 48000);
            int atacs = 0;   // compta els cops (envolupant que torna a pujar)
            float ant = 0.0f; bool baixant = false;
            for (int b = 0; b + 480 <= 48000; b += 480)
            {
                const float pk = picSB (r.L, b, b + 480);
                if (pk > ant * 1.5f && baixant && pk > 0.02f) { ++atacs; baixant = false; }
                if (pk < ant * 0.9f) baixant = true;
                ant = pk;
            }
            comprova (atacs >= 5, "en 1 s hi ha molts cops, no una sola nota (" + std::to_string (atacs) + ")");
            comprova (s.pasArp() >= 7, "l'arpegiador avança");
        });
        prova ("0.89 LEAD: presets", []
        {
            const auto& v = lead::presetsFabrica();
            comprova (v.size() >= 120, "almenys 120 presets (" + std::to_string (v.size()) + ")");
            std::set<std::string> estats, noms;
            std::map<std::string, int> per;
            for (auto& p : v) { estats.insert (p.estat); noms.insert (p.nom); ++per[p.categoria]; }
            comprova (estats.size() == v.size() && noms.size() == v.size(), "tots diferents i sense noms repetits");
            for (auto& c : lead::categories()) comprova (per[c] >= 4, "categoria «" + c + "» amb presets");
            comprova (per.size() == lead::categories().size(), "cap fora de les categories");
            int muts = 0, dolents = 0;
            for (auto& p : v)
            {
                lead::Lead s;
                s.posaEstat (p.estat);
                s.prepara (48000.0, 256);
                auto r = sonaLead (s, { { 0, 0x90, 64, 110 }, { 14400, 0x80, 64, 0 } }, 19200);
                bool nan = false;
                for (float x : r.L) if (! std::isfinite (x)) nan = true;
                const float pk = std::max (picSB (r.L, 0, 19200), picSB (r.R, 0, 19200));
                if (pk < 0.01f) { ++muts; std::printf ("    (mut: %s)\n", p.nom.c_str()); }
                if (nan || pk > 1.0f) { ++dolents; std::printf ("    (dolent: %s %.2f)\n", p.nom.c_str(), pk); }
            }
            comprova (muts == 0, "cap preset mut");
            comprova (dolents == 0, "cap amb NaN ni per sobre de 0 dBFS");
        });
        prova ("0.89 LEAD: afinació, ones, filtres, supersaw i aftertouch", []
        {
            using namespace lead;
            auto fa = [] (std::function<void (Lead&)> cfg, std::vector<std::array<int, 4>> ev = { { 0, 0x90, 57, 100 } })
            {
                auto s = std::make_unique<Lead>();
                s->posa (Niv2, 0.0f); s->posa (FTall, 18000.0f); s->posa (FEnv, 0.0f); s->posa (FRes, 0.0f); s->posa (FKey, 0.0f);
                cfg (*s);
                s->prepara (48000.0, 256);
                return sonaLead (*s, ev, 48000);
            };
            auto r = fa ([] (Lead& s) { s.posa (O1Ona, Sinus); });
            const double hz = hzSB (r.L, 12000, 48000);
            comprova (std::abs (hz - 220.0) < 0.6, "A3 = 220 Hz (" + std::to_string (hz) + ")");
            for (int o = 0; o <= Wavetable; ++o)
            {
                auto x = fa ([o] (Lead& s) { s.posa (O1Ona, (float) o); s.posa (O1Forma, 0.5f); });
                bool ok = picSB (x.L, 0, 48000) > 0.05f;
                for (float y : x.L) if (! std::isfinite (y)) ok = false;
                comprova (ok, std::string ("ona ") + sinte::opcions (info (O1Ona))[(size_t) o] + " sona");
            }
            for (int t = 0; t < wt::numTaules; ++t)
            {
                auto x = fa ([t] (Lead& s) { s.posa (O1Ona, Wavetable); s.posa (O1Taula, (float) t); s.posa (O1Forma, 0.7f); });
                comprova (std::abs (hzSB (x.L, 12000, 48000) - 220.0) < 25.0 || t >= 4, std::string ("wavetable ") + wt::nomTaula (t));
            }
            auto lp = fa ([] (Lead& s) { s.posa (FTall, 300.0f); s.posa (FTipus, 0.0f); s.posa (FPendent, 2.0f); });
            auto hp = fa ([] (Lead& s) { s.posa (FTall, 3000.0f); s.posa (FTipus, 1.0f); s.posa (FPendent, 2.0f); });
            auto obert = fa ([] (Lead&) {});
            comprova (brillSB (hp.L, 6000, 48000) > brillSB (obert.L, 6000, 48000) && brillSB (lp.L, 6000, 48000) < brillSB (obert.L, 6000, 48000) * 0.3,
                      "LP de 36 dB enfosqueix i HP aclareix");
            auto bp = fa ([] (Lead& s) { s.posa (FTall, 1000.0f); s.posa (FTipus, 2.0f); s.posa (FRes, 0.6f); });
            comprova (picSB (bp.L, 6000, 48000) > 0.01f, "BP sona");
            auto ss = fa ([] (Lead& s) { s.posa (O1Ona, Supersaw); s.posa (O1Forma, 0.6f); });
            double dif = 0.0; for (int i = 6000; i < 48000; ++i) dif += std::abs (ss.L[(size_t) i] - ss.R[(size_t) i]);
            comprova (dif > 100.0, "el supersaw és estèreo");
            auto at = fa ([] (Lead& s) { s.posa (O1Ona, Sinus); s.posa (AtVib, 1.0f); s.posa (RodaVib, 0.0f); },
                          { { 0, 0x90, 57, 100 }, { 0, 0xD0, 127, 0 } });
            double mn = 1e9, mx = 0.0;
            for (int b = 6000; b + 4800 <= 48000; b += 4800) { const double h = hzSB (at.L, b, b + 4800); mn = std::min (mn, h); mx = std::max (mx, h); }
            comprova (mx - mn > 3.0, "l'aftertouch fa vibrato");
        });
        prova ("0.89 LEAD: legato amb glide, poli, efectes i desar", []
        {
            using namespace lead;
            Lead s;
            s.posa (O1Ona, Sinus); s.posa (Niv2, 0.0f); s.posa (Mode, 1.0f); s.posa (Glide, 300.0f); s.posa (FTall, 18000.0f); s.posa (FEnv, 0.0f);
            s.prepara (48000.0, 256);
            auto r = sonaLead (s, { { 0, 0x90, 57, 100 }, { 24000, 0x90, 69, 100 }, { 24100, 0x80, 57, 0 } }, 96000);
            const double mig = hzSB (r.L, 25000, 30000), fi = hzSB (r.L, 72000, 96000);
            comprova (mig > 225.0 && mig < 430.0 && std::abs (fi - 440.0) < 2.0, "legato llisca de 220 a 440 Hz");
            Lead p;
            p.posa (Mode, 2.0f);
            p.prepara (48000.0, 256);
            sonaLead (p, { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 }, { 0, 0x90, 67, 100 }, { 0, 0x90, 71, 100 } }, 4800);
            comprova (p.veusActives() == 4, "poli: 4 veus");
            Lead e;
            e.posa (DlMix, 0.5f); e.posa (RvMix, 0.4f); e.posa (ChMix, 0.5f); e.posa (AR, 20.0f);
            e.prepara (48000.0, 256);
            auto re = sonaLead (e, { { 0, 0x90, 64, 100 }, { 4800, 0x80, 64, 0 } }, 48000);
            comprova (picSB (re.L, 30000, 48000) > 0.002f, "el delay i la reverb deixen cua després de la nota");
            comprova (e.fentSo() || picSB (re.L, 47000, 48000) < 1e-3f, "la cua es manté viva mentre sona");
            Lead c;
            c.posaEstat (presetsFabrica()[30].estat);
            c.nomPreset = presetsFabrica()[30].nom;
            c.assignaCC (22, FRes);
            Lead d;
            d.posaEstat (c.estat());
            comprova (d.estatSo() == c.estatSo() && d.nomPreset == c.nomPreset && d.ccDeParam (FRes) == 22, "estat, nom i CC es recuperen");
            comprova (d.numParametres() == numParams && d.indexParametre ("cut") == FTall, "automatitzable per clau");
            comprova (aleatori (3) != aleatori (4) && varia (presetsFabrica()[5].estat, 1, 0.5f) != presetsFabrica()[5].estat, "atzar i variació");
            float o[128];
            comprova (previsualitza (presetsFabrica()[0].estat, o, 128) > 0.05f, "previsualització");
            const auto t0 = std::chrono::steady_clock::now();
            Lead h;
            h.posaEstat (presetsFabrica()[61].estat);   // supersaw doble
            h.posa (Mode, 2.0f);
            h.prepara (48000.0, 256);
            sonaLead (h, { { 0, 0x90, 60, 100 }, { 0, 0x90, 64, 100 }, { 0, 0x90, 67, 100 } }, 48000);
            const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("    (1 s de 3 notes amb 2 supersaws: %.1f ms)\n", seg * 1000.0);
            comprova (seg < 0.5, "per sota del temps real");
        });
    }
}
