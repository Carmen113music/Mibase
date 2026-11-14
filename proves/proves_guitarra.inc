// 0.29: proves de la guitarra (posicions d'acords i rasgueigs).
namespace
{
    void provesGuitarra()
    {
        prova ("guitarra: posicions dels acords", []
        {
            auto pos = [] (const char* n) { return guitarra::posicio (harmonia::llegeixAcord (n)).text(); };
            comprova (pos ("C") == "x32010" && pos ("G") == "320003" && pos ("D") == "xx0232" && pos ("Am") == "x02210" && pos ("Em") == "022000",
                      "els acords oberts de sempre");
            comprova (guitarra::notes (harmonia::llegeixAcord ("D")) == std::vector<int> ({ 50, 57, 62, 66 }), "el Re obert només té les 4 cordes agudes");
            comprova (guitarra::notes (harmonia::llegeixAcord ("E")) == std::vector<int> ({ 40, 47, 52, 56, 59, 64 }), "el Mi obert, les 6");
            comprova (pos ("F#m") == "244222" && pos ("Bb") == "x13331" && pos ("Cm") == "x35543" && pos ("C#m") == "x46654", "celletes (forma de Mi i de La)");
            comprova (pos ("D/F#") == "200232" && pos ("G/B") == "x20003", "amb una altra nota greu");
            comprova (guitarra::posicio ({}).buida() && guitarra::notes ({}).empty(), "un pad buit no té posició");
            comprova (guitarra::llegeixPosicio ("x-10-12-12-11-10").text() == "x-10-12-12-11-10", "trasts de dues xifres");
            // Qualsevol acord dels pads: la corda greu fa el baix, només hi ha notes de l'acord, i hi són la 3a i la 7a.
            bool be = true;
            for (int arrel = 0; arrel < 12 && be; ++arrel)
                for (int tipus = 0; tipus < harmonia::numTipusPad && be; ++tipus)
                    for (unsigned tens : { 0u, (unsigned) harmonia::T7, (unsigned) harmonia::TMaj7, (unsigned) (harmonia::T7 | harmonia::T9) })
                    {
                        harmonia::AcordPad a;
                        a.arrel = arrel; a.tipus = tipus; a.tensions = tens;
                        const auto n = guitarra::posicio (a).notes();
                        const auto ref = harmonia::notesPad (a);
                        if (n.size() < 4 || n.front() % 12 != arrel) { be = false; break; }
                        for (int x : n)
                        {
                            bool hiEs = false;
                            for (int r : ref) if ((r - x) % 12 == 0) hiEs = true;
                            if (! hiEs) be = false;
                        }
                        for (int r : ref)
                        {
                            const int i = ((r - arrel) % 12 + 12) % 12;
                            if (i == 7 || i == 2) continue;   // la quinta i la 9a es poden deixar
                            bool hiEs = false;
                            for (int x : n) if ((r - x) % 12 == 0) hiEs = true;
                            if (! hiEs) be = false;
                        }
                    }
            comprova (be, "288 acords: baix a la corda greu, notes de l'acord, 4 cordes o més");
        });

        prova ("guitarra: rasgueig", []
        {
            const auto re = guitarra::notes (harmonia::llegeixAcord ("D"));
            const auto mi = guitarra::notes (harmonia::llegeixAcord ("E"));
            guitarra::Opcions o;   // 120 bpm: 7 ms = 13 ticks
            const auto* folk = guitarra::patro ("folk");
            comprova (folk != nullptr && folk->passos.size() == 8 && folk->passos[0] == guitarra::Pas { guitarra::Cop::Avall, true }
                      && folk->passos[1].cop == guitarra::Cop::Res && folk->passos[3].cop == guitarra::Cop::Amunt, "el patró es llegeix bé");
            auto n = guitarra::rasgueja (mi, *folk, 3840, o);
            // Primer cop (avall): de greu a aguda, 13 ticks entre cordes, la greu la més forta.
            comprova (n.size() >= 6 && n[0].to == 40 && n[0].ini == 0 && n[1].to == 47 && n[1].ini == 13 && n[5].to == 64 && n[5].ini == 65, "avall: de greu a aguda amb retard");
            comprova (n[0].vel > n[1].vel && n[0].vel > n[5].vel, "avall: la corda greu, la més forta");
            // El primer cop amunt és al pas 3 (1440): comença per l'aguda i només toca 4 cordes.
            std::vector<Nota> amunt;
            for (auto& x : n) if (x.ini >= 1440 && x.ini < 1920) amunt.push_back (x);
            comprova (amunt.size() == 4 && amunt[0].to == 64 && amunt[3].to == 52 && amunt[0].vel > amunt[3].vel, "amunt: 4 cordes, d'aguda a greu, cada cop més fluix");
            // L'accent del primer temps és més fort que el cop sense accent del tercer.
            int vAccent = 0, vNormal = 0;
            for (auto& x : n) { if (x.ini == 0) vAccent = x.vel; if (x.ini == 960) vNormal = x.vel; }
            comprova (vAccent >= vNormal + 15, "l'accent se sent");
            // Cap nota es trepitja amb la següent de la mateixa corda, i cap passa de la durada.
            bool net = true;
            for (size_t i = 0; i < n.size(); ++i)
            {
                if (n[i].ini + n[i].dur > 3840 || n[i].dur < 1 || n[i].vel < 1 || n[i].vel > 127) net = false;
                for (size_t j = i + 1; j < n.size(); ++j)
                    if (n[j].to == n[i].to && n[j].ini < n[i].ini + n[i].dur) net = false;
            }
            comprova (net, "cap nota trepitjada ni fora de la durada");
            // La corda greu del primer cop sona fins que es torna a tocar (al tercer pas, avall), no fins al cop amunt.
            comprova (n[0].dur == 960, "les cordes greus continuen sonant mentre el cop amunt només toca les agudes");
            comprova (guitarra::rasgueja (mi, *folk, 3840, o) == n, "sempre surt igual");
            comprova (n.back().ini + n.back().dur == 3840 && n.back().ini >= 1920, "dos compassos: el patró es repeteix");

            // Cops apagats: curts i fluixos, i tallen el que sonava.
            auto p = guitarra::rasgueja (re, *guitarra::patro ("percussiu"), 1920, o);
            bool muts = false, tallat = true;
            for (auto& x : p)
            {
                if (x.ini >= 960 && x.ini < 1440) { muts = true; if (x.dur > 120 || x.vel > 60) muts = false; }
                if (x.ini < 960 && x.ini + x.dur > 960) tallat = false;
            }
            comprova (muts && tallat, "cop apagat: curt, fluix i apaga les cordes");
            // Contratemps: res al temps fort, i cada cop dura una corxera com a molt.
            auto r = guitarra::rasgueja (re, *guitarra::patro ("contratemps"), 1920, o);
            bool contra = ! r.empty();
            for (auto& x : r) if (x.ini % 960 < 480 || x.dur > 480) contra = false;
            comprova (contra, "contratemps");
            // Un sol rasgueig que dura tot el que s'ha demanat.
            auto u = guitarra::rasgueja (re, *guitarra::patro ("un"), 1920, o);
            comprova (u.size() == 4 && u[0].ini == 0 && u[0].dur == 1920 && u[3].ini == 39 && u[3].ini + u[3].dur == 1920, "un sol rasgueig");
            // A un tempo molt ràpid amb semicorxeres, el rasgueig s'estreny i no envaeix el cop següent.
            guitarra::Opcions rapid; rapid.tempo = 240.0; rapid.msPerCorda = 12.0;
            auto f = guitarra::rasgueja (mi, *guitarra::patro ("funk"), 960, rapid);
            bool cap = true;
            for (auto& x : f) if (x.ini % 240 > 120) cap = false;
            comprova (cap, "rasgueig estret quan no hi ha lloc");

            // Al clip: les notes queden ordenades i es retornen els índexs de les noves.
            Clip c; c.midi = true;
            c.notes.push_back ({ 100, 200, 72, 90, 0 });
            const auto idx = guitarra::afegeix (c, u, 960);
            bool ordenat = true;
            for (size_t i = 1; i < c.notes.size(); ++i) if (c.notes[i].ini < c.notes[i - 1].ini) ordenat = false;
            comprova (c.notes.size() == 5 && idx.size() == 4 && ordenat && c.notes[0].to == 72 && c.notes[(size_t) idx[0]].ini == 960, "afegit al clip sense tocar el que hi havia");
            comprova (guitarra::patrons().size() >= 10 && guitarra::patro ("no-existeix") == nullptr, "llista de patrons");
        });
    }
}
