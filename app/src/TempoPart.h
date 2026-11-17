#pragma once

#include "DialegText.h"
#include "EstatEstudi.h"

/** 0.28: el tempo i el compàs propis d'una part (canvi brusc a l'inici de la part; al final torna al de la cançó).
    Menú del botó dret d'una part (a la regla i a la taula d'estructura). */
namespace tempoPart
{
    inline juce::String text (const mibase::Projecte& p, const mibase::Part& x)
    {
        const double t = x.tempo > 0.0 ? x.tempo : p.tempo;
        const int n = x.compasNum > 0 ? x.compasNum : p.compasNum, d = x.compasNum > 0 && x.compasDen > 0 ? x.compasDen : p.compasDen;
        return juce::String (t, 0) + " bpm · " + juce::String (n) + "/" + juce::String (d);
    }

    inline void canvia (EstatEstudi& estat, const std::string& id, const std::function<void (mibase::Part&)>& f, const juce::String& evolucio)
    {
        auto* x = mibase::edicio::part (estat.projecte, id);
        if (x == nullptr) return;
        estat.desfes.abans (estat.projecte);
        // 0.42: els clips MIDI i els acords queden als mateixos compassos encara que canviï el tempo de la part
        mibase::midi::ambMusicaFixa (estat.projecte, [&] (mibase::Projecte& q) { if (auto* y = mibase::edicio::part (q, id)) f (*y); });
        estat.canviat (evolucio);
    }

    inline void demanaTempo (juce::Component& sobre, EstatEstudi& estat, const std::string& id)
    {
        static std::unique_ptr<DialegText> dialeg;
        auto* x = mibase::edicio::part (estat.projecte, id);
        if (x == nullptr) return;
        const auto nom = dibuix::utf8 (x->nom);
        dialeg = std::make_unique<DialegText> ("Tempo de la part «"_ca + nom + "»"_ca,
            "En bpm (de 30 a 300). Buit = el de la cançó. Canvia de cop a l'inici de la part i torna al de la cançó al final; els clips MIDI segueixen els compassos, els d'àudio no es mouen."_ca,
            "D'acord",
            [] (const juce::String& t) -> juce::String
            {
                const auto v = t.trim().replaceCharacter (',', '.');
                if (v.isEmpty()) return {};
                if (! v.containsOnly ("0123456789.")) return "Escriu només el número (o deixa-ho buit)."_ca;
                const double b = v.getDoubleValue();
                return b < 30.0 || b > 300.0 ? juce::String ("Ha de ser entre 30 i 300.") : juce::String();
            },
            [&estat, id, nom] (const juce::String& t)
            {
                const double b = t.trim().replaceCharacter (',', '.').getDoubleValue();
                canvia (estat, id, [b] (mibase::Part& q) { q.tempo = b >= 30.0 ? b : 0.0; },
                        "Part «"_ca + nom + "»: " + (b >= 30.0 ? juce::String (b, 0) + " bpm" : "tempo de la cançó"_ca));
            }, x->tempo > 0.0 ? juce::String (x->tempo, 0) : juce::String());
        DialegText::mostra (sobre, dialeg);
    }

    /** Afegeix al menú les opcions de tempo i compàs (ids 900–930) i en torna el tractament. */
    inline void afegeix (juce::PopupMenu& m, const mibase::Projecte& p, const mibase::Part& x)
    {
        juce::PopupMenu c;
        c.addItem (910, "El de la cançó"_ca, true, x.compasNum == 0);
        static const int compassos[][2] = { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
        for (int i = 0; i < 8; ++i)
            c.addItem (911 + i, juce::String (compassos[i][0]) + "/" + juce::String (compassos[i][1]), true,
                       x.compasNum == compassos[i][0] && x.compasDen == compassos[i][1]);
        m.addSectionHeader ("Tempo i compàs: "_ca + text (p, x));
        m.addItem (900, "Tempo de la part…"_ca);
        m.addSubMenu ("Compàs de la part"_ca, c);
        m.addItem (901, "Com la cançó (treu el tempo i el compàs propis)"_ca, x.tempo > 0.0 || x.compasNum > 0);
    }

    /** Torna cert si «r» era una d'aquestes opcions (i ja l'ha feta). */
    inline bool tracta (int r, juce::Component& sobre, EstatEstudi& estat, const std::string& id)
    {
        if (r < 900 || r > 930) return false;
        auto* x = mibase::edicio::part (estat.projecte, id);
        if (x == nullptr) return true;
        const auto nom = dibuix::utf8 (x->nom);
        if (r == 900)
        {
            juce::Component::SafePointer<juce::Component> s (&sobre);
            juce::MessageManager::callAsync ([s, &estat, id] { if (s != nullptr) demanaTempo (*s, estat, id); });
        }
        else if (r == 901) canvia (estat, id, [] (mibase::Part& q) { q.tempo = 0.0; q.compasNum = q.compasDen = 0; }, "Part «"_ca + nom + "»: com la cançó"_ca);
        else if (r == 910) canvia (estat, id, [] (mibase::Part& q) { q.compasNum = q.compasDen = 0; }, "Part «"_ca + nom + "»: compàs de la cançó"_ca);
        else if (r >= 911 && r <= 918)
        {
            static const int compassos[][2] = { { 2, 4 }, { 3, 4 }, { 4, 4 }, { 5, 4 }, { 6, 8 }, { 7, 8 }, { 9, 8 }, { 12, 8 } };
            const int num = compassos[r - 911][0], den = compassos[r - 911][1];
            canvia (estat, id, [num, den] (mibase::Part& q) { q.compasNum = num; q.compasDen = den; },
                    "Part «"_ca + nom + "»: compàs "_ca + juce::String (num) + "/" + juce::String (den));
        }
        return true;
    }
}
