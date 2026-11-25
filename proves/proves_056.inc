// 0.56: automatització, sidechain, Dinàmica i convolució.
#include "mibase/Automatitzacio.h"
#include "mibase/Convolucio.h"
#include "mibase/Dinamica.h"
namespace
{
    // Efecte de prova: guany = paràmetre «g» (0..1); apunta el darrer valor rebut i la sidechain.
    struct EfecteProva056 : Efecte
    {
        std::atomic<float> g { 1.0f };
        std::vector<float> valors;   // un per crida a processa
        bool sc = false;
        float picSc = 0.0f;
        void prepara (double, int) override {}
        int numParametres() const override { return 1; }
        InfoParametre infoParametre (int) const override { InfoParametre i; i.id = "g"; i.nom = "Guany"; return i; }
        void posaParametre (int, float v) noexcept override { g.store (v); }
        float parametre (int) const noexcept override { return g.load(); }
        bool admetSidechain() const noexcept override { return true; }
        const float* sL = nullptr;
        void posaSidechain (const float* l, const float*) noexcept override { sL = l; }
        void processa (float* L, float* R, int n) noexcept override
        {
            valors.push_back (g.load());
            sc = sL != nullptr;
            picSc = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                if (sL != nullptr) picSc = std::max (picSc, std::abs (sL[i]));
                L[i] *= g.load(); R[i] *= g.load();
            }
        }
    };

    void proves056()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        namespace au = mibase::automatitzacio;
        const double fs = 48000.0;

        auto audio = [&] (float valor, double segons = 4.0)
        {
            auto a = std::make_shared<AudioPresa>();
            a->fs = fs; a->canals = 1; a->longitud = (long long) (fs * segons);
            a->dades[0].assign ((size_t) a->longitud, valor);
            return a;
        };
        struct Sortida { std::vector<float> L, R; };
        auto sona = [&] (const Pla& pla, double segons, int bloc = 256, Mesclador* mx0 = nullptr)
        {
            Mesclador local;
            Mesclador& mx = mx0 != nullptr ? *mx0 : local;
            mx.prepara (fs, 512);
            Sortida s;
            const int n = (int) (fs * segons);
            s.L.assign ((size_t) n, 0.0f); s.R.assign ((size_t) n, 0.0f);
            for (int p = 0; p < n; p += bloc)
                mx.processa (pla, p, std::min (bloc, n - p), s.L.data() + p, s.R.data() + p, false, nullptr, true);
            return s;
        };

        prova ("0.56 automatització: formes de les corbes i valor a cada compàs", []
        {
            std::vector<PuntAuto> v { { 0.0, 0.0, 0, 0.0 }, { 1.0, 1.0, 1, 0.0 }, { 2.0, 0.0, 2, 0.0 }, { 3.0, 1.0, 0, 0.0 } };
            comprova (std::abs (au::valorA (v, 0.5) - 0.5) < 1e-9, "lineal al mig: 0,5");
            comprova (std::abs (au::valorA (v, 1.25) - (1.0 - (0.5 - 0.5 * std::cos (0.25 * 3.14159265358979)))) < 1e-6, "suau (S)");
            comprova (au::valorA (v, 2.9) == 0.0, "esglaó: es manté fins al punt següent");
            comprova (au::valorA (v, -5) == 0.0 && au::valorA (v, 10) == 1.0, "abans i després: el primer i el darrer");
            comprova (au::avanc (0.5, 0, 0.8) > 0.5 && au::avanc (0.5, 0, -0.8) < 0.5, "la tensió corba el tram");
            comprova (au::valorA ({}, 1.0) < 0.0, "sense punts: -1");
            comprova (std::abs (au::dbDeVolum (0.8)) < 1e-9 && au::dbDeVolum (0.0) == -60.0 && std::abs (au::volumDeDb (-6.0) - 0.8 * std::cbrt (std::pow (10.0, -0.3))) < 1e-9,
                      "volum: 0,8 = 0 dB, 0 = silenci");
            comprova (au::textPan (0.5) == "C" && au::textPan (0.0) == "L 100", "text del panorama");
        });

        prova ("0.56 automatització: edició (punts, moure, quantitzar, copiar, enganxar, duplicar, simplificar)", []
        {
            Automatitzacio a;
            au::posaPunt (a, { 2.0, 0.5, 0, 0 });
            au::posaPunt (a, { 0.0, 0.0, 0, 0 });
            au::posaPunt (a, { 4.0, 1.0, 0, 0 });
            comprova (a.punts.size() == 3 && a.punts[1].compas == 2.0, "en ordre");
            au::posaPunt (a, { 2.0, 0.7, 0, 0 });
            comprova (a.punts.size() == 3 && a.punts[1].valor == 0.7, "al mateix lloc, el substitueix");
            au::mou (a, { 1 }, 5.0, 0.1);
            comprova (a.punts[1].compas < 4.0 && a.punts[1].compas > 3.99 && std::abs (a.punts[1].valor - 0.8) < 1e-9, "no passa per sobre del veí");
            au::mou (a, { 1 }, -1.87, 0.0);
            au::quantitza (a, { 1 }, 0.25);
            comprova (std::abs (a.punts[1].compas - 2.25) < 1e-9, "quantitzat a la negra (" + std::to_string (a.punts[1].compas) + ")");
            const auto c = au::copia (a, 1.0, 3.0);
            comprova (c.size() == 3 && c.front().compas == 0.0 && std::abs (c.back().compas - 2.0) < 1e-9, "copia amb les vores");
            Automatitzacio b = a;
            au::enganxa (b, c, 8.0);
            comprova (std::abs (au::valorA (b.punts, 9.25) - au::valorA (a.punts, 2.25)) < 1e-9, "enganxa: la mateixa corba més tard");
            Automatitzacio d = a;
            au::duplica (d, 0.0, 4.0);
            comprova (std::abs (au::valorA (d.punts, 6.25) - au::valorA (a.punts, 2.25)) < 1e-9 && std::abs (au::valorA (d.punts, 1.0) - au::valorA (a.punts, 1.0)) < 1e-9,
                      "duplica just després sense tocar l'original");
            std::vector<PuntAuto> traç;
            for (int i = 0; i <= 100; ++i) traç.push_back ({ i * 0.01, i < 50 ? i * 0.02 : 1.0, 0, 0 });
            const auto sim = au::simplifica (traç);
            comprova (sim.size() <= 4 && sim.size() >= 3, "el dibuix a mà es simplifica (" + std::to_string (sim.size()) + " punts)");
            au::esborra (a, { 0, 2 });
            comprova (a.punts.size() == 1, "esborra");
        });

        prova ("0.56 automatització: es desa i es recupera amb el projecte (també el sidechain)", []
        {
            const auto dir = std::filesystem::temp_directory_path() / "mibase_056_disc";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            Projecte p;
            p.nom = "Prova";
            auto& t = afegeixPista (p, "Veu", "audio");
            auto& a = au::afegeix (p, t, au::objectiuInsert ("i9", "mix"), "Reverb · Mix");
            au::posaPunt (a, { 1.5, 0.25, 1, 0.0 });
            au::posaPunt (a, { 3.0, 0.75, 0, -0.5 });
            afegeixPista (p, "Rev", "audio");
            Insert x { "i9", "intern:reverb", "Reverb (Mibase)", true, "", {} };
            x.sidechain.push_back ({ p.pistes[0].id, -3.0, 2, false });
            p.pistes[1].inserts.push_back (x);
            const auto rc = disc::creaProjecte (dir, p, "2026-11-17T06:00:00", "17/11/2026 06:00");
            comprova (rc.correcte, "desa");
            const auto q = *disc::llegeix (rc.fitxer);
            comprova (q.pistes.size() == 2 && q.pistes[0].autos.size() == 1 && q.pistes[0].autos[0] == p.pistes[0].autos[0], "l'automatització torna igual");
            comprova (q.pistes[1].inserts.size() == 1 && q.pistes[1].inserts[0].sidechain.size() == 1 && q.pistes[1].inserts[0].sidechain[0] == x.sidechain[0],
                      "el sidechain torna igual");
            std::filesystem::remove_all (dir);
        });

        prova ("1.5.2 automatització: el mesclador diu el volum i el panorama d'ara (perquè el fader es mogui)", [&]
        {
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            auto& t = afegeixPista (q, "A", "audio");
            afegeixClip (q, 0, presa, 0.0);
            auto& a = au::afegeix (q, t, au::volum, "Volum");
            au::posaPunt (a, { 0.0, 0.0, 0, 0 });
            au::posaPunt (a, { 1.0, 0.8, 0, 0 });
            afegeixPista (q, "B", "audio");
            MapaAudios m { { presa, audio (0.5f) } };
            auto pla = construeixPla (q, m);
            Mesclador mx;
            sona (*pla, 1.0, 256, &mx);
            const float v = mx.volumAutomatitzat (slotDePista (q.pistes[0]));
            comprova (std::abs (v - 0.4f) < 0.01f, "volum automatitzat a 1 s: " + std::to_string (v));
            comprova (mx.panAutomatitzat (slotDePista (q.pistes[0])) < 0.0f, "sense panorama automatitzat: -1");
            comprova (mx.volumAutomatitzat (slotDePista (q.pistes[1])) < 0.0f, "la pista sense automatització: -1");
        });

        prova ("0.56 automatització: volum i panorama sonen seguint la corba (i en bucle)", [&]
        {
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            auto& t = afegeixPista (q, "A", "audio");
            afegeixClip (q, 0, presa, 0.0);
            auto& a = au::afegeix (q, t, au::volum, "Volum");
            au::posaPunt (a, { 0.0, 0.0, 0, 0 });   // 120 bpm 4/4: 1 compàs = 2 s
            au::posaPunt (a, { 1.0, 0.8, 0, 0 });
            MapaAudios m { { presa, audio (0.5f) } };
            auto pla = construeixPla (q, m);
            auto s = sona (*pla, 2.5);
            const auto g = [&] (double seg) { return s.L[(size_t) (seg * fs)]; };
            comprova (g (0.01) < 0.01f, "comença en silenci");
            comprova (std::abs (g (1.0) - 0.5f * (float) au::guanyDeVolum (0.4)) < 0.004f, "al mig, el volum de la corba (" + std::to_string (g (1.0)) + ")");
            comprova (std::abs (g (2.2) - 0.5f) < 0.002f, "al final, 0 dB");
            float salt = 0.0f;
            for (size_t i = 1; i < (size_t) (2.0 * fs); ++i) salt = std::max (salt, std::abs (s.L[i] - s.L[i - 1]));
            comprova (salt < 0.001f, "sense salts (rampa per bloc)");
            // Panorama
            auto& b = au::afegeix (q, q.pistes[0], au::pan, "Panorama");
            au::posaPunt (b, { 0.0, 0.0, 0, 0 });
            q.pistes[0].autos[0].activa = false;
            pla = construeixPla (q, m);
            s = sona (*pla, 0.5);
            comprova (s.R[10000] < 1e-6f && s.L[10000] > 0.4f, "panorama tot a l'esquerra");
            // Línia apagada: el fader torna a manar.
            q.pistes[0].autos[1].activa = false;
            q.pistes[0].faderDb = -6.0;
            pla = construeixPla (q, m);
            s = sona (*pla, 0.5);
            comprova (std::abs (s.L[10000] - 0.5f * guanyDb (-6.0)) < 1e-4f, "apagada: mana el fader");
        });

        prova ("0.56 automatització: paràmetre d'un efecte, amb trossos de 128 mostres, canvis de tempo i bucle", [&]
        {
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            auto& t = afegeixPista (q, "A", "audio");
            afegeixClip (q, 0, presa, 0.0);
            t.inserts.push_back ({ "i1", "prova", "Prova", true, "", {} });
            auto& a = au::afegeix (q, t, au::objectiuInsert ("i1", "g"), "Prova · Guany");
            au::posaPunt (a, { 0.0, 0.0, 0, 0 });
            au::posaPunt (a, { 1.0, 1.0, 0, 0 });
            auto e = std::make_shared<EfecteProva056>();
            MapaAudios m { { presa, audio (0.5f) } };
            auto pla = construeixPla (q, m, {}, [&] (const Insert&) { return e; });
            comprova (pla->pistes[0].autoParams.size() == 1 && pla->pistes[0].autoInserts, "el pla té el paràmetre");
            Mesclador mx;
            mx.prepara (fs, 512);
            std::vector<float> L (512), R (512);
            mx.processa (*pla, 48000, 512, L.data(), R.data(), false, nullptr, true);   // a 1 s (mig compàs)
            comprova (e->valors.size() == 4, "a trossos de 128 (" + std::to_string (e->valors.size()) + ")");
            comprova (std::abs (e->g.load() - (float) ((48000 + 512) / fs / 2.0)) < 0.002f, "valor del darrer tros");
            // Bucle: tornar enrere torna el valor.
            mx.processa (*pla, 0, 512, L.data(), R.data(), false, nullptr, true);
            comprova (e->g.load() < 0.01f, "en tornar a l'inici del bucle, el valor de l'inici");
            // Tempo més lent: el mateix compàs cau més tard.
            q.tempo = 60.0;
            pla = construeixPla (q, m, {}, [&] (const Insert&) { return e; });
            mx.processa (*pla, 48000 * 2 - 128, 128, L.data(), R.data(), false, nullptr, true);
            comprova (std::abs (e->g.load() - 0.5f) < 0.002f, "a 60 bpm, la meitat del compàs és als 2 s");
            // Paràmetre que no existeix: no es posa.
            q.pistes[0].autos[0].objectiu = au::objectiuInsert ("i1", "res");
            pla = construeixPla (q, m, {}, [&] (const Insert&) { return e; });
            comprova (pla->pistes[0].autoParams.empty(), "un paràmetre desconegut no fa res");
        });

        prova ("0.56 automatització: inserir espai aparta els punts", []
        {
            Projecte q;
            auto& t = afegeixPista (q, "A", "audio");
            auto& a = au::afegeix (q, t, au::volum, "Volum");
            au::posaPunt (a, { 0.5, 0.2, 0, 0 });
            au::posaPunt (a, { 2.0, 0.6, 0, 0 });
            insereixEspai (q, segonsDeCompas (q, 1.0), segonsPerCompas (q) * 2.0);
            comprova (q.pistes[0].autos[0].punts[0].compas == 0.5 && std::abs (q.pistes[0].autos[0].punts[1].compas - 4.0) < 1e-9, "el de després s'aparta dos compassos");
        });

        prova ("0.56 sidechain: la font es calcula abans, arriba sense sonar i es pot apagar", [&]
        {
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            afegeixPista (q, "Destí", "audio");
            afegeixClip (q, 0, presa, 0.0);
            afegeixPista (q, "Font", "audio");     // després a la llista
            afegeixClip (q, 1, presa, 0.0);
            q.pistes[0].inserts.push_back ({ "i1", "prova", "Prova", true, "", {} });
            q.pistes[0].inserts[0].sidechain.push_back ({ q.pistes[1].id, -6.0, 1, true });
            auto e = std::make_shared<EfecteProva056>();
            MapaAudios m { { presa, audio (0.5f) } };
            auto ef = [&] (const Insert&) { return e; };
            auto pla = construeixPla (q, m, {}, ef);
            comprova (pla->numSidechain == 1 && pla->ordre.size() == 2 && pla->ordre[0] == 1, "la font va primer");
            auto s = sona (*pla, 0.1);
            comprova (e->sc && std::abs (e->picSc - 0.5f * guanyDb (-6.0)) < 1e-4f, "arriba amb el nivell de l'enviament");
            comprova (std::abs (s.L[2000] - 1.0f) < 1e-4f, "no es barreja amb el que se sent (" + std::to_string (s.L[2000]) + ")");
            q.pistes[1].mut = true;
            pla = construeixPla (q, m, {}, ef);
            s = sona (*pla, 0.1);
            comprova (e->sc && e->picSc > 0.1f && std::abs (s.L[2000] - 0.5f) < 1e-4f, "la font callada continua alimentant el sidechain (abans del fader)");
            q.pistes[0].inserts[0].sidechain[0].punt = 2;
            pla = construeixPla (q, m, {}, ef);
            sona (*pla, 0.1);
            comprova (e->picSc < 1e-6f, "després del fader, callada no envia res");
            q.pistes[0].inserts[0].sidechain[0].actiu = false;
            pla = construeixPla (q, m, {}, ef);
            sona (*pla, 0.1);
            comprova (! e->sc && pla->numSidechain == 0, "apagat: l'efecte torna al seu so propi");
        });

        prova ("0.56 sidechain: sense cercles", []
        {
            Projecte q;
            afegeixPista (q, "A", "audio");
            afegeixPista (q, "B", "audio");
            const auto bus = afegeixBus (q, "Bus", false).id;
            const auto A = q.pistes[0].id, B = q.pistes[1].id;
            comprova (posaSortida (q, 0, bus), "A va al bus");
            comprova (potSidechain (q, B, A) && potSidechain (q, A, B), "entre dues pistes, sí");
            comprova (! potSidechain (q, A, A), "a ella mateixa, no");
            comprova (! potSidechain (q, bus, A), "el bus on va A no pot controlar A (cercle)");
            q.pistes[0].inserts.push_back ({ "i1", "intern:dinamica", "D", true, "", { { B, 0.0, 1, true } } });
            comprova (! potSidechain (q, A, B), "si B ja controla A, A no pot controlar B");
            comprova (potSidechain (q, A, ""), "al màster, sempre");
        });

        prova ("0.56 Reverb: ducking amb una veu d'una altra pista (i el ducking intern es manté)", [&]
        {
            using namespace mibase::reverb;
            auto prepara = [&] (Reverb& r)
            {
                Params p;
                p.posa (Alg, Hall); p.posa (Mix, 100); p.posa (Decay, 3.0f); p.posa (DuckOn, 1); p.posa (DuckThr, -40); p.posa (DuckAmt, 24);
                p.posa (DuckAtk, 5); p.posa (DuckRel, 100);
                r.posaParams (p);
                r.prepara (fs, 256);
            };
            Reverb r;
            prepara (r);
            // Entrada contínua (la reverb sempre té cua); la «veu» sona de 1 a 2 s.
            const int n = (int) (3.0 * fs);
            std::vector<float> L ((size_t) n), R ((size_t) n), vL ((size_t) n, 0.0f), vR ((size_t) n, 0.0f);
            for (int i = 0; i < n; ++i) { L[(size_t) i] = R[(size_t) i] = 0.02f * std::sin (i * 0.031f); }
            for (int i = (int) fs; i < 2 * (int) fs; ++i) vL[(size_t) i] = vR[(size_t) i] = 0.5f * std::sin (i * 0.05f);
            auto rms = [&] (const std::vector<float>& x, double a, double b)
            {
                double e = 0; for (int i = (int) (a * fs); i < (int) (b * fs); ++i) e += (double) x[(size_t) i] * x[(size_t) i];
                return std::sqrt (e / ((b - a) * fs));
            };
            for (int i = 0; i < n; i += 256)
            {
                r.posaSidechain (vL.data() + i, vR.data() + i);
                r.processa (L.data() + i, R.data() + i, std::min (256, n - i));
            }
            const double abans = rms (L, 0.7, 0.95), durant = rms (L, 1.3, 1.9), despres = rms (L, 2.6, 2.95);
            comprova (durant < abans * 0.25, "mentre la veu sona, la reverb baixa (" + std::to_string (durant / abans) + ")");
            comprova (despres > abans * 0.6, "quan la veu calla, torna");
            comprova (r.mesures().sidechain.load(), "la finestra sap que escolta una altra pista");
            // Sense font: el comportament intern de sempre (l'entrada és fluixa i no arriba al llindar → no baixa tant).
            Reverb r2;
            prepara (r2);
            std::vector<float> L2 ((size_t) n), R2 ((size_t) n);
            for (int i = 0; i < n; ++i) { L2[(size_t) i] = R2[(size_t) i] = 0.02f * std::sin (i * 0.031f); }
            for (int i = 0; i < n; i += 256) { r2.posaSidechain (nullptr, nullptr); r2.processa (L2.data() + i, R2.data() + i, std::min (256, n - i)); }
            comprova (std::abs (rms (L2, 1.3, 1.9) - rms (L2, 0.7, 0.95)) < rms (L2, 0.7, 0.95) * 0.3, "sense font externa, com sempre (no reacciona a la veu)");
            comprova (! r2.mesures().sidechain.load(), "i ho diu");
            // Detector RMS i filtre de greus: un so greu no l'activa.
            Reverb r3;
            prepara (r3);
            r3.posa (DuckHpf, 1000); r3.posa (DuckDet, 1);
            std::vector<float> L3 ((size_t) n), R3 ((size_t) n), gL ((size_t) n, 0.0f);
            for (int i = 0; i < n; ++i) { L3[(size_t) i] = R3[(size_t) i] = 0.02f * std::sin (i * 0.031f); }
            for (int i = (int) fs; i < 2 * (int) fs; ++i) gL[(size_t) i] = 0.5f * std::sin (i * 2.0f * 3.14159f * 60.0f / (float) fs);
            for (int i = 0; i < n; i += 256) { r3.posaSidechain (gL.data() + i, gL.data() + i); r3.processa (L3.data() + i, R3.data() + i, std::min (256, n - i)); }
            comprova (rms (L3, 1.3, 1.9) > rms (L3, 0.7, 0.95) * 0.5, "amb el filtre de greus, un baix de 60 Hz no fa baixar la reverb");
        });

        prova ("0.56 Dinàmica: bateria → compressor del baix, gate, expander i de-esser", [&]
        {
            using namespace mibase::dinamica;
            const int n = (int) (2.0 * fs);
            auto baix = [&] { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = 0.3f * std::sin (i * 2.0f * 3.14159f * 80.0f / (float) fs); return v; };
            std::vector<float> bombo ((size_t) n, 0.0f);
            for (int k = 0; k < 4; ++k)
                for (int i = 0; i < 4000; ++i) bombo[(size_t) (k * 24000 + i)] = 0.9f * std::exp (-i / 1500.0f) * std::sin (i * 0.02f);
            auto pic = [&] (const std::vector<float>& x, int a, int b) { float m = 0; for (int i = a; i < b; ++i) m = std::max (m, std::abs (x[(size_t) i])); return m; };
            Dinamica d;
            d.posa (Llindar, -30); d.posa (Ratio, 10); d.posa (Atac, 1); d.posa (Alliberament, 80);
            d.prepara (fs, 256);
            auto L = baix(), R = baix();
            for (int i = 0; i < n; i += 256) { d.posaSidechain (bombo.data() + i, bombo.data() + i); d.processa (L.data() + i, R.data() + i, std::min (256, n - i)); }
            comprova (pic (L, 24000 + 400, 24000 + 2000) < 0.15f, "quan pica el bombo, el baix baixa (" + std::to_string (pic (L, 24400, 26000)) + ")");
            comprova (pic (L, 24000 + 16000, 24000 + 22000) > 0.27f, "entre cops, el baix torna");
            comprova (d.mesures().sidechain.load() && d.mesures().reduccio.load() >= 0.0f, "mesures");
            // Gate: per sota del llindar, tancat.
            Dinamica g;
            g.posa (ModeP, Gate); g.posa (Llindar, -20); g.posa (Rang, 60); g.posa (Atac, 0.5f); g.posa (Alliberament, 20);
            g.prepara (fs, 256);
            std::vector<float> x ((size_t) n);
            for (int i = 0; i < n; ++i) x[(size_t) i] = (i < n / 2 ? 0.5f : 0.02f) * std::sin (i * 0.05f);
            auto y = x;
            for (int i = 0; i < n; i += 256) { g.posaSidechain (nullptr, nullptr); g.processa (y.data() + i, x.data() + i, std::min (256, n - i)); }
            comprova (pic (y, 10000, 20000) > 0.45f && pic (y, n / 2 + 10000, n) < 0.001f, "gate: obert amb so fort, tancat amb so fluix");
            // Expander: el so fluix baixa, però menys que el gate.
            Dinamica e;
            e.posa (ModeP, Expander); e.posa (Llindar, -20); e.posa (Ratio, 2); e.posa (Rang, 60);
            e.prepara (fs, 256);
            std::vector<float> a ((size_t) n), b ((size_t) n);
            for (int i = 0; i < n; ++i) a[(size_t) i] = b[(size_t) i] = 0.03f * std::sin (i * 0.05f);
            for (int i = 0; i < n; i += 256) e.processa (a.data() + i, b.data() + i, std::min (256, n - i));
            comprova (pic (a, n / 2, n) < 0.03f * 0.5f && pic (a, n / 2, n) > 0.0001f, "expander");
            // De-esser: baixa els aguts i no els greus.
            Dinamica s;
            s.posa (ModeP, DeEsser); s.posa (Llindar, -40); s.posa (Ratio, 8); s.posa (FreqDeEsser, 5000);
            s.prepara (fs, 256);
            std::vector<float> ag ((size_t) n), gr ((size_t) n);
            for (int i = 0; i < n; ++i) { ag[(size_t) i] = 0.3f * std::sin (i * 2.0f * 3.14159f * 8000.0f / (float) fs); gr[(size_t) i] = 0.3f * std::sin (i * 2.0f * 3.14159f * 200.0f / (float) fs); }
            auto ag2 = ag, gr2 = gr;
            for (int i = 0; i < n; i += 256) s.processa (ag.data() + i, ag2.data() + i, std::min (256, n - i));
            Dinamica s2;
            s2.posaEstat (s.estat());
            s2.prepara (fs, 256);
            for (int i = 0; i < n; i += 256) s2.processa (gr.data() + i, gr2.data() + i, std::min (256, n - i));
            comprova (pic (ag, n / 2, n) < 0.15f && pic (gr, n / 2, n) > 0.28f, "de-esser: S fora, greus intactes (" + std::to_string (pic (ag, n / 2, n)) + ", " + std::to_string (pic (gr, n / 2, n)) + ")");
            comprova (s2.valor (ModeP) == DeEsser && s2.valor (FreqDeEsser) == 5000.0f, "l'estat es desa i es recupera");
            comprova (d.indexParametre ("llindar") == Llindar && std::abs (Dinamica::deNormal (Ratio, Dinamica::normal (Ratio, 4.0f)) - 4.0f) < 1e-3f,
                      "paràmetres comuns");
        });

        prova ("0.56 sidechain dins del mesclador: Dinàmica en una pista de baix i la bateria en una altra", [&]
        {
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 4.0).id;
            const auto presaB = afegeixPresa (q, "Àudios/b.wav", "b", 4.0).id;
            afegeixPista (q, "Baix", "audio");
            afegeixClip (q, 0, presa, 0.0);
            afegeixPista (q, "Bateria", "audio");
            afegeixClip (q, 1, presaB, 0.0);
            q.pistes[1].mut = true;
            auto d = std::make_shared<dinamica::Dinamica>();
            d->posa (dinamica::Llindar, -20); d->posa (dinamica::Ratio, 20); d->posa (dinamica::Atac, 0.5f);
            d->prepara (fs, 512);
            q.pistes[0].inserts.push_back ({ "i1", dinamica::idPlugin, dinamica::nomPlugin, true, "", { { q.pistes[1].id, 0.0, 1, true } } });
            auto bat = std::make_shared<AudioPresa>();
            bat->fs = fs; bat->canals = 1; bat->longitud = (long long) (4 * fs);
            bat->dades[0].assign ((size_t) bat->longitud, 0.0f);
            for (int i = (int) fs; i < (int) (1.5 * fs); ++i) bat->dades[0][(size_t) i] = 0.8f;
            MapaAudios m { { presa, audio (0.3f) }, { presaB, bat } };
            auto pla = construeixPla (q, m, {}, [&] (const Insert&) { return d; });
            auto s = sona (*pla, 2.0);
            comprova (s.L[(size_t) (0.5 * fs)] > 0.29f && s.L[(size_t) (1.25 * fs)] < 0.1f, "el baix baixa només quan sona la bateria (callada)");
        });

        // ---------------- Convolució ----------------
        auto directa = [] (const std::vector<float>& x, const std::vector<float>& h)
        {
            std::vector<double> y (x.size(), 0.0);
            for (size_t i = 0; i < x.size(); ++i)
                if (x[i] != 0.0f)
                    for (size_t k = 0; k < h.size() && i + k < y.size(); ++k) y[i + k] += (double) x[i] * h[k];
            return y;
        };

        prova ("0.56 convolució: el resultat és el d'una convolució directa (mono, estèreo i true stereo)", [&]
        {
            unsigned llavor = 7;
            auto rnd = [&] { llavor = llavor * 1664525u + 1013904223u; return (float) ((llavor >> 8) & 0xffff) / 32768.0f - 1.0f; };
            for (int nc : { 1, 2, 4 })
            {
                auto ir = std::make_shared<conv::IR>();
                ir->fs = fs;
                for (int c = 0; c < nc; ++c)
                {
                    std::vector<float> h (12000);
                    for (size_t i = 0; i < h.size(); ++i) h[i] = rnd() * std::exp (-(float) i / 3000.0f);
                    ir->c.push_back (h);
                }
                conv::Convolucio cv;
                conv::Opcions o; o.normalitza = false;
                cv.posaOpcions (o);
                cv.posaIR (ir);
                cv.prepara (fs);
                const int n = 30000;
                std::vector<float> xl ((size_t) n, 0.0f), xr ((size_t) n, 0.0f), yl ((size_t) n), yr ((size_t) n);
                for (int i = 0; i < 3000; ++i) { xl[(size_t) i] = rnd(); xr[(size_t) i] = rnd(); }
                for (int i = 0; i < n; ++i) cv.mostra (xl[(size_t) i], xr[(size_t) i], yl[(size_t) i], yr[(size_t) i]);
                const auto h = conv::transforma (*ir, o, fs);
                std::vector<double> rl, rr;
                if (nc == 1) { rl = directa (xl, h[0]); rr = directa (xr, h[0]); }
                else if (nc == 2) { rl = directa (xl, h[0]); rr = directa (xr, h[1]); }
                else
                {
                    auto a = directa (xl, h[0]), b = directa (xr, h[2]), c = directa (xl, h[1]), d = directa (xr, h[3]);
                    rl.resize (a.size()); rr.resize (a.size());
                    for (size_t i = 0; i < a.size(); ++i) { rl[i] = a[i] + b[i]; rr[i] = c[i] + d[i]; }
                }
                double err = 0.0, max = 0.0;
                for (int i = 64; i < n; ++i)
                {
                    err = std::max ({ err, std::abs (yl[(size_t) i] - rl[(size_t) (i - 64)]), std::abs (yr[(size_t) i] - rr[(size_t) (i - 64)]) });
                    max = std::max (max, std::abs (rl[(size_t) (i - 64)]));
                }
                comprova (max > 1.0 && err < max * 1.0e-4, std::to_string (nc) + " canals: error " + std::to_string (err / max));
            }
        });

        prova ("0.56 convolució: freqüències diferents, longitud, reversa i normalització", [&]
        {
            auto ir = std::make_shared<conv::IR>();
            ir->fs = 44100.0;
            ir->c.push_back (std::vector<float> (44100, 0.0f));
            ir->c[0][0] = 1.0f;
            ir->c[0][22050] = 0.5f;   // un eco a 0,5 s
            conv::Opcions o; o.normalitza = false;
            auto h = conv::transforma (*ir, o, 48000.0);
            size_t pic = 1000;
            for (size_t i = 1000; i < h[0].size(); ++i) if (std::abs (h[0][i]) > std::abs (h[0][pic])) pic = i;
            comprova (h[0].size() == 48000 && std::abs ((double) pic - 24000.0) <= 1, "de 44,1 a 48 kHz: l'eco continua a 0,5 s (" + std::to_string (h[0].size()) + ", " + std::to_string (pic) + ")");
            o.longitud = 0.25;
            h = conv::transforma (*ir, o, 48000.0);
            comprova (h[0].size() == 12000, "longitud 25 %");
            o.longitud = 1.0; o.reversa = true;
            h = conv::transforma (*ir, o, 48000.0);
            comprova (std::abs (h[0].back()) > 0.3f, "reversa: el cop al final");
            o.reversa = false; o.normalitza = true;
            ir->c[0][0] = 8.0f;
            h = conv::transforma (*ir, o, 48000.0);
            double e = 0; for (float x : h[0]) e += (double) x * x;
            comprova (std::abs (std::sqrt (e) - 0.35) < 1e-3, "normalitzada: mateixa energia sigui quin sigui el volum del fitxer");
        });

        prova ("0.56 convolució: canviar d'IR sonant (fosa, sense salts) i CPU", [&]
        {
            auto fesIR = [&] (double seg, float to)
            {
                auto ir = std::make_shared<conv::IR>();
                ir->fs = fs;
                ir->c.assign (2, std::vector<float> ((size_t) (seg * fs)));
                unsigned ll = 3;
                for (auto& c : ir->c)
                    for (size_t i = 0; i < c.size(); ++i) { ll = ll * 1664525u + 1013904223u; c[i] = ((float) ((ll >> 9) & 0xffff) / 32768.0f - 1.0f) * std::exp (-(float) i / (float) (seg * fs * 0.2)) * to; }
                return ir;
            };
            conv::Convolucio cv;
            cv.posaIR (fesIR (4.0, 1.0f));
            cv.prepara (fs);
            const int n = (int) (10.0 * fs);
            float sL, sR, prev = 0.0f, salt = 0.0f;
            bool finit = true;
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; ++i)
            {
                if (i == (int) (3.0 * fs)) { cv.posaIR (fesIR (2.0, 0.5f)); cv.espera(); }
                const float x = 0.2f * std::sin (i * 0.01f);
                cv.mostra (x, x, sL, sR);
                if (! std::isfinite (sL)) finit = false;
                if (i > 64 && i > (int) (2.9 * fs) && i < (int) (3.5 * fs)) salt = std::max (salt, std::abs (sL - prev));
                prev = sL;
            }
            const double t = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("        IR estèreo de 4 s, 10 s d'àudio: %.0f ms (%.2f %% d'un nucli)\n", t * 1000, t * 10);
            comprova (finit, "sense valors estranys");
            comprova (cv.longitudActiva() == (long long) (2.0 * fs), "després del canvi sona la IR nova");
            comprova (salt < 0.2f, "sense salts durant el canvi (" + std::to_string (salt) + ")");
            comprova (t < 2.0, "molt menys que el temps real");
        });

        prova ("0.56 convolució dins la Reverb: carregar, desar, recuperar i avisar si falta; biblioteca de demostració", [&]
        {
            using namespace mibase::reverb;
            const auto dir = std::filesystem::temp_directory_path() / "mibase_056_ir";
            std::filesystem::remove_all (dir);
            conv::posaArrel (dir);
            const int fets = conv::creaBibliotecaDemo (dir);
            const auto llista = conv::llistaBiblioteca();
            std::set<std::string> cats;
            for (auto& e : llista) cats.insert (e.categoria);
            comprova (fets == 17 && llista.size() == 17 && cats.size() == conv::categoriesDemo().size(), "biblioteca: " + std::to_string (fets) + " IR en " + std::to_string (cats.size()) + " categories");
            comprova (conv::creaBibliotecaDemo (dir) == 0, "no es tornen a crear");
            const auto& hall = *std::find_if (llista.begin(), llista.end(), [] (auto& e) { return e.nom == "Sala de concerts"; });
            const auto ref = conv::referencia (hall.cami);
            comprova (ref == "bib:Halls/Sala de concerts.wav" && conv::resol (ref) == hall.cami, "referència relativa a la biblioteca (" + ref + ")");
            // Mono
            {
                EscriptorWav w;
                std::vector<float> m (4800, 0.0f); m[0] = 1.0f; m[2400] = 0.5f;
                w.obre (dir / "mono.wav", 1, 44100.0, 24); w.escriu (m.data(), 4800); w.tanca();
            }
            Reverb r;
            Params p; p.posa (Alg, Convolucio); p.posa (Mix, 100); p.posa (LowCut, 20); p.posa (HighCut, 20000); p.posa (PreDelay, 0);
            r.posaParams (p);
            std::string err;
            comprova (r.carregaIR (conv::referencia (dir / "mono.wav"), err) && r.convolucio().ir()->canals() == 1, "IR mono a 44,1 kHz");
            r.prepara (fs, 256);
            std::vector<float> L (24000, 0.0f), R (24000, 0.0f);
            L[0] = R[0] = 1.0f;
            for (int i = 0; i < 24000; i += 256) r.processa (L.data() + i, R.data() + i, std::min (256, 24000 - i));
            size_t pic = 1500;
            for (size_t i = 1500; i < L.size(); ++i) if (std::abs (L[i]) > std::abs (L[pic])) pic = i;
            const size_t esperat = (size_t) std::lround (2400.0 / 44100.0 * fs) + 65;   // + 64 de latència + 1 del pre-delay mínim
            comprova (std::abs ((long) pic - (long) esperat) <= 3, "l'eco de la IR surt al seu lloc (" + std::to_string (pic) + " / " + std::to_string (esperat) + ")");
            const auto e = r.estat();
            comprova (e.find (";ir=bib:mono.wav") != std::string::npos, "l'estat desa la IR");
            Reverb r2;
            r2.posaEstat (e);
            comprova (r2.convolucio().ir() != nullptr && ! r2.irFalta() && r2.valor (Alg) == Convolucio, "es recupera");
            std::filesystem::remove (dir / "mono.wav");
            Reverb r3;
            r3.posaEstat (e);
            comprova (r3.irFalta() && r3.referenciaIR() == "bib:mono.wav" && r3.estat().find (";ir=bib:mono.wav") != std::string::npos,
                      "si falta el fitxer, avisa i no perd la referència");
            r3.prepara (fs, 256);
            for (int i = 0; i < 24000; i += 256) r3.processa (L.data() + i, R.data() + i, std::min (256, 24000 - i));
            comprova (std::isfinite (L[1000]), "sense IR no peta");
            // Estèreo de la biblioteca i paràmetres comuns
            comprova (r2.carregaIR (ref, err) && r2.convolucio().ir()->canals() == 2, "IR estèreo de la biblioteca");
            const int ix = r2.indexParametre ("mix");
            r2.posaParametre (ix, 0.5f);
            comprova (ix >= 0 && std::abs (r2.valor (Mix) - 50.0f) < 0.01f && ! r2.infoParametre (r2.indexParametre ("alg")).automatitzable, "paràmetres comuns de la Reverb");
            comprova (r2.indexParametre ("vista") < 0 && r2.textParametre (r2.indexParametre ("duckhpf"), 0.0f) == "Off", "la vista no és un paràmetre");
            std::filesystem::remove_all (dir);
            conv::posaArrel ({});
        });
    }
}
