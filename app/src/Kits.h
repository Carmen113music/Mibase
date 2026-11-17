#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>
#include <mutex>
#include <mibase/Bateria.h>
#include <mibase/ProjecteDisc.h>
#include "Text.h"

/** 0.20: els kits de bateria a l'ordinador: Documents\Mibase\Kits\<kit>\ (els WAV i «kit.json»).
    El kit «Intern» (sons sintetitzats) hi és sempre i no és cap carpeta.
    Importar: cada carpeta amb sons (WAV, MP3, AIFF, FLAC, OGG) es converteix en un kit; els sons es passen a WAV, se'ls treu el
    silenci del principi i del final i es classifiquen pel nom (i, si el nom no ho diu, pel so). Els originals no es toquen. */
namespace kits
{
    namespace bt = mibase::bateria;

    inline juce::File carpeta()
    {
        auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Kits");
        f.createDirectory();
        return f;
    }

    inline std::string ultimKit()
    {
        auto f = carpeta().getChildFile ("ultim-kit.txt");
        auto s = f.existsAsFile() ? f.loadFileAsString().trim().toStdString() : std::string();
        return s.empty() ? std::string (bt::kitIntern) : s;
    }
    inline void setUltimKit (const std::string& nom) { carpeta().getChildFile ("ultim-kit.txt").replaceWithText (juce::String::fromUTF8 (nom.c_str())); }

    /** Tots els kits: el d'Intern primer i després els de la carpeta. */
    inline std::vector<bt::Kit> llista()
    {
        std::vector<bt::Kit> v;
        bt::Kit intern;
        intern.nom = bt::kitIntern;
        for (auto& f : bt::filesInternes()) intern.sons.push_back ({ f.so, f.nom, f.tipus, {} });
        v.push_back (intern);
        auto d = bt::llegeixKits (carpeta().getFullPathName().toStdString());
        v.insert (v.end(), d.begin(), d.end());
        return v;
    }

    inline std::optional<bt::Kit> kit (const std::string& nom)
    {
        for (auto& k : llista()) if (k.nom == nom) return k;
        return std::nullopt;
    }

    /** Posa el kit a la pista (manté els ajustos de les files que ja hi eren). Si no el troba, el d'Intern. */
    inline void posaKit (mibase::Pista& t, const std::string& nom)
    {
        auto k = kit (nom);
        if (! k || k->nom == bt::kitIntern)
        {
            auto noves = bt::filesInternes();
            if (! t.files.empty())
                for (auto& f : noves)
                    for (auto& v : t.files)
                        if (v.tipus == f.tipus) { auto so = f.so; f = v; f.so = so; break; }
            t.files = noves;
            t.kit = bt::kitIntern;
            return;
        }
        t.files = bt::filesDeKit (*k, t.files);
        t.kit = k->nom;
        setUltimKit (k->nom);
    }

    /** El fitxer d'un so (relatiu a la carpeta dels kits). */
    inline juce::File fitxerSo (const std::string& so)
    {
        return carpeta().getChildFile (juce::String::fromUTF8 (so.c_str()));
    }

    inline juce::AudioFormatManager& formats()
    {
        static juce::AudioFormatManager f;
        static std::once_flag un;
        std::call_once (un, [] { f.registerBasicFormats(); });
        return f;
    }

    /** Llegeix un fitxer d'àudio (com a molt «maxSeg» segons). */
    /** 0.28.1: amb un AudioFormatManager concret (per llegir des d'un fil a part). */
    inline std::shared_ptr<bt::Mostra> llegeix (juce::AudioFormatManager& fm, const juce::File& f, double maxSeg);

    inline std::shared_ptr<bt::Mostra> llegeix (const juce::File& f, double maxSeg = 12.0) { return llegeix (formats(), f, maxSeg); }

