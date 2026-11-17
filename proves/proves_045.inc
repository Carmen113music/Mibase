// 0.45: 100 patrons nous (arpegis i folk modern) i el màstil de la guitarra virtual.
namespace
{
    void proves045()
    {
        using namespace mibase;
        prova ("patrons: 50 arpegis de fingerpicking i 50 ritmes de folk modern", []
        {
            int arpegis = 0, folk = 0, dolents = 0;
            std::string quins;
            std::set<std::string> ids, passos;
            for (auto& p : guitarra::patrons())
            {
                ids.insert (p.id);
                const bool f = p.grup == "Fingerpicking: arpegis", k = p.grup == "Folk modern";
                if (! f && ! k) continue;
                (f ? arpegis : folk)++;
                passos.insert (p.compas + "|" + std::to_string (p.pasTicks) + "|" + guitarra::textPassos (p.passos));
                const auto notes = guitarra::rasgueja (guitarra::notes (harmonia::llegeixAcord ("Am")), p, p.pasTicks * (long long) p.passos.size(), {});
                if (notes.empty() || p.passos.empty() || p.dibuix.empty()) { ++dolents; quins += " " + p.id; }
                if (f) for (auto& s : p.passos) if (s.cop != guitarra::Cop::Res && s.dit == guitarra::Dit::Cap) { ++dolents; quins += " " + p.id + "(sense dit)"; break; }
            }
            comprova (arpegis == 50 && folk == 50, "n'hi ha 50 i 50 (" + std::to_string (arpegis) + ", " + std::to_string (folk) + ")");
            comprova (dolents == 0, "tots es llegeixen i fan notes" + quins);
            comprova ((int) passos.size() == 100, "no n'hi ha cap de repetit (" + std::to_string (passos.size()) + ")");
            comprova (ids.size() == guitarra::patrons().size(), "cap id repetit a tota la llista");
            comprova (guitarra::patro ("folk") != nullptr && guitarra::patro ("c50") != nullptr, "els d'abans continuen amb el seu id");
        });

        prova ("màstil: on es toquen les notes que sonen", []
        {
            auto text = [] (const std::vector<guitarra::LlocMastil>& v)
            {
                std::string s (6, 'x');
                for (auto& l : v) s[(size_t) l.corda] = (char) (l.trast < 10 ? '0' + l.trast : 'a' + l.trast - 10);
                return s;
            };
            comprova (text (guitarra::alMastil ({ 40, 47, 52, 55, 59, 64 })) == "022000", "Mi menor obert (" + text (guitarra::alMastil ({ 40, 47, 52, 55, 59, 64 })) + ")");
            comprova (text (guitarra::alMastil ({ 45, 52, 57, 60, 64 })) == "x02210", "La menor obert (" + text (guitarra::alMastil ({ 45, 52, 57, 60, 64 })) + ")");
            comprova (text (guitarra::alMastil ({ 48, 52, 55, 60, 64 })) == "x32010", "Do obert (" + text (guitarra::alMastil ({ 48, 52, 55, 60, 64 })) + ")");
            comprova (text (guitarra::alMastil ({ 43, 47, 50, 55, 59, 67 })) == "320003", "Sol obert (" + text (guitarra::alMastil ({ 43, 47, 50, 55, 59, 67 })) + ")");
            comprova (text (guitarra::alMastil ({ 41, 48, 53, 57, 60, 65 })) == "133211", "Fa amb celleta (" + text (guitarra::alMastil ({ 41, 48, 53, 57, 60, 65 })) + ")");
            comprova (text (guitarra::alMastil ({ 64 })) == "xxxxx0" && text (guitarra::alMastil ({ 69 })) == "xxxxx5", "una nota sola");
            comprova (guitarra::alMastil ({}).empty(), "sense notes, res");
            const auto greu = guitarra::alMastil ({ 28 });
            comprova (greu.size() == 1 && greu[0].corda == 0 && greu[0].trast == 0, "una nota massa greu puja d'octava");
            bool be = true;
            for (const char* ac : { "C", "D", "E", "G", "A", "Am", "Em", "Dm", "F", "Bm", "B7", "Cmaj7", "F#m", "Bb", "C#m7" })
                for (auto& l : guitarra::alMastil (guitarra::notes (harmonia::llegeixAcord (ac))))
                    be = be && l.corda >= 0 && l.corda < 6 && l.trast >= 0 && l.trast <= 19;
            comprova (be, "tots els acords cauen dins del màstil");
            const auto molts = guitarra::alMastil ({ 60, 61, 62, 63, 64, 65, 66, 67 });
            comprova (molts.size() == 8, "amb més notes que cordes, cada una al seu lloc (sense penjar-se)");
        });
    }
}
