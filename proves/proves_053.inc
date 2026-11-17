// 0.53: variacions pròpies (rasgueigs desats com a variació d'un patró), arrel i part d'un id.
namespace
{
    void proves053()
    {
        using namespace mibase;
        namespace gt = mibase::guitarra;
        prova ("0.53 guitarra: arrel, part i variacions de variacions", []
        {
            comprova (gt::arrelDe ("folk") == "folk" && gt::arrelDe ("folk~tornada~ple") == "folk" && gt::arrelDe ("pa01~pont") == "pa01", "arrel");
            comprova (gt::partDe ("folk") == PartCanco::Original && gt::partDe ("folk~tornada~ple") == PartCanco::Tornada
                      && gt::partDe ("folk~ple") == PartCanco::Original && gt::partDe ("pa01~pretornada") == PartCanco::PreTornada, "part");
            comprova (gt::idPart ("folk", PartCanco::Pont) == "folk~pont" && gt::idPart ("folk", PartCanco::Original) == "folk", "idPart");
            // les variacions de les parts es troben per id (dos nivells), també al piano
            const auto* to = gt::patro ("folk~tornada");
            comprova (to != nullptr, "la tornada existeix");
            const auto vars = gt::variacions (*to);
            comprova (! vars.empty() && gt::patro (vars.front().id) != nullptr && vars.front().id.rfind ("folk~tornada~", 0) == 0, "una variació de la tornada es troba per id");
            const auto* pt = gt::patro ("pa01~tornada");
            const auto pv = pt != nullptr ? gt::variacions (*pt) : std::vector<gt::Patro>();
            comprova (! pv.empty() && gt::patro (pv.front().id) != nullptr && piano::patro (pv.front().id) != nullptr, "piano: variacions de la tornada");
            const auto b = gt::blocDe (pv.empty() ? nullptr : gt::patro (pv.front().id), 0, 1920, 0.0, 1, 0);
            comprova (b.tipus == "piano" && ! b.passos.empty(), "piano: es pot posar");
        });

        prova ("0.53 guitarra: rasgueigs propis com a variació d'un patró", []
        {
            const auto abans = gt::propis();
            auto nou = gt::fesPropi ("El meu", "4/4", 480, gt::llegeixPassos ("D . D U"));
            auto var = gt::fesPropi ("Folk amb apagats", "4/4", 480, gt::llegeixPassos ("D x D U"), gt::Forma::Oberts, "folk");
            comprova (nou.de.empty() && var.de == "folk", "fesPropi porta «de»");
            const auto text = gt::textPropis ({ nou, var });
            const auto llegits = gt::llegeixPropis (text);
            comprova (llegits.size() == 2 && llegits[0].de.empty() && llegits[1].de == "folk", "«de» es desa i es llegeix");
            gt::posaPropis ({ nou, var });
            const auto vp = gt::variacionsPropies ("folk");
            comprova (vp.size() == 1 && vp[0]->id == "propi:Folk amb apagats", "surt a les variacions del folk");
            comprova (gt::variacionsPropies ("balada").empty(), "i no a les d'un altre");
            comprova (gt::arrelDe ("propi:Folk amb apagats") == "folk" && gt::arrelDe ("propi:El meu") == "propi:El meu", "l'arrel d'una variació pròpia és el seu patró");
            comprova (gt::patro ("propi:Folk amb apagats") != nullptr, "es troba per id");
            gt::posaPropis (abans);
        });
    }
}
