// 0.36: pista d'Acords.
namespace
{
    void proves036()
    {
        using namespace mibase::edicio;
        prova ("pista d'Acords: una, a dalt, blocs que es trepitgen bé", []
        {
            Projecte q;
            afegeixPista (q, "A", "audio");
            afegeixBus (q, "GRUP", false);
            comprova (pistaAcords (q) < 0, "al principi no n'hi ha");
            comprova (posaAcord (q, 0.0, 4.0, "G") >= 0, "posa G (crea la pista)");
            comprova (pistaAcords (q) == 0 && q.pistes.size() == 3, "la pista d'Acords va a dalt");
            comprova (creaPistaAcords (q) == 0 && q.pistes.size() == 3, "no se'n fa una altra");
            afegeixPista (q, "B", "midi");
            comprova (esAcords (q.pistes[0]) && q.pistes[2].nom == "B" && esBus (q.pistes[3]), "les pistes noves, entre els acords i els busos");
            posaAcord (q, 4.0, 8.0, "D");
            posaAcord (q, 8.0, 12.0, "Am");
            posaAcord (q, 12.0, 16.0, "G");
            comprova (acordA (q, 0.5) == "G" && acordA (q, 5.0) == "D" && acordA (q, 11.9) == "Am" && acordA (q, 16.5).empty(), "quin acord sona a cada moment");
            // un acord al mig d'un altre el parteix
            posaAcord (q, 9.0, 10.0, "C/E");
            const auto& cl = q.pistes[0].clips;
            comprova (cl.size() == 6 && acordA (q, 8.5) == "Am" && acordA (q, 9.5) == "C/E" && acordA (q, 10.5) == "Am", "parteix l'Am");
            // un acord que en tapa dos
            posaAcord (q, 3.0, 13.0, "Em");
            comprova (acordA (q, 2.0) == "G" && acordA (q, 5.0) == "Em" && acordA (q, 12.9) == "Em" && acordA (q, 13.5) == "G", "en tapa uns quants");
            comprova (posaAcord (q, 20.0, 24.0, "xyz") < 0 && acordA (q, 21.0).empty(), "un nom que no és un acord, no");
            // no es mou de dalt; ningú hi passa per sobre
            mouPista (q, 0, 2);
            comprova (esAcords (q.pistes[0]), "la pista d'Acords no es mou");
            mouPista (q, 2, 0);
            comprova (esAcords (q.pistes[0]) && q.pistes[1].nom == "B", "ningú hi passa per sobre");
            // els blocs només van a la pista d'Acords
            comprova (midi::compatible (q.pistes[0], cl[0]) && ! midi::compatible (q.pistes[1], cl[0]), "un acord no va a una pista normal");
            Clip audio;
            comprova (! midi::compatible (q.pistes[0], audio), "un clip normal no va a la pista d'Acords");
            // es desa i es torna a llegir
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_acords036";
            fs::remove_all (dir);
            fs::create_directories (dir);
            q.pistes[1].segueixAcords = true;   // 0.37: «segueix la pista d'Acords» es desa
            const auto idSegueix = q.pistes[1].id;
            std::rotate (q.pistes.begin(), q.pistes.begin() + 1, q.pistes.end());   // (barrejat a propòsit)
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-13T09:00:00", "2026-11-13 090000", false).correcte, "desa");
            const auto llegit = disc::llegeix (dir / "p.mibase");
            comprova (llegit.has_value() && esAcords (llegit->pistes[0]) && acordA (*llegit, 9.5) == "Em" && acordA (*llegit, 14.0) == "G",
                      "en obrir-lo, els acords hi són i la pista torna a dalt");
            comprova (llegit.has_value() && llegit->pistes[(size_t) pistaPerId (*llegit, idSegueix)].segueixAcords
                      && ! llegit->pistes[(size_t) pistaPerId (*llegit, q.pistes[3].id)].segueixAcords, "«segueix la pista d'Acords» es desa (0.37)");
            fs::remove_all (dir);
        });

        prova ("pista d'Acords: un acord nou va del temps on cau al final del compàs (0.37)", []
        {
            Projecte q;   // 120 bpm, 4/4: un compàs = 2 s, un temps = 0,5 s
            auto tros = trosAcordNou (q, 2.6);   // compàs 2, temps 2
            comprova (std::abs (tros.first - 2.5) < 1e-9 && std::abs (tros.second - 4.0) < 1e-9, "del temps 2 al final del compàs");
            posaAcord (q, 2.0, 4.0, "G");
            posaAcord (q, 3.0, 4.0, "D");   // dos acords al mateix compàs
            comprova (acordA (q, 2.4) == "G" && acordA (q, 3.2) == "D", "dos acords dins un compàs");
            tros = trosAcordNou (q, 2.2);
            comprova (std::abs (tros.first - 2.0) < 1e-9 && std::abs (tros.second - 3.0) < 1e-9, "s'atura on comença el D");
        });
        prova ("0.39: el kit de bateria és un instrument i les files del Drumpad són de la pista", []
        {
            namespace fs = std::filesystem;
            Projecte q;
            q.nom = "Eines";
            auto& a = afegeixPista (q, "Bat", "midi");
            a.instrument = mibase::instrumentKit; a.kit = "Intern"; a.files = mibase::bateria::filesInternes();
            auto& b = afegeixPista (q, "Piano", "midi");
            b.instrument = "intern:piano"; b.files = mibase::bateria::filesInternes();   // Drumpad obert sobre un piano
            auto& c = afegeixPista (q, "Sinte", "midi");
            c.instrument = "intern:piano";
            comprova (mibase::esKit (q.pistes[0]) && ! mibase::esKit (q.pistes[1]) && mibase::esMidi (q.pistes[0]), "esKit mira l'instrument, no el tipus");
            const auto arrel = fs::temp_directory_path() / "mibase_proves39";
            fs::remove_all (arrel);
            fs::create_directories (arrel);
            auto rc = mibase::disc::creaProjecte (arrel, q, "2026-11-13T12:00:00", "13/11/2026 12:00");
            auto l = mibase::disc::llegeix (rc.fitxer);
            comprova (l && l->pistes.size() == 3 && l->pistes[0].tipus == "midi" && mibase::esKit (l->pistes[0])
                      && l->pistes[0].files == q.pistes[0].files, "la pista amb kit es desa com a pista d'instrument");
            comprova (l && l->pistes[1].instrument == "intern:piano" && l->pistes[1].files == q.pistes[1].files, "una pista amb un altre instrument conserva les files del Drumpad");
            comprova (l && l->pistes[2].files.empty(), "sense Drumpad no hi ha files");
        });
    }
}
