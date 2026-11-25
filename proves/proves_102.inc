// 1.2: Generate Master Suggestion (cadena mínima i justificada) i els passos del mode guiat.
namespace
{
    void proves102()
    {
        using namespace mibase;
        using namespace mibase::rack;
        using namespace mibase::diagnosi;
        prova ("1.2 Suggerència: una mescla equilibrada i aixafada surt amb poca cosa i sense compressor", []
        {
            std::array<float, numBandes> z {};
            auto m = mescla100 (z);
            auto a = analitza (m.L.data(), m.R.data(), m.L.size(), 48000);
            a.loud.integrated = -12.0f; a.loud.truePeakMax = -6.0f;   // pics a 6 dB del volum: ja ve comprimida
            const auto s = suggereixMaster (a, 0, {}, 0);   // Natural
            Rack r (Tipus::Master);
            r.posaEstat (s.estat);
            bool comp = false, eq = false;
            for (int i = 0; i < r.numModuls(); ++i) { comp = comp || r.modul (i)->clau == "glue"; eq = eq || r.modul (i)->clau == "eqm"; }
            comprova (! comp && ! eq, "ni compressor ni EQ");
            comprova (r.numModuls() <= 3 && r.modul (r.numModuls() - 1)->clau == "limitm", "cadena curta, limitador al final (" + std::to_string (r.numModuls()) + " mòduls)");
            bool diu = false; for (auto& f : s.fora) diu = diu || f.find ("ja ve comprimida") != std::string::npos;
            comprova (diu && ! s.moduls.empty() && ! s.resum.empty(), "explica per què no hi ha compressor");
        });

        prova ("1.2 Suggerència: corregeix el fang que ha mesurat, segons l'estil, i el guany depèn del mix", []
        {
            std::array<float, numBandes> fang {};
            fang[9] = fang[10] = 8.0f;
            auto m = mescla100 (fang);
            auto a = analitza (m.L.data(), m.R.data(), m.L.size(), 48000);
            a.loud.integrated = -20.0f; a.loud.truePeakMax = -9.0f;   // pics a 11 dB: amb marge, però amb poc cop
            const auto s = suggereixMaster (a, 7, {}, 0);   // Indie Rock
            Rack r (Tipus::Master);
            r.posaEstat (s.estat);
            int ie = -1, il = -1; bool sat = false, tr = false;
            for (int i = 0; i < r.numModuls(); ++i)
            {
                const auto& k = r.modul (i)->clau;
                if (k == "eqm") ie = i; if (k == "limitm") il = i; sat = sat || k == "colorm"; tr = tr || k == "transitoris";
            }
            comprova (ie >= 0, "hi ha EQ");
            auto* q = dynamic_cast<eq::Equalitzador*> (r.modul (ie)->fx.get());
            comprova (q->banda (3).guany <= -2.0f && q->banda (3).freq >= 240.0f && q->banda (3).freq <= 330.0f, "talla el fang on l'ha trobat (" + std::to_string (q->banda (3).freq) + " Hz, " + std::to_string (q->banda (3).guany) + " dB)");
            comprova (sat && tr, "indie rock: cinta i transient shaper");
            comprova (r.objectiu() == 4 && guanyLimitador (r) > 6.0f, "perfil Indie Rock / Pop i guany alt per a un mix a -20 LUFS (" + std::to_string (guanyLimitador (r)) + ")");
            const auto t = suggereixMaster (a, 7, {}, 3);
            Rack r2 (Tipus::Master); r2.posaEstat (t.estat);
            comprova (r2.objectiu() == 3, "respecta el perfil que ja tenies triat");
            comprova (estilsSuggeriment().size() == 13, "13 estils");
        });

        prova ("1.2 Mode guiat: cada troballa sap a quin pas va", []
        {
            std::array<float, numBandes> fang {};
            fang[9] = fang[10] = 8.0f;
            auto m = mescla100 (fang);
            for (size_t i = 0; i < m.L.size(); ++i) { const float v = 0.05f * (float) std::sin (2.0 * 3.14159265358979 * 60.0 * (double) i / 48000.0); m.L[i] += v; m.R[i] -= v; }
            auto a = analitza (m.L.data(), m.R.data(), m.L.size(), 48000);
            Analisi ms = a; ms.loud.integrated = -6.0f; ms.loud.truePeakMax = 0.5f;
            Context ctx; ctx.lufsObjectiu = -14.0f; ctx.truePeakObjectiu = -1.0f;
            const auto inf = diagnostica (a, &ms, {}, ctx);
            auto pasDe = [&] (const std::string& t) { for (auto& x : inf.troballes) if (x.titol == t) return x.pas; return -1; };
            comprova (pasDe ("Acumulació de greus-mitjans (fang)") == 2 && pasDe ("Greus als costats") == 5 && pasDe ("El màster és més fort que el perfil") == 6, "to 2, estèreo 5, loudness 6");
            bool delM = false; for (auto& x : inf.troballes) delM = delM || (x.titol == "True peak per sobre del perfil" && x.delMaster);
            comprova (delM, "el true peak del màster es torna a mirar a la comprovació final");
        });
    }
}
