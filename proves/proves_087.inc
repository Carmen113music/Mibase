// 0.87: acords d'un àudio i notes de pista.
namespace
{
    // Un acord «sonant»: cada nota amb 4 harmònics, amb un baix a la fonamental una octava avall.
    float notaRica (double t, double hz)
    {
        double v = 0.0;
        for (int h = 1; h <= 4; ++h) v += std::sin (2.0 * 3.14159265358979 * hz * h * t) / (h * h);
        return (float) v;
    }

    void proves087()
    {
        using namespace mibase;
        prova ("0.87 Acords d'un àudio: C Am F G7 a 120 bpm", []
        {
            // 4 compassos (2 s cada un). Notes MIDI de cada acord.
            const std::vector<std::vector<int>> acords { { 60, 64, 67 }, { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62, 65 } };
            auto hz = [] (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); };
            AudioPresa a;
            a.fs = 48000.0; a.canals = 1; a.longitud = 8 * 48000;
            a.dades[0].resize ((size_t) a.longitud);
            for (long long i = 0; i < a.longitud; ++i)
            {
                const double t = (double) i / a.fs;
                const auto& ac = acords[(size_t) std::min (3, (int) (t / 2.0))];
                double v = notaRica (t, hz (ac[0] - 12)) * 0.6;
                for (int m : ac) v += notaRica (t, hz (m)) * 0.3;
                // una mica de «bateria»: soroll curt a cada temps
                const double d = std::fmod (t, 0.5);
                if (d < 0.03) v += 0.3 * std::sin (t * 91234.0) * std::exp (-d * 120.0);
                a.dades[0][(size_t) i] = (float) (v * 0.4);
            }
            Projecte p;
            p.tempo = 120.0;
            Clip c; c.presa = "s1"; c.pos = 2.0; c.inici = 0.0; c.fi = 8.0;   // comença al compàs 2
            const auto r = acordsAudio::detecta (p, c, a);
            std::string tots;
            for (auto& x : r) tots += x.nom + "(" + std::to_string (x.ini).substr (0, 4) + ") ";
            comprova (r.size() == 4, "quatre acords: " + tots);
            if (r.size() == 4)
            {
                comprova (r[0].nom == "C" && r[1].nom == "Am" && r[2].nom == "F" && r[3].nom == "G7", "els noms: " + tots);
                comprova (std::abs (r[0].ini - 2.0) < 1e-6 && std::abs (r[1].ini - 4.0) < 1e-6 && std::abs (r[3].fi - 10.0) < 1e-6, "un per compàs, al seu lloc");
                bool segurs = true;
                for (auto& x : r) segurs = segurs && ! x.dubtos;
                comprova (segurs, "cap dubtós (so net)");
            }
            // El silenci no fa acords; amb Elastic Audio es mira on sona (aquí: el clip retallat només 2 compassos)
            Clip d = c; d.fi = 4.0;
            comprova (acordsAudio::detecta (p, d, a).size() == 2, "només la part que es veu");
            AudioPresa buit = a;
            std::fill (buit.dades[0].begin(), buit.dades[0].end(), 0.0f);
            comprova (acordsAudio::detecta (p, c, buit).empty(), "silenci: cap acord");
        });

        prova ("0.87 Acords posats a la pista d'Acords (dubtosos) i notes de pista: es desen", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Guitarra", "audio");
            p.pistes[0].notes = "Repetir la tornada\namb la Les Paul";
            const int i = edicio::posaAcord (p, 0.0, 2.0, "Am");
            const int pa = edicio::pistaAcords (p);
            p.pistes[(size_t) pa].clips[(size_t) i].dubtos = true;
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_087";
            fs::remove_all (dir);
            fs::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", p, "2026-11-23T11:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova (l.has_value() && l->pistes[0].notes == p.pistes[0].notes, "les notes de la pista tornen igual");
            comprova (l.has_value() && l->pistes[(size_t) edicio::pistaAcords (*l)].clips[0].dubtos, "l'acord dubtós continua marcat");
            fs::remove_all (dir);
        });
    }
}
