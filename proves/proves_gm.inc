// 0.31: proves de l'instrument General MIDI (amb un .sf2 si n'hi ha: variable MIBASE_SF2).
namespace
{
    void provesGeneralMidi()
    {
        prova ("general midi: noms i identificadors", []
        {
            comprova (gm::programa ("gm:0") == 0 && gm::programa ("gm:127") == 127 && gm::programa ("gm:128") == -1
                      && gm::programa ("intern:piano") == -1 && gm::programa ("gm:") == -1 && gm::programa ("gm:2a") == -1, "llegeix «gm:N»");
            comprova (gm::instrument (25) == "gm:25" && gm::programa (gm::instrument (25)) == 25, "anada i tornada");
            comprova (std::string (gm::nomPrograma (0)) == "Piano de cua" && std::string (gm::nomFamilia (3)) == "Guitarres", "noms en català");
            // Sense banc: silenci, i no peta.
            gm::SinteGM s (nullptr, 0);
            s.prepara (48000.0, 256);
            std::vector<float> L (256, 1.0f), R (256, 1.0f);
            EventInstrument on { 0, 0x90, 60, 100 };
            s.processa (&on, 1, L.data(), R.data(), 256);
            comprova (std::all_of (L.begin(), L.end(), [] (float v) { return v == 0.0f; }) && ! s.fentSo(), "sense banc, silenci");
        });

        const char* cami = std::getenv ("MIBASE_SF2");
        if (cami == nullptr) { std::printf ("  (sense MIBASE_SF2: no provo el so General MIDI)\n"); return; }
        prova ("general midi: sona amb el banc", [cami]
        {
            auto banc = gm::llegeix (cami);
            comprova (banc != nullptr && gm::numPresets (*banc) >= 128, "llegeix el .sf2");
            if (banc == nullptr) return;
            for (int prog : { 0, 25, 33, 48 })
            {
                gm::SinteGM s (banc, prog);
                s.prepara (48000.0, 512);
                std::vector<float> L (512), R (512);
                float pic = 0.0f;
                EventInstrument on { 100, 0x90, 60, 110 };
                s.processa (&on, 1, L.data(), R.data(), 512);
                for (int i = 0; i < 100; ++i) pic = std::max (pic, std::abs (L[(size_t) i]));
                comprova (pic == 0.0f, "abans de la nota, silenci (programa " + std::to_string (prog) + ")");
                for (int b = 0; b < 20; ++b)
                {
                    s.processa (nullptr, 0, L.data(), R.data(), 512);
                    for (float v : L) pic = std::max (pic, std::abs (v));
                }
                comprova (pic > 0.02f && pic < 1.0f, "sona sense saturar (programa " + std::to_string (prog) + ", pic " + std::to_string (pic) + ")");
                EventInstrument off { 0, 0x80, 60, 0 };
                s.processa (&off, 1, L.data(), R.data(), 512);
                int blocs = 0;
                float cua = 0.0f;
                for (; blocs < 2000 && s.fentSo(); ++blocs) s.processa (nullptr, 0, L.data(), R.data(), 512);
                for (float v : L) cua = std::max (cua, std::abs (v));
                std::printf ("  programa %d: s'apaga en %.1f s (cua %.5f)\n", prog, blocs * 512 / 48000.0, cua);
                comprova (cua < 0.001f, "la nota s'apaga (programa " + std::to_string (prog) + ")");
            }
            // Moltes notes alhora: més que les veus reservades, sense problemes.
            gm::SinteGM s (banc, 48);
            s.prepara (48000.0, 256);
            std::vector<EventInstrument> ev;
            for (int k = 0; k < 100; ++k) ev.push_back ({ 0, 0x90, (std::uint8_t) (30 + k % 60), 90 });
            std::vector<float> L (256), R (256);
            s.processa (ev.data(), (int) ev.size(), L.data(), R.data(), 256);
            comprova (std::all_of (L.begin(), L.end(), [] (float v) { return std::isfinite (v); }), "100 notes alhora: so vàlid");
        });
    }
}
