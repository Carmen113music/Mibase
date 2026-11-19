// 0.59: EQ Pro (Mibase), equalitzador paramètric intern.
namespace
{
    namespace eqp
    {
        using namespace mibase::eq;

        Banda banda (int tipus, float f, float g = 0.0f, float q = qDefecte, int pendent = 1, int canal = Estereo)
        {
            Banda b;
            b.usada = true; b.activa = true;
            b.tipus = tipus; b.freq = f; b.guany = g; b.q = q; b.pendent = pendent; b.canal = canal;
            return b;
        }

        double dbDigital (const Banda& b, double fs, double f)
        {
            Seccio s[maxSeccions];
            const int n = dissenya (b, fs, s);
            return 20.0 * std::log10 (std::max (1.0e-12, std::abs (resposta (s, n, f, fs))));
        }

        double dbAnalog (const Banda& b, double f) { return 20.0 * std::log10 (std::max (1.0e-12, magnitudAnalogica (b, f))); }

        /** Diferència màxima (dB) entre el digital i l'analògic, de 20 Hz fins a «fins», on l'analògic és > pis. */
        double error (const Banda& b, double fs, double fins, double pis = -40.0)
        {
            double e = 0.0;
            for (double f = 20.0; f <= fins; f *= 1.03)
            {
                const double a = dbAnalog (b, f);
                if (a < pis) continue;
                e = std::max (e, std::abs (dbDigital (b, fs, f) - a));
            }
            return e;
        }

        std::vector<float> soroll (size_t n, unsigned llavor)
        {
            std::vector<float> v (n);
            unsigned x = llavor;
            for (auto& s : v) { x = x * 1664525u + 1013904223u; s = ((float) (x >> 8) / 16777216.0f - 0.5f) * 0.5f; }
            return v;
        }

        void passa (Equalitzador& e, std::vector<float>& L, std::vector<float>& R, int bloc)
        {
            const int n = (int) L.size();
            for (int i = 0; i < n; i += bloc) e.processa (L.data() + i, R.data() + i, std::min (bloc, n - i));
        }

        double rms (const std::vector<float>& v, size_t a, size_t b)
        {
            double s = 0.0;
            for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
            return std::sqrt (s / (double) std::max<size_t> (1, b - a));
        }

        /** Quant «agut» hi ha en un tros (tercera diferència) respecte del volum del tros: un clic el fa pujar molt. */
        double aspresa (const std::vector<float>& v, size_t a, size_t b)
        {
            double m = 0.0, amp = 1.0e-9;
            for (size_t i = std::max<size_t> (a, 3); i < b; ++i)
            {
                m = std::max (m, std::abs ((double) v[i] - 3.0 * v[i - 1] + 3.0 * v[i - 2] - v[i - 3]));
                amp = std::max (amp, (double) std::abs (v[i]));
            }
            return m / amp;
        }
    }

