#include "mibase/Automatitzacio.h"
#include "mibase/Edicio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <mutex>

namespace mibase::automatitzacio
{
    std::string objectiuInsert (const std::string& idInsert, const std::string& idParam) { return "insert:" + idInsert + ":" + idParam; }
    std::string objectiuInstrument (const std::string& idParam) { return "instrument:" + idParam; }

    int llegeixObjectiu (const std::string& o, std::string& idInsert, std::string& idParam)
    {
        idInsert.clear();
        idParam.clear();
        if (o == volum) return 0;
        if (o == pan) return 1;
        if (o.rfind ("insert:", 0) == 0)
        {
            const auto r = o.substr (7);
            const auto d = r.find (':');
            if (d == std::string::npos) return -1;
            idInsert = r.substr (0, d);
            idParam = r.substr (d + 1);
            return 2;
        }
        if (o.rfind ("instrument:", 0) == 0) { idParam = o.substr (11); return 3; }
        return -1;
    }

    std::shared_ptr<std::atomic<bool>> bandera (const std::string& id)
    {
        static std::mutex m;
        static std::map<std::string, std::shared_ptr<std::atomic<bool>>> b;
        std::lock_guard<std::mutex> l (m);
        auto& x = b[id];
        if (x == nullptr) x = std::make_shared<std::atomic<bool>> (false);
        return x;
    }

    const char* nomGrava (int mode)
    {
        static const char* const n[] = { "No", "Toca", "Enganxa", "Escriu" };
        return n[std::clamp (mode, 0, 3)];
    }

    double guanyDeVolum (double v) noexcept
    {
        v = std::clamp (v, 0.0, 1.0);
        const double x = v / 0.8;
        return x * x * x;
    }

    double dbDeVolum (double v) noexcept
    {
        const double g = guanyDeVolum (v);
        return g <= 1.0e-3 ? -60.0 : 20.0 * std::log10 (g);
    }

    double volumDeDb (double db) noexcept
    {
        if (db <= -60.0) return 0.0;
        return std::clamp (0.8 * std::cbrt (std::pow (10.0, db / 20.0)), 0.0, 1.0);
    }

    static std::string ambComa (double x, int dec)
    {
        char b[32];
        std::snprintf (b, sizeof b, "%.*f", dec, x);
        std::string s = b;
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }

    std::string textVolum (double v)
    {
        const double db = dbDeVolum (v);
        if (db <= -59.9) return "-inf dB";
        return (db > 0.05 ? "+" : "") + ambComa (db, 1) + " dB";
    }

    std::string textPan (double v)
    {
        const int x = (int) std::lround (panDe (v) * 100.0);
        if (x == 0) return "C";
        return (x < 0 ? "L " : "R ") + std::to_string (std::abs (x));
    }

    double avanc (double x, int forma, double tensio) noexcept
    {
        x = std::clamp (x, 0.0, 1.0);
        if (forma == 2) return 0.0;
        if (forma == 1) return 0.5 - 0.5 * std::cos (x * 3.14159265358979323846);
        if (std::abs (tensio) < 1.0e-4) return x;
        const double k = 1.0 + 4.0 * std::abs (tensio);
        return tensio > 0.0 ? 1.0 - std::pow (1.0 - x, k) : std::pow (x, k);
    }

    double valorA (const std::vector<PuntAuto>& p, double c) noexcept
    {
        if (p.empty()) return -1.0;
        if (c <= p.front().compas) return p.front().valor;
        if (c >= p.back().compas) return p.back().valor;
        const auto it = std::upper_bound (p.begin(), p.end(), c, [] (double x, const PuntAuto& q) { return x < q.compas; });
        const auto& b = *it;
        const auto& a = *(it - 1);
        const double w = b.compas - a.compas;
        if (w <= 1.0e-12) return b.valor;
        return a.valor + (b.valor - a.valor) * avanc ((c - a.compas) / w, a.forma, a.tensio);
    }

    float Corba::a (double t) const noexcept
    {
        if (p.empty()) return 0.0f;
        if (t <= p.front().t) return p.front().v;
        if (t >= p.back().t) return p.back().v;
        size_t lo = 0, hi = p.size() - 1;   // p[lo].t <= t < p[hi].t
        while (hi - lo > 1)
        {
            const size_t m = (lo + hi) / 2;
            if (p[m].t <= t) lo = m; else hi = m;
        }
        const auto& x = p[lo];
        const auto& y = p[hi];
        const double w = y.t - x.t;
        if (w <= 1.0e-12) return y.v;
        return x.v + (y.v - x.v) * (float) avanc ((t - x.t) / w, x.forma, x.tensio);
    }

