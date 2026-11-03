#pragma once

#include "Tema.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <mibase/ProjecteDisc.h>

/** Icones pròpies (dibuixades amb línies) i petites ajudes de dibuix i de camins. */
namespace dibuix
{
    enum class Icona { Carpeta, Fitxer, Disc, Rellotge, Lupa, Mes, Play, Obre, Ajustos, Tanca, Fletxa, FletxaAvall, Casa,
                       Principi, Grava, Desfes, Refes, Metronom, Importa };

    inline juce::Path icona (Icona i, juce::Rectangle<float> r)
    {
        // Totes es dibuixen en una quadrícula de 24 x 24 i s'escalen.
        juce::Path p;
        switch (i)
        {
            case Icona::Carpeta:
                p.startNewSubPath (3, 7); p.quadraticTo (3, 5, 5, 5); p.lineTo (9, 5); p.lineTo (11, 7); p.lineTo (19, 7);
                p.quadraticTo (21, 7, 21, 9); p.lineTo (21, 17); p.quadraticTo (21, 19, 19, 19); p.lineTo (5, 19);
                p.quadraticTo (3, 19, 3, 17); p.closeSubPath();
                break;
            case Icona::Fitxer:
                p.startNewSubPath (6, 3); p.lineTo (14, 3); p.lineTo (18, 7); p.lineTo (18, 21); p.lineTo (6, 21); p.closeSubPath();
                p.startNewSubPath (14, 3); p.lineTo (14, 7); p.lineTo (18, 7);
                break;
            case Icona::Disc:
                p.addRoundedRectangle (3.0f, 6.0f, 18.0f, 12.0f, 2.0f);
                p.startNewSubPath (7, 14); p.lineTo (9, 14);
                break;
            case Icona::Rellotge:
                p.addEllipse (3, 3, 18, 18);
                p.startNewSubPath (12, 7); p.lineTo (12, 12); p.lineTo (15, 14);
                break;
            case Icona::Lupa:
                p.addEllipse (4, 4, 14, 14);
                p.startNewSubPath (16.5f, 16.5f); p.lineTo (20, 20);
                break;
            case Icona::Mes:
                p.startNewSubPath (12, 5); p.lineTo (12, 19);
                p.startNewSubPath (5, 12); p.lineTo (19, 12);
                break;
            case Icona::Play:
                p.startNewSubPath (7, 5); p.lineTo (19, 12); p.lineTo (7, 19); p.closeSubPath();
                break;
            case Icona::Obre:
                p.startNewSubPath (3, 17); p.lineTo (3, 7); p.quadraticTo (3, 5, 5, 5); p.lineTo (9, 5); p.lineTo (11, 7);
                p.lineTo (19, 7); p.quadraticTo (21, 7, 21, 9); p.lineTo (21, 10);
                p.startNewSubPath (3, 19); p.lineTo (7, 10); p.lineTo (22, 10); p.lineTo (18, 19); p.closeSubPath();
                break;
            case Icona::Ajustos:
                p.startNewSubPath (4, 6); p.lineTo (14, 6); p.startNewSubPath (18, 6); p.lineTo (20, 6);
                p.startNewSubPath (4, 12); p.lineTo (8, 12); p.startNewSubPath (12, 12); p.lineTo (20, 12);
                p.startNewSubPath (4, 18); p.lineTo (16, 18);
                p.addEllipse (14, 4, 4, 4); p.addEllipse (8, 10, 4, 4); p.addEllipse (16, 16, 4, 4);
                break;
            case Icona::Tanca:
                p.startNewSubPath (6, 6); p.lineTo (18, 18);
                p.startNewSubPath (18, 6); p.lineTo (6, 18);
                break;
            case Icona::Fletxa:
                p.startNewSubPath (9, 6); p.lineTo (15, 12); p.lineTo (9, 18);
                break;
            case Icona::FletxaAvall:
                p.startNewSubPath (6, 9); p.lineTo (12, 15); p.lineTo (18, 9);
                break;
            case Icona::Principi:
                p.startNewSubPath (6, 5); p.lineTo (6, 19);
                p.startNewSubPath (19, 5); p.lineTo (9, 12); p.lineTo (19, 19); p.closeSubPath();
                break;
            case Icona::Grava:
                p.addEllipse (6, 6, 12, 12);
                break;
            case Icona::Desfes:
                p.startNewSubPath (9, 5); p.lineTo (4, 10); p.lineTo (9, 15);
                p.startNewSubPath (4, 10); p.lineTo (14, 10); p.quadraticTo (20, 10, 20, 15); p.quadraticTo (20, 20, 14, 20); p.lineTo (10, 20);
                break;
            case Icona::Refes:
                p.startNewSubPath (15, 5); p.lineTo (20, 10); p.lineTo (15, 15);
                p.startNewSubPath (20, 10); p.lineTo (10, 10); p.quadraticTo (4, 10, 4, 15); p.quadraticTo (4, 20, 10, 20); p.lineTo (14, 20);
                break;
            case Icona::Metronom:
                p.startNewSubPath (9, 3.5f); p.lineTo (15, 3.5f); p.lineTo (19, 20); p.lineTo (5, 20); p.closeSubPath();
                p.startNewSubPath (12, 16); p.lineTo (17, 7);
                break;
            case Icona::Importa:
                p.startNewSubPath (12, 3); p.lineTo (12, 14);
                p.startNewSubPath (7, 9); p.lineTo (12, 14); p.lineTo (17, 9);
                p.startNewSubPath (4, 15); p.lineTo (4, 20); p.lineTo (20, 20); p.lineTo (20, 15);
                break;
            case Icona::Casa:
                p.startNewSubPath (4, 11); p.lineTo (12, 4); p.lineTo (20, 11);
                p.startNewSubPath (6, 9.5f); p.lineTo (6, 20); p.lineTo (18, 20); p.lineTo (18, 9.5f);
                break;
        }
        p.applyTransform (juce::AffineTransform::scale (r.getWidth() / 24.0f, r.getHeight() / 24.0f).translated (r.getX(), r.getY()));
        return p;
    }