    inline std::shared_ptr<bt::Mostra> llegeix (juce::AudioFormatManager& fm, const juce::File& f, double maxSeg)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0) return nullptr;
        const int n = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (maxSeg * r->sampleRate));
        juce::AudioBuffer<float> b ((int) std::min (2u, r->numChannels), n);
        r->read (&b, 0, n, 0, true, b.getNumChannels() > 1);
        auto m = std::make_shared<bt::Mostra>();
        m->fs = r->sampleRate;
        m->L.assign (b.getReadPointer (0), b.getReadPointer (0) + n);
        if (b.getNumChannels() > 1) m->R.assign (b.getReadPointer (1), b.getReadPointer (1) + n);
        return m;
    }

    /** El so d'una fila, a punt per sonar (amb memòria: el mateix fitxer només es llegeix un cop). Fil de la interfície. */
    inline std::shared_ptr<const bt::Mostra> carrega (const std::string& so)
    {
        if (so.empty()) return nullptr;
        if (so.rfind ("intern:", 0) == 0) return bt::mostraInterna (so, 48000.0);
        static std::map<std::string, std::shared_ptr<const bt::Mostra>> cau;
        const auto f = fitxerSo (so);
        const auto clau = so + "|" + std::to_string (f.getLastModificationTime().toMilliseconds());
        if (auto it = cau.find (clau); it != cau.end()) return it->second;
        auto m = llegeix (f);
        cau[clau] = m;
        return m;
    }

    /** Un sampler nou per a la pista. */
    inline std::shared_ptr<bt::SamplerBateria> sampler (const mibase::Pista& t, double fs, int bloc)
    {
        std::vector<bt::SamplerBateria::So> sons;
        for (auto& f : t.files) sons.push_back ({ f.nota, carrega (f.so) });
        auto s = std::make_shared<bt::SamplerBateria> (sons);
        s->posaParams (t.files);
        s->prepara (fs, bloc);
        return s;
    }

    // ---- Importar ----
    struct ResultatImport
    {
        juce::StringArray kits;   // noms dels kits nous
        int sons = 0, perSo = 0, error = 0;
    };

    inline bool esSo (const juce::File& f)
    {
        return f.hasFileExtension ("wav;mp3;aif;aiff;flac;ogg");
    }

    /** Importa una carpeta: ella mateixa (si té sons) i cada subcarpeta amb sons, cadascuna com un kit. Fil de la interfície
        (pot trigar uns segons amb molts sons). */
    inline ResultatImport importa (const juce::File& origen)
    {
        ResultatImport res;
        juce::Array<juce::File> dirs;
        dirs.add (origen);
        for (auto& d : origen.findChildFiles (juce::File::findDirectories, true)) dirs.add (d);
        for (auto& d : dirs)
        {
            auto sons = d.findChildFiles (juce::File::findFiles, false);
            sons.removeIf ([] (const juce::File& f) { return ! esSo (f); });
            if (sons.isEmpty()) continue;
            sons.sort();
            // Nom del kit: el de la carpeta (si ja n'hi ha un d'igual, «Nom 2»…).
            const auto base = juce::File::createLegalFileName (d.getFileName());
            auto desti = carpeta().getChildFile (base);
            for (int k = 2; desti.exists(); ++k) desti = carpeta().getChildFile (base + " " + juce::String (k));
            desti.createDirectory();
            bt::Kit kit;
            kit.nom = desti.getFileName().toStdString();
            kit.carpeta = desti.getFullPathName().toStdString();
            for (auto& f : sons)
            {
                auto m = llegeix (f);
                if (m == nullptr) { ++res.error; continue; }
                // Fora el silenci del principi i del final (-60 dB).
                const size_t n = m->L.size();
                auto fort = [&] (size_t i) { return std::abs (m->L[i]) > 0.001f || (! m->R.empty() && std::abs (m->R[i]) > 0.001f); };
                size_t a = 0, b = n;
                while (a < n && ! fort (a)) ++a;
                while (b > a && ! fort (b - 1)) --b;
                if (b <= a) { ++res.error; continue; }
                a = a > 16 ? a - 16 : 0;   // uns pocs mostres abans, per no tallar l'atac
                juce::AudioBuffer<float> out (m->R.empty() ? 1 : 2, (int) (b - a));
                out.copyFrom (0, 0, m->L.data() + a, (int) (b - a));
                if (! m->R.empty()) out.copyFrom (1, 0, m->R.data() + a, (int) (b - a));
                // Nom del fitxer nou.
                auto nom = juce::File::createLegalFileName (f.getFileNameWithoutExtension());
                auto fitxer = desti.getChildFile (nom + ".wav");
                for (int k = 2; fitxer.exists(); ++k) fitxer = desti.getChildFile (nom + " " + juce::String (k) + ".wav");
                {
                    std::unique_ptr<juce::OutputStream> os = fitxer.createOutputStream();
                    juce::WavAudioFormat wav;
                    auto w = os != nullptr ? wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (m->fs)
                                                                         .withNumChannels (out.getNumChannels()).withBitsPerSample (24))
                                           : nullptr;
                    if (w == nullptr || ! w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples())) { ++res.error; continue; }
                }
                bt::SoKit s;
                s.fitxer = fitxer.getFileName().toStdString();
                s.nom = fitxer.getFileNameWithoutExtension().toStdString();
                s.original = f.getFullPathName().toStdString();
                s.tipus = bt::classificaNom (f.getFileName().toStdString());
                if (s.tipus.empty())
                {
                    std::vector<float> mono (b - a);
                    for (size_t i = 0; i < mono.size(); ++i) mono[i] = m->R.empty() ? m->L[a + i] : 0.5f * (m->L[a + i] + m->R[a + i]);
                    s.tipus = bt::classificaSo (mono.data(), (int) mono.size(), m->fs);
                    ++res.perSo;
                }
                kit.sons.push_back (s);
                ++res.sons;
            }
            if (kit.sons.empty()) { desti.deleteRecursively(); continue; }
            bt::desaKit (kit);
            res.kits.add (juce::String::fromUTF8 (kit.nom.c_str()));
        }
        return res;
    }

    /** Desa el kit amb els tipus corregits. */
    inline bool desa (const bt::Kit& k) { return k.nom != bt::kitIntern && bt::desaKit (k); }

    /** 0.31: desa els sons i les files d'una pista de bateria com un kit nou (una carpeta amb còpies dels sons; els
        sons interns es desen com a WAV). Retorna el nom del kit desat (si ja n'hi havia un amb aquest nom, «Nom 2»…)
        o buit si no s'ha pogut; «error» diu per què. */
    inline juce::String desaDePista (const mibase::Pista& t, const juce::String& nomDemanat, juce::String& error)
    {
        auto nom = juce::File::createLegalFileName (nomDemanat.trim());
        if (nom.isEmpty() || nom == juce::String (bt::kitIntern)) { error = "Aquest nom no es pot fer servir."_ca; return {}; }
        auto dir = carpeta().getChildFile (nom);
        for (int i = 2; dir.exists(); ++i) dir = carpeta().getChildFile (nom + " " + juce::String (i));
        if (! dir.createDirectory()) { error = "No s'ha pogut crear la carpeta del kit."_ca; return {}; }

        bt::Kit kit;
        kit.nom = dir.getFileName().toStdString();
        kit.carpeta = dir.getFullPathName().toStdString();
        juce::StringArray usats;
        int noLlegits = 0;
        for (const auto& f : t.files)
        {
            auto base = juce::File::createLegalFileName (juce::String::fromUTF8 (f.nom.c_str()).trim());
            if (base.isEmpty()) base = "So";
            juce::String fitxer;
            if (f.so.rfind ("intern:", 0) == 0)
            {
                // so sintetitzat: el desem com a WAV
                auto m = bt::mostraInterna (f.so, 48000.0);
                if (m == nullptr || m->L.empty()) { ++noLlegits; continue; }
                fitxer = base + ".wav";
                for (int i = 2; usats.contains (fitxer, true); ++i) fitxer = base + " " + juce::String (i) + ".wav";
                const bool estereo = ! m->R.empty();
                juce::AudioBuffer<float> b (estereo ? 2 : 1, (int) m->L.size());
                b.copyFrom (0, 0, m->L.data(), (int) m->L.size());
                if (estereo) b.copyFrom (1, 0, m->R.data(), (int) juce::jmin (m->R.size(), m->L.size()));
                std::unique_ptr<juce::OutputStream> os = dir.getChildFile (fitxer).createOutputStream();
                juce::WavAudioFormat wav;
                auto w = os != nullptr ? wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (m->fs)
                                                                     .withNumChannels (b.getNumChannels()).withBitsPerSample (24))
                                       : nullptr;
                if (w == nullptr || ! w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples())) { ++noLlegits; continue; }
            }
            else
            {
                const auto origen = fitxerSo (f.so);
                if (! origen.existsAsFile()) { ++noLlegits; continue; }
                fitxer = base + origen.getFileExtension();
                for (int i = 2; usats.contains (fitxer, true); ++i) fitxer = base + " " + juce::String (i) + origen.getFileExtension();
                if (! origen.copyFileTo (dir.getChildFile (fitxer))) { ++noLlegits; continue; }
            }
            usats.add (fitxer);
            kit.sons.push_back ({ fitxer.toStdString(), f.nom, f.tipus, f.so });
            auto fila = f;
            fila.so = fitxer.toStdString();
            kit.files.push_back (fila);
        }
        if (kit.files.empty() || ! bt::desaKit (kit))
        {
            dir.deleteRecursively();
            error = "La pista no té cap so que es pugui desar."_ca;
            return {};
        }
        if (noLlegits > 0) error = juce::String (noLlegits) + (noLlegits == 1 ? " so no s'ha trobat i no s'ha desat."_ca : " sons no s'han trobat i no s'han desat."_ca);
        return dir.getFileName();
    }

    /** Adreça dels kits de Riffs (un zip amb una carpeta per kit). */
    inline juce::URL urlKitsRiffs()
    {
        return juce::URL ("https://github.com/Carmen113music/carmen113music/releases/download/kits/kits-bateria.zip");
    }
}
