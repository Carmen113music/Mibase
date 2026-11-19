// 0.61: inversions de piano i posicions CAGED de guitarra per compàs.
namespace
{
    void proves061()
    {
        using namespace mibase;

        prova ("0.61 piano: inversions reals (majors, menors, disminuïts, augmentats i de 7a), pujant i baixant d'octava", []
        {
            auto pcs = [] (std::vector<int> v) { for (auto& x : v) x = ((x % 12) + 12) % 12; std::sort (v.begin(), v.end()); return v; };
            const std::vector<int> c { 60, 64, 67 };
            comprova (piano::invertida (c, 0) == c, "0 = posició fonamental");
            comprova (piano::invertida (c, 1) == std::vector<int> ({ 64, 67, 72 }), "1a: E G C");
            comprova (piano::invertida (c, 2) == std::vector<int> ({ 67, 72, 76 }), "2a: G C E");
            comprova (piano::invertida (c, 3) == std::vector<int> ({ 72, 76, 79 }), "3 passos: la fonamental una octava amunt (sense cicle)");
            comprova (piano::invertida (c, -1) == std::vector<int> ({ 55, 60, 64 }) && piano::invertida (c, -3) == std::vector<int> ({ 48, 52, 55 }),
                      "enrere: va baixant (2a inversió a sota, i l'octava de sota)");
            for (const char* nom : { "C", "Cm", "Cdim", "Caug", "C7", "Cm7", "Cmaj7", "F#m7", "Bb7" })
            {
                const auto a = harmonia::llegeixAcord (nom);
                auto b = a; b.inversio = 0; b.baix = -1;
                auto f = harmonia::notesPad (b, 4);
                std::sort (f.begin(), f.end());
                const int n = (int) f.size();
                double m0 = 0.0; for (int x : f) m0 += x; m0 /= n;
                bool be = true;
                for (int k = 0; k < n; ++k)
                {
                    const auto v = piano::invertida (f, k);
                    double m = 0.0; for (int x : v) m += x; m /= n;
                    be = be && pcs (v) == pcs (f) && (int) v.size() == n
                         && ((v.front() - f[(size_t) k]) % 12 + 12) % 12 == 0   // la greu és la nota k de l'acord
                         && m >= m0 - 0.01 && m <= m0 + 12.01 && v.back() - v.front() < 12;
                }
                auto amunt = f; for (auto& x : amunt) x += 12;
                auto avall = f; for (auto& x : avall) x -= 12;
                be = be && piano::invertida (f, n) == amunt && piano::invertida (f, -n) == avall;
                comprova (be, std::string (nom) + ": " + std::to_string (n) + " inversions, mateixes notes, la greu correcta, sense salts");
            }
        });

        prova ("0.61 piano: la inversió canvia les notes de la mà dreta, no el ritme ni la mà esquerra", []
        {
            const auto* p = piano::patro ("pa01");
            comprova (p != nullptr, "hi ha el patró");
            if (p == nullptr) return;
            piano::Opcions o0, o1;
            o1.inversio = 1;
            const auto a = piano::toca (harmonia::llegeixAcord ("C"), *p, 3840, o0), b = piano::toca (harmonia::llegeixAcord ("C"), *p, 3840, o1);
            comprova (a.size() == b.size(), "les mateixes notes (quantes)");
            std::multiset<std::pair<long long, long long>> ta, tb;
            std::vector<int> esqA, esqB;
            for (auto& n : a) { ta.insert ({ n.ini, n.dur }); if (n.to < 48) esqA.push_back (n.to); }
            for (auto& n : b) { tb.insert ({ n.ini, n.dur }); if (n.to < 48) esqB.push_back (n.to); }
            comprova (ta == tb, "el ritme i les durades no canvien");
            comprova (esqA == esqB, "la mà esquerra no canvia");
            comprova (! (a == b), "les notes sí que canvien");
        });

        prova ("0.61 guitarra: les cinc formes CAGED de Do, pujant pel mànec", []
        {
            const auto v = guitarra::posicionsCaged (harmonia::llegeixAcord ("C"));
            std::string formes, textos;
            for (auto& x : v) { formes += x.forma ? x.forma : '?'; textos += x.pos.text() + " "; }
            std::printf ("        (Do: %s · %s)\n", formes.c_str(), textos.c_str());
            comprova (formes == "CAGED", "C, A, G, E i D, per ordre");
            if (v.size() == 5)
            {
                comprova (v[0].pos.text() == "x32010", "forma de C: l'obert");
                comprova (v[1].pos.text() == "x35553", "forma de A: celleta al 3");
                comprova (v[3].pos.text() == "8-10-10-9-8-8", "forma de E: celleta al 8");
                comprova (v[4].pos.text() == "x-x-10-12-13-12", "forma de D: al 10");
            }
            comprova (guitarra::posicioCaged (harmonia::llegeixAcord ("C"), guitarra::Forma::Oberts, 0).pos == guitarra::posicio (harmonia::llegeixAcord ("C")),
                      "pas 0 = la de sempre");
            comprova (guitarra::posicioCaged (harmonia::llegeixAcord ("C"), guitarra::Forma::Oberts, 5).pos == guitarra::posicio (harmonia::llegeixAcord ("C")),
                      "en cicle: cinc passos tornen a l'original");
        });

        prova ("0.61 guitarra: CAGED de molts acords, sempre amb les seves notes i tocables", []
        {
            auto pcsDe = [] (const harmonia::AcordPad& a) { auto b = a; b.inversio = 0; b.baix = -1; std::set<int> s; for (int n : harmonia::notesPad (b)) s.insert (n % 12); return s; };
            for (const char* nom : { "C", "G", "D", "A", "E", "F", "Bb", "F#", "Am", "Em", "Dm", "Bm", "C#m", "G7", "A7", "Am7", "Dmaj7", "Fmaj7",
                                     "Bdim", "Caug", "Dsus4", "Asus2", "C6", "E9" })
            {
                const auto a = harmonia::llegeixAcord (nom);
                const auto v = guitarra::posicionsCaged (a);
                const auto pcs = pcsDe (a);
                bool be = v.size() >= 3, puja = true;
                for (size_t i = 0; i < v.size(); ++i)
                {
                    const auto notes = v[i].pos.notes();
                    std::set<int> te;
                    for (int n : notes) { te.insert (n % 12); if (! pcs.count (n % 12)) be = false; }
                    if (notes.empty() || notes.front() % 12 != a.arrel % 12) be = false;          // la fonamental a baix
                    if (te.size() + 1 < pcs.size()) be = false;                                     // com a molt en falta una (la 5a)
                    int mn = 99, mx = 0, dits = 0;
                    for (int t : v[i].pos.trast) if (t > 0) { mn = std::min (mn, t); mx = std::max (mx, t); }
                    for (int t : v[i].pos.trast) if (t > mn) ++dits;
                    if (mx - mn > 3 || mx > 17 || dits > 4) be = false;                             // es pot tocar
                    if (i > 0)
                    {
                        auto alt = [] (const guitarra::Posicio& p) { double s = 0; int n = 0; for (int t : p.trast) if (t >= 0) { s += t; ++n; } return s / n; };
                        puja = puja && alt (v[i].pos) >= alt (v[i - 1].pos);
                    }
                }
                comprova (be && puja, std::string (nom) + ": " + std::to_string (v.size()) + " posicions bones, de greu a aguda");
            }
            // powerchords: la fonamental a diverses cordes, sempre fonamental i quinta
            const auto pw = guitarra::posicionsCaged (harmonia::llegeixAcord ("A"), guitarra::Forma::Power);
            bool be = pw.size() >= 3;
            for (auto& x : pw) { const auto n = x.pos.notes(); be = be && n.size() == 2 && n[0] % 12 == 9 && n[1] - n[0] == 7; }
            comprova (be, "powerchords de La a diverses cordes");
            comprova (guitarra::posicionsCaged (harmonia::llegeixAcord ("C/E")).size() == 1, "un acord amb una altra nota greu es queda com està");
        });

        prova ("0.61 compassos: cada compàs la seva disposició, sense tocar el ritme ni els altres compassos; es desa", []
        {
            Clip c;
            c.midi = true;
            c.acords = { { 0, 3840, "C" }, { 3840, 3840, "G" }, { 7680, 3840, "Am" } };
            c.blocs.push_back (guitarra::blocDe (guitarra::patro ("pa01"), 0, 11520, 0.0, 1, 0));   // piano
            Projecte p;
            p.pistes.emplace_back();
            p.pistes[0].id = "a";
            const auto& t = p.pistes[0];
            comprova (guitarra::escriuBloc (c, 0, guitarra::acordsDe (p, t, c), 120.0), "s'escriu");
            const auto abans = c.notes;
            comprova (guitarra::mouVeu (c, 3840, 3840, 1) == 1 && guitarra::veuDe (c, 3840) == 1, "el compàs 2 passa a la 1a inversió");
            const auto tr = guitarra::acordsDe (p, t, c);
            comprova (tr.size() == 3 && tr[0].pas == 0 && tr[1].pas == 1 && tr[2].pas == 0, "només el compàs 2 té pas");
            comprova (guitarra::escriuBloc (c, 0, tr, 120.0), "es reescriu");
            bool altresIguals = true, ritme = true, canvia = false;
            std::multiset<std::pair<long long, long long>> ra, rb;
            for (auto& n : abans) if (n.ini >= 3840 && n.ini < 7680) ra.insert ({ n.ini, n.dur });
            for (auto& n : c.notes) if (n.ini >= 3840 && n.ini < 7680) rb.insert ({ n.ini, n.dur });
            ritme = ra == rb;
            std::vector<Nota> fa, fb;
            for (auto& n : abans) if (n.ini < 3840 || n.ini >= 7680) fa.push_back (n);
            for (auto& n : c.notes) if (n.ini < 3840 || n.ini >= 7680) fb.push_back (n);
            altresIguals = fa == fb;
            for (auto& n : c.notes) if (n.ini >= 3840 && n.ini < 7680 && std::find (abans.begin(), abans.end(), n) == abans.end()) canvia = true;
            comprova (altresIguals, "els altres compassos no canvien");
            comprova (ritme, "el ritme i les durades del compàs no canvien");
            comprova (canvia, "les notes del compàs sí que canvien");

            // es desa i es torna a llegir
            p.pistes[0].clips.push_back (c);
            const auto dir = std::filesystem::temp_directory_path() / "mibase_p061";
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", p, "2026-11-19T12:00:00", "x", false).correcte, "desat");
            const auto q = disc::llegeix (dir / "p.mibase");
            comprova (q && ! q->pistes.empty() && ! q->pistes[0].clips.empty() && q->pistes[0].clips[0].veus == c.veus, "les disposicions es desen");

            // tornar a l'original
            comprova (guitarra::mouVeu (c, 3840, 3840, -1) == 0 && c.veus.empty(), "un pas enrere: original (i no es desa res)");
            comprova (guitarra::escriuBloc (c, 0, guitarra::acordsDe (p, t, c), 120.0) && c.notes == abans, "les notes tornen a ser les d'abans");

            // guitarra: un altre bloc, posició CAGED
            Clip g;
            g.midi = true;
            g.acords = { { 0, 3840, "C" }, { 3840, 3840, "C" } };
            g.blocs.push_back (guitarra::blocDe (nullptr, 0, 7680, 0.0, 1, 90));   // l'acord sencer
            guitarra::escriuBloc (g, 0, guitarra::acordsDe (p, t, g), 120.0);
            const auto ga = g.notes;
            guitarra::mouVeu (g, 3840, 3840, 1);
            guitarra::escriuBloc (g, 0, guitarra::acordsDe (p, t, g), 120.0);
            std::vector<int> c1, c2;
            for (auto& n : g.notes) (n.ini < 3840 ? c1 : c2).push_back (n.to);
            std::sort (c1.begin(), c1.end()); std::sort (c2.begin(), c2.end());
            const auto cag = guitarra::posicioCaged (harmonia::llegeixAcord ("C"), guitarra::Forma::Oberts, 1).pos.notes();
            comprova (c1 == guitarra::notes (harmonia::llegeixAcord ("C")) && c2 == cag, "guitarra: el compàs 2 sona amb la forma de A");
        });
    }
}
