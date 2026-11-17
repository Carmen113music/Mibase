// 0.55: la Reverb de Mibase (efecte intern).
namespace
{
    namespace rvb
    {
        using namespace mibase::reverb;

        struct So { std::vector<float> L, R; };

        So silenci (double fs, double segons) { So s; s.L.assign ((size_t) (fs * segons), 0.0f); s.R = s.L; return s; }

        void passa (Reverb& r, So& s, int bloc)
        {
            const int n = (int) s.L.size();
            for (int i = 0; i < n; i += bloc)
                r.processa (s.L.data() + i, s.R.data() + i, std::min (bloc, n - i));
        }

        // Resposta a un impuls només de la reverb (Mix 100 %).
        So impuls (Params p, double fs, double segons, int bloc = 256)
        {
            p.posa (Mix, 100);
            Reverb r;
            r.posaParams (p);
            r.prepara (fs, 512);
            So s = silenci (fs, segons);
            s.L[100] = s.R[100] = 1.0f;
            passa (r, s, bloc);
            return s;
        }

        double energia (const So& s, double fs, double t0, double t1)
        {
            double e = 0.0;
            const size_t a = (size_t) (t0 * fs), b = std::min (s.L.size(), (size_t) (t1 * fs));
            for (size_t i = a; i < b; ++i) e += (double) s.L[i] * s.L[i] + (double) s.R[i] * s.R[i];
            return e / std::max<size_t> (1, b - a);
        }