    void proves059()
    {
        using namespace eqp;

        prova ("0.59 EQ: cada tipus segueix la corba analògica (44,1 / 48 / 96 kHz), també a prop dels aguts", []
        {
            for (double fs : { 44100.0, 48000.0, 96000.0 })
            {
                // fins a 12 kHz, menys de mig dB; de 12 kHz fins a dalt, menys d'1,2 dB (HP/LP molt drets: 3 dB a la zona on ja han caigut)
                const double fins = std::min (20000.0, 0.45 * fs);
                const std::string t = " a " + std::to_string ((int) fs);
                auto mira = [&] (const Banda& b, const std::string& nom, double pis, double toleranciaDalt)
                {
                    if (b.freq <= 8000.0f) comprova (error (b, fs, 12000.0, pis) < 0.5, nom + " (fins a 12 kHz)" + t);
                    comprova (error (b, fs, fins, pis) < toleranciaDalt, nom + " (fins a dalt)" + t);
                };
                for (float f : { 60.0f, 1000.0f, 8000.0f, 16000.0f })
                {
                    const std::string ft = " " + std::to_string ((int) f) + " Hz";
                    for (float g : { -12.0f, 6.0f, 18.0f })
                        for (float q : { 0.4f, 1.0f, 4.0f })
                            mira (banda (Bell, f, g, q), "Bell" + ft, -40.0, 1.2);
                    mira (banda (LowShelf, f, 9.0f), "Low Shelf" + ft, -40.0, 1.2);
                    mira (banda (HighShelf, f, -9.0f), "High Shelf" + ft, -40.0, 1.2);
                    mira (banda (Tilt, f, 6.0f), "Tilt" + ft, -40.0, 1.2);
                    mira (banda (BandPass, f, 0.0f, 2.0f), "Band Pass" + ft, -24.0, 1.2);
                    for (int p = 0; p < numPendents; ++p)
                    {
                        mira (banda (HighPass, f, 0.0f, qDefecte, p), "High Pass " + std::to_string (dbPerOctava (p)) + ft, -24.0, 3.0);
                        const auto lp = banda (LowPass, f, 0.0f, qDefecte, p);
                        if (ordre (p) < 4) mira (lp, "Low Pass " + std::to_string (dbPerOctava (p)) + ft, -24.0, 3.0);
                        else
                        {
                            // els drets (bilineal): -3 dB just a la freqüència, la zona que passa com l'analògic i, a dalt,
                            // més caiguda que l'analògic (com a molt +0,3 dB de més per sota del tall; +1,3 dB amb el tall a 16 kHz)
                            comprova (std::abs (dbDigital (lp, fs, f) + 3.01) < 0.05, "Low Pass " + std::to_string (dbPerOctava (p)) + ft + " -3 dB al tall" + t);
                            if (f <= 8000.0f) comprova (error (lp, fs, fins, -1.0) < 0.3, "Low Pass " + std::to_string (dbPerOctava (p)) + ft + " zona que passa" + t);
                            bool mai = true;
                            for (double x = 20.0; x <= fins; x *= 1.03) mai = mai && dbDigital (lp, fs, x) <= dbAnalog (lp, x) + (f <= 8000.0f ? 0.3 : 1.3);
                            comprova (mai, "Low Pass " + std::to_string (dbPerOctava (p)) + ft + " no atenua menys" + t);
                        }
                    }
                }
                // Notch: rebuig a la freqüència i pla lluny d'ella
                const auto n = banda (Notch, 1000.0f, 0.0f, 4.0f);
                comprova (dbDigital (n, fs, 1000.0) < -60.0, "Notch fa rebuig al centre" + t);
                comprova (std::abs (dbDigital (n, fs, 100.0)) < 0.2 && std::abs (dbDigital (n, fs, 10000.0)) < 0.2, "Notch pla lluny" + t);
                // All Pass: pla a tot arreu
                const auto ap = banda (AllPass, 2000.0f, 0.0f, 2.0f);
                double m = 0.0;
                for (double f = 20.0; f < fins; f *= 1.2) m = std::max (m, std::abs (dbDigital (ap, fs, f)));
                comprova (m < 1.0e-6, "All Pass no canvia el volum" + t);
                // Bell: guany exacte al centre
                comprova (std::abs (dbDigital (banda (Bell, 15000.0f, 12.0f, 2.0f), fs, 15000.0) - 12.0) < 0.3, "Bell a 15 kHz fa +12 dB" + t);
            }
            // pendent real: una octava per sota de la freqüència, molt avall
            const auto hp = banda (HighPass, 1000.0f, 0.0f, qDefecte, 3);   // 24 dB/oct
            const double d = dbDigital (hp, 48000.0, 125.0) - dbDigital (hp, 48000.0, 62.5);
            comprova (std::abs (d - 24.0) < 0.6, "High Pass 24 dB/oct cau 24 dB per octava");
            const auto lp = banda (LowPass, 500.0f, 0.0f, qDefecte, 7);     // 96 dB/oct
            comprova (dbDigital (lp, 48000.0, 1000.0) < -90.0, "Low Pass 96 dB/oct: -96 dB una octava amunt");
        });

        prova ("0.59 EQ: el so passa pel motor tal com diu la corba (to de prova)", []
        {
            for (double fs : { 44100.0, 96000.0 })
            {
                Equalitzador e;
                e.prepara (fs, 512);
                e.posaBanda (0, banda (Bell, 1000.0f, 9.0f, 1.0f));
                e.posaBanda (1, banda (HighPass, 80.0f, 0.0f, qDefecte, 3));
                for (double f : { 50.0, 300.0, 1000.0, 5000.0 })
                {
                    const size_t n = (size_t) fs;
                    std::vector<float> L (n), R (n);
                    for (size_t i = 0; i < n; ++i) L[i] = R[i] = (float) (0.25 * std::sin (2.0 * 3.14159265358979 * f * (double) i / fs));
                    std::vector<float> ent = L;
                    e.buida();
                    passa (e, L, R, 256);
                    const double mesurat = 20.0 * std::log10 (rms (L, n / 2, n) / rms (ent, n / 2, n));
                    const double esperat = dbDigital (banda (Bell, 1000.0f, 9.0f, 1.0f), fs, f) + dbDigital (banda (HighPass, 80.0f, 0.0f, qDefecte, 3), fs, f);
                    comprova (std::abs (mesurat - esperat) < 0.1, "guany mesurat a " + std::to_string ((int) f) + " Hz (" + std::to_string (mesurat) + " / " + std::to_string (esperat) + ")");
                }
            }
        });

        prova ("0.59 EQ: el resultat és idèntic amb qualsevol mida de buffer", []
        {
            const auto ent = soroll (48000, 7);
            std::vector<float> ref;
            for (int bloc : { 4096, 1, 7, 64, 333 })
            {
                Equalitzador e;
                e.prepara (48000.0, 4096);
                e.posaBanda (0, banda (Bell, 300.0f, -6.0f, 2.0f));
                e.posaBanda (5, banda (HighShelf, 6000.0f, 4.0f));
                e.posaBanda (9, banda (LowPass, 12000.0f, 0.0f, 1.2f, 5, Side));
                e.posaSortida (-2.0f);
                auto L = ent, R = soroll (48000, 9);
                passa (e, L, R, bloc);
                if (ref.empty()) { ref = L; ref.insert (ref.end(), R.begin(), R.end()); continue; }
                bool igual = true;
                for (size_t i = 0; i < L.size(); ++i) if (L[i] != ref[i] || R[i] != ref[L.size() + i]) { igual = false; break; }
                comprova (igual, "bloc de " + std::to_string (bloc));
            }
        });

        prova ("0.59 EQ: bandes apagades, sense bandes o en bypass, el so surt idèntic (bit a bit)", []
        {
            const auto ent = soroll (24000, 3);
            {
                Equalitzador e;
                e.prepara (48000.0, 512);
                auto L = ent, R = ent;
                passa (e, L, R, 512);
                comprova (L == ent && R == ent, "sense bandes");
            }
            {
                Equalitzador e;
                e.prepara (48000.0, 512);
                auto b = banda (Bell, 1000.0f, 12.0f);
                b.activa = false;
                e.posaBanda (0, b);
                auto L = ent, R = ent;
                passa (e, L, R, 512);
                comprova (L == ent && R == ent, "banda apagada");
            }
            {
                Equalitzador e;
                e.posaBanda (0, banda (Bell, 1000.0f, 12.0f));
                e.posaSortida (6.0f);
                e.posaBypass (true);
                e.prepara (48000.0, 512);
                auto L = ent, R = ent;
                passa (e, L, R, 512);
                comprova (L == ent && R == ent, "bypass");
            }
            {
                // en apagar la banda, al cap d'una estona torna a ser idèntic
                Equalitzador e;
                e.prepara (48000.0, 512);
                e.posaBanda (0, banda (Bell, 1000.0f, 12.0f));
                auto L = ent, R = ent;
                passa (e, L, R, 512);
                e.esborra (0);
                L = ent; R = ent;
                passa (e, L, R, 512);
                bool final = true;
                for (size_t i = 4800; i < L.size(); ++i) if (L[i] != ent[i]) { final = false; break; }
                comprova (final, "després d'esborrar la banda");
            }
        });

        prova ("0.59 EQ: canviar guany, freqüència, tipus o canal mentre sona no fa clics", []
        {
            const double fs = 48000.0;
            auto to = [&] (size_t n) { std::vector<float> v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (0.3 * std::sin (2.0 * 3.14159265358979 * 440.0 * (double) i / fs)); return v; };
            struct Cas { const char* nom; Banda abans, despres; };
            const Cas casos[] = {
                { "guany 0 → +18 dB", banda (Bell, 440.0f, 0.0f), banda (Bell, 440.0f, 18.0f) },
                { "freqüència 100 Hz → 5 kHz", banda (Bell, 100.0f, 12.0f), banda (Bell, 5000.0f, 12.0f) },
                { "tipus Bell → Low Pass", banda (Bell, 440.0f, 12.0f), banda (LowPass, 300.0f, 0.0f, qDefecte, 3) },
                { "canal Estèreo → Side", banda (Bell, 440.0f, 12.0f), banda (Bell, 440.0f, 12.0f, qDefecte, 1, Side) },
                { "encendre una banda", [] { auto b = banda (Bell, 440.0f, 15.0f); b.activa = false; return b; }(), banda (Bell, 440.0f, 15.0f) },
            };
            for (const auto& c : casos)
            {
                Equalitzador e;
                e.prepara (fs, 256);
                e.posaBanda (0, c.abans);
                auto L = to (48000), R = L;
                std::vector<float> A (L.begin(), L.begin() + 24000), B (L.begin(), L.begin() + 24000);
                std::vector<float> A2 (L.begin() + 24000, L.end()), B2 = A2;
                passa (e, A, B, 256);
                e.posaBanda (0, c.despres);
                passa (e, A2, B2, 256);
                std::vector<float> tot = A;
                tot.insert (tot.end(), A2.begin(), A2.end());
                const double estable = std::max (aspresa (tot, 12000, 24000), aspresa (tot, 40000, 48000));
                const double canvi = aspresa (tot, 23990, 30000);
                comprova (canvi <= 2.0 * estable, std::string (c.nom) + " (" + std::to_string (canvi) + " / " + std::to_string (estable) + ")");
                comprova (std::all_of (tot.begin(), tot.end(), [] (float x) { return std::isfinite (x); }), std::string (c.nom) + ": tot finit");
            }
        });

        prova ("0.59 EQ: canals per banda (L, R, Mid, Side)", []
        {
            const auto ent = soroll (24000, 11);
            {
                // so mono: una banda al Side no hi fa res
                Equalitzador e;
                e.prepara (48000.0, 512);
                e.posaBanda (0, banda (Bell, 1000.0f, -20.0f, 0.5f, 1, Side));
                auto L = ent, R = ent;
                passa (e, L, R, 512);
                double d = 0.0;
                for (size_t i = 0; i < L.size(); ++i) d = std::max (d, (double) std::abs (L[i] - ent[i]) + std::abs (R[i] - ent[i]));
                comprova (d < 1.0e-6, "mono amb banda al Side queda igual");
            }
            {
                // banda a l'esquerre: el dret no es toca (bit a bit)
                Equalitzador e;
                e.prepara (48000.0, 512);
                e.posaBanda (0, banda (HighShelf, 2000.0f, 10.0f, qDefecte, 1, Esquerre));
                auto L = ent, R = soroll (24000, 12);
                const auto R0 = R;
                passa (e, L, R, 512);
                comprova (R == R0, "el dret no canvia");
                comprova (L != ent, "l'esquerre sí");
            }
            {
                // banda al Mid amb so oposat (L = -R): no fa res
                Equalitzador e;
                e.prepara (48000.0, 512);
                e.posaBanda (0, banda (Bell, 500.0f, 15.0f, 1.0f, 1, Mid));
                auto L = ent, R = ent;
                for (auto& x : R) x = -x;
                const auto R0 = R;
                passa (e, L, R, 512);
                double d = 0.0;
                for (size_t i = 0; i < L.size(); ++i) d = std::max (d, (double) std::abs (L[i] - ent[i]) + std::abs (R[i] - R0[i]));
                comprova (d < 1.0e-6, "so només de Side amb banda al Mid queda igual");
            }
        });

        prova ("0.59 EQ: estat, paràmetres i automatització", []
        {
            Equalitzador e;
            e.posaBanda (3, banda (LowShelf, 120.0f, 4.5f, 0.9f));
            e.posaBanda (17, banda (HighPass, 35.0f, 0.0f, 1.1f, 5, Mid));
            auto b = banda (Notch, 3150.0f, 0.0f, 8.0f, 1, Dret);
            b.activa = false;
            e.posaBanda (20, b);
            e.posaSortida (-3.5f);
            const auto s = e.estat();
            Equalitzador f;
            f.posaEstat (s);
            bool igual = f.estat() == s && std::abs (f.sortida() + 3.5f) < 1.0e-4f;
            for (int k = 0; k < maxBandes; ++k) igual = igual && f.banda (k) == e.banda (k);
            comprova (igual, "estat igual en tornar-lo a llegir");
            comprova (f.bandaLliure() == 0, "primera banda lliure");
            f.posaEstat ("mibase-eq=1");
            comprova (f.banda (3).usada == false, "un estat buit treu les bandes");

            std::set<std::string> ids;
            int automat = 0;
            for (int i = 0; i < e.numParametres(); ++i)
            {
                const auto inf = e.infoParametre (i);
                ids.insert (inf.id);
                if (inf.automatitzable) ++automat;
            }
            comprova ((int) ids.size() == numParams, "identificadors únics");
            comprova (automat == 2 + 3 * paramsPerBanda, "només surten les bandes que existeixen");
            const int iq = e.indexParametre ("b04.freq");
            comprova (iq == indexParam (3, PFreq), "id de la freqüència de la banda 4");
            e.posaParametre (iq, e.parametre (iq));
            comprova (std::abs (e.banda (3).freq - 120.0f) < 0.01f, "anada i tornada del valor normalitzat");
            e.posaParametre (indexParam (3, PGuany), 0.5f);
            comprova (std::abs (e.banda (3).guany) < 1.0e-4f, "guany al mig = 0 dB");
            comprova (e.textParametre (indexParam (3, PFreq), e.parametre (indexParam (3, PFreq))) == "120 Hz", "text de la freqüència");
        });

        prova ("0.59 EQ: valors extrems i canvi de freqüència de mostreig sense problemes; consum amb 24 bandes", []
        {
            Equalitzador e;
            for (int k = 0; k < maxBandes; ++k)
            {
                const int t = k % numTipus;
                e.posaBanda (k, banda (t, k % 2 ? freqMax : freqMin, k % 3 ? guanyMax : -guanyMax, k % 2 ? qMax : qMin, numPendents - 1, k % numCanals));
            }
            bool finit = true;
            for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
            {
                e.prepara (fs, 1024);
                auto L = soroll (8192, 5), R = soroll (8192, 6);
                passa (e, L, R, 1024);
                for (size_t i = 0; i < L.size(); ++i) finit = finit && std::isfinite (L[i]) && std::isfinite (R[i]) && std::abs (L[i]) < 1.0e8f;
            }
            comprova (finit, "tot finit amb valors extrems");

            // consum: 24 bandes reals (6 a 96 dB/oct) a 48 kHz
            Equalitzador c;
            c.prepara (48000.0, 256);
            for (int k = 0; k < maxBandes; ++k)
                c.posaBanda (k, k < 6 ? banda (k % 2 ? HighPass : LowPass, 40.0f + 300.0f * (float) k, 0.0f, qDefecte, 7, k % 3)
                                      : banda (Bell, 50.0f * (float) (k + 1), (k % 2 ? 3.0f : -3.0f), 1.5f, 1, k % numCanals));
            auto L = soroll (48000 * 5, 1), R = soroll (48000 * 5, 2);
            const auto t0 = std::chrono::steady_clock::now();
            passa (c, L, R, 256);
            const double seg = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("        (24 bandes: %.1f %% d'un nucli en temps real)\n", 100.0 * seg / 5.0);
            comprova (seg / 5.0 < 0.25, "consum raonable amb 24 bandes");
        });
    }
}
