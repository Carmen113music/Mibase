// 0.43: guitarres en SoundFont (un so concret d'un .sf2). Amb MIBASE_SF2_GUITARRA = camí d'un .sf2, també se'n prova el so.
namespace
{
    void proves043()
    {
        using namespace mibase;
        prova ("sf2: l'instrument «sf2:fitxer|banc|programa|nom»", []
        {
            gm::RefSf2 r;
            comprova (gm::llegeixRef ("sf2:Jaguar.sf2|8|27|Jaguar · chorus", r) && r.fitxer == "Jaguar.sf2" && r.banc == 8 && r.programa == 27 && r.nom == "Jaguar · chorus", "es llegeix");
            comprova (gm::instrumentSf2 (r) == "sf2:Jaguar.sf2|8|27|Jaguar · chorus", "i es torna a escriure igual");
            comprova (gm::llegeixRef ("sf2:A.sf2|0|25|Nom | amb barra", r) && r.nom == "Nom | amb barra", "el nom pot portar una barra");
            comprova (! gm::llegeixRef ("gm:12", r) && ! gm::llegeixRef ("intern:piano", r) && ! gm::llegeixRef ("sf2:", r) && gm::programa ("sf2:A.sf2|0|25|x") < 0, "els altres instruments no ho són");
            comprova (midi::nomInstrument ("sf2:A.sf2|0|25|Seagull · 12Str") == "Seagull · 12Str", "es mostra amb el seu nom");
            comprova (gm::presetsDeFitxer ("/no/hi/es.sf2").empty(), "un fitxer que no hi és no té sons");
        });

        const char* cami = std::getenv ("MIBASE_SF2_GUITARRA");
        if (cami == nullptr) { std::printf ("  (sense MIBASE_SF2_GUITARRA: no provo el so de les guitarres)\n"); return; }
        prova ("sf2: els sons d'un fitxer de guitarra, i com sonen", [cami]
        {
            const auto ps = gm::presetsDeFitxer (cami);
            comprova (ps.size() >= 5, "la llista de sons es llegeix sense carregar el fitxer (" + std::to_string (ps.size()) + ")");
            auto banc = gm::llegeix (cami);
            comprova (banc != nullptr && gm::numPresets (*banc) == (int) ps.size(), "coincideix amb el que diu el banc carregat");
            if (banc == nullptr || ps.size() < 2) return;
            auto toca = [&] (int programa, int bm, int vel)
            {
                gm::SinteGM s (banc, programa, bm);
                s.prepara (48000.0, 512);
                std::vector<float> L (48000, 0.0f), R (48000, 0.0f);
                const EventInstrument on { 0, 0x90, 64, (std::uint8_t) vel };
                s.processa (&on, 1, L.data(), R.data(), 512);
                for (int p = 512; p + 512 <= 24000; p += 512) s.processa (nullptr, 0, L.data() + p, R.data() + p, 512);
                double e = 0.0;
                for (int i = 0; i < 24000; ++i) e += (double) L[(size_t) i] * L[(size_t) i];
                return std::pair<double, std::vector<float>> (e, L);
            };
            const auto a = toca (ps[0].programa, ps[0].banc, 100), b = toca (ps[1].programa, ps[1].banc, 100), fluix = toca (ps[0].programa, ps[0].banc, 30);
            comprova (a.first > 1.0e-3 && b.first > 1.0e-3, "els dos primers sons sonen");
            comprova (a.second != b.second, "i són diferents");
            comprova (fluix.first < a.first * 0.6, "tocat fluix sona més fluix");
            // Un so d'un altre banc MIDI (els «-chorus», «12Str»… van al banc 8, 32…)
            for (auto& p : ps)
                if (p.banc > 0 && p.banc < 127)
                {
                    comprova (toca (p.programa, p.banc, 100).first > 1.0e-3, "un so d'un altre banc també sona (" + p.nom + ")");
                    break;
                }
        });
    }
}