    Corba corbaEnSegons (const Projecte& pr, const Automatitzacio& a)
    {
        Corba c;
        c.p.reserve (a.punts.size());
        for (const auto& q : a.punts)
            c.p.push_back ({ edicio::segonsDeCompas (pr, q.compas), (float) std::clamp (q.valor, 0.0, 1.0), (unsigned char) std::clamp (q.forma, 0, 2),
                             (float) std::clamp (q.tensio, -1.0, 1.0) });
        return c;
    }

    Automatitzacio& afegeix (Projecte& p, Pista& t, const std::string& objectiu, const std::string& nom)
    {
        if (auto* x = busca (t, objectiu)) return *x;
        Automatitzacio a;
        a.id = edicio::idNou (p, 'a');
        a.objectiu = objectiu;
        a.nom = nom;
        t.autos.push_back (std::move (a));
        return t.autos.back();
    }

    Automatitzacio* busca (Pista& t, const std::string& objectiu)
    {
        for (auto& a : t.autos) if (a.objectiu == objectiu) return &a;
        return nullptr;
    }

    int posaPunt (Automatitzacio& a, const PuntAuto& pt)
    {
        PuntAuto q = pt;
        q.valor = std::clamp (q.valor, 0.0, 1.0);
        for (size_t i = 0; i < a.punts.size(); ++i)
            if (std::abs (a.punts[i].compas - q.compas) < 1.0e-9) { a.punts[i] = q; return (int) i; }
        const auto it = std::upper_bound (a.punts.begin(), a.punts.end(), q.compas, [] (double x, const PuntAuto& y) { return x < y.compas; });
        return (int) (a.punts.insert (it, q) - a.punts.begin());
    }

    void esborra (Automatitzacio& a, std::vector<int> ix)
    {
        std::sort (ix.begin(), ix.end());
        ix.erase (std::unique (ix.begin(), ix.end()), ix.end());
        for (auto it = ix.rbegin(); it != ix.rend(); ++it)
            if (*it >= 0 && *it < (int) a.punts.size()) a.punts.erase (a.punts.begin() + *it);
    }

    void mou (Automatitzacio& a, const std::vector<int>& ix, double dc, double dv)
    {
        if (ix.empty()) return;
        std::vector<char> triat (a.punts.size(), 0);
        for (int i : ix) if (i >= 0 && i < (int) a.punts.size()) triat[(size_t) i] = 1;
        // Límits: els punts triats no poden passar per sobre dels no triats.
        int primer = -1, darrer = -1;
        for (size_t i = 0; i < triat.size(); ++i) if (triat[i]) { if (primer < 0) primer = (int) i; darrer = (int) i; }
        if (primer < 0) return;
        double lo = -1.0e9, hi = 1.0e9;
        for (int i = primer - 1; i >= 0; --i) if (! triat[(size_t) i]) { lo = a.punts[(size_t) i].compas + 1.0e-6; break; }
        for (int i = darrer + 1; i < (int) a.punts.size(); ++i) if (! triat[(size_t) i]) { hi = a.punts[(size_t) i].compas - 1.0e-6; break; }
        lo = std::max (lo, -1.0);
        dc = std::clamp (dc, lo - a.punts[(size_t) primer].compas, hi - a.punts[(size_t) darrer].compas);
        for (size_t i = 0; i < triat.size(); ++i)
            if (triat[i])
            {
                a.punts[i].compas += dc;
                a.punts[i].valor = std::clamp (a.punts[i].valor + dv, 0.0, 1.0);
            }
    }

    void quantitza (Automatitzacio& a, const std::vector<int>& ix, double g)
    {
        if (g <= 1.0e-9) return;
        for (int i : ix)
            if (i >= 0 && i < (int) a.punts.size())
                a.punts[(size_t) i].compas = std::round (a.punts[(size_t) i].compas / g) * g;
        std::stable_sort (a.punts.begin(), a.punts.end(), [] (const PuntAuto& x, const PuntAuto& y) { return x.compas < y.compas; });
        // Dos punts al mateix lloc: es queda el darrer (el que s'hi ha portat).
        std::vector<PuntAuto> v;
        for (const auto& q : a.punts)
        {
            if (! v.empty() && std::abs (v.back().compas - q.compas) < 1.0e-9) v.back() = q;
            else v.push_back (q);
        }
        a.punts = std::move (v);
    }

