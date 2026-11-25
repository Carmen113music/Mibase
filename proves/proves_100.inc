// 1.00: ANALYZE del Mastering Studio (anàlisi, diagnòstic, culpables, aplicar) i renderitzaPla.
#include "mibase/Diagnosi.h"
namespace
{
    /** Una «mescla» feta d'un sinus a cada terç d'octava amb la forma de la corba de referència (+ extres en dB per banda). */
    Estereo094 mescla100 (const std::array<float, mibase::diagnosi::numBandes>& extra, double segons = 30.0, double nivell = -30.0, bool nomesExtra = false)
    {
        using namespace mibase::diagnosi;
        Estereo094 s;
        const double fs = 48000.0;
        const size_t n = (size_t) (fs * segons);
        s.L.assign (n, 0.0f); s.R.assign (n, 0.0f);
        for (int b = 0; b < numBandes; ++b)
        {
            if (nomesExtra && extra[(size_t) b] == 0.0f) continue;
            const double a = std::pow (10.0, (nivell + referencia (b) + (nomesExtra ? 0.0f : extra[(size_t) b]) + (nomesExtra ? extra[(size_t) b] : 0.0f)) / 20.0);
            const double w = 2.0 * 3.14159265358979 * freqBanda (b) / fs, ph = 0.7 * b;
            for (size_t i = 0; i < n; ++i) { const float v = (float) (a * std::sin (w * (double) i + ph)); s.L[i] += v; s.R[i] += v; }
        }
        return s;
    }
    bool te100 (const mibase::diagnosi::Informe& inf, const std::string& titol)
    {
        for (auto& t : inf.troballes) if (t.titol == titol) return true;
        return false;
    }
    const mibase::diagnosi::Troballa* troba100 (const mibase::diagnosi::Informe& inf, const std::string& titol)
    {
        for (auto& t : inf.troballes) if (t.titol == titol) return &t;
        return nullptr;
    }

