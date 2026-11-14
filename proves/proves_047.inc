// 0.47: patrons de piano
namespace
{
    void proves047()
    {
        using namespace mibase;
prova ("0.47 piano: els 200 patrons es llegeixen i omplen el seu compàs", []
{
    comprova (piano::patrons().size() == 200, "200 patrons");
    int malament = 0, buits = 0;
    const auto do_ = harmonia::llegeixAcord ("C");
    for (auto& p : piano::patrons())
    {
        const long long bar = piano::ticksCompas (p.compas);
        auto compta = [] (const std::string& s) { std::istringstream f (s); int n = 0; for (std::string w; f >> w;) ++n; return n; };
        if (compta (p.dreta) * p.pasDreta != bar || compta (p.esquerra) * p.pasEsquerra != bar) ++malament;
        const auto n = piano::toca (do_, p, bar);
        if (n.empty()) ++buits;
        for (auto& x : n) if (x.ini < 0 || x.ini + x.dur > bar || x.to < 21 || x.to > 108 || x.vel < 1 || x.vel > 127) { ++malament; break; }
    }
    comprova (malament == 0, "cap patró surt del compàs ni del teclat");
    comprova (buits == 0, "tots fan notes");
});

prova ("0.47 piano: les mans toquen el que diu el patró", []
{
    piano::Patro p;
    p.compas = "4/4"; p.dreta = "C . T . P . 5 ."; p.esquerra = "R+8 . . . 5 . . ."; p.gate = 1.0;
    const auto n = piano::toca (harmonia::llegeixAcord ("Am"), p, 3840);
    auto a = [&] (long long t0, long long t1, bool dreta) { std::vector<int> v; for (auto& x : n) if (x.ini >= t0 && x.ini < t1 && (x.to >= 55) == dreta) v.push_back (x.to); std::sort (v.begin(), v.end()); return v; };
    comprova (a (0, 400, true) == std::vector<int> ({ 57, 60, 64 }), "C: La menor tancat");
    comprova (a (960, 1300, true) == std::vector<int> ({ 60, 64 }), "T: les dues agudes");
    comprova (a (1920, 2300, true) == std::vector<int> ({ 57 }), "P: el polze");
    comprova (a (2880, 3300, true) == std::vector<int> ({ 64 }), "5: la quinta");
    comprova (a (0, 400, false) == std::vector<int> ({ 33, 45 }), "R+8: La1 i La2");
    comprova (a (1920, 2300, false) == std::vector<int> ({ 40 }), "5: Mi2");
    // mateixa llavor, mateixes notes; amb variació, una altra llavor canvia alguna cosa
    piano::Opcions o; o.variacio = 0.5; o.llavor = 7;
    auto o2 = o; o2.llavor = 8;
    comprova (piano::toca (harmonia::llegeixAcord ("Am"), p, 3840, o) == piano::toca (harmonia::llegeixAcord ("Am"), p, 3840, o), "repetible");
    comprova (! (piano::toca (harmonia::llegeixAcord ("Am"), p, 3840, o) == piano::toca (harmonia::llegeixAcord ("Am"), p, 3840, o2)), "una altra llavor, diferent");
    comprova (piano::variacions (p).size() == 3 && piano::patro ("pa01~8va") != nullptr && guitarra::patro ("pa01~ple") != nullptr, "variacions");
});

prova ("0.47 piano: un bloc de piano segueix els acords i posa el pedal", []
{
    Clip c;
    c.blocs.push_back (guitarra::blocDe (guitarra::patro ("pa01"), 0, 3840, 0.0, 1, 0));
    comprova (c.blocs[0].tipus == "piano" && ! c.blocs[0].passosEsq.empty(), "el bloc és de piano");
    std::vector<guitarra::Tram> ac { { 0, 1920, "C" }, { 1920, 3840, "G" } };
    comprova (guitarra::escriuBloc (c, 0, ac, 120.0), "s'escriu");
    bool do1 = false, sol2 = false, creua = false;
    for (auto& n : c.notes)
    {
        if (n.ini < 1920 && n.to % 12 == 0) do1 = true;
        if (n.ini >= 1920 && n.to % 12 == 7 && n.to < 48) sol2 = true;
        if (n.ini < 1920 && n.ini + n.dur > 1920) creua = true;
    }
    comprova (do1 && sol2 && ! creua, "Do la primera meitat, Sol la segona, sense trepitjar-se");
    int amunt = 0, avall = 0;
    for (auto& e : c.events) if (e.tipus == 0xB0 && e.num == 64) (e.valor > 0 ? amunt : avall)++;
    comprova (amunt == 2 && avall == 2, "pedal: un cop per acord, i es deixa anar");
    const auto notes = c.notes;
    comprova (! guitarra::escriuBloc (c, 0, ac, 120.0) && c.notes == notes, "tornar-lo a escriure no canvia res");
    ac[1].acord = "F";
    comprova (guitarra::escriuBloc (c, 0, ac, 120.0), "canvia l'acord: es reescriu");
    int ped = 0; for (auto& e : c.events) if (e.num == 64) ++ped;
    comprova (ped == 4, "el pedal no s'acumula");
    // es desa i es torna a llegir
    Projecte p; p.pistes.emplace_back(); p.pistes[0].id = "a"; p.pistes[0].clips.push_back (c);
    const auto dir = std::filesystem::temp_directory_path() / "mibase_p047";
    std::filesystem::create_directories (dir);
    comprova (disc::desa (dir / "p.mibase", p, "2026-11-14T12:00:00", "x", false).correcte, "desat");
    const auto q = disc::llegeix (dir / "p.mibase");
    comprova (q && q->pistes.size() == 1 && ! q->pistes[0].clips.empty() && ! q->pistes[0].clips[0].blocs.empty(), "llegit");
    if (q && ! q->pistes.empty() && ! q->pistes[0].clips.empty() && ! q->pistes[0].clips[0].blocs.empty())
    {
        const auto& b = q->pistes[0].clips[0].blocs[0];
        comprova (b.tipus == "piano" && b.passosEsq == c.blocs[0].passosEsq && b.pedal == c.blocs[0].pedal && b.compas == "4/4", "el bloc de piano es desa sencer");
    }
});
    }
}
