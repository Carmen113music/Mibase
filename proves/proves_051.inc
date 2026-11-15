// 0.51: variacions per parts de la cançó dels patrons de guitarra i de piano.
namespace
{
    void proves051()
    {
        using namespace mibase;
        namespace gt = mibase::guitarra;
        prova ("0.51 guitarra: variacions per parts de la cançó", []
        {
            const auto* folk = gt::patro ("folk");
            comprova (folk != nullptr, "hi ha el folk");
            if (folk == nullptr) return;
            for (int k = 1; k < numPartsCanco; ++k)
            {
                const auto v = gt::variacioPart (*folk, (PartCanco) k);
                const auto* r = gt::patro (folk->id + "~" + sufixPart ((PartCanco) k));
                comprova (r != nullptr && r->passos == v.passos && r->nom == v.nom, std::string ("es troba per id: ") + nomPart ((PartCanco) k));
            }
            const auto es = gt::variacioPart (*folk, PartCanco::Estrofa);
            comprova (std::none_of (es.passos.begin() + 1, es.passos.end(), [] (const gt::Pas& x) { return x.cop == gt::Cop::Amunt && ! x.accent; }), "estrofa: sense amunts sense accent");
            const auto pre = gt::variacioPart (*folk, PartCanco::PreTornada);
            bool bo = pre.crescendo >= 22;
            for (size_t i = 0; i < pre.passos.size(); ++i)
            {
                const long long t = (long long) i * pre.pasTicks;
                bo = bo && (t % 480 == 0 ? pre.passos[i].cop == gt::Cop::Avall : pre.passos[i].cop == gt::Cop::Res);
            }
            comprova (bo, "pre-tornada: corxeres avall i crescendo");
            const auto to = gt::variacioPart (*folk, PartCanco::Tornada);
            comprova (std::none_of (to.passos.begin(), to.passos.end(), [] (const gt::Pas& x) { return x.cop == gt::Cop::Res || x.cop == gt::Cop::Silenci; }), "tornada: plena");
            const auto po = gt::variacioPart (*folk, PartCanco::Pont);
            int cops = 0;
            for (auto& x : po.passos) if (x.cop != gt::Cop::Res) ++cops;
            comprova (cops >= 1 && cops <= 2 && po.passos[0].cop == gt::Cop::Avall && po.passos[0].accent, "pont: un cop per mig compàs");
            // a tots els patrons: cap variació buida, i gairebé totes diferents de l'original
            int iguals = 0, buides = 0, total = 0;
            for (auto& p : gt::patrons())
                for (int k = 1; k < numPartsCanco; ++k)
                {
                    const auto v = gt::variacioPart (p, (PartCanco) k);
                    ++total;
                    if (v.passos == p.passos) ++iguals;
                    if (v.passos.empty() != p.passos.empty()) ++buides;
                }
            comprova (buides == 0, "cap variació perd els passos");
            comprova (iguals * 5 < total, "la majoria de variacions canvien alguna cosa");
            std::printf ("        (guitarra: %d de %d variacions iguals a l'original)\n", iguals, total);
            // un bloc fet amb la variació porta els seus passos
            const auto b = gt::blocDe (gt::patro ("folk~pont"), 0, 4 * 1920, 0.0, 1, 90);
            comprova (b.passos == gt::textPassos (po.passos) && b.patro == "folk~pont", "el bloc porta la variació");
        });

        prova ("0.51 piano: variacions per parts de la cançó", []
        {
            const auto* p = piano::patro ("pa01");
            comprova (p != nullptr, "hi ha el pa01");
            if (p == nullptr) return;
            const auto es = piano::variacioPart (*p, PartCanco::Estrofa);
            comprova (es.suau && es.vel < p->vel && es.esquerra == "R+8 . . . R+8 . . .", "estrofa: suau i més fluixa (el baix ja era espaiat)");
            const auto* p2 = piano::patro ("pa02");
            comprova (p2 != nullptr && piano::variacioPart (*p2, PartCanco::Estrofa).esquerra == "R . . . . . . .", "estrofa: el baix només al principi i a mig compàs");
            const auto pre = piano::variacioPart (*p, PartCanco::PreTornada);
            comprova (pre.esquerra == "R+8 . R . R+8 . R ." && pre.vel > p->vel, "pre-tornada: baix a cada temps");
            const auto to = piano::variacioPart (*p, PartCanco::Tornada);
            comprova (to.esquerraPlena && to.vel > p->vel, "tornada: plena");
            const auto po = piano::variacioPart (*p, PartCanco::Pont);
            comprova (po.suau && po.gate >= 1.0, "pont: deixant sonar");
            // es troben per id, també des de la llista de guitarra (patrons de piano vistos com a patrons)
            const auto* g = guitarra::patro ("pa01~tornada");
            comprova (piano::patro ("pa01~tornada") != nullptr && g != nullptr && g->piano, "es troben per id");
            const auto b = guitarra::blocDe (g, 0, 1920, 0.0, 1, 0);
            comprova (b.tipus == "piano" && b.passosEsq == to.esquerra && b.vel == to.vel, "el bloc porta la variació");
            // i el bloc torna a ser el patró de la variació (esquerra plena)
            comprova (piano::patroDeBloc (b).esquerraPlena, "patroDeBloc recupera l'esquerra plena");
            int iguals = 0, total = 0;
            for (auto& q : piano::patrons())
                for (int k = 1; k < numPartsCanco; ++k)
                {
                    const auto v = piano::variacioPart (q, (PartCanco) k);
                    ++total;
                    if (v.dreta == q.dreta && v.esquerra == q.esquerra && v.vel == q.vel && v.suau == q.suau && v.esquerraPlena == q.esquerraPlena) ++iguals;
                }
            comprova (iguals == 0, "les variacions de piano sempre canvien alguna cosa");
            std::printf ("        (piano: %d patrons, %d variacions)\n", (int) piano::patrons().size(), total);
        });
    }
}