    void proves100()
    {
        using namespace mibase;
        using namespace mibase::diagnosi;
        prova ("1.00 analitza: una mescla amb la forma de referència no té problemes de to", []
        {
            std::array<float, numBandes> z {};
            auto s = mescla100 (z);
            const auto a = analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            comprova (a.valid && std::abs (a.segons - 30.0) < 0.01, "vàlida, 30 s");
            const auto c = a.corba();
            // per sota de 63 Hz les bandes són tan estretes (9 Hz) que un sinus pur s'escampa a la del costat: allà, més marge
            float pitjor = 0, pitjorGreus = 0;
            for (int b = 0; b < numBandes; ++b) (b < 3 ? pitjorGreus : pitjor) = std::max (b < 3 ? pitjorGreus : pitjor, std::abs (c[(size_t) b] - referencia (b)));
            comprova (pitjor < 1.0f && pitjorGreus < 2.5f, "la corba mesurada segueix la referència (±" + std::to_string (pitjor) + " dB; subgreus ±" + std::to_string (pitjorGreus) + ")");
            const auto inf = diagnostica (a, nullptr, {}, {});
            bool to = false;
            for (auto& t : inf.troballes) to = to || t.titol.find ("fang") != std::string::npos || t.titol == "Massa greus" || t.titol == "Duresa" || t.titol == "Falta presència";
            comprova (! to, "cap problema de to");
            comprova (a.sideMidDb < -50.0f && te100 (inf, "Mescla estreta"), "mono: ho veu com a preferència (mescla estreta)");
        });

        prova ("1.00 diagnostica: fang a 250–315 Hz, amb el canvi calculat i la pista que en té la culpa", []
        {
            std::array<float, numBandes> z {}, fang {};
            fang[9] = 8.0f; fang[10] = 8.0f;
            auto s = mescla100 (fang);
            const auto a = analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            // dues «pistes»: la guitarra té el fang; la resta, la forma normal
            auto g = mescla100 (fang, 30.0, -30.0, true);
            auto resta = mescla100 (z);
            std::vector<std::pair<std::string, Analisi>> pistes {
                { "Guitarra 2", analitza (g.L.data(), g.R.data(), g.L.size(), 48000) },
                { "Resta", analitza (resta.L.data(), resta.R.data(), resta.L.size(), 48000) } };
            const auto inf = diagnostica (a, nullptr, pistes, {});
            const auto* t = troba100 (inf, "Acumulació de greus-mitjans (fang)");
            comprova (t != nullptr && t->mena == Mena::Problema, "el troba com a problema");
            comprova (t != nullptr && t->accio.tipus == Accio::EqCampana && t->accio.freq >= 240.0f && t->accio.freq <= 330.0f && t->accio.guany <= -2.0f && t->accio.guany >= -3.5f,
                      "proposa una campana a 250–315 Hz de -2 a -3,5 dB (" + (t ? std::to_string (t->accio.freq) + " Hz " + std::to_string (t->accio.guany) : std::string ("—")) + ")");
            comprova (t != nullptr && ! t->culpables.empty() && t->culpables.front().pista == "Guitarra 2" && t->deMescla, "culpable: Guitarra 2, i és cosa de la mescla");
            comprova (inf.enginyer.find ("Guitarra 2") != std::string::npos && inf.pistesAnalitzades == 2, "l'enginyer diu de tornar a la mescla");
            comprova (inf.confianca == Confianca::Mitjana, "30 s: confiança mitjana");
        });

        prova ("1.00 diagnostica: greus als costats, mescla aixafada i loudness del màster", []
        {
            std::array<float, numBandes> z {};
            auto s = mescla100 (z);
            for (size_t i = 0; i < s.L.size(); ++i) { const float v = 0.05f * (float) std::sin (2.0 * 3.14159265358979 * 60.0 * (double) i / 48000.0); s.L[i] += v; s.R[i] -= v; }
            const auto a = analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            const auto inf = diagnostica (a, nullptr, {}, {});
            const auto* t = troba100 (inf, "Greus als costats");
            comprova (t != nullptr && t->accio.tipus == Accio::Mono, "greus oberts: proposa Mono");
            // un màster a uns -8 LUFS amb el perfil a -14
            Analisi m = a;
            m.loud.integrated = -8.0f; m.loud.truePeakMax = -0.2f;
            Context ctx; ctx.lufsObjectiu = -14.0f; ctx.truePeakObjectiu = -1.0f; ctx.perfil = "Streaming Standard";
            const auto i2 = diagnostica (a, &m, {}, ctx);
            const auto* l = troba100 (i2, "El màster és més fort que el perfil");
            comprova (l != nullptr && l->accio.tipus == Accio::Loudness && std::abs (l->accio.guany + 6.0f) < 0.01f, "proposa baixar 6 dB el limitador");
            comprova (te100 (i2, "True peak per sobre del perfil"), "i avisa del true peak");
            Analisi x = a;
            x.loud.truePeakMax = -3.0f; x.loud.integrated = -8.0f;
            comprova (te100 (diagnostica (x, nullptr, {}, {}), "La mescla ja ve molt comprimida"), "PLR de 5 dB: mescla aixafada");
            comprova (inf.troballes.front().mena == Mena::Problema, "els problemes surten primer");
        });

        prova ("1.00 aplica: EQ, de-esser, mono i loudness al Mastering Studio", []
        {
            using namespace mibase::rack;
            Rack r (Tipus::Master);
            r.posaEstat ({});   // «Màster net»
            comprova (aplica (r, { Accio::EqCampana, 300.0f, -2.0f, 0.9f }), "campana aplicada");
            int ie = -1;
            for (int i = 0; i < r.numModuls(); ++i) if (r.modul (i)->clau == "eqm") ie = i;
            auto* q = dynamic_cast<eq::Equalitzador*> (r.modul (ie)->fx.get());
            bool campana = false;
            for (int k = 0; k < eq::maxBandes; ++k) { const auto b = q->banda (k); campana = campana || (b.usada && b.tipus == eq::Bell && std::abs (b.freq - 300.0f) < 1.0f && std::abs (b.guany + 2.0f) < 0.01f); }
            comprova (campana, "hi ha una campana de -2 dB a 300 Hz");
            const float abans = guanyLimitador (r);
            comprova (aplica (r, { Accio::Loudness, 0.0f, -2.0f }) && std::abs (guanyLimitador (r) - (abans - 2.0f)) < 0.05f, "loudness: -2 dB al limitador");
            const int n0 = r.numModuls();
            comprova (aplica (r, { Accio::DeEsser, 7000.0f }) && r.numModuls() == n0 + 1 && r.modul (ie + 1)->clau == "deessm", "de-esser afegit just després de l'EQ");
            comprova (aplica (r, { Accio::Mono, 120.0f }), "mono");
            comprova (r.modul (r.numModuls() - 1)->clau == "limitm", "el limitador continua l'últim");
            Rack buit (Tipus::Master);
            comprova (aplica (buit, { Accio::Mono, 120.0f }) && buit.numModuls() == 1 && buit.modul (0)->clau == "lowend", "rack buit: afegeix el Low End Focus");
            comprova (! aplica (buit, {}), "«cap acció» no fa res");
        });

        prova ("1.00 renderitzaPla: la mescla a memòria, seguint l'estructura", []
        {
            using namespace mibase::edicio;
            const double fs = 48000.0;
            Projecte p;
            p.clic = false;
            afegeixPista (p, "Veu", "audio");
            afegeixGravacio (p, 0, "Àudios/a.wav", "a", 4.0, 0.0, 0.0, 0.0);
            auto au = std::make_shared<AudioPresa>();
            au->fs = fs; au->canals = 1; au->longitud = (long long) (4 * fs);
            au->dades[0].assign ((size_t) au->longitud, 0.25f);
            MapaAudios mapa { { p.pistes[0].clips[0].presa, au } };
            auto pla = construeixPla (p, mapa);
            std::vector<float> L, R;
            renderitzaPla (*pla, fs, 0.0, 2.0, L, R);
            comprova (L.size() == (size_t) (2 * fs) && R.size() == L.size() && std::abs (L[48000] - 0.25f) < 1e-4f, "2 s, el so hi és");
            pla->ruta = { { 0.0, 1.0, {} }, { 2.0, 2.5, {} } };
            renderitzaPla (*pla, fs, 0.0, 99.0, L, R);
            comprova (L.size() == (size_t) (1.5 * fs), "amb estructura: la suma dels trams");
        });
    }
}