    std::vector<PuntAuto> copia (const Automatitzacio& a, double de, double fins)
    {
        std::vector<PuntAuto> r;
        if (a.punts.empty() || fins <= de) return r;
        const auto vora = [&] (double c)
        {
            PuntAuto q;
            q.compas = c - de;
            q.valor = valorA (a.punts, c);
            return q;
        };
        r.push_back (vora (de));
        for (const auto& q : a.punts)
            if (q.compas > de + 1.0e-9 && q.compas < fins - 1.0e-9)
            {
                PuntAuto x = q;
                x.compas -= de;
                r.push_back (x);
            }
        // La forma del primer punt ha de ser la del tram on cau «de».
        for (size_t i = 0; i + 1 < a.punts.size(); ++i)
            if (a.punts[i].compas <= de && a.punts[i + 1].compas > de && a.punts[i].forma == 2) r.front().forma = 2;
        r.push_back (vora (fins));
        return r;
    }

    void substitueix (Automatitzacio& a, double de, double fins, const std::vector<PuntAuto>& nous)
    {
        if (fins < de) std::swap (de, fins);
        const double vDe = valorA (a.punts, de), vFins = valorA (a.punts, fins);
        const bool hiHavia = ! a.punts.empty();
        std::vector<PuntAuto> v;
        for (const auto& q : a.punts) if (q.compas < de - 1.0e-9 || q.compas > fins + 1.0e-9) v.push_back (q);
        a.punts = std::move (v);
        // Vores: la corba de fora es manté igual (si hi havia corba i el nou tros no comença o acaba just a la vora).
        if (hiHavia && ! nous.empty())
        {
            if (nous.front().compas > de + 1.0e-9 || std::abs (nous.front().valor - vDe) > 1.0e-6)
                if (! a.punts.empty() && a.punts.front().compas < de) posaPunt (a, { de - 1.0e-6, vDe, 0, 0.0 });
            if (nous.back().compas < fins - 1.0e-9 || std::abs (nous.back().valor - vFins) > 1.0e-6)
                if (! a.punts.empty() && a.punts.back().compas > fins) posaPunt (a, { fins + 1.0e-6, vFins, 0, 0.0 });
        }
        for (const auto& q : nous) posaPunt (a, q);
    }

    void enganxa (Automatitzacio& a, const std::vector<PuntAuto>& tros, double on)
    {
        if (tros.empty()) return;
        std::vector<PuntAuto> v = tros;
        for (auto& q : v) q.compas += on;
        substitueix (a, v.front().compas, v.back().compas, v);
    }

    void duplica (Automatitzacio& a, double de, double fins)
    {
        if (fins <= de) return;
        enganxa (a, copia (a, de, fins), fins);
    }

    std::vector<PuntAuto> simplifica (const std::vector<PuntAuto>& v, double tol)
    {
        if (v.size() <= 2) return v;
        // Ramer-Douglas-Peucker sobre (compàs, valor), amb el valor com a mesura de l'error.
        std::vector<char> queda (v.size(), 0);
        queda.front() = queda.back() = 1;
        std::vector<std::pair<size_t, size_t>> pila { { 0, v.size() - 1 } };
        while (! pila.empty())
        {
            const auto [a, b] = pila.back();
            pila.pop_back();
            double maxErr = -1.0;
            size_t k = a;
            const double w = v[b].compas - v[a].compas;
            for (size_t i = a + 1; i < b; ++i)
            {
                const double x = w > 1.0e-12 ? (v[i].compas - v[a].compas) / w : 0.0;
                const double e = std::abs (v[i].valor - (v[a].valor + (v[b].valor - v[a].valor) * x));
                if (e > maxErr) { maxErr = e; k = i; }
            }
            if (maxErr > tol)
            {
                queda[k] = 1;
                pila.push_back ({ a, k });
                pila.push_back ({ k, b });
            }
        }
        std::vector<PuntAuto> r;
        for (size_t i = 0; i < v.size(); ++i) if (queda[i]) r.push_back (v[i]);
        return r;
    }

    std::vector<int> dins (const Automatitzacio& a, double c0, double c1, double v0, double v1)
    {
        if (c1 < c0) std::swap (c0, c1);
        if (v1 < v0) std::swap (v0, v1);
        std::vector<int> r;
        for (size_t i = 0; i < a.punts.size(); ++i)
        {
            const auto& q = a.punts[i];
            if (q.compas >= c0 && q.compas <= c1 && q.valor >= v0 && q.valor <= v1) r.push_back ((int) i);
        }
        return r;
    }

    void desplaca (Pista& t, double des, double compassos)
    {
        for (auto& a : t.autos)
            for (auto& q : a.punts)
                if (q.compas >= des - 1.0e-9) q.compas += compassos;
    }
}