        bool finit (const So& s, float maxim = 8.0f)
        {
            for (size_t i = 0; i < s.L.size(); ++i)
                if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i]) || std::fabs (s.L[i]) > maxim || std::fabs (s.R[i]) > maxim) return false;
            return true;
        }

        // RT60 mesurat amb la corba d'Schroeder (de -5 a -25 dB, extrapolat).
        double rt60 (const So& s, double fs)
        {
            std::vector<double> e (s.L.size());
            double acc = 0.0;
            for (size_t i = s.L.size(); i-- > 0;) { acc += (double) s.L[i] * s.L[i] + (double) s.R[i] * s.R[i]; e[i] = acc; }
            const double tot = e[0];
            double t5 = -1, t25 = -1;
            for (size_t i = 0; i < e.size(); ++i)
            {
                const double db = 10.0 * std::log10 (std::max (1e-30, e[i] / tot));
                if (t5 < 0 && db <= -5.0) t5 = (double) i / fs;
                if (t25 < 0 && db <= -25.0) { t25 = (double) i / fs; break; }
            }
            return t5 >= 0 && t25 > t5 ? (t25 - t5) * 3.0 : -1.0;
        }

        // Energia d'una banda (Goertzel aproximat per finestres) entre t0 i t1.
        double banda (const std::vector<float>& x, double fs, double f, double t0, double t1)
        {
            const size_t a = (size_t) (t0 * fs), b = std::min (x.size(), (size_t) (t1 * fs));
            const double w = 2.0 * 3.14159265358979 * f / fs, c = 2.0 * std::cos (w);
            double total = 0.0;
            const size_t fin = 2048;
            for (size_t i = a; i + fin <= b; i += fin)
            {
                double s1 = 0, s2 = 0;
                for (size_t k = 0; k < fin; ++k)
                {
                    const double win = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * (double) k / (double) fin);
                    const double s0 = x[i + k] * win + c * s1 - s2;
                    s2 = s1; s1 = s0;
                }
                total += s1 * s1 + s2 * s2 - c * s1 * s2;
            }
            return total;
        }
    }

    void proves055()
    {
        using namespace rvb;

        prova ("0.55 reverb: 100 presets ben repartits, amb noms únics i els demanats", []
        {
            const auto& v = presetsFabrica();
            comprova (v.size() == 100, "hi ha 100 presets (" + std::to_string (v.size()) + ")");
            std::map<std::string, int> perCat;
            std::set<std::string> noms;
            for (const auto& p : v) { ++perCat[p.categoria]; noms.insert (p.nom); }
            comprova (noms.size() == v.size(), "els noms no es repeteixen");
            const int esperat[] = { 15, 15, 15, 5, 10, 10, 10, 10, 10 };
            for (size_t i = 0; i < categories().size(); ++i)
                comprova (perCat[categories()[i]] == esperat[i], categories()[i] + ": " + std::to_string (perCat[categories()[i]]));
            for (const char* n : { "Vocal Studio", "Cathedral Vocal", "Gated Snare 80s", "Sub Controlled Reverb", "Soft Rhodes Room",
                                   "Huge Synth Hall", "Wooden Room", "Floating Clouds", "Long Modulated Tail", "Vintage Spring Guitar" })
                comprova (noms.count (n) == 1, std::string ("hi és «") + n + "»");
            // Cap parell de presets idèntic en el so, i cada algorisme s'utilitza.
            int iguals = 0;
            for (size_t a = 0; a < v.size(); ++a)
                for (size_t b = a + 1; b < v.size(); ++b)
                    if (v[a].p.igualSo (v[b].p)) ++iguals;
            comprova (iguals == 0, "no hi ha dos presets iguals");
            std::set<int> algs;
            for (const auto& p : v) algs.insert ((int) p.p[Alg]);
            comprova ((int) algs.size() == numAlgorismes, "tots els algorismes tenen presets");
        });

        prova ("0.55 reverb: l'estat es desa i es recupera (i les claus que falten queden per defecte)", []
        {
            Params p;
            p.posa (Alg, Plate); p.posa (Decay, 3.3f); p.posa (Eq1G, -4.5f); p.posa (DuckOn, 1); p.posa (ModeVista, 1);
            Params q;
            comprova (q.llegeix (p.text()) && q == p, "el text torna els mateixos valors");
            comprova (q.llegeix ("mibase-reverb=1;decay=50;mix=30") && q[Decay] == 20.0f && q[Mix] == 30.0f && q[Size] == Params()[Size],
                      "valors fora de límits retallats i la resta per defecte");
            comprova (! q.llegeix ("una altra cosa"), "un estat que no és de la reverb no es llegeix");
            float x = 0;
            comprova (llegeixValor (Decay, "850 ms", x) && std::fabs (x - 0.85f) < 1e-4f, "«850 ms» al Decay");
            comprova (llegeixValor (HighCut, "4,5k", x) && std::fabs (x - 4500.0f) < 0.1f, "«4,5k» al High Cut");
            comprova (llegeixValor (Mix, "33 %", x) && x == 33.0f, "«33 %» al Mix");
            comprova (! llegeixValor (Alg, "2", x), "els selectors no s'escriuen a mà");
            comprova (textValor (Decay, 2.4f) == "2,40 s" && textValor (HighCut, 4500) == "4,5 kHz" && textValor (Eq1G, 3) == "+3,0 dB",
                      "textos amb unitats: " + textValor (Decay, 2.4f) + " · " + textValor (HighCut, 4500));
            comprova (coincideix ("Veus · Vocal Plate", "veu plate") && coincideix ("Bateria i percussió", "PERCUSSIO") && ! coincideix ("Hall", "plate"),
                      "el cercador no distingeix majúscules ni accents");
        });

        prova ("0.55 reverb: cada algorisme sona diferent, és estable i té cua", []
        {
            const double fs = 48000.0;
            std::vector<So> resp;
            for (int a = 0; a < numAlgorismes; ++a)
            {
                Params p;
                p.posa (Alg, (float) a);
                p.posa (Decay, 2.0f);
                p.posa (EarlyLate, 60);
                auto s = impuls (p, fs, 3.0);
                comprova (finit (s, 2.0f), std::string (nomAlgorisme (a)) + ": valors vàlids i sense saturar");
                const double e = energia (s, fs, 0.0, 1.0);
                comprova (e > 1e-8, std::string (nomAlgorisme (a)) + ": fa so");
                resp.push_back (std::move (s));
            }
            // Correlació entre respostes (els 0,6 primers segons): han de ser clarament diferents.
            int massaSemblants = 0;
            for (int a = 0; a < numAlgorismes; ++a)
                for (int b = a + 1; b < numAlgorismes; ++b)
                {
                    double ab = 0, aa = 0, bb = 0;
                    for (size_t i = 0; i < (size_t) (0.6 * fs); ++i)
                    {
                        ab += (double) resp[(size_t) a].L[i] * resp[(size_t) b].L[i];
                        aa += (double) resp[(size_t) a].L[i] * resp[(size_t) a].L[i];
                        bb += (double) resp[(size_t) b].L[i] * resp[(size_t) b].L[i];
                    }
                    const double c = ab / std::sqrt (std::max (1e-30, aa * bb));
                    if (std::fabs (c) > 0.5) { ++massaSemblants; std::printf ("        (%s ~ %s: %.2f)\n", nomAlgorisme (a), nomAlgorisme (b), c); }
                }
            comprova (massaSemblants == 0, "cap parell d'algorismes té la mateixa resposta");
            // Early Reflections: sense cua després de 0,3 s.
            comprova (energia (resp[Early], fs, 0.35, 1.0) < 1e-9, "Early Reflections no té cua");
            // Gated: es talla en sec.
            comprova (energia (resp[Gated], fs, 1.2, 2.0) < energia (resp[Gated], fs, 0.05, 0.5) * 1e-4, "Gated es talla");
            // Reverse: creix (més energia al final de la finestra que al principi).
            comprova (energia (resp[Reverse], fs, 1.4, 2.0) > energia (resp[Reverse], fs, 0.0, 0.6) * 2.0, "Reverse creix cap al final");
        });

        prova ("0.55 reverb: el Decay mana la durada de la cua (RT60 mesurat)", []
        {
            const double fs = 48000.0;
            for (int a : { (int) Room, (int) Hall, (int) Chamber, (int) Plate, (int) Ambient, (int) Digital })
                for (float d : { 1.0f, 4.0f })
                {
                    Params p;
                    p.posa (Alg, (float) a); p.posa (Decay, d); p.posa (DecGreus, 1.0f); p.posa (DecAguts, 1.0f); p.posa (Damping, 0);
                    p.posa (EarlyLate, 100); p.posa (LowCut, 20); p.posa (HighCut, 20000);
                    const auto s = impuls (p, fs, d * 2.0 + 1.0);
                    const double rt = rt60 (s, fs);
                    std::printf ("        %s Decay %.1f s → RT60 mesurat %.2f s\n", nomAlgorisme (a), d, rt);
                    comprova (rt > d * 0.6 && rt < d * 1.45, std::string (nomAlgorisme (a)) + ": RT60 proper al Decay");
                }
        });

        prova ("0.55 reverb: Decay EQ (greus curts i aguts llargs, i al revés)", []
        {
            const double fs = 48000.0;
            auto mesura = [&] (float dg, float da)
            {
                Params p;
                p.posa (Alg, Hall); p.posa (Decay, 3.0f); p.posa (DecGreus, dg); p.posa (DecAguts, da); p.posa (Damping, 0);
                p.posa (LowCut, 20); p.posa (HighCut, 20000); p.posa (EarlyLate, 100);
                const auto s = impuls (p, fs, 4.0);
                const double g = banda (s.L, fs, 150, 1.5, 3.0) / std::max (1e-30, banda (s.L, fs, 150, 0.1, 0.6));
                const double h = banda (s.L, fs, 6000, 1.5, 3.0) / std::max (1e-30, banda (s.L, fs, 6000, 0.1, 0.6));
                return std::make_pair (g, h);
            };
            const auto [g1, h1] = mesura (0.3f, 1.8f);
            const auto [g2, h2] = mesura (1.8f, 0.3f);
            std::printf ("        greus curts: queda greus %.1f dB, aguts %.1f dB · aguts curts: greus %.1f dB, aguts %.1f dB\n",
                         10 * std::log10 (g1), 10 * std::log10 (h1), 10 * std::log10 (g2), 10 * std::log10 (h2));
            comprova (h1 > g1 * 30.0, "amb greus ×0,3 i aguts ×1,8, els aguts duren molt més");
            comprova (g2 > h2 * 30.0, "amb greus ×1,8 i aguts ×0,3, els greus duren molt més");
            Params p;
            p.posa (DecGreus, 0.5f); p.posa (DecAguts, 1.5f); p.posa (Decay, 2.0f); p.posa (Damping, 0);
            comprova (decaimentA (p, 60) < decaimentA (p, 1000) && decaimentA (p, 1000) < decaimentA (p, 12000), "la corba de la finestra segueix el Decay EQ");
        });

        prova ("0.55 reverb: Mix 0 deixa el so igual; Mix 100 no deixa el so original", []
        {
            const double fs = 48000.0;
            Reverb r;
            Params p = presetInicial().p;
            p.posa (Mix, 0);
            r.posaParams (p);
            r.prepara (fs, 512);
            So s = silenci (fs, 0.5), o;
            for (size_t i = 0; i < s.L.size(); ++i) { s.L[i] = std::sin ((float) i * 0.05f) * 0.5f; s.R[i] = std::cos ((float) i * 0.031f) * 0.4f; }
            o = s;
            passa (r, s, 256);
            float dif = 0;
            for (size_t i = 0; i < s.L.size(); ++i) dif = std::max (dif, std::max (std::fabs (s.L[i] - o.L[i]), std::fabs (s.R[i] - o.R[i])));
            comprova (dif < 1e-6f, "Mix 0: idèntic (diferència " + std::to_string (dif) + ")");
            p.posa (Mix, 100);
            p.posa (PreDelay, 100);
            Reverb r2;
            r2.posaParams (p);
            r2.prepara (fs, 512);
            So t = silenci (fs, 0.2);
            t.L[10] = t.R[10] = 1.0f;
            passa (r2, t, 64);
            comprova (std::fabs (t.L[10]) < 1e-4f, "Mix 100: l'impuls original no hi és");
        });

        prova ("0.55 reverb: ducking i gate", []
        {
            const double fs = 48000.0;
            auto cuaAmbSo = [&] (bool duck, bool gate)
            {
                Params p;
                p.posa (Alg, Hall); p.posa (Decay, 3.0f); p.posa (Mix, 100);
                p.posa (DuckOn, duck ? 1.0f : 0.0f); p.posa (DuckThr, -30); p.posa (DuckAmt, 18);
                p.posa (GateOn, gate ? 1.0f : 0.0f); p.posa (GateThr, -40); p.posa (GateHold, 50); p.posa (GateRel, 30);
                Reverb r;
                r.posaParams (p);
                r.prepara (fs, 512);
                So s = silenci (fs, 2.6);
                for (size_t i = 0; i < (size_t) (0.8 * fs); ++i) s.L[i] = s.R[i] = 0.3f * std::sin ((float) i * 0.07f);
                // Mentre sona, la reverb del Mix 100 no inclou la font: tot el que surt és reverb.
                passa (r, s, 128);
                return std::make_pair (energia (s, fs, 0.4, 0.8), energia (s, fs, 1.7, 2.5));
            };
            const auto [sonaNormal, cuaNormal] = cuaAmbSo (false, false);
            const auto [sonaDuck, cuaDuck] = cuaAmbSo (true, false);
            const auto [sonaGate, cuaGate] = cuaAmbSo (false, true);
            comprova (sonaDuck < sonaNormal * 0.1, "el ducking abaixa la reverb mentre sona la font (" + std::to_string (10 * std::log10 (sonaDuck / sonaNormal)) + " dB)");
            comprova (cuaDuck > cuaNormal * 0.5, "i la cua torna quan la font calla");
            comprova (cuaGate < cuaNormal * 1e-3, "el gate tanca la cua quan la font calla");
            comprova (sonaGate > sonaNormal * 0.5, "i la deixa sonar mentre hi ha so");
        });

        prova ("0.55 reverb: Freeze congela la cua, no hi deixa entrar res i s'allibera de mica en mica", []
        {
            const double fs = 48000.0;
            Params p;
            p.posa (Alg, Ambient); p.posa (Decay, 1.5f); p.posa (Mix, 100); p.posa (FreezeRel, 1.0f);
            Reverb r;
            r.posaParams (p);
            r.prepara (fs, 512);
            So s = silenci (fs, 0.4);
            for (size_t i = 0; i < (size_t) (0.25 * fs); ++i) s.L[i] = s.R[i] = 0.3f * std::sin ((float) i * 0.03f);
            passa (r, s, 256);
            r.posa (FreezeOn, 1);
            So f = silenci (fs, 6.0);
            for (size_t i = (size_t) (3.0 * fs); i < (size_t) (3.5 * fs); ++i) f.L[i] = f.R[i] = 0.5f * std::sin ((float) i * 0.2f);   // so nou durant el freeze
            passa (r, f, 256);
            const double e1 = energia (f, fs, 0.5, 1.5), e2 = energia (f, fs, 4.5, 5.9);
            std::printf ("        energia als 0,5-1,5 s: %.2e · als 4,5-5,9 s: %.2e (%.1f dB)\n", e1, e2, 10 * std::log10 (e2 / e1));
            comprova (finit (f, 3.0f), "valors vàlids");
            comprova (e2 > e1 * 0.5, "al cap de 6 s la cua congelada continua (menys de 3 dB de pèrdua)");
            comprova (energia (f, fs, 3.0, 3.6) < e1 * 2.0, "el so nou no entra a la cua congelada");
            // Salts: cap discontinuïtat gran en congelar.
            float salt = 0;
            for (size_t i = 1; i < f.L.size(); ++i) salt = std::max (salt, std::fabs (f.L[i] - f.L[i - 1]));
            comprova (salt < 0.2f, "sense clics (salt màxim " + std::to_string (salt) + ")");
            r.posa (FreezeOn, 0);
            So g = silenci (fs, 4.0);
            passa (r, g, 256);
            const double a = energia (g, fs, 0.0, 0.3), b = energia (g, fs, 0.6, 0.9), c = energia (g, fs, 3.0, 4.0);
            comprova (b < a && b > a * 0.01, "en treure'l s'apaga de mica en mica (no de cop)");
            comprova (c < a * 1e-3, "i acaba callant");
        });

        prova ("0.55 reverb: shimmer puja una octava; feedback 100 % i shimmer no es desboquen", []
        {
            const double fs = 48000.0;
            auto prova = [&] (bool shim)
            {
                Params p;
                p.posa (Alg, Hall); p.posa (Decay, 4.0f); p.posa (Mix, 100); p.posa (ShimOn, shim ? 1.0f : 0.0f); p.posa (ShimAmt, 60);
                p.posa (ShimPitch, 0); p.posa (LowCut, 20); p.posa (HighCut, 20000); p.posa (Damping, 0);
                Reverb r;
                r.posaParams (p);
                r.prepara (fs, 512);
                So s = silenci (fs, 3.0);
                for (size_t i = 0; i < (size_t) (0.3 * fs); ++i) s.L[i] = s.R[i] = 0.3f * std::sin (2.0f * 3.14159265f * 440.0f * (float) i / (float) fs);
                passa (r, s, 256);
                return banda (s.L, fs, 880, 1.0, 2.5) / std::max (1e-30, banda (s.L, fs, 440, 1.0, 2.5));
            };
            const double sense = prova (false), amb = prova (true);
            std::printf ("        relació 880/440 Hz a la cua: sense %.3f · amb shimmer %.3f\n", sense, amb);
            comprova (amb > sense * 20.0, "amb shimmer apareix l'octava de dalt");
            // Estabilitat extrema: feedback 100 %, shimmer al màxim, soroll fort durant 2 s.
            Params p;
            p.posa (Alg, Shimmer); p.posa (Decay, 20); p.posa (Feedback, 100); p.posa (ShimAmt, 100); p.posa (Mix, 100); p.posa (Damping, 0);
            Reverb r;
            r.posaParams (p);
            r.prepara (fs, 512);
            So s = silenci (fs, 8.0);
            std::mt19937 g (5);
            std::uniform_real_distribution<float> u (-1.0f, 1.0f);
            for (size_t i = 0; i < (size_t) (2.0 * fs); ++i) { s.L[i] = u (g); s.R[i] = u (g); }
            passa (r, s, 333);
            comprova (finit (s, 3.5f), "no es desboca (valors vàlids i limitats)");
        });

        prova ("0.55 reverb: els 100 presets són estables a 44,1, 48 i 96 kHz i amb blocs de mides diferents", []
        {
            int malament = 0;
            std::mt19937 g (11);
            std::uniform_real_distribution<float> u (-0.5f, 0.5f);
            int k = 0;
            for (const auto& pr : presetsFabrica())
            {
                const double fs = (k % 3 == 0) ? 44100.0 : (k % 3 == 1 ? 48000.0 : 96000.0);
                const int bloc = (k % 4 == 0) ? 1 : (k % 4 == 1 ? 17 : (k % 4 == 2 ? 256 : 1024));
                ++k;
                Reverb r;
                r.posaParams (pr.p);
                r.prepara (fs, 1024);
                So s = silenci (fs, 2.5);
                for (size_t i = 0; i < (size_t) (0.6 * fs); ++i) { s.L[i] = u (g); s.R[i] = u (g); }
                passa (r, s, bloc);
                if (! finit (s, 3.5f)) { ++malament; std::printf ("        INESTABLE: %s\n", pr.nom.c_str()); }
            }
            comprova (malament == 0, "tots estables");
        });

        prova ("0.55 reverb: canviar d'algorisme o de preset sonant no fa clics", []
        {
            const double fs = 48000.0;
            Reverb r;
            r.posaParams (presetsFabrica()[0].p);
            r.prepara (fs, 512);
            float saltMax = 0.0f, ant = 0.0f;
            std::vector<float> L (256), R (256);
            for (int b = 0; b < 400; ++b)
            {
                for (int i = 0; i < 256; ++i) { const float x = 0.25f * std::sin ((float) (b * 256 + i) * 0.02f); L[(size_t) i] = x; R[(size_t) i] = x; }
                if (b % 40 == 20) r.posaParams (presetsFabrica()[(size_t) (b * 7) % 100].p);
                if (b % 40 == 30) r.posa (Alg, (float) ((b / 40) % numAlgorismes));
                r.processa (L.data(), R.data(), 256);
                for (int i = 0; i < 256; ++i) { saltMax = std::max (saltMax, std::fabs (L[(size_t) i] - ant)); ant = L[(size_t) i]; }
            }
            // La sinusoide sola canvia com a molt 0,005 per mostra; la reverb hi afegeix el seu so.
            std::printf ("        salt màxim entre mostres: %.3f\n", saltMax);
            comprova (saltMax < 0.35f, "sense salts grans");
        });

        prova ("0.55 reverb: biblioteca de presets de l'usuari i favorits", []
        {
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_reverb";
            std::filesystem::remove_all (dir);
            const auto f = dir / "presets.json";
            BibliotecaPresets b;
            comprova (b.carrega (f) && b.usuari.empty(), "sense fitxer: buida");
            Preset p;
            p.nom = "La meva veu";
            p.categoria = "Carmen 113";
            p.p.posa (Decay, 3.1f);
            b.desaPreset (p);
            p.nom = b.nomLliure ("La meva veu");
            comprova (p.nom == "La meva veu 2", "nom lliure");
            b.desaPreset (p);
            b.commutaFavorit (BibliotecaPresets::clauUsuari ("La meva veu"));
            b.commutaFavorit (BibliotecaPresets::clauFabrica ("Vocal Plate"));
            comprova (b.reanomena ("La meva veu", "Veu Roger"), "reanomena");
            comprova (b.esFavorit (BibliotecaPresets::clauUsuari ("Veu Roger")), "el favorit segueix el nom nou");
            comprova (! b.reanomena ("Veu Roger", "La meva veu 2"), "no es pot repetir un nom");
            comprova (b.mou ("La meva veu 2", "Guitarres meves"), "es pot moure de carpeta");
            comprova (b.desa (f), "desa");
            BibliotecaPresets c;
            comprova (c.carrega (f) && c.usuari.size() == 2 && c.busca ("Veu Roger") != nullptr && c.busca ("Veu Roger")->p[Decay] == 3.1f,
                      "es recupera");
            comprova (c.carpetes().size() == 2 && c.esFavorit ("f:Vocal Plate"), "carpetes i favorits");
            comprova (c.esborra ("Veu Roger") && c.usuari.size() == 1 && ! c.esFavorit ("u:Veu Roger"), "esborra (i el treu dels favorits)");
            std::filesystem::remove_all (dir);
        });

        prova ("0.55 reverb: CPU segons la qualitat (Eco < Normal < Alta)", []
        {
            const double fs = 48000.0;
            double t[3] {};
            for (int q = 0; q < 3; ++q)
            {
                Params p;
                p.posa (Alg, Hall); p.posa (Qualitat, (float) q); p.posa (Mix, 30); p.posa (ModDepth, 40);
                Reverb r;
                r.posaParams (p);
                r.prepara (fs, 256);
                So s = silenci (fs, 10.0);
                for (size_t i = 0; i < s.L.size(); i += 4800) s.L[i] = s.R[i] = 0.8f;
                const auto a = std::chrono::steady_clock::now();
                passa (r, s, 256);
                t[q] = std::chrono::duration<double> (std::chrono::steady_clock::now() - a).count();
            }
            std::printf ("        10 s d'àudio: Eco %.0f ms · Normal %.0f ms · Alta %.0f ms (%.2f %% · %.2f %% · %.2f %% d'un nucli)\n",
                         t[0] * 1000, t[1] * 1000, t[2] * 1000, t[0] * 10, t[1] * 10, t[2] * 10);
            comprova (t[0] < t[1] && t[1] < t[2], "la qualitat canvia el consum");
            comprova (t[2] < 1.0, "fins i tot en Alta, molt menys que el temps real");
        });
    }
}
