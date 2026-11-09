// 0.28: proves del mapa de tempo (canvis bruscos per parts).
namespace
{
    void provesTempo()
    {
        prova ("tempo per parts: mapa, compassos, segons i ticks", []
        {
            Projecte p;
            p.tempo = 120.0; p.compasNum = 4; p.compasDen = 4;
            Part t { "m1", "Tornada", 4.0, 8.0, 0 };
            t.tempo = 60.0; t.compasNum = 3; t.compasDen = 4;
            comprova (! edicio::teCanvisTempo (p) && std::abs (edicio::segonsDeCompas (p, 5.0) - 10.0) < 1e-9, "sense canvis, com sempre");
            p.parts.push_back (t);
            comprova (edicio::teCanvisTempo (p) && edicio::mapaTempo (p).size() == 3, "tres trams (abans, la tornada, després)");
            comprova (std::abs (edicio::segonsDeCompas (p, 4.0) - 8.0) < 1e-9, "fins a la tornada, a 120");
            comprova (std::abs (edicio::segonsDeCompas (p, 5.0) - 11.0) < 1e-9, "a la tornada, 3 s per compàs (60 bpm, 3/4)");
            comprova (std::abs (edicio::segonsDeCompas (p, 8.0) - 20.0) < 1e-9 && std::abs (edicio::segonsDeCompas (p, 9.0) - 22.0) < 1e-9,
                      "després, torna a 120 i 4/4");
            comprova (std::abs (edicio::compasDeSegons (p, 11.0) - 5.0) < 1e-9 && std::abs (edicio::compasDeSegons (p, 21.0) - 8.5) < 1e-9, "i al revés");
            comprova (std::abs (edicio::enganxa (p, 10.4, Snap::Compas) - 11.0) < 1e-9, "la graella de compàs segueix el mapa");
            comprova (std::abs (edicio::enganxa (p, 12.1, Snap::Temps) - 12.0) < 1e-9, "i la de temps (1 s per temps a la tornada)");
            comprova (std::abs (edicio::tramA (p, 12.0).tempo - 60.0) < 1e-9 && edicio::tramA (p, 12.0).num == 3 && std::abs (edicio::tramA (p, 21.0).tempo - 120.0) < 1e-9,
                      "tempo i compàs en cada moment");
            comprova (std::abs (midi::ticks (p, 8.0) - 15360.0) < 1e-6 && std::abs (midi::ticks (p, 11.0) - 18240.0) < 1e-6
                      && std::abs (midi::segons (p, 18240.0) - 11.0) < 1e-9, "ticks ↔ segons amb el mapa");
            comprova (edicio::textPosicio (p, 11.0) == edicio::textPosicio (p, 11.0) && edicio::compasA (p, 11.5) == 5, "posició i compàs");
            p.bucle.marcat = p.bucle.actiu = true; p.bucle.de = 4; p.bucle.a = 6;
            comprova (std::abs (edicio::iniciBucle (p) - 8.0) < 1e-9 && std::abs (edicio::fiBucle (p) - 14.0) < 1e-9, "el bucle segueix el mapa");
        });

        prova ("tempo per parts: les notes MIDI sonen al seu moment", []
        {
            Projecte p;
            p.tempo = 120.0; p.compasNum = 4;
            Part t { "m1", "Tornada", 4.0, 8.0, 0 };
            t.tempo = 60.0;
            p.parts.push_back (t);
            Pista tr; tr.id = "t1"; tr.tipus = "midi"; tr.canals = 2; tr.instrument = "intern:piano";
            Clip c = midi::clipNou (0.0, 30.0);
            midi::afegeix (c, { 4 * 3840 + 960, 480, 60, 100, 0 });   // compàs 4, temps 2: a 60 bpm, 8 s + 1 s
            midi::afegeix (c, { 9 * 3840, 480, 62, 100, 0 });         // compàs 9: 8 + 16 + 2 = 26 s
            tr.clips = { c };
            p.pistes.push_back (tr);
            auto inst = std::make_shared<InstrumentProva>();
            auto pla = construeixPla (p, {}, [&] (const Pista&) { return inst; });
            const auto& pp = pla->pistes[0];
            comprova (pp.notes.size() == 2 && std::abs (pp.notes[0].ini - 9.0) < 1e-6 && std::abs (pp.notes[0].fi - 9.5) < 1e-6,
                      "la nota de la tornada, a 60 bpm (9 s, dura 0,5 s)");
            comprova (pp.notes.size() == 2 && std::abs (pp.notes[1].ini - 26.0) < 1e-6, "la de després, al seu lloc (26 s)");
            comprova (pla->mapaTempo.size() == 3, "la claqueta té el mapa");
        });
    }
}
