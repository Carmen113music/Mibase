#pragma once
// 0.54: com la vista segueix el capçal mentre sona o grava (demanat per en Roger el 16/11/2026).
// Al principi la vista no es mou. Quan el capçal arriba a la vora dreta, la vista comença a lliscar, suau al principi;
// durant uns segons llisca una mica més de pressa que el capçal, de manera que el capçal va cap al centre; quan hi
// arriba, la vista llisca a la mateixa velocitat que la música i el capçal queda quiet al mig.
// Si algú mou la vista a mà mentre sona, el seguiment s'atura fins que es torna a començar (reinicia).
// Unitats lliures (segons a la taula de treball, ticks als editors); el temps (dt, tempsCentrar) sempre en segons.
#include <algorithm>
#include <cmath>

namespace mibase
{
    class SeguimentCapcal
    {
    public:
        double llindar = 0.9;      // a quina part de l'amplada comença a lliscar
        double suavitat = 0.8;     // segons que triga la velocitat a arribar a la que toca (com més, més suau)

        void reinicia() { *this = SeguimentCapcal { llindar, suavitat }; }
        bool pausat() const noexcept { return pausa; }
        bool actiu() const noexcept { return seguint; }

        /** Torna on ha de començar la vista. capcal i inici en les mateixes unitats; amplada = el que es veu;
            dt = segons des de la crida anterior; tempsCentrar = segons per portar el capçal de la vora al mig. */
        double pas (double capcal, double inici, double amplada, double dt, double tempsCentrar, double minInici = 0.0)
        {
            if (amplada <= 0.0 || dt <= 0.0 || pausa) return inici;
            if (! primer && std::abs (inici - ultimaVista) > 1.0e-6 * std::max (1.0, std::abs (inici))) { pausa = true; return inici; }   // l'han mogut a mà
            if (! primer)
            {
                const double d = capcal - ultimCapcal;
                if (d >= 0.0 && d < amplada) vCap = teVelocitat ? 0.7 * vCap + 0.3 * (d / dt) : d / dt;   // salts (bucle, situar) no compten
                teVelocitat = teVelocitat || (d >= 0.0 && d < amplada);
            }
            primer = false;
            ultimCapcal = capcal;

            double nou = inici;
            const double rel = capcal - inici;
            if (rel < 0.0)
            {
                // ha tornat enrere fora de la pantalla (un bucle, o han situat el capçal): queda a prop de l'esquerra,
                // perquè es vegi tot el que ve (un bucle que hi cap es queda quiet); torna a seguir quan arribi a la vora
                nou = std::max (minInici, capcal - 0.1 * amplada);
                seguint = false;
                v = 0.0;
            }
            else if (rel > amplada * 1.5)
            {
                // ha saltat lluny endavant: el capçal al mig, i segueix
                nou = std::max (minInici, capcal - 0.5 * amplada);
                seguint = true;
                v = vCap;
            }
            else
            {
                if (! seguint && rel >= llindar * amplada) { seguint = true; v = 0.0; }
                if (seguint)
                {
                    const double error = capcal - (inici + 0.5 * amplada);     // > 0: el capçal és a la dreta del mig
                    const double k = 3.0 / std::max (0.5, tempsCentrar);        // en «tempsCentrar» queda a menys d'un 5 % del mig
                    const double desitjada = std::max (0.0, vCap + k * error);  // mai enrere
                    v += (desitjada - v) * std::min (1.0, dt / std::max (0.05, suavitat));
                    nou = inici + v * dt;
                    nou = std::max (nou, capcal - 0.97 * amplada);              // el capçal no surt mai per la dreta
                    nou = std::max (nou, minInici);
                }
            }
            ultimaVista = nou;
            return nou;
        }

    private:
        SeguimentCapcal (double l, double s) : llindar (l), suavitat (s) {}
    public:
        SeguimentCapcal() = default;
    private:
        bool primer = true, seguint = false, pausa = false, teVelocitat = false;
        double v = 0.0, vCap = 0.0, ultimCapcal = 0.0, ultimaVista = 0.0;
    };
}
