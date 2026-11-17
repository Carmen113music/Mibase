// 0.48: acords fixats a mà en un clip que segueix la pista d'Acords, i variacions de piano més evidents.
namespace
{
    void proves048()
    {
        using namespace mibase;
        prova ("0.48 acords fixats: manen sobre la pista d'Acords al seu compàs", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Guitarra", "midi");
            afegeixPista (q, "Acords", "acords");
            const int G = esAcords (q.pistes[0]) ? 1 : 0, A = 1 - G;
            Clip c; c.midi = true; c.pos = 0.0; c.inici = 0.0; c.fi = 8.0;   // 4 compassos a 120
            q.pistes[(size_t) G].clips.push_back (c);
            auto acord = [&] (const char* nom, double a, double b) { Clip k; k.acord = nom; k.pos = a; k.inici = 0.0; k.fi = b - a; q.pistes[(size_t) A].clips.push_back (k); };
            acord ("Am", 0.0, 4.0);
            acord ("F", 4.0, 8.0);
            q.pistes[(size_t) G].segueixAcords = true;
            auto& cl = q.pistes[(size_t) G].clips[0];
            auto sona = [&] (long long x, long long y, int to) { for (auto& z : cl.notes) if (z.ini >= x && z.ini < y && z.to == to) return true; return false; };
            for (int i = 0; i < 4; ++i) guitarra::posaBloc (cl, guitarra::blocDe (guitarra::patro ("folk"), i * 3840LL, 3840, 0.0, 1, 88));
            guitarra::actualitza (q);
            comprova (sona (3840, 7680, 45) && ! sona (3840, 7680, 43), "abans: La menor al segon compàs");
            // un pad arrossegat al segon compàs
            guitarra::posaAcordFix (cl, { 3840, 3840, "G" });
            auto tr = guitarra::acordsDe (q, q.pistes[(size_t) G], cl);
            comprova (tr.size() == 3 && tr[0].fi == 3840 && ! tr[0].fix && tr[1].acord == "G" && tr[1].fix && tr[2].acord == "F" && tr[2].ini == 7680, "la línia dels acords diu Am | G (fix) | F");
            comprova (guitarra::actualitza (q) && sona (3840, 7680, 43) && sona (0, 3840, 45), "el compàs fixat sona en Sol; la resta, igual");
            q.pistes[(size_t) A].clips[0].acord = "C";
            guitarra::actualitza (q);
            comprova (sona (3840, 7680, 43) && sona (0, 3840, 48), "canviar la pista d'Acords no toca el compàs fixat");
            comprova (guitarra::treuAcordsFixos (cl, 4000, 4001) && cl.acordsFixos.empty(), "es pot treure");
            guitarra::actualitza (q);
            comprova (sona (3840, 7680, 48), "i torna a manar la pista d'Acords");

            // un bloc bloquejat es queda amb el seu acord: la línia dels acords diu el que sona
            cl.blocs[2].fix = true;
            guitarra::actualitza (q);
            comprova (cl.acordsFixos.size() == 1 && cl.acordsFixos[0].ini == 7680 && cl.acordsFixos[0].acord == "F", "en bloquejar-lo, l'acord queda fixat");
            q.pistes[(size_t) A].clips[1].acord = "Dm";
            guitarra::actualitza (q);
            tr = guitarra::acordsDe (q, q.pistes[(size_t) G], cl);
            bool diuF = false; for (auto& x : tr) if (x.ini == 7680 && x.acord == "F" && x.fix) diuF = true;
            comprova (diuF && sona (7680, 11520, 41) && sona (11520, 15360, 50), "la pista canvia a Dm: el bloquejat continua en Fa (i ho diu); el següent, Re menor");

            const auto dir = std::filesystem::temp_directory_path() / "mibase_p048";
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "p.mibase", q, "2026-11-15T09:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova (l && l->pistes[(size_t) G].clips[0].acordsFixos == cl.acordsFixos, "els acords fixats es desen");
        });

        prova ("0.48 piano: les variacions són clarament diferents", []
        {
            const auto do_ = harmonia::llegeixAcord ("C");
            int poques = 0, iguals = 0, malament = 0;
            for (auto& p : piano::patrons())
            {
                const long long bar = piano::ticksCompas (p.compas);
                const auto base = piano::toca (do_, p, bar);
                const auto vs = piano::variacions (p);
                if (vs.size() < 3) ++poques;
                for (auto& v : vs)
                {
                    const auto n = piano::toca (do_, v, bar);
                    // «diferent» vol dir un altre ritme o unes altres notes, no només la força
                    auto forma = [] (const std::vector<Nota>& w) { std::vector<std::pair<long long, int>> f; for (auto& x : w) f.push_back ({ x.ini / 60, x.to }); std::sort (f.begin(), f.end()); return f; };
                    if (forma (n) == forma (base)) ++iguals;
                    if (n.empty()) ++malament;
                    for (auto& x : n) if (x.ini + x.dur > bar || x.to < 21 || x.to > 108) { ++malament; break; }
                    if (piano::patro (v.id) == nullptr) ++malament;
                }
            }
            comprova (poques == 0, "cada patró té almenys tres variacions");
            comprova (iguals == 0, "cap variació fa les mateixes notes al mateix lloc que l'original");
            comprova (malament == 0, "totes caben al compàs i al teclat");
            const auto* a = piano::patro ("pa01~arpegi");
            comprova (a != nullptr && a->dreta == "1 3 5 8 5 3 1 3" , "Pop Quarter Pulse · arpegi");
        });
    }
}
