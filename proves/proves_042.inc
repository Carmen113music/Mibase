// 0.42: canvis de tempo que no mouen la música, graella de la taula de treball, biblioteca.
namespace
{
    void proves042()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        prova ("tempo: canviar-lo deixa els clips MIDI i els acords als mateixos compassos", []
        {
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            afegeixPista (q, "Acords", "acords");
            afegeixPista (q, "Veu", "audio");
            const int G = pistaPerId (q, "") < 0 ? 0 : 0;
            int iG = -1, iA = -1, iV = -1;
            for (int i = 0; i < (int) q.pistes.size(); ++i) { if (esAcords (q.pistes[(size_t) i])) iA = i; else if (esMidi (q.pistes[(size_t) i])) iG = i; else iV = i; }
            (void) G;
            Clip m; m.midi = true; m.pos = 4.0; m.inici = 2.0; m.fi = 10.0;       // del compàs 3 al 7 (a 120, 2 s per compàs)
            q.pistes[(size_t) iG].clips.push_back (m);
            Clip a; a.acord = "Am"; a.pos = 6.0; a.fi = 2.0;
            q.pistes[(size_t) iA].clips.push_back (a);
            Clip v; v.presa = "s1"; v.pos = 6.0; v.fi = 3.0;
            q.pistes[(size_t) iV].clips.push_back (v);
            auto compassos = [&] (const Clip& c) { return std::pair<double, double> (compasDeSegons (q, c.iniciVisible()), compasDeSegons (q, c.fiVisible())); };
            const auto m0 = compassos (q.pistes[(size_t) iG].clips[0]), a0 = compassos (q.pistes[(size_t) iA].clips[0]);
            midi::canviaTempo (q, 100.0);
            const auto m1 = compassos (q.pistes[(size_t) iG].clips[0]), a1 = compassos (q.pistes[(size_t) iA].clips[0]);
            comprova (std::abs (m0.first - m1.first) < 1e-6 && std::abs (m0.second - m1.second) < 1e-6, "el clip MIDI comença i acaba als mateixos compassos");
            comprova (std::abs (a0.first - a1.first) < 1e-6 && std::abs (a0.second - a1.second) < 1e-6, "l'acord també (abans es quedava on era)");
            comprova (q.pistes[(size_t) iV].clips[0].pos == 6.0 && q.pistes[(size_t) iV].clips[0].fi == 3.0, "l'àudio no es mou");
            comprova (std::abs (q.pistes[(size_t) iG].clips[0].fiVisible() - q.pistes[(size_t) iG].clips[0].iniciVisible() - 8.0 * 1.2) < 1e-6, "en segons, dura més (va més a poc a poc)");
            // Amb una part que té el seu tempo: canviar el tempo de la part tampoc mou la música.
            Part x; x.id = "p1"; x.nom = "Tornada"; x.de = 2.0; x.a = 4.0; x.tempo = 0.0;
            q.parts.push_back (x);
            const auto abans = compassos (q.pistes[(size_t) iG].clips[0]);
            midi::ambMusicaFixa (q, [] (Projecte& p) { p.parts[0].tempo = 150.0; });
            comprova (teCanvisTempo (q), "ara hi ha un canvi de tempo");
            const auto despres = compassos (q.pistes[(size_t) iG].clips[0]);
            comprova (std::abs (abans.first - despres.first) < 1e-6 && std::abs (abans.second - despres.second) < 1e-6, "canviar el tempo d'una part deixa el clip als mateixos compassos");
            midi::canviaTempo (q, 90.0);
            const auto final = compassos (q.pistes[(size_t) iG].clips[0]);
            comprova (std::abs (abans.first - final.first) < 1e-6 && std::abs (abans.second - final.second) < 1e-6, "i canviar el de la cançó, amb la part pel mig, també");
        });

        prova ("graella: la subdivisió triada per a la taula de treball", []
        {
            Projecte q;
            q.tempo = 120.0;                       // un temps = 0,5 s
            comprova (std::abs (enganxa (q, 0.30, Snap::Temps) - 0.5) < 1e-9, "per defecte, negres");
            q.graella = 240;                       // semicorxeres: 0,125 s
            comprova (std::abs (enganxa (q, 0.30, Snap::Temps) - 0.25) < 1e-9 && std::abs (enganxa (q, 0.33, Snap::Temps) - 0.375) < 1e-9, "semicorxeres");
            q.graella = 320;                       // tresets de corxera: 1/6 s
            comprova (std::abs (enganxa (q, 0.30, Snap::Temps) - 1.0 / 3.0) < 1e-9, "tresets");
            comprova (std::abs (enganxa (q, 0.9, Snap::Compas) - 2.0) < 1e-9 || std::abs (enganxa (q, 0.9, Snap::Compas)) < 1e-9, "el mode Compàs no canvia");
        });

        prova ("biblioteca: loops que acaben en silenci, one-shots llargs i fitxers que no s'obren", []
        {
            const double fs = 22050.0;
            auto cop = [&] (std::vector<float>& x, double t, double llarg)
            {
                const auto i0 = (size_t) (t * fs);
                for (size_t i = i0; i < x.size() && i < i0 + (size_t) (llarg * fs); ++i)
                    x[i] += 0.8f * (float) std::exp (-(double) (i - i0) / (0.04 * fs)) * (float) std::sin ((double) i * 0.3);
            };
            double bpm = 0.0;
            std::vector<float> bateria ((size_t) (2.0 * fs), 0.0f);          // 4 cops curts, cada 0,5 s, amb silenci entremig i al final
            for (int k = 0; k < 4; ++k) cop (bateria, 0.5 * k, 0.2);
            comprova (biblioteca::detectaForma (bateria.data(), (long long) bateria.size(), fs, bpm) == biblioteca::loop, "un loop de bateria que acaba en silenci és un loop");
            comprova (bpm > 0.0, "i se n'estima el tempo");
            std::vector<float> plat ((size_t) (3.0 * fs), 0.0f);             // un sol cop que s'apaga a poc a poc
            for (size_t i = 0; i < plat.size(); ++i) plat[i] = 0.8f * (float) std::exp (-(double) i / (0.6 * fs)) * (float) std::sin ((double) i * 0.9);
            comprova (biblioteca::detectaForma (plat.data(), (long long) plat.size(), fs, bpm) == biblioteca::oneShot, "un plat llarg és un one-shot");
            std::vector<float> curt ((size_t) (0.4 * fs), 0.5f);
            comprova (biblioteca::detectaForma (curt.data(), (long long) curt.size(), fs, bpm) == biblioteca::oneShot, "un so curt és un one-shot");

            biblioteca::Index idx;
            biblioteca::Sample a; a.fitxer = "bo.wav"; a.nom = "bo";
            biblioteca::Sample b; b.fitxer = "dolent.wav"; b.nom = "dolent"; b.noObre = true;
            idx.samples = { a, b };
            const auto v = biblioteca::filtra (idx, "", std::vector<std::string>(), std::vector<std::string>(), biblioteca::Ordre::Nom, false, false);
            comprova (v.size() == 1 && idx.samples[(size_t) v[0]].nom == "bo", "els que no s'han pogut obrir no surten a la llista");
        });
    }
}
