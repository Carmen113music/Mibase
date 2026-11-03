#include "mibase/MesuraLatencia.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace mibase
{
    void MesuraLatencia::prepara (double freqMostreig)
    {
        estat.store (Inactiu);
        fs = freqMostreig > 0 ? freqMostreig : 48000.0;
        longitudCicle = (int) (fs * 0.5);

        // Soroll «blanc» fix (sempre el mateix), amb entrada i sortida suaus.
        senyal.assign (longitudSenyal, 0.0f);
        std::uint32_t llavor = 0x13579BDFu;
        for (int i = 0; i < longitudSenyal; ++i)
        {
            llavor = llavor * 1664525u + 1013904223u;
            const float valor = ((llavor >> 8) & 1) ? 1.0f : -1.0f;
            const int vora = std::min (i, longitudSenyal - 1 - i);
            const float suau = std::min (1.0f, (float) vora / 16.0f);
            senyal[(size_t) i] = 0.3f * valor * suau;
        }

        gravacio.assign ((size_t) (longitudCicle * repeticions), 0.0f);
        posicio.store (0);
    }

    bool MesuraLatencia::demana() noexcept
    {
        if (fs <= 0.0 || gravacio.empty() || estaMesurant())
            return false;

        estat.store (Demanat, std::memory_order_release);
        return true;
    }

    void MesuraLatencia::cancella() noexcept
    {
        estat.store (Inactiu, std::memory_order_release);
    }

    bool MesuraLatencia::estaMesurant() const noexcept
    {
        const int e = estat.load (std::memory_order_acquire);
        return e == Demanat || e == Mesurant;
    }

    bool MesuraLatencia::teDades() const noexcept
    {
        return estat.load (std::memory_order_acquire) == Fet;
    }

    float MesuraLatencia::progres() const noexcept
    {
        if (gravacio.empty())
            return 0.0f;
        return (float) posicio.load (std::memory_order_relaxed) / (float) gravacio.size();
    }

    bool MesuraLatencia::processa (const float* const* entrades, int numEntrades,
                                   float* sortida, int numMostres) noexcept
    {
        int e = estat.load (std::memory_order_acquire);

        if (e == Demanat)
        {
            posicio.store (0, std::memory_order_relaxed);
            estat.store (Mesurant, std::memory_order_relaxed);
            e = Mesurant;
        }

        if (e != Mesurant)
            return false;

        const int total = (int) gravacio.size();
        int pos = posicio.load (std::memory_order_relaxed);

        for (int i = 0; i < numMostres; ++i)
        {
            if (pos >= total)
            {
                sortida[i] = 0.0f;
                continue;
            }

            const int dinsCicle = pos % longitudCicle;
            sortida[i] = dinsCicle < longitudSenyal ? senyal[(size_t) dinsCicle] : 0.0f;

            float suma = 0.0f;
            for (int c = 0; c < numEntrades; ++c)
                if (entrades[c] != nullptr)
                    suma += entrades[c][i];

            gravacio[(size_t) pos] = suma;
            ++pos;
        }

        posicio.store (pos, std::memory_order_relaxed);

        if (pos >= total)
            estat.store (Fet, std::memory_order_release);

        return true;
    }

    MesuraLatencia::Resultat MesuraLatencia::calcula()
    {
        Resultat r;

        if (! teDades())
        {
            r.missatge = "No hi ha cap mesura feta.";
            return r;
        }

        double energiaSenyal = 0.0;
        for (auto s : senyal)
            energiaSenyal += (double) s * s;

        std::vector<int> retards;
        double sumaQualitat = 0.0;

        for (int k = 0; k < repeticions; ++k)
        {
            const float* tros = gravacio.data() + (size_t) k * (size_t) longitudCicle;
            const int maxRetard = longitudCicle - longitudSenyal;

            // Prefix d'energia per normalitzar ràpid.
            double millor = 0.0;
            int millorRetard = -1;
            double millorQualitat = 0.0;

            for (int d = 0; d <= maxRetard; ++d)
            {
                double c = 0.0;
                for (int j = 0; j < longitudSenyal; ++j)
                    c += (double) tros[d + j] * senyal[(size_t) j];

                const double a = std::abs (c);
                if (a > millor)
                {
                    millor = a;
                    millorRetard = d;
                }
            }

            if (millorRetard >= 0)
            {
                double energiaTros = 0.0;
                for (int j = 0; j < longitudSenyal; ++j)
                    energiaTros += (double) tros[millorRetard + j] * tros[millorRetard + j];

                if (energiaTros > 0.0)
                    millorQualitat = millor / std::sqrt (energiaSenyal * energiaTros);
            }

            if (millorQualitat >= 0.5)
            {
                retards.push_back (millorRetard);
                sumaQualitat += millorQualitat;
            }
        }

        r.mesuresValides = (int) retards.size();
        estat.store (Inactiu, std::memory_order_release);

        if (retards.size() < 3)
        {
            r.missatge = "No s'ha detectat el senyal de prova. Comprova el cable de la sortida "
                         "a l'entrada, que l'entrada estigui oberta i el volum.";
            return r;
        }

        std::sort (retards.begin(), retards.end());
        r.mostres = retards[retards.size() / 2];
        r.dispersio = retards.back() - retards.front();
        r.ms = 1000.0 * r.mostres / fs;
        r.qualitat = sumaQualitat / (double) retards.size();
        r.correcte = true;

        char text[160];
        if (r.dispersio <= 2)
            std::snprintf (text, sizeof (text), "Mesura correcta (%d de %d repeticions).", r.mesuresValides, repeticions);
        else
            std::snprintf (text, sizeof (text), "Mesura poc estable: varia %d mostres entre repeticions. Torna-ho a provar.", r.dispersio);
        r.missatge = text;
        return r;
    }
}
