// 0.86: Elastic Audio.
namespace
{
    // Un àudio de prova: «durada» segons a 48 kHz, mono.
    mibase::AudioPresa audioDeProva (double durada, const std::function<float (double)>& so)
    {
        mibase::AudioPresa a;
        a.fs = 48000.0;
        a.canals = 1;
        a.longitud = (long long) std::llround (durada * a.fs);
        a.dades[0].resize ((size_t) a.longitud);
        for (long long i = 0; i < a.longitud; ++i) a.dades[0][(size_t) i] = so ((double) i / a.fs);
        return a;
    }

    // Freqüència (Hz) per passos per zero en un tros.
    double frequencia (const mibase::AudioPresa& a, double de, double fins)
    {
        const long long i0 = (long long) (de * a.fs), i1 = std::min (a.longitud, (long long) (fins * a.fs));
        int creus = 0;
        long long primer = -1, darrer = -1;
        for (long long i = i0 + 1; i < i1; ++i)
            if (a.dades[0][(size_t) i - 1] < 0.0f && a.dades[0][(size_t) i] >= 0.0f)
            {
                if (primer < 0) primer = i;
                darrer = i;
                ++creus;
            }
        return creus > 1 ? (double) (creus - 1) * a.fs / (double) (darrer - primer) : 0.0;
    }

    // Un cop sec: on el senyal passa per primer cop de «llindar» a partir de «de».
    double primerCop (const mibase::AudioPresa& a, double de, float llindar = 0.5f)
    {
        for (long long i = (long long) (de * a.fs); i < a.longitud; ++i)
            if (std::abs (a.dades[0][(size_t) i]) >= llindar) return (double) i / a.fs;
        return -1.0;
    }

    float picEntre (const mibase::AudioPresa& a, double de, double fins)
    {
        float m = 0.0f;
        for (long long i = (long long) (de * a.fs); i < std::min (a.longitud, (long long) (fins * a.fs)); ++i)
            m = std::max (m, std::abs (a.dades[0][(size_t) i]));
        return m;
    }

    // Bateria de prova: un cop (soroll que s'apaga en 60 ms) cada «pas» segons, i un to suau a sota.
    float cops (double t, double pas)
    {
        const double d = std::fmod (t, pas);
        const double env = d < 0.06 ? std::exp (-d * 60.0) : 0.0;
        const double soroll = std::sin (t * 7919.0 * 6.2831853) * std::sin (t * 104729.0);
        return (float) (0.9 * env * soroll + 0.05 * std::sin (2.0 * 3.14159265 * 110.0 * t));
    }