    inline void pintaIcona (juce::Graphics& g, Icona i, juce::Rectangle<float> r, juce::Colour color,
                            float gruix = 1.8f, juce::Colour ple = juce::Colours::transparentBlack)
    {
        auto p = icona (i, r);
        if (! ple.isTransparent())
        {
            g.setColour (ple);
            g.fillPath (p);
        }
        g.setColour (color);
        if (i == Icona::Play || i == Icona::Grava)
            g.fillPath (p);
        else
            g.strokePath (p, juce::PathStrokeType (gruix * r.getWidth() / 24.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /** Logotip: quadrat vermell amb una petita ona. */
    inline void pintaLogo (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (tema::accent);
        g.fillRoundedRectangle (r, r.getWidth() * 0.27f);
        juce::Path p;
        const float s = r.getWidth() / 26.0f;
        auto barra = [&] (float x, float y1, float y2) { p.startNewSubPath (r.getX() + x * s, r.getY() + y1 * s); p.lineTo (r.getX() + x * s, r.getY() + y2 * s); };
        barra (6, 13, 13.01f); barra (10, 9, 17); barra (14, 6, 20); barra (18, 10, 16); barra (21, 12, 14);
        g.setColour (tema::fons);
        g.strokePath (p, juce::PathStrokeType (2.2f * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    inline juce::Font lletra (float mida, bool negreta = false)
    {
        return juce::Font (juce::FontOptions (mida, negreta ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font mono (float mida)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), mida, juce::Font::plain));
    }

    /** Majúscules també per a les lletres amb accent (toUpperCase no sempre les fa). */
    inline juce::String majuscules (const juce::String& t)
    {
        auto r = t.toUpperCase();
        const juce::String minus = juce::String::fromUTF8 ("àèéíïòóúüç");
        const juce::String majus = juce::String::fromUTF8 ("ÀÈÉÍÏÒÓÚÜÇ");
        for (int i = 0; i < minus.length(); ++i)
            r = r.replaceCharacter (minus[i], majus[i]);
        return r;
    }

    /** Títol de secció petit, en majúscules. */
    inline void pintaEtiquetaSeccio (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour (tema::suau);
        g.setFont (lletra (11.5f, true));
        g.drawText (majuscules (text), r, juce::Justification::centredLeft);
    }

    // ---- Pont entre juce::File i el motor (camins UTF-8) ----
    inline std::filesystem::path cami (const juce::File& f)
    {
        return mibase::disc::cami (f.getFullPathName().toStdString());
    }

    inline juce::File fitxer (const std::filesystem::path& p)
    {
        return juce::File (juce::String::fromUTF8 (mibase::disc::text (p).c_str()));
    }

    inline juce::String utf8 (const std::string& s)
    {
        return juce::String::fromUTF8 (s.c_str(), (int) s.size());
    }

    /** «Avui, 08:12», «Ahir, 22:40», «Dissabte», «28/10» o «28/10/2025». */
    inline juce::String quan (juce::int64 milisegons)
    {
        if (milisegons <= 0)
            return {};

        const juce::Time t (milisegons);
        const auto ara = juce::Time::getCurrentTime();
        const juce::Time avui (ara.getYear(), ara.getMonth(), ara.getDayOfMonth(), 0, 0);
        const auto dies = (int) std::floor ((double) (avui.toMilliseconds() - juce::Time (t.getYear(), t.getMonth(), t.getDayOfMonth(), 0, 0).toMilliseconds()) / 86400000.0 + 0.5);

        static const char* diesSetmana[] = { "Diumenge", "Dilluns", "Dimarts", "Dimecres", "Dijous", "Divendres", "Dissabte" };

        if (dies <= 0) return "Avui, "_ca + t.formatted ("%H:%M");
        if (dies == 1) return "Ahir, "_ca + t.formatted ("%H:%M");
        if (dies < 7)  return juce::String (diesSetmana[t.getDayOfWeek()]);
        if (t.getYear() == ara.getYear()) return t.formatted ("%d/%m");
        return t.formatted ("%d/%m/%Y");
    }

    inline juce::String araIso()   { return juce::Time::getCurrentTime().formatted ("%Y-%m-%dT%H:%M:%S"); }
    inline juce::String araText()  { return juce::Time::getCurrentTime().formatted ("%d/%m/%Y %H:%M"); }
    inline juce::String araPerNom() { return juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S"); }
}
