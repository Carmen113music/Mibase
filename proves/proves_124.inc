// 1.24: Mix Assistant (fase 2): conflictes entre pistes, equilibri, la mescla sencera; soroll i gravació fluixa; preses òrfenes.
namespace
{
    mx::PistaMescla pistaMescla (const std::string& id, const std::string& nom, mx::Instrument i, const mx::AnalisiPista& a,
                                 double fader = 0.0, double pan = 0.0)
    {
        mx::PistaMescla p;
        p.id = id;
        p.nom = nom;
        p.ident = idDe (i);
        p.analisi = &a;
        p.faderDb = fader;
        p.pan = pan;
        return p;
    }
    std::vector<float> escala (std::vector<float> s, float g) { for (auto& v : s) v *= g; return s; }
    const mx::Incidencia* ambId (const std::vector<mx::Incidencia>& v, const std::string& prefix)
    {
        for (const auto& i : v) if (i.id.rfind (prefix, 0) == 0) return &i;
        return nullptr;
    }

    void proves124()
    {
        using namespace mibase;
        const auto fnVeu = [] (double f) { return rosa (f) * (f > 1000 && f < 5000 ? 3.0 : 1.0) * (f < 150 ? 0.1 : 1.0) * (f > 6000 ? 0.3 : 1.0); };

        prova ("1.24 Mix Assistant: una guitarra que tapa la veu (i una que no)", [&]
        {
            const auto veu = sorollAmb (fnVeu, 21);
            const auto gtr = sorollAmb ([] (double f) { return rosa (f) * (f > 1000 && f < 5000 ? 3.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); }, 22);
            const auto fosca = sorollAmb ([] (double f) { return rosa (f) * (f > 800 ? 0.01 : 1.0); }, 23);
            const auto aV = mx::analitzaPista (veu.data(), veu.data(), veu.size(), 48000.0);
            const auto aG = mx::analitzaPista (gtr.data(), gtr.data(), gtr.size(), 48000.0);
            const auto aF = mx::analitzaPista (fosca.data(), fosca.data(), fosca.size(), 48000.0);
            const std::vector<mx::PistaMescla> ps { pistaMescla ("t1", "Veu", mx::Instrument::Veu, aV),
                                                    pistaMescla ("t2", "Gtr", mx::Instrument::GuitarraElectrica, aG),
                                                    pistaMescla ("t3", "Rhodes", mx::Instrument::Teclats, aF) };
            const auto c = mx::conflictes (ps);
            const auto* i = ambId (c, "conflicte:t1:t2");
            comprova (i != nullptr, "la guitarra tapa la veu");
            comprova (i != nullptr && i->pista == "t2" && i->pistes.size() == 2, "la correcció va a la guitarra");
            comprova (i != nullptr && i->proposta.tipus == mx::Proposta::Campana && i->proposta.guany < 0.0f
                      && i->proposta.freq >= 1000.0f && i->proposta.freq <= 5000.0f, "proposa retallar la guitarra on viu la veu");
            comprova (i != nullptr && i->altres.size() >= 3, "i altres solucions (EQ dinàmica, sidechain, panorama, volum)");
            comprova (ambId (c, "conflicte:t1:t3") == nullptr, "uns teclats foscos no tapen la veu (comparteixen poc)");
        });

        prova ("1.24 Mix Assistant: si no sonen alhora, no hi ha conflicte", [&]
        {
            auto veu = sorollAmb (fnVeu, 24);
            auto gtr = sorollAmb ([] (double f) { return rosa (f) * (f > 1000 && f < 5000 ? 3.0 : 1.0); }, 25);
            for (size_t k = 0; k < veu.size() / 2; ++k) veu[k] = 0.0f;   // la veu, només la segona meitat
            for (size_t k = gtr.size() / 2; k < gtr.size(); ++k) gtr[k] = 0.0f;   // la guitarra, només la primera
            const auto aV = mx::analitzaPista (veu.data(), veu.data(), veu.size(), 48000.0);
            const auto aG = mx::analitzaPista (gtr.data(), gtr.data(), gtr.size(), 48000.0);
            const auto c = mx::conflictes ({ pistaMescla ("t1", "Veu", mx::Instrument::Veu, aV), pistaMescla ("t2", "Gtr", mx::Instrument::GuitarraElectrica, aG) });
            comprova (c.empty(), "compartir freqüències en moments diferents no és emmascarament");
        });

        prova ("1.24 Mix Assistant: el baix i el bombo es confonen", []
        {
            std::vector<float> k ((size_t) (1 << 19), 0.0f), b (k.size());
            for (size_t c = 0; c + 9000 < k.size(); c += 12000)
                for (int i = 0; i < 9000; ++i) k[c + (size_t) i] = 0.8f * std::exp (-i / 1500.0f) * (float) std::sin (2.0 * 3.14159265 * 55.0 * i / 48000.0);
            for (size_t i = 0; i < b.size(); ++i)
                b[i] = 0.5f * (float) std::sin (2.0 * 3.14159265 * 55.0 * (double) i / 48000.0) + 0.1f * (float) std::sin (2.0 * 3.14159265 * 110.0 * (double) i / 48000.0);
            const auto aK = mx::analitzaPista (k.data(), k.data(), k.size(), 48000.0);
            const auto aB = mx::analitzaPista (b.data(), b.data(), b.size(), 48000.0);
            const auto c = mx::conflictes ({ pistaMescla ("t1", "Kick", mx::Instrument::Bombo, aK), pistaMescla ("t2", "Baix", mx::Instrument::Baix, aB) });
            const auto* i = ambId (c, "conflicte:t1:t2");
            comprova (i != nullptr && i->titol.find ("bombo") != std::string::npos, "ho veu");
            comprova (i != nullptr && i->pista == "t2" && i->proposta.freq < 130.0f, "proposa un forat al baix als greus");
        });

        prova ("1.24 Mix Assistant: Balance Assistant", [&]
        {
            const auto veu = escala (sorollAmb (fnVeu, 26), 0.15f);   // −16 dB
            const auto gtr = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 27);
            const auto gtr2 = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 28);
            const auto aV = mx::analitzaPista (veu.data(), veu.data(), veu.size(), 48000.0);
            const auto aG = mx::analitzaPista (gtr.data(), gtr.data(), gtr.size(), 48000.0);
            const auto aG2 = mx::analitzaPista (gtr2.data(), gtr2.data(), gtr2.size(), 48000.0);
            std::vector<mx::PistaMescla> ps { pistaMescla ("t1", "Veu", mx::Instrument::Veu, aV, -2.0, 0.6),
                                              pistaMescla ("t2", "Gtr L", mx::Instrument::GuitarraElectrica, aG),
                                              pistaMescla ("t3", "Gtr R", mx::Instrument::GuitarraElectrica, aG2) };
            std::vector<mx::CanviBalanc> prop;
            const auto b = mx::balanc (ps, prop);
            const auto* i = ambId (b, "balanc:t1:veu-enrere");
            comprova (i != nullptr && i->proposta.tipus == mx::Proposta::Fader && i->proposta.despres > i->proposta.abans, "la veu queda enrere: puja-la");
            comprova (i != nullptr && i->prioritat != mx::Prioritat::Alta, "és una proposta, mai obligatòria");
            const auto veuProp = std::find_if (prop.begin(), prop.end(), [] (const mx::CanviBalanc& c) { return c.pista == "t1"; });
            comprova (veuProp != prop.end() && veuProp->fader > -2.0 && std::abs (veuProp->pan) < 0.01, "al Balanç proposat: més fader i al mig");
            int obertes = 0;
            for (const auto& c : prop) if ((c.pista == "t2" || c.pista == "t3") && std::abs (c.pan) > 0.5) ++obertes;
            comprova (obertes == 2, "les dues guitarres del mig, una a cada costat");
            ps[0].autoVolum = true;
            const auto b2 = mx::balanc (ps, prop);
            const auto* j = ambId (b2, "balanc:t1:veu-enrere");
            comprova (j != nullptr && j->proposta.tipus == mx::Proposta::Revisa, "amb automatització de volum no toca el fader");
            const auto vp = std::find_if (prop.begin(), prop.end(), [] (const mx::CanviBalanc& c) { return c.pista == "t1"; });
            comprova (vp == prop.end() || ! vp->tocaFader(), "i el Balanç proposat tampoc");
        });

        prova ("1.24 Mix Assistant: la mescla sencera (guitarres que s'acumulen i quan entra tothom)", []
        {
            const auto fnFang = [] (double f) { return rosa (f) * (f > 140 && f < 520 ? 8.0 : 1.0) * (f > 6000 ? 0.3 : 1.0); };
            auto g1 = sorollAmb (fnFang, 31), g2 = sorollAmb (fnFang, 32), g3 = sorollAmb (fnFang, 33);
            const auto veu = sorollAmb ([] (double f) { return rosa (f) * (f > 1000 && f < 5000 ? 3.0 : 1.0) * (f < 150 ? 0.1 : 1.0); }, 34);
            for (auto* g : { &g2, &g3 }) for (size_t k = 0; k < g->size() / 2; ++k) (*g)[k] = 0.0f;   // dues guitarres, només a la segona meitat
            std::vector<float> m (veu.size());
            g1 = escala (g1, 0.3f);   // a l'estrofa, una guitarra suau; a la tornada n'entren dues més, fortes
            for (size_t k = 0; k < m.size(); ++k) m[k] = 0.15f * veu[k] + 0.4f * (g1[k] + g2[k] + g3[k]);
            const auto aV = mx::analitzaPista (veu.data(), veu.data(), veu.size(), 48000.0);
            const auto a1 = mx::analitzaPista (g1.data(), g1.data(), g1.size(), 48000.0);
            const auto a2 = mx::analitzaPista (g2.data(), g2.data(), g2.size(), 48000.0);
            const auto a3 = mx::analitzaPista (g3.data(), g3.data(), g3.size(), 48000.0);
            const auto aM = mx::analitzaPista (m.data(), m.data(), m.size(), 48000.0);
            const std::vector<mx::PistaMescla> ps { pistaMescla ("t1", "Veu", mx::Instrument::Veu, aV), pistaMescla ("t2", "Gtr 1", mx::Instrument::GuitarraElectrica, a1),
                                                    pistaMescla ("t3", "Gtr 2", mx::Instrument::GuitarraElectrica, a2), pistaMescla ("t4", "Gtr 3", mx::Instrument::GuitarraAcustica, a3) };
            const std::vector<mx::Tram> trams { { "Estrofa", 0.0, 5.4 }, { "Tornada", 5.5, 11.0 } };
            const auto d = mx::diagnosticaMescla (aM, ps, trams);
            const auto* f = ambId (d, "mescla:sobra:mitjos baixos");
            comprova (f != nullptr && f->titol.find ("guitarres") != std::string::npos && f->pistes.size() == 3, "les guitarres acumulen mitjos baixos");
            const auto* dn = ambId (d, "mescla:densitat");
            comprova (dn != nullptr, "quan entra tothom, es perd claredat");
            comprova (dn != nullptr && dn->on.find ("Tornada") != std::string::npos, "i diu on (a la tornada)");
            comprova (ambId (d, "mescla:mono") == nullptr, "una mescla mono no té problemes de fase");
        });

        prova ("1.24 Mix Assistant: soroll de fons i gravació fluixa", []
        {
            auto s = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 35);
            std::mt19937 rng (36);
            std::normal_distribution<float> nd (0.0f, 1.0f);
            for (size_t k = 0; k < s.size(); ++k)
                if ((k / 48000) % 3 == 2) s[k] = 0.0015f * nd (rng);   // cada tres segons, un d'aire amb soroll (≈ −56 dBFS)
            const auto a = mx::analitzaPista (s.data(), s.data(), s.size(), 48000.0);
            comprova (a.sorollDb > -65.0f && a.sorollDb < -50.0f, "mesura el soroll entre frases");
            const auto d = mx::diagnosticaPista ("t1", "Gtr", a, idDe (mx::Instrument::GuitarraElectrica), {}, true, 0.0f);
            const auto* i = troba (d, "soroll");
            comprova (i != nullptr && i->origen, "soroll de fons (cosa de la gravació)");
            const auto net = sorollAmb ([] (double f) { return rosa (f) * (f > 6000 ? 0.3 : 1.0); }, 37);
            const auto b = mx::analitzaPista (net.data(), net.data(), net.size(), 48000.0);
            comprova (! te (mx::diagnosticaPista ("t1", "Gtr", b, idDe (mx::Instrument::GuitarraElectrica)), "soroll"), "sense silencis, no n'hi ha");
            // el pic és a −8 dBFS; amb el fader a +20 dB, la gravació era a −28
            const auto e = mx::diagnosticaPista ("t1", "Gtr", b, idDe (mx::Instrument::GuitarraElectrica), {}, true, 20.0f);
            comprova (te (e, "fluixa") && troba (e, "fluixa")->origen, "gravació fluixa");
            comprova (! te (mx::diagnosticaPista ("t1", "Gtr", b, idDe (mx::Instrument::GuitarraElectrica), {}, true, NAN), "fluixa"),
                      "amb automatització de volum no es pot saber");
        });

        prova ("1.24 Preses: si la pista es queda sense clips, fora les preses", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Guitarra", "audio");
            const auto id = edicio::afegeixPresa (p, "Àudios/a.wav", "a", 10.0).id;
            edicio::afegeixClip (p, 0, id, 0.0);
            p.pistes[0].takes = { p.pistes[0].clips[0], p.pistes[0].clips[0] };
            comprova (edicio::treuTakesSenseComp (p) == 0 && p.pistes[0].takes.size() == 2, "amb clips, les preses es queden");
            p.pistes[0].clips.clear();
            comprova (edicio::treuTakesSenseComp (p) == 2 && p.pistes[0].takes.empty(), "sense clips, se'n van");
            p.pistes[0].takes = { Clip {}, Clip {} };
            comprova (edicio::treuTotesLesTakes (p, 0) == 2 && p.pistes[0].takes.empty(), "«Esborra totes les preses»");
            comprova (p.preses.size() == 1, "els àudios no es toquen");
        });
    }
}
