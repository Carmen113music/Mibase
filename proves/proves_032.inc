// 0.32: patrons dels PDF d'en Roger (tècniques noves) i variació del rasgueig.
namespace
{
    void proves032()
    {
        prova ("rasgueig: els patrons quadren amb el seu compàs", []
        {
            int dolents = 0;
            std::string quins;
            for (auto& p : guitarra::patrons())
            {
                if (p.pasTicks <= 0) continue;
                const long long llarg = p.pasTicks * (long long) p.passos.size();
                long long compas = 3840;
                if (p.compas == "3/4" || p.compas == "6/8") compas = 2880;
                else if (p.compas == "2/4") compas = 1920;
                else if (p.compas == "12/8") compas = 5760;
                else if (p.compas == "5/4") compas = 4800;
                else if (p.compas == "7/8") compas = 3360;
                if (llarg != compas && llarg * 2 != compas) { ++dolents; quins += " " + p.id + "(" + std::to_string (llarg) + ")"; }
            }
            comprova (dolents == 0, "cada patró fa un compàs o mig" + quins);
            comprova (guitarra::patrons().size() >= 160, "hi ha els patrons dels PDF (" + std::to_string (guitarra::patrons().size()) + ")");
            std::set<std::string> ids;
            for (auto& p : guitarra::patrons()) ids.insert (p.id);
            comprova (ids.size() == guitarra::patrons().size(), "els ids no es repeteixen");
        });

        prova ("rasgueig: palm mute, slap, baix, aguts i dits", []
        {
            const std::vector<int> cordes { 40, 47, 52, 56, 59, 64 };   // Mi major
            auto toca = [&] (const char* passos, long long pas = 480)
            {
                guitarra::Patro p;
                p.pasTicks = pas;
                p.passos = guitarra::llegeixPassos (passos);
                guitarra::Opcions o;
                o.vel = 90;
                return guitarra::rasgueja (cordes, p, pas * (long long) p.passos.size(), o);
            };
            auto palm = toca ("PD");
            comprova (palm.size() == 4 && palm[0].to == 40 && palm.back().to == 56, "palm: les 4 greus");
            comprova (std::all_of (palm.begin(), palm.end(), [] (const Nota& n) { return n.dur <= 480 * 40 / 100 && n.vel < 95; }), "palm: curt i més fluix");
            auto obert = toca ("D");
            comprova (obert.size() == 6 && obert[0].dur > 400, "obert: sis cordes que sonen");
            auto slap = toca ("S");
            comprova (slap.size() == 6 && std::all_of (slap.begin(), slap.end(), [] (const Nota& n) { return n.dur < 100 && n.vel >= 95; }), "slap: cop sec i fort");
            auto baix = toca ("D(b)");
            comprova (baix.size() == 1 && baix[0].to == 40, "(b): només la greu");
            auto aguts = toca ("D(t)");
            comprova (aguts.size() == 4 && aguts[0].to == 52, "(t): només les 4 agudes");
            auto dits = toca ("[p] [i] [m] [a]");
            comprova (dits.size() == 4 && dits[0].to == 40 && dits[1].to == 56 && dits[2].to == 59 && dits[3].to == 64, "dits p i m a");
            auto pc = toca ("[pc]");
            comprova (pc.size() == 4 && pc[0].ini == 0 && pc.back().ini == 0, "polze + acord: tot alhora");
            auto curt = toca ("D! .");
            comprova (curt.size() == 6 && std::all_of (curt.begin(), curt.end(), [] (const Nota& n) { return n.dur <= 480 / 3 + 1; }), "«!»: curt");
            auto rasg = toca ("[r]");
            comprova (rasg.size() == 24, "rasgueado: quatre dits");
            auto suau = toca ("d"), fort = toca ("D");
            comprova (suau[0].vel < fort[0].vel - 10, "minúscula: més fluix");
            auto tap = toca ("[tap]");
            comprova (tap.size() == 1 && tap[0].to == 76, "tapping: l'aguda una octava amunt");
        });

        prova ("rasgueig: variació (humanize)", []
        {
            const std::vector<int> cordes { 40, 47, 52, 56, 59, 64 };
            const auto* p = guitarra::patro ("folk");
            comprova (p != nullptr, "hi ha el folk");
            if (p == nullptr) return;
            const long long durada = 3840 * 8;
            guitarra::Opcions o;
            o.llavor = 5;
            const auto base = guitarra::rasgueja (cordes, *p, durada, o);
            o.llavor = 99;
            comprova (guitarra::rasgueja (cordes, *p, durada, o) == base, "sense variació, la llavor no canvia res");
            o.variacio = 1.0;
            o.llavor = 5;
            const auto a = guitarra::rasgueja (cordes, *p, durada, o);
            const auto a2 = guitarra::rasgueja (cordes, *p, durada, o);
            o.llavor = 6;
            const auto b = guitarra::rasgueja (cordes, *p, durada, o);
            comprova (a == a2, "mateixa llavor, mateix resultat");
            comprova (a != b && a != base, "amb variació canvia");
            // els compassos no són tots iguals entre si
            auto compas = [&] (const std::vector<Nota>& v, int c)
            {
                std::vector<std::pair<long long, int>> r;
                for (auto& n : v) if (n.ini >= c * 3840 && n.ini < (c + 1) * 3840) r.push_back ({ n.ini - c * 3840, n.vel });
                return r;
            };
            int diferents = 0;
            for (int c = 1; c < 8; ++c) diferents += compas (a, c) != compas (a, 0) ? 1 : 0;
            comprova (diferents >= 6, "cada compàs una mica diferent");
            // el primer temps de cada compàs continua al seu lloc
            bool primers = true;
            for (int c = 0; c < 8; ++c)
                primers = primers && std::any_of (a.begin(), a.end(), [c] (const Nota& n) { return n.ini == c * 3840; });
            comprova (primers, "el primer cop de cada compàs no es mou");
            const double ratio = (double) a.size() / (double) base.size();
            comprova (ratio > 0.8 && ratio < 1.25, "més o menys les mateixes notes (" + std::to_string (ratio) + ")");
            bool dins = std::all_of (a.begin(), a.end(), [durada] (const Nota& n) { return n.ini >= 0 && n.ini + n.dur <= durada && n.vel >= 1 && n.vel <= 127; });
            comprova (dins, "tot dins la durada i amb força vàlida");
        });
    }
}