    void proves086()
    {
        using namespace mibase;
        namespace el = mibase::elastic;

        prova ("0.86 Elastic: el mapa de temps (fitxer <-> clip)", []
        {
            Clip c;
            c.presa = "s1"; c.fi = 4.0;
            comprova (! el::actiu (c) && el::aClip (c, 1.5) == 1.5 && el::aFont (c, 2.0) == 2.0, "sense punts, l'original");
            c.warp = { { 0.0, 0.0 }, { 2.0, 1.0 }, { 4.0, 5.0 } };
            comprova (std::abs (el::aClip (c, 1.0) - 0.5) < 1e-9 && std::abs (el::aClip (c, 3.0) - 3.0) < 1e-9, "entre punts, en línia");
            for (double f = 0.0; f <= 4.0; f += 0.37)
                comprova (std::abs (el::aFont (c, el::aClip (c, f)) - f) < 1e-9, "anar i tornar");
            comprova (std::abs (el::aClip (c, 5.0) - 6.0) < 1e-9, "després de l'últim punt, a velocitat normal");
            comprova (std::abs (el::duradaClip (c, 4.0) - 5.0) < 1e-9 && std::abs (el::ratioTros (c.warp, 0) - 0.5) < 1e-9, "durada i ritme");
        });

        prova ("0.86 Elastic mode A: estirar des de les vores", []
        {
            Clip c;
            c.presa = "s1"; c.pos = 10.0; c.inici = 0.0; c.fi = 4.0;
            const double k = el::estiraVora (c, 4.0, true, 18.0);   // la vora dreta, de 14 s a 18 s
            comprova (std::abs (k - 2.0) < 1e-9 && std::abs (c.iniciVisible() - 10.0) < 1e-9 && std::abs (c.fiVisible() - 18.0) < 1e-9,
                      "la dreta s'allarga i l'esquerra no es mou");
            comprova (std::abs (el::duradaClip (c, 4.0) - 8.0) < 1e-9, "tot el fitxer al doble");
            el::estiraVora (c, 4.0, true, 100.0);
            comprova (std::abs (c.fiVisible() - 26.0) < 1e-9, "com a molt 4 vegades més lent");
            el::estiraVora (c, 4.0, false, 22.0);   // ara la vora esquerra: el final (26 s) no es mou
            comprova (std::abs (c.fiVisible() - 26.0) < 1e-9 && std::abs (c.iniciVisible() - 22.0) < 1e-9, "la vora esquerra");
            comprova (std::abs (c.warp.front().desti) < 1e-9, "el començament del fitxer sempre és el 0 del clip");
            el::restaura (c);
            comprova (! el::actiu (c) && std::abs (c.iniciVisible() - 22.0) < 1e-9 && std::abs (c.longitud() - 4.0) < 1e-9,
                      "restaurar: l'original, on començava");
            // Retallat abans: estira només el que es veu i l'amagat el segueix
            Clip d; d.presa = "s1"; d.pos = 0.0; d.inici = 1.0; d.fi = 3.0;
            el::estiraVora (d, 4.0, true, 5.0);   // 2 s -> 4 s
            comprova (std::abs (d.iniciVisible() - 1.0) < 1e-9 && std::abs (d.fiVisible() - 5.0) < 1e-9
                      && std::abs (el::aFont (d, d.inici) - 1.0) < 1e-9 && std::abs (el::aFont (d, d.fi) - 3.0) < 1e-9,
                      "es veu el mateix tros, el doble de llarg");
        });

        prova ("0.86 Elastic mode B: el marcador del mig reparteix el temps", []
        {
            Clip c;
            c.presa = "s1"; c.pos = 2.0; c.inici = 0.0; c.fi = 4.0;
            const int i = el::afegeixMarcador (c, 4.0, 4.0);   // al mig (2 s dins el clip)
            comprova (i > 0 && el::marcadors (c).size() == 1, "un marcador al mig");
            comprova (el::aClip (c, 2.0) == 2.0, "posar-lo no canvia res");
            el::mouMarcador (c, i, 3.0);   // cap a l'esquerra: 1 s
            comprova (std::abs (c.longitud() - 4.0) < 1e-9, "la durada total no canvia");
            comprova (std::abs (el::ratioTros (c.warp, (size_t) i - 1) - 0.5) < 1e-9 && std::abs (el::ratioTros (c.warp, (size_t) i) - 1.5) < 1e-9,
                      "A s'accelera (0,5) i B es ralenteix (1,5)");
            el::mouMarcador (c, i, 2.001);
            comprova (c.pos + c.warp[(size_t) i].desti >= 2.0 + 0.5 - 1e-9, "no arriba mai a la vora (com a molt 4 vegades més ràpid)");
            el::mouMarcador (c, i, 4.0);
            comprova (std::abs (el::ratioTros (c.warp, (size_t) i - 1) - 1.0) < 1e-9, "al lloc original, el ritme original");
            comprova (el::treuMarcador (c, i) && ! el::actiu (c), "treure'l: torna a l'original");
            // amb el clip retallat, les vores queden fixes
            Clip d; d.presa = "s1"; d.pos = 0.0; d.inici = 1.0; d.fi = 3.0;
            const int j = el::afegeixMarcador (d, 4.0, 2.0);
            el::mouMarcador (d, j, 1.5);
            comprova (std::abs (el::aFont (d, 1.0) - 1.0) < 1e-9 && std::abs (el::aFont (d, 3.0) - 3.0) < 1e-9, "vores fixes (marcadors 1 i 3)");
            comprova (std::abs (el::aClip (d, 0.5) - 0.5) < 1e-9, "el que hi ha amagat fora la finestra no es toca");
        });

        prova ("0.86 Elastic mode C: marcadors als cops i quantitzar", []
        {
            // 120 bpm: un temps = 0,5 s. Cops lleugerament tard (no quadren amb la graella).
            const auto a = audioDeProva (4.0, [] (double t) { return cops (t + 0.5 - 0.03, 0.5); });
            const auto cs = el::cops (a, 0.5);
            comprova (cs.size() >= 7 && cs.size() <= 8, "troba els cops (" + std::to_string (cs.size()) + ")");
            bool bons = true;
            for (double t : cs)
            {
                const double fase = std::fmod (t - 0.03 + 5.0, 0.5);
                bons = bons && (fase < 0.004 || fase > 0.496);
            }
            comprova (bons, "cada cop on toca (±4 ms)");
            Projecte p;
            p.tempo = 120.0;
            p.graella = ticksPerNegra;   // graella de negres
            Clip c; c.presa = "s1"; c.fi = 4.0;
            const int n = el::marcadorsAlsCops (c, 4.0, cs);
            comprova (n >= 6, "un marcador a cada cop de dins");
            comprova (el::quantitza (c, p) >= 6, "quantitzats");
            bool aGraella = true;
            for (int i : el::marcadors (c))
            {
                const double t = c.pos + c.warp[(size_t) i].desti;
                if (t > 0.25) aGraella = aGraella && std::abs (t / 0.5 - std::round (t / 0.5)) < 1e-6;   // el primer no pot anar al 0
            }
            comprova (aGraella, "tots a la graella");
            comprova (std::abs (c.longitud() - 4.0) < 1e-9, "el clip dura el mateix");
            Clip d = c;
            d.pos = 7.0; d.inici = 0.2;
            comprova (el::clau (c, false) == el::clau (d, false) && el::clau (c, false) != el::clau (c, true), "la clau no canvia en moure o retallar");
            d.warp[2].desti += 0.01;
            comprova (el::clau (c, false) != el::clau (d, false), "però sí si canvien els punts");
        });

        prova ("0.86 Elastic: el so (to i durada)", []
        {
            const auto a = audioDeProva (2.0, [] (double t) { return (float) (0.5 * std::sin (2.0 * 3.14159265358979 * 440.0 * t)); });
            for (int mode : { 0, 2 })
                for (double k : { 1.5, 0.7 })
                {
                    const std::vector<PuntWarp> w { { 0.0, 0.0 }, { 2.0, 2.0 * k } };
                    const auto r = el::renderitza (a, w, mode);
                    const std::string nom = std::string (el::nomMode (mode)) + " x" + std::to_string (k);
                    comprova (r != nullptr && std::abs (r->durada() - 2.0 * k) < 0.001, nom + ": durada");
                    const double hz = frequencia (*r, 0.2, 2.0 * k - 0.2);
                    comprova (std::abs (hz - 440.0) < 3.0, nom + ": el to no canvia (" + std::to_string (hz) + " Hz)");
                    comprova (picEntre (*r, 0.3, 0.4) > 0.4 && picEntre (*r, 0.3, 0.4) < 0.6, nom + ": el volum no canvia");
                }
            const auto r = el::renderitza (a, {}, 0);
            comprova (r->longitud == a.longitud && std::abs (r->dades[0][24000] - a.dades[0][24000]) < 1e-5, "sense punts, idèntic");
        });

        prova ("0.86 Elastic: els cops cauen on diu el marcador i amb l'atac intacte", []
        {
            const auto a = audioDeProva (4.0, [] (double t) { return cops (t, 0.5); });
            // Mode A: tot x1,25: el cop de 1,0 s ha de sonar a 1,25 s.
            const auto r = el::renderitza (a, { { 0.0, 0.0 }, { 4.0, 5.0 } }, el::Ritme);
            const double t1 = primerCop (*r, 1.15);
            comprova (std::abs (t1 - 1.25) < 0.003, "cop a 1,25 s (" + std::to_string (t1) + ")");
            comprova (picEntre (*r, 1.2, 1.3) > 0.95f * picEntre (a, 0.95, 1.05), "l'atac no s'esmorteeix");
            const double t2 = primerCop (*r, 3.6);
            comprova (std::abs (t2 - 3.75) < 0.003, "cop a 3,75 s (" + std::to_string (t2) + ")");
            // Mode B: el marcador del mig de 2,0 s a 1,8 s: el cop de 2 s a 1,8, els d'abans més junts, els de després més separats.
            const auto b = el::renderitza (a, { { 0.0, 0.0 }, { 2.0, 1.8 }, { 4.0, 4.0 } }, el::General);
            comprova (std::abs (b->durada() - 4.0) < 0.001, "la durada total no canvia");
            comprova (std::abs (primerCop (*b, 1.7) - 1.8) < 0.003 && std::abs (primerCop (*b, 0.85) - 0.9) < 0.003, "cops a 0,9 i 1,8 s");
            const double t3 = primerCop (*b, 2.85);
            comprova (std::abs (t3 - (1.8 + 1.0 * 1.1)) < 0.003, "després del marcador, 1,1 vegades més lent (" + std::to_string (t3) + ")");
            // Estèreo: els dos canals igual de desplaçats
            auto s = a;
            s.canals = 2;
            s.dades[1] = s.dades[0];
            const auto e = el::renderitza (s, { { 0.0, 0.0 }, { 4.0, 3.0 } }, el::General);
            float dif = 0.0f;
            for (long long i = 0; i < e->longitud; ++i) dif = std::max (dif, std::abs (e->dades[0][(size_t) i] - e->dades[1][(size_t) i]));
            comprova (e->canals == 2 && dif < 1e-6, "estèreo: L i R iguals (no es desfà la imatge)");
        });

        prova ("0.86 Elastic: el pla fa sonar el so estirat; es desa i s'obre", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Bateria", "audio");
            const auto id = edicio::afegeixPresa (p, "Àudios/b.wav", "b", 4.0).id;
            edicio::afegeixClip (p, 0, id, 1.0);
            auto& c = p.pistes[0].clips[0];
            el::estiraVora (c, 4.0, true, 1.0 + 6.0);
            c.elastic = el::Ritme;
            auto orig = std::make_shared<AudioPresa> (audioDeProva (4.0, [] (double t) { return cops (t, 0.5); }));
            MapaAudios audios { { id, orig } };
            MapaAudios estirats { { el::clau (c, false), el::renderitza (*orig, c.warp, c.elastic) } };
            auto pla = construeixPla (p, audios, {}, {}, &estirats);
            comprova (pla->pistes[0].clips.size() == 1 && pla->pistes[0].clips[0].audio == estirats.begin()->second.get(), "sona l'estirat");
            auto pla2 = construeixPla (p, audios, {}, {}, nullptr);
            comprova (pla2->pistes[0].clips[0].audio == orig.get(), "si encara no està fet, l'original");
            // bandera: s'estira amb el clip
            p.preses[0].primer = 2.0;
            comprova (std::abs (edicio::puntSnap (p, c) - (1.0 + 3.0)) < 1e-9, "la bandera del primer temps segueix l'estirament");
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_elastic086";
            fs::remove_all (dir);
            fs::create_directories (dir);
            const int m = el::afegeixMarcador (c, 4.0, 3.0);
            el::mouMarcador (c, m, 2.7);
            comprova (disc::desa (dir / "p.mibase", p, "2026-11-23T10:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova (l.has_value() && l->pistes[0].clips[0].warp == c.warp && l->pistes[0].clips[0].elastic == el::Ritme, "els punts i l'algorisme tornen igual");
            fs::remove_all (dir);
        });
    }
}
