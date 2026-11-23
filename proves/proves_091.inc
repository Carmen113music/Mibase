// 0.91: una pista MIDI buida que segueix els acords toca l'acord de cada moment.
namespace
{
    void proves091()
    {
        using namespace mibase;
        prova ("0.91 pista MIDI sense notes que segueix els acords: toca l'acord de la pista d'Acords", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Pad", "midi");
            afegeixPista (q, "Acords", "acords");
            const int M = esAcords (q.pistes[0]) ? 1 : 0, A = 1 - M;
            auto acord = [&] (const char* nom, double a, double b) { Clip k; k.acord = nom; k.pos = a; k.inici = 0.0; k.fi = b - a; q.pistes[(size_t) A].clips.push_back (k); };
            acord ("Am", 0.0, 4.0);
            acord ("F", 4.0, 8.0);
            auto& t = q.pistes[(size_t) M];
            comprova (! tocaAcordsSols (t) && notesAcordsSols (q, t).empty(), "sense «segueix els acords», res");
            t.segueixAcords = true;
            auto n = notesAcordsSols (q, t);
            comprova (tocaAcordsSols (t) && n.size() == 6, "dos acords de 3 notes (" + std::to_string (n.size()) + ")");
            std::set<int> a1, a2;
            for (auto& x : n) (x.ini < 1.0 ? a1 : a2).insert (x.to % 12);
            comprova (a1 == std::set<int> { 9, 0, 4 } && a2 == std::set<int> { 5, 9, 0 }, "La menor i després Fa");
            bool temps = true;
            for (auto& x : n) temps = temps && (x.ini < 1.0 ? (x.ini == 0.0 && x.fi > 3.9 && x.fi < 4.0) : (x.ini == 4.0 && x.fi > 7.9));
            comprova (temps, "cada acord dura tot el seu tros");
            // un clip buit no canvia res; amb una nota escrita, ja no toca sol
            Clip c; c.midi = true; c.pos = 0.0; c.inici = 0.0; c.fi = 8.0;
            t.clips.push_back (c);
            comprova (tocaAcordsSols (t), "un clip buit: encara toca l'acord");
            t.clips[0].notes.push_back ({ 0, 480, 60, 100, 0 });
            comprova (! tocaAcordsSols (t), "amb notes al piano roll, sonen les notes");
        });
    }
}
