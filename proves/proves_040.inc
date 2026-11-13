// 0.40: editor de rasgueigs (text dels passos), rasgueigs propis i blocs de patró dins dels clips.
namespace
{
    void proves040()
    {
        using namespace mibase;
        prova ("patrons: el text d'un rasgueig es llegeix i es torna a escriure igual", []
        {
            int malament = 0;
            std::string quin;
            for (auto& p : guitarra::patrons())
                if (guitarra::llegeixPassos (guitarra::textPassos (p.passos)) != p.passos) { ++malament; quin = p.id; }
            comprova (malament == 0, "els " + std::to_string (guitarra::patrons().size()) + " patrons fan l'anada i la tornada (" + quin + ")");
            const auto v = guitarra::llegeixPassos ("D> . PD U! x _ [p] [i]> d(b) u(t) S X");
            comprova (v.size() == 12 && guitarra::llegeixPassos (guitarra::textPassos (v)) == v, "totes les instruccions");
            comprova (guitarra::textPassos (v) == "D> . PD U! x _ [p] [i]> d(b) u(t) S X", "i el text queda igual (" + guitarra::textPassos (v) + ")");
        });

        prova ("patrons: rasgueigs propis (desar, llegir i trobar)", []
        {
            const auto meu = guitarra::fesPropi ("El meu", "4/4", 480, guitarra::llegeixPassos ("D . D U . U D U"));
            comprova (meu.id == "propi:El meu" && meu.grup == guitarra::grupPropis && ! meu.dibuix.empty(), "id, grup i dibuix");
            const auto text = guitarra::textPropis ({ meu, guitarra::fesPropi ("Àgil", "3/4", 240, guitarra::llegeixPassos ("PD PD U>")) });
            const auto v = guitarra::llegeixPropis (text);
            comprova (v.size() == 2 && v[0].passos == meu.passos && v[1].nom == "Àgil" && v[1].pasTicks == 240 && v[1].compas == "3/4", "es desen i es llegeixen");
            comprova (guitarra::llegeixPropis ("això no és res").empty() && guitarra::llegeixPropis ("{}").empty(), "un fitxer dolent no fa res");
            comprova (guitarra::patro ("propi:El meu") == nullptr, "abans de posar-los no hi són");
            guitarra::posaPropis (v);
            comprova (guitarra::patro ("propi:El meu") != nullptr && guitarra::patro ("folk") != nullptr, "després sí, i els de sempre també");
            guitarra::posaPropis ({});
        });

        prova ("patrons: blocs dins d'un clip (posar, trepitjar, regenerar, treure)", []
        {
            Clip c;
            c.midi = true;
            c.notes.push_back ({ 0, 8000, 30, 100, 0 });        // una nota llarga d'abans
            c.notes.push_back ({ 5000, 100, 31, 100, 0 });      // i una de fora dels blocs
            const auto* folk = guitarra::patro ("folk");
            const int a = guitarra::posaBloc (c, guitarra::blocDe (folk, "Am", 960, 3840, 0.0, 1, 88), 120.0);
            comprova (a == 0 && c.blocs.size() == 1 && c.blocs[0].passos == guitarra::textPassos (folk->passos), "el bloc porta el rasgueig sencer");
            auto dins = [&] (long long x, long long y) { int n = 0; for (auto& k : c.notes) if (k.ini >= x && k.ini < y) ++n; return n; };
            const int notesAm = dins (960, 4800);
            comprova (notesAm > 10, "s'han escrit les notes (" + std::to_string (notesAm) + ")");
            comprova (c.notes[0].to == 30 && c.notes[0].dur == 960, "la nota que venia sonant s'apaga on comença el bloc");
            comprova (dins (5000, 5200) == 1, "el que hi ha fora no es toca");
            bool passa = false;
            for (auto& k : c.notes) if (k.ini >= 960 && k.ini < 4800 && k.ini + k.dur > 4800) passa = true;
            comprova (! passa, "cap nota surt del bloc");
            // Regenerar sense variació dona el mateix; amb variació i una altra llavor, no.
            const auto abans = c.notes;
            guitarra::escriuBloc (c, 0, 120.0);
            comprova (c.notes == abans, "regenerar sense variació: igual");
            c.blocs[0].variacio = 1.0; c.blocs[0].llavor = 77;
            guitarra::escriuBloc (c, 0, 120.0);
            comprova (c.notes != abans && dins (5000, 5200) == 1, "amb variació canvia, i només dins del bloc");
            // Un bloc nou a sobre de la segona meitat: el primer s'escurça.
            const int b = guitarra::posaBloc (c, guitarra::blocDe (folk, "C", 2880, 3840, 0.0, 1, 88), 120.0);
            comprova (b == 1 && c.blocs.size() == 2 && c.blocs[0].dur == 1920 && c.blocs[1].ini == 2880, "trepitjar escurça el d'abans");
            bool hiHaLa = false, hiHaDoBaix = false;
            for (auto& k : c.notes) if (k.ini >= 2880 && k.ini < 6720) { hiHaLa = hiHaLa || k.to == 45; hiHaDoBaix = hiHaDoBaix || k.to == 48; }
            comprova (! hiHaLa && hiHaDoBaix, "al tros nou hi sona el Do, no el La menor");
            // Un bloc al mig d'un altre: en queda el davant; i un que el tapa sencer el fa desaparèixer.
            guitarra::posaBloc (c, guitarra::blocDe (nullptr, "G", 0, 9000, 0.0, 1, 80), 120.0);
            comprova (c.blocs.size() == 1 && c.blocs[0].acord == "G", "un bloc que ho tapa tot es queda sol");
            comprova (c.notes.size() == 6 && c.notes[0].dur == 9000, "sense rasgueig: l'acord sencer (les 6 cordes del Sol)");
            guitarra::treuBloc (c, 0, false);
            comprova (c.blocs.empty() && c.notes.size() == 6, "treure el bloc deixant les notes");
            guitarra::posaBloc (c, guitarra::blocDe (folk, "Em", 0, 1920, 0.0, 1, 88), 120.0);
            guitarra::treuBloc (c, 0, true);
            comprova (c.blocs.empty() && dins (0, 1920) == 0, "treure el bloc amb les notes");
        });

        prova ("patrons: blocs a partir de la pista d'Acords, i es desen", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.nom = "Patrons";
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            afegeixPista (q, "Acords", "acords");
            // La pista d'Acords es pot haver posat a dalt: es busquen pel tipus.
            const int G = esAcords (q.pistes[0]) ? 1 : 0, A = 1 - G;
            Clip c; c.midi = true; c.pos = 2.0; c.inici = 0.0; c.fi = 8.0;      // de 2 s a 10 s (4 compassos a 120)
            q.pistes[(size_t) G].clips.push_back (c);
            auto acord = [&] (const char* nom, double a, double b) { Clip k; k.acord = nom; k.pos = a; k.inici = 0.0; k.fi = b - a; q.pistes[(size_t) A].clips.push_back (k); };
            acord ("Am", 0.0, 4.0);     // la meitat cau abans del clip
            acord ("F", 4.0, 6.0);
            acord ("G", 6.0, 20.0);     // s'acaba amb el clip
            acord ("C", 30.0, 32.0);    // fora
            const int n = guitarra::blocsDeAcords (q, G, 0, guitarra::patro ("folk"), 0.5, 90, 120.0, 5);
            auto& cl = q.pistes[(size_t) G].clips[0];
            comprova (n == 3 && cl.blocs.size() == 3, "tres acords cauen sobre el clip");
            if (cl.blocs.size() == 3)
            {
                comprova (cl.blocs[0].acord == "Am" && cl.blocs[0].ini == 0 && cl.blocs[0].dur == 1920 * 2, "el primer, retallat a l'inici del clip");
                comprova (cl.blocs[1].acord == "F" && cl.blocs[1].ini == 3840 && cl.blocs[1].dur == 3840, "el segon, sencer");
                comprova (cl.blocs[2].acord == "G" && cl.blocs[2].ini == 7680 && cl.blocs[2].dur == 7680, "el tercer, retallat al final del clip");
                comprova (cl.blocs[0].llavor != cl.blocs[1].llavor, "cada bloc, la seva variació");
            }
            comprova (! cl.notes.empty(), "amb les seves notes");
            // Tornar-hi no duplica res.
            const auto notes = cl.notes.size();
            guitarra::blocsDeAcords (q, G, 0, guitarra::patro ("folk"), 0.0, 90, 120.0, 5);
            comprova (cl.blocs.size() == 3 && cl.notes.size() <= notes + 6, "tornar-ho a fer no duplica blocs ni notes");

            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_040";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-13T21:00:00", "x", false).correcte, "desat");
            auto l = disc::llegeix (dir / "p.mibase");
            comprova (l && l->pistes[(size_t) G].clips[0].blocs == cl.blocs && l->pistes[(size_t) G].clips[0].notes == cl.notes, "els blocs i les notes es recuperen iguals");
            std::filesystem::remove_all (dir);
        });
    }
}
