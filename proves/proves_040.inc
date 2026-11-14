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

prova ("patrons: els blocs llegeixen els acords del clip (posats a mà)", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            q.pistes[0].segueixAcords = false;
            Clip k; k.midi = true; k.pos = 0.0; k.fi = 16.0;
            k.notes.push_back ({ 0, 8000, 30, 100, 0 });       // una nota llarga d'abans
            k.notes.push_back ({ 20000, 100, 31, 100, 0 });    // i una de fora dels blocs
            q.pistes[0].clips.push_back (k);
            auto& c = q.pistes[0].clips[0];
            const auto* folk = guitarra::patro ("folk");
            auto dins = [&] (long long x, long long y) { int n = 0; for (auto& z : c.notes) if (z.ini >= x && z.ini < y) ++n; return n; };
            auto sona = [&] (long long x, long long y, int to) { for (auto& z : c.notes) if (z.ini >= x && z.ini < y && z.to == to) return true; return false; };

            // Un bloc sense cap acord: hi és, però en silenci.
            comprova (guitarra::posaBloc (c, guitarra::blocDe (folk, 3840, 3840, 0.0, 1, 88)) == 0 && c.blocs[0].passos == guitarra::textPassos (folk->passos), "el bloc porta el rasgueig sencer");
            guitarra::actualitza (q);
            comprova (dins (3840, 7680) == 0 && c.notes.size() == 2, "sense acord, el bloc espera en silenci");
            // Arriba l'acord: s'escriuen les notes soles.
            guitarra::posaAcord (c, { 3840, 3840, "Am" });
            comprova (guitarra::actualitza (q), "posar l'acord fa escriure les notes");
            comprova (dins (3840, 7680) > 10 && sona (3840, 7680, 45), "sona el La menor");
            comprova (c.notes[0].to == 30 && c.notes[0].dur == 3840, "la nota que venia sonant s'apaga on comença el bloc");
            comprova (dins (20000, 20200) == 1, "el que hi ha fora no es toca");
            bool passa = false;
            for (auto& z : c.notes) if (z.ini >= 3840 && z.ini < 7680 && z.ini + z.dur > 7680) passa = true;
            comprova (! passa, "cap nota surt del bloc");
            comprova (! guitarra::actualitza (q), "si no canvia res, no es toca res");
            // Canvi d'acord a mig bloc: el ritme segueix i les notes canvien on toca.
            const auto abans = c.notes;
            guitarra::posaAcord (c, { 5760, 1920, "C" });
            guitarra::actualitza (q);
            comprova (c.acords.size() == 2 && c.acords[0].dur == 1920, "l'acord nou escurça el d'abans");
            comprova (sona (3840, 5760, 45) && ! sona (5760, 7680, 45) && sona (5760, 7680, 48), "primera meitat La menor, segona Do");
            std::vector<long long> a1, a2;
            for (auto& z : abans) if (z.ini >= 3840 && z.ini < 7680) a1.push_back (z.ini);
            for (auto& z : c.notes) if (z.ini >= 3840 && z.ini < 7680) a2.push_back (z.ini);
            std::sort (a1.begin(), a1.end()); a1.erase (std::unique (a1.begin(), a1.end()), a1.end());
            std::sort (a2.begin(), a2.end()); a2.erase (std::unique (a2.begin(), a2.end()), a2.end());
            int comuns = 0;
            for (auto x : a2) if (std::binary_search (a1.begin(), a1.end(), x)) ++comuns;
            comprova (comuns * 10 >= (int) a2.size() * 8, "els cops cauen als mateixos llocs: el rasgueig no torna a començar");
            // Retoc a mà: el bloc queda fix i ja no es reescriu; desbloquejat, torna a seguir.
            auto copia = c;
            for (auto& z : c.notes) if (z.ini >= 3840 && z.ini < 7680) { z.to += 1; break; }
            comprova (guitarra::marcaEditats (c, copia) == 1 && c.blocs[0].fix, "retocar una nota fixa el bloc");
            const auto retocat = c.notes;
            c.acords[0].acord = "Em";
            comprova (! guitarra::actualitza (q) && c.notes == retocat, "un bloc fix no es reescriu encara que canviï l'acord");
            c.blocs[0].fix = false;
            comprova (guitarra::actualitza (q) && sona (3840, 5760, 40), "desbloquejat, torna a seguir els acords (Mi menor)");
            // Un altre patró a sobre, i treure.
            const int b2 = guitarra::posaBloc (c, guitarra::blocDe (nullptr, 5760, 3840, 0.0, 1, 80));
            guitarra::actualitza (q);
            comprova (b2 == 1 && c.blocs.size() == 2 && c.blocs[0].dur == 1920, "un bloc nou escurça el que trepitja");
            comprova (dins (5760, 7680) == 5 && dins (7680, 9600) == 0, "sense rasgueig: l'acord sencer, només on hi ha acord");
            guitarra::treuBloc (c, 1, true);
            comprova (c.blocs.size() == 1 && dins (5760, 9600) == 0, "treure el bloc amb les notes");
            guitarra::treuAcord (c, 0);
            guitarra::actualitza (q);
            comprova (dins (3840, 5760) == 0, "sense l'acord, el bloc torna a callar");
        });

        prova ("patrons: seguir la pista d'Acords, variacions i desar", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.nom = "Patrons";
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            afegeixPista (q, "Acords", "acords");
            const int G = esAcords (q.pistes[0]) ? 1 : 0, A = 1 - G;
            Clip c; c.midi = true; c.pos = 2.0; c.inici = 0.0; c.fi = 8.0;      // de 2 s a 10 s (4 compassos a 120)
            q.pistes[(size_t) G].clips.push_back (c);
            auto acord = [&] (const char* nom, double a, double b) { Clip k; k.acord = nom; k.pos = a; k.inici = 0.0; k.fi = b - a; q.pistes[(size_t) A].clips.push_back (k); };
            acord ("Am", 0.0, 4.0);     // la meitat cau abans del clip
            acord ("F", 4.0, 6.0);
            acord ("G", 6.0, 20.0);
            auto& cl = q.pistes[(size_t) G].clips[0];
            auto sona = [&] (long long x, long long y, int to) { for (auto& z : cl.notes) if (z.ini >= x && z.ini < y && z.to == to) return true; return false; };
            q.pistes[(size_t) G].segueixAcords = true;
            const auto trams = guitarra::acordsDe (q, q.pistes[(size_t) G], cl);
            comprova (trams.size() == 3 && trams[0].ini == -3840 && trams[0].fi == 3840 && trams[1].acord == "F" && trams[1].ini == 3840, "els acords de la pista, en ticks del clip");
            for (int i = 0; i < 4; ++i) guitarra::posaBloc (cl, guitarra::blocDe (guitarra::patro (i == 1 ? "balada" : "folk"), i * 3840LL, 3840, 0.0, 1, 88));
            guitarra::actualitza (q);
            comprova (sona (0, 3840, 45) && sona (3840, 7680, 41) && sona (7680, 15360, 43), "cada compàs, l'acord que hi ha a la pista d'Acords");
            // Canviar un acord a la pista d'Acords canvia les notes soles.
            q.pistes[(size_t) A].clips[1].acord = "Dm";
            comprova (guitarra::actualitza (q) && sona (3840, 7680, 50) && ! sona (3840, 7680, 41), "canviar l'acord a la pista d'Acords reescriu el compàs");
            // Apagar «segueix»: manen els acords del clip (aquí, cap).
            q.pistes[(size_t) G].segueixAcords = false;
            guitarra::actualitza (q);
            comprova (cl.notes.empty(), "sense seguir i sense acords posats a mà, silenci");
            q.pistes[(size_t) G].segueixAcords = true;
            guitarra::actualitza (q);

            const auto vars = guitarra::variacions (*guitarra::patro ("folk"));
            comprova (vars.size() >= 2, "el folk té variacions (" + std::to_string (vars.size()) + ")");
            bool diferents = true;
            for (auto& v : vars) diferents = diferents && v.passos != guitarra::patro ("folk")->passos && v.passos.size() == guitarra::patro ("folk")->passos.size()
                                             && guitarra::llegeixPassos (guitarra::textPassos (v.passos)) == v.passos && ! v.dibuix.empty();
            comprova (diferents, "totes diferents de l'original, de la mateixa mida i es poden desar");

            cl.blocs[2].fix = true;
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_041";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            q.pistes[(size_t) G].clips[0].acords.push_back ({ 0, 960, "E7" });
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-14T09:00:00", "x", false).correcte, "desat");
            auto l = disc::llegeix (dir / "p.mibase");
            comprova (l && l->pistes[(size_t) G].clips[0].blocs == cl.blocs && l->pistes[(size_t) G].clips[0].notes == cl.notes
                      && l->pistes[(size_t) G].clips[0].acords == cl.acords && l->pistes[(size_t) G].segueixAcords, "blocs (amb el cadenat), acords del clip i notes es recuperen");
            // Un projecte de la 0.40 (l'acord dins del bloc) s'obre amb l'acord a la llista del clip.
            {
                std::ifstream f (dir / "p.mibase");
                std::string txt ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
                const auto on = txt.find ("\"patro\": \"folk\"");
                comprova (on != std::string::npos, "hi ha el bloc al fitxer");
                if (on != std::string::npos)
                {
                    txt.insert (on, "\"acord\": \"Bm\", ");
                    std::ofstream (dir / "vell.mibase") << txt;
                    auto v = disc::llegeix (dir / "vell.mibase");
                    bool hiEs = false;
                    if (v) for (auto& a : v->pistes[(size_t) G].clips[0].acords) hiEs = hiEs || a.acord == "Bm";
                    comprova (hiEs, "l'acord d'un bloc de la 0.40 passa als acords del clip");
                }
            }
            std::filesystem::remove_all (dir);
        });
    }
}
