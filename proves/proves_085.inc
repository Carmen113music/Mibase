// 0.85: el Pool.
namespace
{
    void proves085()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        prova ("0.85 Pool: on es fa servir cada àudio i quins sobren", [&]
        {
            Projecte p;
            afegeixPista (p, "Veu", "audio");
            afegeixPista (p, "Guitarra", "audio");
            const auto a = afegeixPresa (p, "Àudios/a.wav", "a", 4.0).id;
            const auto b = afegeixPresa (p, "Àudios/b.wav", "b", 4.0).id;
            const auto c = afegeixPresa (p, "Àudios/c.wav", "c", 4.0).id;
            afegeixClip (p, 1, a, 8.0);
            afegeixClip (p, 0, a, 2.0);
            Clip tk; tk.presa = b; tk.fi = 4.0; p.pistes[0].takes = { tk, tk };
            comprova (usPresa (p, a).clips == 2 && usPresa (p, b).takes == 2 && ! usPresa (p, c).enUs(), "usos");
            const auto ll = llocsPresa (p, a);
            comprova (ll.size() == 2 && ll[0].pista == 0 && ll[1].pista == 1, "llocs per ordre de temps (2 s, 8 s)");
            const auto sobren = presesSenseUs (p);
            comprova (sobren.size() == 1 && sobren[0] == c, "només sobra la c (la b és una presa)");
            comprova (! treuPresa (p, a) && ! treuPresa (p, b), "les que es fan servir no es poden treure");
            comprova (treuPresa (p, c) && p.preses.size() == 2, "la que sobra, sí");
            canviaFitxerPresa (p, a, "Àudios/Veu bona.wav", "Veu bona");
            comprova (presa (p, a)->fitxer == "Àudios/Veu bona.wav" && presa (p, a)->nom == "Veu bona", "canvi de nom");
            Desfes d;
            d.abans (p);
            d.perTots ([&] (Projecte& q) { canviaFitxerPresa (q, a, "Àudios/x.wav"); });
            comprova (d.desfes (p) && presa (p, a)->fitxer == "Àudios/x.wav", "el canvi arriba també a les còpies de desfer");
        });
    }
}
